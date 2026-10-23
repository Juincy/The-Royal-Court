namespace RoyalCourt.Core;

public struct ModIssue { public int Sev; public string Text; public ModIssue(int s, string t) { Sev = s; Text = t; } }   // sev: 0 info, 1 warning, 2 problem

public static class Analyze
{
    public static string Trimmed(string s) => Util.Trimmed(s);

    /// <summary>Checks every mod of a playset against its descriptor. Result[i] belongs to ps.Mods[i].</summary>
    public static List<ModIssue>[] AnalyzePlayset(Playset ps, IReadOnlyDictionary<string, ModInfo> info, string gameVersion)
    {
        var outv = new List<ModIssue>[ps.Mods.Count];
        for (int i = 0; i < outv.Length; i++) outv[i] = new List<ModIssue>();
        var enabledPos = new Dictionary<string, int>(StringComparer.Ordinal);
        var sameName = new Dictionary<string, List<int>>(StringComparer.Ordinal);
        var installedNames = new HashSet<string>(StringComparer.Ordinal);
        foreach (var kv in info) installedNames.Add(Util.Lower(Trimmed(kv.Value.Name)));
        for (int i = 0; i < ps.Mods.Count; i++)
        {
            if (!ps.Mods[i].Enabled || !info.TryGetValue(ps.Mods[i].Id, out var mi)) continue;
            string n = Util.Lower(Trimmed(mi.Name));
            enabledPos.TryAdd(n, i);
            if (!sameName.TryGetValue(n, out var l)) sameName[n] = l = new List<int>();
            l.Add(i);
        }
        for (int i = 0; i < ps.Mods.Count; i++)
        {
            var iss = outv[i];
            if (!info.TryGetValue(ps.Mods[i].Id, out var m))
            {
                iss.Add(new ModIssue(ps.Mods[i].Enabled ? 2 : 0, Tr("Not installed")));
                continue;
            }
            if (m.Pending) iss.Add(new ModIssue(0, Tr("Subscribed on Steam, not added yet. Tick the box to add it")));
            if (!ps.Mods[i].Enabled) continue;
            string self = Util.Lower(Trimmed(m.Name));
            if (m.ContentState == 2) iss.Add(new ModIssue(2, Tr("Mod files not found")));
            foreach (var d in m.Deps)
            {
                string dn = Util.Lower(Trimmed(d));
                if (dn.Length == 0 || dn == self) continue;
                if (!enabledPos.TryGetValue(dn, out int ep))
                    iss.Add(new ModIssue(2, installedNames.Contains(dn) ? Trf("Needs \"{0}\" (installed, not enabled)", d) : Trf("Needs \"{0}\" (not installed)", d)));
                else if (ep > i)
                    iss.Add(new ModIssue(1, Trf("\"{0}\" should load before this mod (it is #{1})", d, ep + 1)));
            }
            if (sameName.TryGetValue(self, out var sn) && sn.Count > 1)
            {
                int other = sn[0] == i ? sn[1] : sn[0];
                iss.Add(new ModIssue(1, Trf("Same name as enabled mod #{0}", other + 1)));
            }
            var vm = ModScan.MatchGameVersion(m.Supported, gameVersion);
            if (vm == VerMatch.Mismatch)
                iss.Add(new ModIssue(1, Trf("Made for game {0} (installed {1})", m.Supported, gameVersion)));
            if (vm == VerMatch.Mismatch && m.Source == "Workshop" && !m.Pending)
                iss.Add(new ModIssue(0, Tr("If the author has updated this mod, Steam may be serving you an old copy: unsubscribe, close Steam, start it again and subscribe again")));
            if (m.ReplacePaths.Count > 0)
                iss.Add(new ModIssue(0, Trf("Replaces vanilla folder(s): {0}", string.Join(", ", m.ReplacePaths))));
            StableSortBySevDesc(iss);
        }
        return outv;
    }

    public static void StableSortBySevDesc(List<ModIssue> l)
    {
        if (l.Count < 2) return;
        var sorted = l.Select((x, i) => (x, i)).OrderByDescending(t => t.x.Sev).ThenBy(t => t.i).Select(t => t.x).ToList();
        l.Clear(); l.AddRange(sorted);
    }

    /// <summary>One-line summary for the list: worst issue (problems and warnings only) plus how many more.</summary>
    public static string IssueSummary(IReadOnlyList<ModIssue> v)
    {
        int shown = 0, worst = -1;
        for (int i = 0; i < v.Count; i++) if (v[i].Sev >= 1) { shown++; if (worst < 0) worst = i; }
        if (worst < 0) return "";
        return shown > 1 ? Trf("{0}  (+{1} more)", v[worst].Text, shown - 1) : v[worst].Text;
    }
    public static int IssueSeverity(IReadOnlyList<ModIssue> v) { int s = -1; foreach (var i in v) if (i.Sev > s) s = i.Sev; return s < 1 ? 0 : s; }

    public static void AddConflictIssues(List<ModIssue>[] issues, ConflictReport cr, Playset ps, IReadOnlyDictionary<string, ModInfo> info, string gameVer = "")
    {
        if (!cr.Valid || issues.Length != ps.Mods.Count) return;
        string NameOf(int i) => info.TryGetValue(ps.Mods[i].Id, out var m) ? m.Name : ps.Mods[i].Id;
        foreach (var w in cr.Wipes)
        {
            issues[w.Replacer].Add(new ModIssue(1, Trf("replace_path \"{0}\" removes {1} file(s) of \"{2}\" (loaded earlier)", w.Folder, w.Files, NameOf(w.Victim))));
            issues[w.Victim].Add(new ModIssue(1, Trf("{0} file(s) in \"{1}\" removed by \"{2}\" (replace_path)", w.Files, w.Folder, NameOf(w.Replacer))));
        }
        if (cr.VanillaChecked && cr.VanillaCount.Length == ps.Mods.Count)
        {
            foreach (var w in cr.VanillaWipes)
                issues[w.Mod].Add(new ModIssue(1, Trf("replace_path \"{0}\" removes {1} file(s) of the base game", w.Folder, w.Files)));
            for (int i = 0; i < ps.Mods.Count; i++)
            {
                if (cr.VanillaCount[i] <= 0) continue;
                bool old = info.TryGetValue(ps.Mods[i].Id, out var m) && ModScan.MatchGameVersion(m.Supported, gameVer) == VerMatch.Mismatch;
                string t = old ? Trf("Replaces {0} file(s) of the base game and was made for an older game version: changes the update made to those files are lost", cr.VanillaCount[i])
                               : Trf("Replaces {0} file(s) of the base game", cr.VanillaCount[i]);
                issues[i].Add(new ModIssue(old ? 1 : 0, t));
            }
        }
        for (int i = 0; i < ps.Mods.Count; i++)
        {
            if (cr.Wins[i] > 0) issues[i].Add(new ModIssue(0, Trf("Overrides {0} file(s) from earlier mods", cr.Wins[i])));
            if (cr.Loses[i] > 0) issues[i].Add(new ModIssue(0, Trf("{0} of its files are overridden by later mods", cr.Loses[i])));
            StableSortBySevDesc(issues[i]);
        }
    }
}
