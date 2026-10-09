// Windows-only check (run under Wine or on Windows): the native folder walker gives the same files as the portable one.
//   x86_64-w64-mingw32-g++ -std=c++17 -O2 -static -o walk_equiv.exe src/tests/walk_equiv.cpp
#include <cstdio>
#include <iostream>
#include "../fastwalk.hpp"
using namespace rc;

static std::string wu(const fs::path& p) { return p.u8string(); }
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static std::map<std::string, uint64_t> collect(bool native, const std::string& root, int* rcOut = nullptr, const std::atomic<bool>* cancel = nullptr) {
    std::map<std::string, uint64_t> m;
    auto cb = [&](const std::string& rel, uint64_t size, int64_t) { m[rel] = size; };
    int r = native ? nativeWalkFiles(root, cb, cancel) : walkFilesPortable(root, cb, cancel);
    if (rcOut) *rcOut = r;
    return m;
}

int main(int argc, char** argv) {
    fs::path T = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "rc_walk_equiv";
    std::error_code ec;
    fs::remove_all(T, ec);
    fs::create_directories(T, ec);
    auto put = [&](const fs::path& p, const std::string& c) { fs::create_directories(p.parent_path(), ec); std::ofstream f(p, std::ios::binary); f << c; };
    put(T / "root.txt", "r");
    put(T / "common/traits/00_Traits.TXT", "abc");
    put(T / "Events/E1.txt", "12345");
    put(T / "gfx/x.dds", "");
    put(T / "localization/english/a_l_english.yml", "l");
    put(T / ".git/config", "g"); put(T / ".github/x.yml", "g"); put(T / "common/.svn/entries", "s");
    put(T / "Thumbs.db", "t"); put(T / "gfx/thumbs.db", "t"); put(T / "gfx/Desktop.ini", "d");
    put(fs::u8path(wu(T) + "/gfx/\xC3\xBCn\xC3\xAF/\xE6\x97\xA5\xE6\x9C\xAC.txt"), "unicode");
    put(fs::u8path(wu(T) + "/common/\xC3\x84rger/File With Spaces.txt"), "sp");
    fs::create_directories(T / "empty/inner", ec);
    std::string root = wu(T);
    int ra = 0, rb = 0;
    auto a = collect(false, root, &ra), b = collect(true, root, &rb);
    CHECK(ra == WALK_OK && rb == WALK_OK);
    CHECK(a == b);
    for (auto& kv : b) if (kv.first.find('\xC3') != std::string::npos || kv.first.find('\xE6') != std::string::npos) std::printf("unicode entry: %s\n", kv.first.c_str());
    CHECK(b.count("common/traits/00_traits.txt") && b["events/e1.txt"] == 5 && b.count("gfx/x.dds") && b["gfx/x.dds"] == 0);
    CHECK(b.count("root.txt") && !b.count(".git/config") && !b.count("thumbs.db") && !b.count("gfx/thumbs.db") && !b.count("gfx/desktop.ini") && !b.count("common/.svn/entries"));
    CHECK(b.count("gfx/\xC3\xBCn\xC3\xAF/\xE6\x97\xA5\xE6\x9C\xAC.txt") == 1 && b.count("common/\xC3\x84rger/file with spaces.txt") == 1);
    // trailing slashes and forward slashes in the root
    CHECK(collect(true, root + "/") == b && collect(true, root + "\\") == b);
    // an unreadable or missing root, and cancelling
    int rm = 0;
    collect(true, root + "/nope", &rm); CHECK(rm == WALK_FAILED);
    collect(false, root + "/nope", &rm); CHECK(rm == WALK_FAILED || rm == WALK_OK);   // the portable one reports it only for some errors
    std::atomic<bool> stop(true);
    collect(true, root, &rm, &stop); CHECK(rm == WALK_CANCELLED);
    // through the indexer: same result with the native walker installed and without
    g_fastWalk = nullptr;
    auto m1 = indexModFiles(root);
    installFastWalker();
    auto m2 = indexModFiles(root);
    CHECK(m1.complete && m2.complete);
    std::sort(m2.files.begin(), m2.files.end());
    CHECK(m2.files == m1.files);
    auto m3 = indexModFiles(root);
    CHECK(m3.deep == m2.deep && !m3.deep.empty());
    put(T / "common/traits/new.txt", "n");
    CHECK(indexModFiles(root).deep != m3.deep);
    // a path far beyond 260 characters
    std::wstring cur = L"\\\\?\\" + fwWiden(root);
    for (auto& c : cur) if (c == L'/') c = L'\\';
    for (int i = 0; i < 12; i++) { cur += L"\\folder_with_a_rather_long_name_number_" + std::to_wstring(i); CreateDirectoryW(cur.c_str(), nullptr); }
    HANDLE h = CreateFileW((cur + L"\\deep.txt").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, "deep", 4, &w, nullptr); CloseHandle(h); }
    auto c2 = collect(true, root, &rm);
    bool foundDeep = false;
    for (auto& kv : c2) if (kv.first.size() > 260 && kv.first.find("deep.txt") != std::string::npos && kv.second == 4) foundDeep = true;
    CHECK(rm == WALK_OK && foundDeep);
    std::printf(fails ? "FAILED (%d)\n" : "WALK EQUIVALENCE OK\n", fails);
    return fails ? 1 : 0;
}
