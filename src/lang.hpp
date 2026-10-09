// The Royal Court - languages.
// Every sentence the program shows is written once in English in the source and looked up here by its English text.
// A translation may move the numbers and names around, so sentences that contain them are templates: "{0} of {1} mods".
// The translations are generated (lang_data.hpp) and compiled into the program; nothing is read from disk.
// A text with no translation is shown in English, so a missing line never breaks a screen.
#pragma once
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace rc {

struct LangInfo { const char* code; const char* native; };
// Index 0 is English (the source language). The order here is the order of the language menu and of the columns in lang_data.hpp.
inline const LangInfo LANGS[] = {
    {"en", "English"}, {"ru", "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9"},                       // Русский
    {"zh", "\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87"},                                                   // 简体中文
    {"de", "Deutsch"}, {"es", "Espa\xC3\xB1ol"}, {"fr", "Fran\xC3\xA7" "ais"}, {"pl", "Polski"},
    {"pt-BR", "Portugu\xC3\xAA" "s (Brasil)"}, {"tr", "T\xC3\xBC" "rk\xC3\xA7" "e"},
    {"ko", "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4"},                                                               // 한국어
    {"ja", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"},                                                               // 日本語
};
inline constexpr int LANG_COUNT = (int)(sizeof(LANGS) / sizeof(LANGS[0]));

// "pt-BR", "zh-Hans-CN", "de_DE" ... -> index into LANGS (0 = English when nothing fits)
inline int langFromTag(std::string tag) {
    for (auto& c : tag) { if (c == '_') c = '-'; c = (char)tolower((unsigned char)c); }
    if (tag.empty()) return 0;
    std::string head = tag.substr(0, tag.find('-'));
    for (int i = 1; i < LANG_COUNT; i++) {
        std::string code = LANGS[i].code;
        for (auto& c : code) c = (char)tolower((unsigned char)c);
        if (tag == code) return i;
        if (code.find('-') == std::string::npos && head == code) {
            if (head == "zh" && (tag.find("hant") != std::string::npos || tag.find("-tw") != std::string::npos || tag.find("-hk") != std::string::npos)) return 0;   // traditional Chinese is not included
            return i;
        }
        if (code == "pt-br" && head == "pt") return i;
    }
    return 0;
}
inline int langFromCode(const std::string& code) {   // exact code only; -1 if unknown
    for (int i = 0; i < LANG_COUNT; i++) if (code == LANGS[i].code) return i;
    return -1;
}

}  // namespace rc

#include "lang_data.hpp"   // generated: LANG_KEYS, LANG_N, LANG_VALUES[LANG_COUNT]

namespace rc {

inline int g_lang = 0;
inline bool g_langPseudo = false;                                // test language: every translated text is wrapped so missing ones stand out
inline std::unordered_map<std::string, const char*> g_langMap;   // English text -> translation, for the active language
inline std::set<std::string> g_langMissing;                      // English texts asked for that the active language has no translation for
inline std::mutex g_langMissingLock;                            // tr() is also called from the background threads

inline void setLanguage(int idx) {
    if (idx < 0 || idx >= LANG_COUNT) idx = 0;
    g_lang = idx;
    g_langMap.clear();
    if (idx == 0 || LANG_N == 0) return;
    const char* const* vals = LANG_VALUES[idx];
    if (!vals) return;
    g_langMap.reserve((size_t)LANG_N * 2);
    for (int i = 0; i < LANG_N; i++) if (vals[i] && vals[i][0]) g_langMap.emplace(LANG_KEYS[i], vals[i]);
}

// The text in the active language (the English text itself when there is no translation).
inline std::string trText(const char* english) {
    if (g_langPseudo) return std::string("\xE2\x80\xB9") + english + "\xE2\x80\xBA";
    if (g_lang == 0) return english;
    auto it = g_langMap.find(english);
    if (it == g_langMap.end()) { std::lock_guard<std::mutex> lk(g_langMissingLock); if (g_langMissing.size() < 5000) g_langMissing.insert(english); return english; }
    return it->second;
}

// Replaces {0}..{9} in a template. Text that merely contains braces is left alone.
template <class S>
inline S langFill(const S& tmpl, const std::vector<S>& args) {
    S out;
    out.reserve(tmpl.size() + 32);
    for (size_t i = 0; i < tmpl.size(); i++) {
        if (tmpl[i] == '{' && i + 2 < tmpl.size() && tmpl[i + 1] >= '0' && tmpl[i + 1] <= '9' && tmpl[i + 2] == '}') {
            size_t k = (size_t)(tmpl[i + 1] - '0');
            if (k < args.size()) { out += args[k]; i += 2; continue; }
        }
        out += tmpl[i];
    }
    return out;
}

// An argument of a template: text or a number.
struct TA {
    std::string s;
    TA(const char* v) : s(v) {}
    TA(const std::string& v) : s(v) {}
    TA(int v) : s(std::to_string(v)) {}
    TA(unsigned v) : s(std::to_string(v)) {}
    TA(long v) : s(std::to_string(v)) {}
    TA(unsigned long v) : s(std::to_string(v)) {}
    TA(long long v) : s(std::to_string(v)) {}
    TA(unsigned long long v) : s(std::to_string(v)) {}
};
// Marks an English literal that is stored in a table and translated later with tr(); the key extractor collects K("...") literals.
inline const char* K(const char* s) { return s; }
inline std::string tr(const char* english) { return trText(english); }
inline std::string trf(const char* english, std::initializer_list<TA> args) {
    std::vector<std::string> v;
    for (auto& a : args) v.push_back(a.s);
    return langFill<std::string>(trText(english), v);
}

}  // namespace rc
