using System.Text;
namespace RoyalCourt.Core;

// Share codes (see share.hpp): "RC1:" + URL-safe base64. The byte format is identical to the C++ program's, so codes work in both directions.

public sealed class ShareMod { public string WorkshopId = "", Name = ""; public bool Enabled = true, Local; }
public sealed class ShareDecode { public bool Ok; public string Error = "", Name = ""; public bool WithNames; public List<ShareMod> Mods = new(); }

public static class Share
{
    public const string Prefix = "RC1:";

    static uint[]? tab;
    public static uint Crc32(ReadOnlySpan<byte> s)
    {
        if (tab == null)
        {
            var t = new uint[256];
            for (uint i = 0; i < 256; i++) { uint c = i; for (int k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1; t[i] = c; }
            tab = t;
        }
        uint r = 0xFFFFFFFFu;
        foreach (byte ch in s) r = tab[(r ^ ch) & 0xFF] ^ (r >> 8);
        return r ^ 0xFFFFFFFFu;
    }
    static void PutVar(List<byte> o, ulong v) { while (v >= 0x80) { o.Add((byte)(0x80 | (v & 0x7F))); v >>= 7; } o.Add((byte)v); }
    static bool GetVar(byte[] s, ref int i, out ulong v)
    {
        v = 0;
        for (int sh = 0; sh < 63 && i < s.Length; sh += 7)
        {
            byte b = s[i++];
            v |= (ulong)(b & 0x7F) << sh;
            if ((b & 0x80) == 0) return true;
        }
        return false;
    }
    const string A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    public static string B64UrlEncode(byte[] inp)
    {
        var o = new StringBuilder();
        int i = 0;
        for (; i + 2 < inp.Length; i += 3)
        {
            uint v = ((uint)inp[i] << 16) | ((uint)inp[i + 1] << 8) | inp[i + 2];
            o.Append(A[(int)(v >> 18)]).Append(A[(int)((v >> 12) & 63)]).Append(A[(int)((v >> 6) & 63)]).Append(A[(int)(v & 63)]);
        }
        if (i + 1 == inp.Length) { uint v = (uint)inp[i] << 16; o.Append(A[(int)(v >> 18)]).Append(A[(int)((v >> 12) & 63)]); }
        else if (i + 2 == inp.Length) { uint v = ((uint)inp[i] << 16) | ((uint)inp[i + 1] << 8); o.Append(A[(int)(v >> 18)]).Append(A[(int)((v >> 12) & 63)]).Append(A[(int)((v >> 6) & 63)]); }
        return o.ToString();
    }
    public static bool B64UrlDecode(string inp, out byte[] outv)
    {
        var o = new List<byte>();
        uint acc = 0; int bits = 0;
        outv = Array.Empty<byte>();
        foreach (char c in inp)
        {
            int v;
            if (c >= 'A' && c <= 'Z') v = c - 'A'; else if (c >= 'a' && c <= 'z') v = c - 'a' + 26; else if (c >= '0' && c <= '9') v = c - '0' + 52;
            else if (c == '-' || c == '+') v = 62; else if (c == '_' || c == '/') v = 63; else if (c == '=') continue; else return false;
            acc = (acc << 6) | (uint)v; bits += 6;
            if (bits >= 8) { bits -= 8; o.Add((byte)((acc >> bits) & 0xFF)); }
        }
        outv = o.ToArray();
        return bits < 6;
    }

    static byte[] CutUtf8(string s, int maxBytes)
    {
        var b = Encoding.UTF8.GetBytes(s);
        if (b.Length <= maxBytes) return b;
        int n = maxBytes;
        while (n > 0 && (b[n] & 0xC0) == 0x80) n--;      // do not cut a character in half
        return b[..n];
    }

    /// <summary>withNames: also carry every Workshop mod's name. Mods that are not installed and have no id (placeholders) are written by name.</summary>
    public static string Make(Playset ps, IReadOnlyDictionary<string, ModInfo> info, bool withNames)
    {
        var b = new List<byte> { 1, (byte)(withNames ? 1 : 0) };
        var nm = CutUtf8(ps.Name, 80);
        PutVar(b, (ulong)nm.Length); b.AddRange(nm);
        var body = new List<byte>(); ulong count = 0;
        foreach (var m in ps.Mods)
        {
            bool installed = info.TryGetValue(m.Id, out var mi);
            if (!m.Enabled && installed) continue;
            string sid = Names.SteamIdOf(m.Id);
            string name = installed ? mi!.Name : m.Name;
            byte[] nb = CutUtf8(name, 120);
            ulong en = m.Enabled ? 1UL : 0UL;
            if (sid.Length > 0 && sid.Length <= 15)
            {
                PutVar(body, (ulong.Parse(sid) << 2) | en);
                if (withNames) { PutVar(body, (ulong)nb.Length); body.AddRange(nb); }
            }
            else
            {
                if (nb.Length == 0) nb = Encoding.UTF8.GetBytes(m.Id);
                PutVar(body, ((ulong)nb.Length << 2) | 2 | en);
                body.AddRange(nb);
            }
            count++;
        }
        PutVar(b, count);
        b.AddRange(body);
        uint c = Crc32(b.ToArray());
        for (int i = 0; i < 4; i++) b.Add((byte)((c >> (i * 8)) & 0xFF));
        return Prefix + B64UrlEncode(b.ToArray());
    }

    /// <summary>Finds a code inside pasted text (it may have words around it, but it must be one unbroken piece).</summary>
    public static string Find(string text)
    {
        int a = text.IndexOf(Prefix, StringComparison.Ordinal);
        if (a < 0) return "";
        var o = new StringBuilder(Prefix);
        for (int i = a + 4; i < text.Length && o.Length < 400000; i++)
        {
            char c = text[i];
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_') o.Append(c); else break;
        }
        return o.ToString();
    }

    public static ShareDecode Decode(string pasted)
    {
        var d = new ShareDecode();
        string code = Find(pasted);
        if (code.Length == 0) { d.Error = Tr("No share code found. A share code starts with \"RC1:\"."); return d; }
        if (!B64UrlDecode(code[4..], out var b) || b.Length < 8) { d.Error = Tr("That share code is damaged (it may have been cut short)."); return d; }
        var body = b[..^4];
        uint want = 0;
        for (int i = 0; i < 4; i++) want |= (uint)b[b.Length - 4 + i] << (i * 8);
        if (Crc32(body) != want) { d.Error = Tr("That share code is damaged (the check failed). Copy it again, all of it."); return d; }
        if (body.Length < 2 || body[0] != 1) { d.Error = Tr("That share code is from a newer version of the program. Update The Royal Court and try again."); return d; }
        d.WithNames = (body[1] & 1) != 0;
        int i2 = 2;
        string Damaged() => Tr("That share code is damaged.");
        if (!GetVar(body, ref i2, out ulong len) || len > 200 || (ulong)i2 + len > (ulong)body.Length) { d.Error = Damaged(); return d; }
        d.Name = Encoding.UTF8.GetString(body, i2, (int)len); i2 += (int)len;
        if (!GetVar(body, ref i2, out ulong count) || count > 5000) { d.Error = Damaged(); return d; }
        for (ulong k = 0; k < count; k++)
        {
            if (!GetVar(body, ref i2, out ulong v)) { d.Error = Damaged(); return d; }
            var m = new ShareMod { Enabled = (v & 1) != 0 };
            if ((v & 2) != 0)
            {
                ulong n = v >> 2;
                if (n > 400 || (ulong)i2 + n > (ulong)body.Length) { d.Error = Damaged(); return d; }
                m.Local = true; m.Name = Encoding.UTF8.GetString(body, i2, (int)n); i2 += (int)n;
            }
            else
            {
                m.WorkshopId = (v >> 2).ToString();
                if (d.WithNames)
                {
                    if (!GetVar(body, ref i2, out ulong n) || n > 400 || (ulong)i2 + n > (ulong)body.Length) { d.Error = Damaged(); return d; }
                    m.Name = Encoding.UTF8.GetString(body, i2, (int)n); i2 += (int)n;
                }
            }
            d.Mods.Add(m);
        }
        d.Ok = true;
        return d;
    }

    /// <summary>The code turned into a playset. Installed mods are matched by Workshop id (or name for local mods); the rest keep their id so they show as "not installed".</summary>
    public static Playset ToPlayset(ShareDecode d, IReadOnlyList<ModInfo> installed)
    {
        var p = new Playset { Name = d.Name.Length > 100 ? d.Name[..100] : d.Name };
        var byName = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var m in installed) byName.TryAdd(Util.Lower(m.Name), m.Id);
        var seen = new HashSet<string>(StringComparer.Ordinal);
        foreach (var sm in d.Mods)
        {
            var r = new ModRef { Enabled = sm.Enabled, Name = sm.Name };
            if (!sm.Local) r.Id = "ugc_" + sm.WorkshopId + ".mod";
            else if (byName.TryGetValue(Util.Lower(sm.Name), out var id)) r.Id = id;
            else r.Id = Playsets.LocalPrefix + sm.Name;
            r.Name = new string(r.Name.Select(c => c < 32 ? ' ' : c).ToArray());
            if (!seen.Add(r.Id)) continue;
            p.Mods.Add(r);
        }
        return p;
    }
}
