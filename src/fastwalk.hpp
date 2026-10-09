// Native Windows folder walker. Listing a folder with FindFirstFileEx gives names, sizes and times in one go, and is several times
// faster than the standard library's directory iterator, which asks for the size and time of every file separately.
// Gives exactly what rc::walkFilesPortable gives (a test compares them).
#pragma once
#include <windows.h>
#include "core.hpp"

namespace rc {

inline std::wstring fwWiden(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}
inline std::string fwNarrow(const wchar_t* w, size_t len) {
    if (!len) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w, (int)len, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, (int)len, &s[0], n, nullptr, nullptr);
    return s;
}

inline int nativeWalkFiles(const std::string& rootUtf8, const WalkCallback& f, const std::atomic<bool>* cancel) {
    std::wstring root = fwWiden(rootUtf8);
    if (root.empty()) return WALK_FAILED;
    for (auto& c : root) if (c == L'/') c = L'\\';
    while (root.size() > 3 && root.back() == L'\\') root.pop_back();
    // "\\?\" lifts the 260 character limit for absolute paths (Workshop folders plus deep mod paths can pass it)
    if (root.size() >= 3 && root[1] == L':' && root[2] == L'\\') root = L"\\\\?\\" + root;
    else if (root.size() > 2 && root[0] == L'\\' && root[1] == L'\\' && root[2] != L'?' && root[2] != L'.') root = L"\\\\?\\UNC\\" + root.substr(2);
    struct Dir { std::wstring path; std::string rel; };      // rel: lower case, '/' after every folder name, "" for the root
    std::vector<Dir> stack;
    stack.push_back({root, ""});
    bool failed = false;
    while (!stack.empty()) {
        if (cancel && cancel->load()) return WALK_CANCELLED;
        Dir d = std::move(stack.back());
        stack.pop_back();
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileExW((d.path + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (h == INVALID_HANDLE_VALUE) {
            DWORD e = GetLastError();
            if (!(e == ERROR_ACCESS_DENIED && !d.rel.empty())) failed = true;      // an unreadable sub-folder is skipped, like the portable walker does
            continue;
        }
        do {
            const wchar_t* n = fd.cFileName;
            if (n[0] == L'.' && (!n[1] || (n[1] == L'.' && !n[2]))) continue;
            std::string leaf = lower(fwNarrow(n, wcslen(n)));
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) || skipDirName(leaf)) continue;   // links are never followed
                stack.push_back({d.path + L"\\" + n, d.rel + leaf + "/"});
            } else {
                if (skipFileName(leaf)) continue;
                uint64_t size = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
                int64_t mt = (int64_t)(((uint64_t)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime);
                f(d.rel + leaf, size, mt);
            }
        } while (FindNextFileW(h, &fd));
        DWORD e = GetLastError();
        FindClose(h);
        if (e != ERROR_NO_MORE_FILES) failed = true;
    }
    return failed ? WALK_FAILED : WALK_OK;
}

inline void installFastWalker() { g_fastWalk = nativeWalkFiles; }

}  // namespace rc
