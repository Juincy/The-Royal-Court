// The Royal Court - share codes.
// A playset as one line of text that can be pasted into a chat: "RC1:" followed by URL-safe base64.
// Inside: a format byte, the playset name, and the mods in load order (Steam Workshop id or, for a local mod, its name, plus on/off).
// Short form: about 5 bytes a Workshop mod (a 100-mod playset is roughly 700 characters). Long form also carries every mod's name,
// so the person who receives it can see what is missing. A checksum catches a code that was cut or edited on the way.
#pragma once
#include "core.hpp"

namespace rc {

inline uint32_t crc32Of(const std::string& s) {
    static uint32_t tab[256]; static bool init = false;
    if (!init) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1; tab[i] = c; } init = true; }
    uint32_t c = 0xFFFFFFFFu;
    for (unsigned char ch : s) c = tab[(c ^ ch) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
inline void putVar(std::string& o, uint64_t v) { while (v >= 0x80) { o += (char)(0x80 | (v & 0x7F)); v >>= 7; } o += (char)v; }
inline bool getVar(const std::string& s, size_t& i, uint64_t& v) {
    v = 0;
    for (int sh = 0; sh < 63 && i < s.size(); sh += 7) {
        unsigned char b = (unsigned char)s[i++];
        v |= (uint64_t)(b & 0x7F) << sh;
        if (!(b & 0x80)) return true;
    }
    return false;
}
inline std::string b64urlEncode(const std::string& in) {
    static const char* A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string o;
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        uint32_t v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8) | (unsigned char)in[i + 2];
        o += A[v >> 18]; o += A[(v >> 12) & 63]; o += A[(v >> 6) & 63]; o += A[v & 63];
    }
    if (i + 1 == in.size()) { uint32_t v = (unsigned char)in[i] << 16; o += A[v >> 18]; o += A[(v >> 12) & 63]; }
    else if (i + 2 == in.size()) { uint32_t v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8); o += A[v >> 18]; o += A[(v >> 12) & 63]; o += A[(v >> 6) & 63]; }
    return o;
}
inline bool b64urlDecode(const std::string& in, std::string& out) {
    out.clear();
    uint32_t acc = 0; int bits = 0;
    for (char c : in) {
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A'; else if (c >= 'a' && c <= 'z') v = c - 'a' + 26; else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '-' || c == '+') v = 62; else if (c == '_' || c == '/') v = 63; else if (c == '=') continue; else return false;
        acc = (acc << 6) | (uint32_t)v; bits += 6;
        if (bits >= 8) { bits -= 8; out += (char)((acc >> bits) & 0xFF); }
    }
    return bits < 6;   // 1 dangling character cannot encode a byte
}

inline const char* SHARE_PREFIX = "RC1:";

// withNames: also carry every Workshop mod's name. Mods that are not installed and have no id (placeholders) are written by name.
inline std::string makeShareCode(const Playset& ps, const std::map<std::string, ModInfo>& info, bool withNames) {
    std::string b;
    b += (char)1;                       // format
    b += (char)(withNames ? 1 : 0);     // flags
    std::string nm = ps.name.substr(0, 80);
    putVar(b, nm.size()); b += nm;
    size_t countPos = b.size();
    std::string body; uint64_t count = 0;
    for (auto& m : ps.mods) {
        auto it = info.find(m.id);
        bool installed = it != info.end();
        if (!m.enabled && installed) continue;     // a disabled, installed mod is not part of the playset
        std::string sid = steamIdOf(m.id);
        std::string name = installed ? it->second.name : m.name;
        if (name.size() > 120) name.resize(120);
        uint64_t en = m.enabled ? 1 : 0;
        if (!sid.empty() && sid.size() <= 15) {
            putVar(body, ((uint64_t)std::stoull(sid) << 2) | en);   // kind 0
            if (withNames) { putVar(body, name.size()); body += name; }
        } else {
            if (name.empty()) name = m.id;
            putVar(body, ((uint64_t)name.size() << 2) | 2 | en);    // kind 1: a local mod, by name
            body += name;
        }
        count++;
    }
    (void)countPos;
    putVar(b, count);
    b += body;
    uint32_t c = crc32Of(b);
    for (int i = 0; i < 4; i++) b += (char)((c >> (i * 8)) & 0xFF);
    return std::string(SHARE_PREFIX) + b64urlEncode(b);
}

// Finds a code inside pasted text (it may have words around it, but it must be one unbroken piece).
inline std::string findShareCode(const std::string& text) {
    size_t a = text.find(SHARE_PREFIX);
    if (a == std::string::npos) return "";
    std::string out = SHARE_PREFIX;
    for (size_t i = a + 4; i < text.size() && out.size() < 400000; i++) {
        char c = text[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_') out += c; else break;
    }
    return out;
}

struct ShareMod { std::string workshopId, name; bool enabled = true; bool local = false; };
struct ShareDecode { bool ok = false; std::string error, name; bool withNames = false; std::vector<ShareMod> mods; };

inline ShareDecode decodeShareCode(const std::string& pasted) {
    ShareDecode d;
    std::string code = findShareCode(pasted);
    if (code.empty()) { d.error = "No share code found. A share code starts with \"RC1:\"."; return d; }
    std::string b;
    if (!b64urlDecode(code.substr(4), b) || b.size() < 8) { d.error = "That share code is damaged (it may have been cut short)."; return d; }
    std::string body = b.substr(0, b.size() - 4);
    uint32_t want = 0;
    for (int i = 0; i < 4; i++) want |= (uint32_t)(unsigned char)b[b.size() - 4 + (size_t)i] << (i * 8);
    if (crc32Of(body) != want) { d.error = "That share code is damaged (the check failed). Copy it again, all of it."; return d; }
    size_t i = 0;
    if (body.size() < 2 || body[0] != 1) { d.error = "That share code is from a newer version of the program. Update The Royal Court and try again."; return d; }
    d.withNames = (body[1] & 1) != 0;
    i = 2;
    uint64_t len;
    if (!getVar(body, i, len) || len > 200 || i + len > body.size()) { d.error = "That share code is damaged."; return d; }
    d.name = body.substr(i, (size_t)len); i += (size_t)len;
    uint64_t count;
    if (!getVar(body, i, count) || count > 5000) { d.error = "That share code is damaged."; return d; }
    for (uint64_t k = 0; k < count; k++) {
        uint64_t v;
        if (!getVar(body, i, v)) { d.error = "That share code is damaged."; return d; }
        ShareMod m; m.enabled = (v & 1) != 0;
        if (v & 2) {
            uint64_t n = v >> 2;
            if (n > 400 || i + n > body.size()) { d.error = "That share code is damaged."; return d; }
            m.local = true; m.name = body.substr(i, (size_t)n); i += (size_t)n;
        } else {
            m.workshopId = std::to_string(v >> 2);
            if (d.withNames) {
                uint64_t n;
                if (!getVar(body, i, n) || n > 400 || i + n > body.size()) { d.error = "That share code is damaged."; return d; }
                m.name = body.substr(i, (size_t)n); i += (size_t)n;
            }
        }
        d.mods.push_back(std::move(m));
    }
    d.ok = true;
    return d;
}

// The code turned into a playset. Installed mods are matched by Workshop id (or name for local mods); the rest keep their id so they
// show as "not installed". Names are cleaned: a name from a code is only text.
inline Playset playsetFromShare(const ShareDecode& d, const std::vector<ModInfo>& installed) {
    Playset p;
    p.name = d.name.substr(0, 100);
    std::map<std::string, std::string> byName;
    for (auto& m : installed) byName.emplace(lower(m.name), m.id);
    std::set<std::string> seen;
    for (auto& sm : d.mods) {
        ModRef r; r.enabled = sm.enabled; r.name = sm.name;
        if (!sm.local) r.id = "ugc_" + sm.workshopId + ".mod";
        else if (auto it = byName.find(lower(sm.name)); it != byName.end()) r.id = it->second;
        else r.id = std::string(LOCAL_PREFIX) + sm.name;
        for (auto& c : r.name) if ((unsigned char)c < 32) c = ' ';
        if (!seen.insert(r.id).second) continue;
        p.mods.push_back(std::move(r));
    }
    return p;
}

}  // namespace rc
