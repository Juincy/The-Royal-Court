using System.Globalization;
using System.Text;
namespace RoyalCourt.Core;

// Minimal JSON with ordered keys, written in exactly the format of the C++ program (tabs, "%.15g" numbers), so settings.json and the
// playset files stay readable by both versions.
public enum JT { Null, Bool, Num, Str, Arr, Obj }

public sealed class J
{
    public JT T = JT.Null;
    public bool B;
    public double N;
    public string S = "";
    public List<J> A = new();
    public List<string> Keys = new();
    public List<J> Vals = new();
    public static J Str(string v) => new J { T = JT.Str, S = v };
    public static J Num(double v) => new J { T = JT.Num, N = v };
    public static J Bool(bool v) => new J { T = JT.Bool, B = v };
    public static J Arr() => new J { T = JT.Arr };
    public static J Obj() => new J { T = JT.Obj };
    public J? Get(string k)
    {
        for (int i = 0; i < Keys.Count; i++) if (Keys[i] == k) return Vals[i];
        return null;
    }
    public void Set(string k, J v)
    {
        for (int i = 0; i < Keys.Count; i++) if (Keys[i] == k) { Vals[i] = v; return; }
        Keys.Add(k); Vals.Add(v);
    }
    public bool IsObj => T == JT.Obj;
    public bool IsArr => T == JT.Arr;
    public bool IsStr => T == JT.Str;
    public bool IsNum => T == JT.Num;
    public bool IsBool => T == JT.Bool;

    // ---- parsing ----
    sealed class Parser
    {
        readonly string s; public int p; public bool ok = true;
        public Parser(string str) { s = str; }
        void Ws() { while (p < s.Length && (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r')) p++; }
        uint Hex4()
        {
            uint v = 0;
            for (int i = 0; i < 4 && p < s.Length; i++)
            {
                char c = s[p++]; v <<= 4;
                if (c >= '0' && c <= '9') v |= (uint)(c - '0');
                else if (c >= 'a' && c <= 'f') v |= (uint)(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') v |= (uint)(c - 'A' + 10);
                else ok = false;
            }
            return v;
        }
        string Str()
        {
            var o = new StringBuilder(); p++;
            while (p < s.Length && s[p] != '"')
            {
                char c = s[p++];
                if (c == '\\' && p < s.Length)
                {
                    char e = s[p++];
                    switch (e)
                    {
                        case 'n': o.Append('\n'); break;
                        case 't': o.Append('\t'); break;
                        case 'r': o.Append('\r'); break;
                        case 'b': o.Append('\b'); break;
                        case 'f': o.Append('\f'); break;
                        case 'u':
                        {
                            uint cp = Hex4();
                            // a UTF-16 string keeps a surrogate pair as two \u escapes: just append both code units
                            o.Append((char)cp);
                            break;
                        }
                        default: o.Append(e); break;
                    }
                }
                else o.Append(c);
            }
            if (p < s.Length) p++; else ok = false;
            return o.ToString();
        }
        public J Val(int depth = 0)
        {
            Ws();
            var j = new J();
            if (p >= s.Length || depth > 64) { ok = false; return j; }
            char c = s[p];
            if (c == '{')
            {
                j.T = JT.Obj; p++; Ws();
                if (p < s.Length && s[p] == '}') { p++; return j; }
                while (ok)
                {
                    Ws();
                    if (p >= s.Length || s[p] != '"') { ok = false; break; }
                    string k = Str(); Ws();
                    if (p >= s.Length || s[p] != ':') { ok = false; break; }
                    p++;
                    J v = Val(depth + 1);
                    j.Keys.Add(k); j.Vals.Add(v); Ws();
                    if (p < s.Length && s[p] == ',') { p++; continue; }
                    if (p < s.Length && s[p] == '}') { p++; break; }
                    ok = false;
                }
            }
            else if (c == '[')
            {
                j.T = JT.Arr; p++; Ws();
                if (p < s.Length && s[p] == ']') { p++; return j; }
                while (ok)
                {
                    j.A.Add(Val(depth + 1)); Ws();
                    if (p < s.Length && s[p] == ',') { p++; continue; }
                    if (p < s.Length && s[p] == ']') { p++; break; }
                    ok = false;
                }
            }
            else if (c == '"') { j.T = JT.Str; j.S = Str(); }
            else if (string.CompareOrdinal(s, p, "true", 0, 4) == 0) { j.T = JT.Bool; j.B = true; p += 4; }
            else if (string.CompareOrdinal(s, p, "false", 0, 5) == 0) { j.T = JT.Bool; p += 5; }
            else if (string.CompareOrdinal(s, p, "null", 0, 4) == 0) { p += 4; }
            else
            {
                // like strtod: the longest numeric prefix ([sign] digits [. digits] [e [sign] digits]); the rest is left for the caller
                int q = p;
                if (q < s.Length && (s[q] == '-' || s[q] == '+')) q++;
                int ds = q;
                while (q < s.Length && char.IsAsciiDigit(s[q])) q++;
                int nd = q - ds;
                if (q < s.Length && s[q] == '.') { int f = q + 1; while (f < s.Length && char.IsAsciiDigit(s[f])) f++; nd += f - q - 1; if (nd > 0) q = f; }
                if (nd > 0 && q < s.Length && (s[q] == 'e' || s[q] == 'E'))
                {
                    int x = q + 1;
                    if (x < s.Length && (s[x] == '-' || s[x] == '+')) x++;
                    int es = x;
                    while (x < s.Length && char.IsAsciiDigit(s[x])) x++;
                    if (x > es) q = x;
                }
                if (nd > 0 && double.TryParse(s.AsSpan(p, q - p), NumberStyles.Float, CultureInfo.InvariantCulture, out double d)) { j.T = JT.Num; j.N = d; p = q; }
                else ok = false;
            }
            return j;
        }
    }

    public static bool TryParse(string text, out J root)
    {
        var ps = new Parser(text);
        if (text.Length >= 1 && text[0] == '﻿') ps.p = 1;
        root = ps.Val();
        return ps.ok && root.T != JT.Null;
    }

    // ---- writing ----
    static void DumpStr(StringBuilder o, string s)
    {
        o.Append('"');
        foreach (char c in s)
        {
            switch (c)
            {
                case '"': o.Append("\\\""); break;
                case '\\': o.Append("\\\\"); break;
                case '\n': o.Append("\\n"); break;
                case '\r': o.Append("\\r"); break;
                case '\t': o.Append("\\t"); break;
                default:
                    if (c < 0x20) o.Append("\\u").Append(((int)c).ToString("x4"));
                    else o.Append(c);
                    break;
            }
        }
        o.Append('"');
    }
    static string NumText(double d) => Util.G(d, 15);
    public void Dump(StringBuilder o, int ind = 0)
    {
        switch (T)
        {
            case JT.Null: o.Append("null"); break;
            case JT.Bool: o.Append(B ? "true" : "false"); break;
            case JT.Num: o.Append(NumText(N)); break;
            case JT.Str: DumpStr(o, S); break;
            case JT.Arr:
                if (A.Count == 0) { o.Append("[]"); break; }
                o.Append("[\n");
                for (int i = 0; i < A.Count; i++)
                {
                    o.Append('\t', ind + 1); A[i].Dump(o, ind + 1);
                    if (i + 1 < A.Count) o.Append(',');
                    o.Append('\n');
                }
                o.Append('\t', ind).Append(']');
                break;
            case JT.Obj:
                if (Keys.Count == 0) { o.Append("{}"); break; }
                o.Append("{\n");
                for (int i = 0; i < Keys.Count; i++)
                {
                    o.Append('\t', ind + 1); DumpStr(o, Keys[i]); o.Append(": "); Vals[i].Dump(o, ind + 1);
                    if (i + 1 < Keys.Count) o.Append(',');
                    o.Append('\n');
                }
                o.Append('\t', ind).Append('}');
                break;
        }
    }
    public string Dump() { var sb = new StringBuilder(); Dump(sb); return sb.ToString(); }
    public void DumpMin(StringBuilder o)
    {
        switch (T)
        {
            case JT.Arr:
                o.Append('[');
                for (int i = 0; i < A.Count; i++) { if (i > 0) o.Append(','); A[i].DumpMin(o); }
                o.Append(']');
                break;
            case JT.Obj:
                o.Append('{');
                for (int i = 0; i < Keys.Count; i++) { if (i > 0) o.Append(','); DumpStr(o, Keys[i]); o.Append(':'); Vals[i].DumpMin(o); }
                o.Append('}');
                break;
            default: Dump(o, 0); break;
        }
    }
    public string DumpMin() { var sb = new StringBuilder(); DumpMin(sb); return sb.ToString(); }
}
