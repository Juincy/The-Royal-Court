using System.Text.Json;
using RoyalCourt.Core;
using Xunit;
namespace RoyalCourt.Tests;

/// <summary>Guards the hand-maintained known-mods database: a rule that names a mod nobody knows silently does nothing, so every name used in an After/Before list, a compatch parent or a known-conflict pair must be a name of a known mod, the title of a Workshop page in the ledger, or be listed (reviewed) in fixtures/external_names.txt.</summary>
public class DatabaseTests
{
    static string? RepoFile(params string[] rel)
    {
        string? d = AppContext.BaseDirectory;
        while (d != null) { string p = Path.Combine(new[] { d }.Concat(rel).ToArray()); if (File.Exists(p)) return p; d = Path.GetDirectoryName(d); }
        return null;
    }

    static HashSet<string> LedgerTitles()
    {
        var set = new HashSet<string>();
        string? p = RepoFile("tools", "workshop_ledger.json");
        if (p == null) return set;
        using var doc = JsonDocument.Parse(File.ReadAllText(p));
        foreach (var e in doc.RootElement.GetProperty("items").EnumerateArray())
            if (e.TryGetProperty("title", out var t) && t.GetString() is string s && s.Length > 0) set.Add(Names.Norm(s));
        return set;
    }

    static (HashSet<string> known, List<(string who, string name)> used) Collect()
    {
        var known = new HashSet<string>(); var used = new List<(string, string)>();
        foreach (var k in KnownData.Builtin())
        {
            foreach (var n in k.Names) known.Add(n);
            string who = k.Names.Count > 0 ? k.Names[0] : "?";
            foreach (var n in k.After) used.Add((who, n));
            foreach (var n in k.Before) used.Add((who, n));
        }
        foreach (var c in KnownData.Patches) { known.Add(Names.Norm(c.Name)); foreach (var par in c.Parents) foreach (var n in par) used.Add((c.Name, n)); }
        foreach (var p in KnownConflicts.List)
        {
            foreach (var n in p.NamesA) used.Add(("conflict " + p.Text.Substring(0, Math.Min(30, p.Text.Length)), n));
            foreach (var n in p.NamesB) used.Add(("conflict " + p.Text.Substring(0, Math.Min(30, p.Text.Length)), n));
        }
        return (known, used);
    }

    static HashSet<string> Approved()
    {
        string? p = RepoFile("tests", "RoyalCourt.Tests", "fixtures", "external_names.txt");
        var s = new HashSet<string>();
        string path = p ?? Path.Combine(AppContext.BaseDirectory, "fixtures", "external_names.txt");
        if (File.Exists(path)) foreach (var l in File.ReadAllLines(path)) { var t = l.Trim(); if (t.Length > 0 && !t.StartsWith('#')) s.Add(t); }
        return s;
    }

    [Fact]
    public void EveryNameInARuleIsKnown()
    {
        var (known, used) = Collect(); var titles = LedgerTitles(); var ok = Approved();
        var bad = used.Where(u => !known.Contains(u.name) && !titles.Contains(u.name) && !ok.Contains(u.name)).GroupBy(u => u.name).OrderBy(g => g.Key).ToList();
        if (Environment.GetEnvironmentVariable("RC_DUMP_NAMES") == "1")
            File.WriteAllLines("/tmp/unresolved_names.txt", bad.Select(g => g.Key + "\t" + string.Join("; ", g.Select(x => x.who).Distinct().Take(3))));
        Assert.True(bad.Count == 0, "Names used in rules that match no known mod, no ledger title and are not in fixtures/external_names.txt (typo?):\n" + string.Join("\n", bad.Select(g => "  " + g.Key + "   (used by " + string.Join(", ", g.Select(x => x.who).Distinct().Take(3)) + ")")));
    }

    [Fact]
    public void RuleNamesCanMatchARealWorkshopTitle()
    {
        // A name that is only an alias of another entry (not a real title) can never match the installed mod: e.g. "bigger castles" when the title says "and bigger castles".
        var (known, used) = Collect(); var titles = LedgerTitles(); var ok = Approved();
        var bad = used.Where(u => !ok.Contains(u.name) && !titles.Any(t => Names.NameIs(t, u.name))).GroupBy(u => u.name).OrderBy(g => g.Key).ToList();
        if (Environment.GetEnvironmentVariable("RC_DUMP_NAMES") == "1") File.WriteAllLines("/tmp/unmatched_names.txt", bad.Select(g => g.Key + "\t" + string.Join("; ", g.Select(x => x.who).Distinct().Take(3))));
        Assert.True(bad.Count == 0, "Rule names that match no Workshop title in the ledger:\n" + string.Join("\n", bad.Select(g => "  " + g.Key)));
    }

    [Fact]
    public void NamesAndIdsAreUnique()
    {
        var names = new Dictionary<string, string>(); var ids = new Dictionary<string, string>(); var dup = new List<string>();
        foreach (var k in KnownData.Builtin())
        {
            string who = k.Names.Count > 0 ? k.Names[0] : "?";
            foreach (var n in k.Names) if (names.TryGetValue(n, out var o) && o != who) dup.Add($"name \"{n}\" is in both \"{o}\" and \"{who}\""); else names[n] = who;
            foreach (var i in k.Ids) if (ids.TryGetValue(i, out var o) && o != who) dup.Add($"id {i} is in both \"{o}\" and \"{who}\""); else ids[i] = who;
        }
        Assert.True(dup.Count == 0, string.Join("\n", dup));
    }

    [Fact]
    public void NoRuleNamesItself()
    {
        var self = KnownData.Builtin().Where(k => k.After.Concat(k.Before).Any(n => k.Names.Contains(n))).Select(k => k.Names[0]).ToList();
        Assert.True(self.Count == 0, "Rules that say a mod goes after/before itself: " + string.Join(", ", self));
    }
}
