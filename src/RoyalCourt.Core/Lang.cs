using System.Reflection;
using System.Text;
namespace RoyalCourt.Core;

public sealed record LangInfo(string Code, string Native);

/// <summary>
/// Every sentence the program shows is written once in English in the source and looked up by its English text.
/// Sentences with names or numbers are templates: "{0} of {1} mods". The translations are the lang/*.json files, built into the program.
/// A text with no translation is shown in English, so a missing line never breaks a screen.
/// </summary>
public static class Lang
{
    // Index 0 is English (the source language). The order is the order of the language menu.
    public static readonly LangInfo[] Langs =
    {
        new("en", "English"), new("ru", "Русский"), new("zh", "简体中文"), new("de", "Deutsch"), new("es", "Español"), new("fr", "Français"),
        new("pl", "Polski"), new("pt-BR", "Português (Brasil)"), new("tr", "Türkçe"), new("ko", "한국어"), new("ja", "日本語"),
    };
    public static int Count => Langs.Length;

    /// <summary>"pt-BR", "zh-Hans-CN", "de_DE" ... -> index into Langs (0 = English when nothing fits).</summary>
    public static int FromTag(string tag)
    {
        tag = tag.Replace('_', '-').ToLowerInvariant();
        if (tag.Length == 0) return 0;
        int dash = tag.IndexOf('-');
        string head = dash < 0 ? tag : tag.Substring(0, dash);
        for (int i = 1; i < Count; i++)
        {
            string code = Langs[i].Code.ToLowerInvariant();
            if (tag == code) return i;
            if (!code.Contains('-') && head == code)
            {
                if (head == "zh" && (tag.Contains("hant") || tag.Contains("-tw") || tag.Contains("-hk"))) return 0;   // traditional Chinese is not included
                return i;
            }
            if (code == "pt-br" && head == "pt") return i;
        }
        return 0;
    }
    /// <summary>Exact code only; -1 if unknown.</summary>
    public static int FromCode(string code)
    {
        for (int i = 0; i < Count; i++) if (code == Langs[i].Code) return i;
        return -1;
    }

    static int current;
    static bool pseudo;
    static Dictionary<string, string> map = new();
    static readonly HashSet<string> missing = new();
    static readonly object lk = new();
    static readonly Dictionary<int, Dictionary<string, string>> cache = new();

    public static int Current => current;
    public static bool Pseudo { get => pseudo; set => pseudo = value; }
    public static IReadOnlyCollection<string> Missing { get { lock (lk) return missing.ToArray(); } }

    static Dictionary<string, string> Load(int idx)
    {
        lock (lk)
        {
            if (cache.TryGetValue(idx, out var c)) return c;
            var d = new Dictionary<string, string>(StringComparer.Ordinal);
            var asm = typeof(Lang).Assembly;
            using var s = asm.GetManifestResourceStream("lang." + Langs[idx].Code + ".json");
            if (s != null)
            {
                using var r = new StreamReader(s, Encoding.UTF8);
                if (J.TryParse(r.ReadToEnd(), out var root) && root.IsObj)
                    for (int i = 0; i < root.Keys.Count; i++) if (root.Vals[i].IsStr && root.Vals[i].S.Length > 0) d[root.Keys[i]] = root.Vals[i].S;
            }
            cache[idx] = d;
            return d;
        }
    }

    public static void Set(int idx)
    {
        if (idx < 0 || idx >= Count) idx = 0;
        current = idx;
        map = idx == 0 ? new Dictionary<string, string>() : Load(idx);
    }

    /// <summary>The text in the active language (the English text itself when there is no translation).</summary>
    public static string Tr(string english)
    {
        if (pseudo) return "‹" + english + "›";
        if (current == 0) return english;
        if (map.TryGetValue(english, out var t)) return t;
        lock (lk) { if (missing.Count < 5000) missing.Add(english); }
        return english;
    }

    /// <summary>Replaces {0}..{9} in a template. Text that merely contains braces is left alone.</summary>
    public static string Fill(string tmpl, IReadOnlyList<string> args)
    {
        var sb = new StringBuilder(tmpl.Length + 32);
        for (int i = 0; i < tmpl.Length; i++)
        {
            if (tmpl[i] == '{' && i + 2 < tmpl.Length && tmpl[i + 1] >= '0' && tmpl[i + 1] <= '9' && tmpl[i + 2] == '}')
            {
                int k = tmpl[i + 1] - '0';
                if (k < args.Count) { sb.Append(args[k]); i += 2; continue; }
            }
            sb.Append(tmpl[i]);
        }
        return sb.ToString();
    }
    public static string Trf(string english, params object[] args)
    {
        var v = new string[args.Length];
        for (int i = 0; i < args.Length; i++) v[i] = Convert.ToString(args[i], System.Globalization.CultureInfo.InvariantCulture) ?? "";
        return Fill(Tr(english), v);
    }
    /// <summary>Marks an English literal stored in a table and translated later with Tr(); the key extractor collects K("...") literals.</summary>
    public static string K(string s) => s;
}
