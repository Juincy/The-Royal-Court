#!/usr/bin/env python3
"""Language tooling for The Royal Court.

  python3 tools/make_lang.py extract      -> lang/keys.json  (every English text the program can show, with where it is used)
  python3 tools/make_lang.py check        -> checks lang/<code>.json against the keys (missing, extra, placeholders, newlines)
  python3 tools/make_lang.py build        -> writes src/lang_data.hpp from lang/<code>.json

The English text is the key. A translation file is a JSON object {english text: translation}. Placeholders {0}..{9} must
appear in a translation exactly as in the English text (they may be reordered). A missing translation falls back to English.
"""
import json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'src')
LANGDIR = os.path.join(ROOT, 'lang')
FILES = ['main.cpp', 'core.hpp', 'changes.hpp', 'share.hpp', 'gamelog.hpp', 'crash.hpp']
# order = lang.hpp LANGS (index 0 is English)
CODES = ['en', 'ru', 'zh', 'de', 'es', 'fr', 'pl', 'pt-BR', 'tr', 'ko', 'ja']

CALL = re.compile(r'\b(TLF|TLK|TL|TS|trf|tr|K)\s*\(\s*')
LIT = re.compile(r'(?:u8|L)?"((?:[^"\\\n]|\\.)*)"')
ESC = {'n': '\n', 'r': '\r', 't': '\t', '"': '"', '\\': '\\', "'": "'", '0': '\0', 'a': '\a', 'b': '\b', 'f': '\f', 'v': '\v'}


def unescape(s):
    out = []
    i = 0
    while i < len(s):
        c = s[i]
        if c != '\\':
            out.append(c); i += 1; continue
        n = s[i + 1]
        if n == 'u':
            out.append(chr(int(s[i + 2:i + 6], 16))); i += 6
        elif n == 'U':
            out.append(chr(int(s[i + 2:i + 10], 16))); i += 10
        elif n == 'x':
            j = i + 2
            while j < len(s) and s[j] in '0123456789abcdefABCDEF': j += 1
            out.append(chr(int(s[i + 2:j], 16))); i = j
        elif n in ESC:
            out.append(ESC[n]); i += 2
        else:
            raise ValueError('unknown escape \\%s in %r' % (n, s))
    # a lone surrogate pair written as two \u escapes
    t = ''.join(out)
    return t.encode('utf-16', 'surrogatepass').decode('utf-16') if any(0xD800 <= ord(ch) <= 0xDFFF for ch in t) else t


def skip_space(t, p):
    while p < len(t):
        if t[p].isspace(): p += 1
        elif t.startswith('/*', p):
            q = t.find('*/', p + 2); p = len(t) if q < 0 else q + 2
        elif t.startswith('//', p):
            q = t.find('\n', p); p = len(t) if q < 0 else q + 1
        else: break
    return p


def extract():
    keys = {}
    for fn in FILES:
        t = open(os.path.join(SRC, fn), encoding='utf8').read()
        for m in CALL.finditer(t):
            p = m.end()
            lits = []
            while True:
                lm = LIT.match(t, p)
                if not lm: break
                lits.append(lm.group(1))
                p = skip_space(t, lm.end())
            if not lits: continue
            # tr(x) where the first token is a literal followed by something else than , or ) is an expression such as "a" + b: not a key
            if p < len(t) and t[p] not in ',)':
                print('note: not a plain literal key at %s: %s' % (fn, t[m.start():m.start() + 80].replace('\n', ' ')))
                continue
            key = unescape(''.join(lits))
            if not key.strip() or not re.search(r'[A-Za-z]{2}', key): continue
            line = t.count('\n', 0, m.start()) + 1
            keys.setdefault(key, []).append('%s:%d' % (fn, line))
    return keys


def placeholders(s):
    return sorted(re.findall(r'\{\d\}', s))


def write_keys(keys):
    os.makedirs(LANGDIR, exist_ok=True)
    out = [{'key': k, 'where': v[:3]} for k, v in keys.items()]
    json.dump(out, open(os.path.join(LANGDIR, 'keys.json'), 'w', encoding='utf8'), ensure_ascii=False, indent=1)
    print('%d keys' % len(keys))


def load_lang(code):
    p = os.path.join(LANGDIR, code + '.json')
    if not os.path.exists(p): return None
    return json.load(open(p, encoding='utf8'))


def check(keys, verbose=True):
    bad = 0
    for code in CODES[1:]:
        d = load_lang(code)
        if d is None:
            print('%-6s no file' % code); continue
        missing = [k for k in keys if k not in d or not str(d[k]).strip()]
        extra = [k for k in d if k not in keys]
        ph = [k for k in keys if k in d and placeholders(k) != placeholders(d[k])]
        nl = [k for k in keys if k in d and (k.count('\n') != d[k].count('\n'))]
        lead = [k for k in keys if k in d and (k[:1].isspace() != d[k][:1].isspace())]
        print('%-6s %d/%d translated, %d missing, %d unused, %d placeholder mismatches, %d line-break mismatches, %d leading-space mismatches' %
              (code, len(keys) - len(missing), len(keys), len(missing), len(extra), len(ph), len(nl), len(lead)))
        if verbose:
            for lab, lst in (('missing', missing[:5]), ('placeholders', ph[:5]), ('linebreaks', nl[:5])):
                for k in lst: print('   %s: %r' % (lab, k[:90]))
        bad += len(ph) + len(nl)
    return bad


def cstr(s):
    """UTF-8 bytes of s as a C string literal body (octal escapes for non-ASCII so no \\x hex pitfalls)."""
    out = []
    for b in s.encode('utf-8'):
        c = chr(b)
        if c == '"': out.append('\\"')
        elif c == '\\': out.append('\\\\')
        elif c == '\n': out.append('\\n')
        elif c == '\r': out.append('\\r')
        elif c == '\t': out.append('\\t')
        elif 32 <= b < 127: out.append(c)
        else: out.append('\\%03o' % b)
    return ''.join(out)


def build(keys):
    klist = list(keys)
    L = ['// Generated by tools/make_lang.py from lang/*.json - do not edit by hand.', '#pragma once', 'namespace rc {',
         'inline constexpr int LANG_N = %d;' % len(klist), 'inline const char* const LANG_KEYS[] = {']
    for k in klist: L.append('    "%s",' % cstr(k))
    L.append('};')
    names = []
    for i, code in enumerate(CODES):
        d = load_lang(code) if i else None
        if not d:
            names.append('nullptr'); continue
        nm = 'LANG_' + re.sub(r'\W', '_', code)
        L.append('inline const char* const %s[] = {' % nm)
        for k in klist:
            v = d.get(k)
            L.append('    %s,' % ('"%s"' % cstr(v) if v and str(v).strip() else 'nullptr'))
        L.append('};')
        names.append(nm)
    L.append('inline const char* const* const LANG_VALUES[%d] = {%s};' % (len(CODES), ', '.join(names)))
    L.append('}  // namespace rc')
    open(os.path.join(SRC, 'lang_data.hpp'), 'w', encoding='utf8').write('\n'.join(L) + '\n')
    print('lang_data.hpp: %d keys, %d languages' % (len(klist), sum(1 for n in names if n != 'nullptr')))


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'check'
    keys = extract()
    if cmd == 'extract': write_keys(keys)
    elif cmd == 'check': sys.exit(1 if check(keys) else 0)
    elif cmd == 'build':
        if check(keys, False): print('fix the mismatches first'); sys.exit(1)
        build(keys)
