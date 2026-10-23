using System.Text;
using RoyalCourt.Core;

// Prints the same canonical report as tests/oracle/oracle.cpp. The differential test compares the two outputs.
static class Oracle
{
    static string Esc(string s)
    {
        var o = new StringBuilder();
        foreach (char c in s) { if (c == '\n') o.Append("\\n"); else if (c == '\r') o.Append("\\r"); else if (c == '\t') o.Append("\\t"); else if (c == '\\') o.Append("\\\\"); else o.Append(c); }
        return o.ToString();
    }
    static string Join(IEnumerable<string> v, string sep = ",") => string.Join(sep, v.Select(Esc));
    static string Ints(IEnumerable<int> v) => string.Join(",", v);
    static StringBuilder O = new();
    static void W(string s) => O.Append(s).Append('\n');


    static string Unesc(string s)
    {
        var o = new StringBuilder();
        for (int i = 0; i < s.Length; i++)
        {
            if (s[i] == '\\' && i + 1 < s.Length) { char c = s[++i]; o.Append(c == 'n' ? '\n' : c == 'r' ? '\r' : c == 't' ? '\t' : c); }
            else o.Append(s[i]);
        }
        return o.ToString();
    }
    static string D2S(double d) => Util.G(d, 10);
    static string TableOut(TimeTable t)
    {
        var o = new StringBuilder((t.Ok ? 1 : 0) + ";" + D2S(t.Total));
        foreach (var r in t.Rows) o.Append(";" + r.Id + "," + D2S(r.Sum) + "," + D2S(r.Own) + "," + D2S(r.Share) + "," + r.Count);
        return o.ToString();
    }
    static void Funcs(string file)
    {
        Util.ReadFile(file, out var text);
        int idx = 0;
        foreach (var raw in text.Split('\n'))
        {
            var line = raw.EndsWith('\r') ? raw[..^1] : raw;
            if (line.Length == 0 && raw.Length == 0) continue;
            var a = line.Split('\t').Select(Unesc).ToList();
            string fn = a[0], outp = "";
            string Arg(int i) => i < a.Count ? a[i] : "";
            switch (fn)
            {
                case "desc": foreach (var e in ModScan.ParseDescriptor(Arg(1)).OrderBy(x => x.Key, StringComparer.Ordinal)) { outp += e.Key + "=["; foreach (var v in e.Value) outp += Esc(v) + ","; outp += "];"; } break;
                case "ver": outp = (int)ModScan.MatchGameVersion(Arg(1), Arg(2)) + "|" + Join(ModScan.VersionSegments(Arg(1))) + "|" + Join(ModScan.VersionSegments(Arg(2))); break;
                case "gv": outp = ModScan.GameVersionFromSettings(Arg(1)); break;
                case "nat": outp = Health.NaturalCompare(Arg(1), Arg(2)).ToString(); break;
                case "norm": outp = Names.Norm(Arg(1)) + "|" + Names.Acronym(Names.Norm(Arg(1))); break;
                case "nameis": outp = (Names.NameIs(Arg(1), Arg(2)) ? 1 : 0).ToString(); break;
                case "keys": { var k = new List<ScriptKey>(); Scripts.ScanScriptKeys(Arg(2), Util.Atoi(Arg(1)), k); foreach (var x in k) outp += x.Key + ":" + x.Flags + ";"; break; }
                case "loc": { var k = new List<string>(); Scripts.ScanLocKeys(Arg(1), k); outp = Join(k, ";"); break; }
                case "fk": { var k = Scripts.FileKind(Arg(1)); outp = k.Kind + "|" + k.Area + "|" + k.Mode + "|" + k.Flags; break; }
                case "json": { bool ok = J.TryParse(Arg(1), out var r); outp = (ok ? 1 : 0).ToString(); if (ok) outp += "|" + r.Dump() + "|" + r.DumpMin(); break; }
                case "csvev": outp = TableOut(ScriptTime.ParseStatsCsv(Arg(1), false)); break;
                case "csvoa": outp = TableOut(ScriptTime.ParseStatsCsv(Arg(1), true)); break;
                case "year": foreach (var y in ScriptTime.ParseYearStats(Arg(1))) { outp += y.Year + "{"; foreach (var kv in y.V) outp += kv.Key + "=" + D2S(kv.Value) + ","; outp += "};"; } break;
                case "log": { var r = GameLog.Parse(Arg(1)); outp = r.Lines + "," + r.Errors + "," + r.Warnings + "," + r.Dropped + ";"; foreach (var e in r.Entries) outp += e.Level + "|" + e.Src + "|" + e.Msg + "|" + e.Detail + "|" + e.File + "|" + e.Line + "|" + string.Join("~", e.Names) + "|" + e.Count + ";"; break; }
                case "loaded": foreach (var m in GameLog.ParseLoadedMods(Arg(1))) outp += m.Name + "|" + m.Id + "|" + (m.Enabled ? 1 : 0) + ";"; break;
                case "cex": { var d = new CrashData(); Crash.ParseException(Arg(1), d); outp = (int)d.Kind + "|" + (d.HaveException ? 1 : 0) + "|" + d.Code + "|" + Join(d.Frames, ";"); break; }
                case "cmeta": { var d = new CrashData(); Crash.ParseMeta(Arg(1), d); outp = (d.HaveMeta ? 1 : 0) + "|" + Join(d.Meta, ";"); break; }
                case "ckind": outp = ((int)Crash.KindOf(Arg(1))).ToString(); break;
                case "cname": { var f = new CrashFolder(); bool ok = Crash.NameParse(Arg(1), f); outp = (ok ? 1 : 0) + "|" + f.Y + "," + f.Mo + "," + f.D + "," + f.H + "," + f.Mi + "," + f.S; break; }
                case "tail": outp = Crash.TailLines(Arg(1), Util.Atoi(Arg(2))); break;
                case "vdf": outp = Join(Steam.VdfValues(Arg(1), Arg(2)), ";"); break;
                case "regdesc": outp = Steam.RegisteredDescriptorText(Arg(1), Arg(2), Arg(3)); break;
                case "rel": { var r = Release.Parse(Arg(1)); outp = (r.Ok ? 1 : 0) + "|" + r.Tag + "|" + r.Name + "|" + r.Body + "|" + r.PageUrl; foreach (var x in r.Assets) outp += "|" + x.Name + "," + x.Url + "," + x.Size; break; }
                case "cmpver": outp = Release.CompareVersions(Arg(1), Arg(2)).ToString(); break;
                case "sum": outp = Release.FindSumFor(Arg(1), Arg(2)); break;
                case "share": { var d = Share.Decode(Arg(1)); outp = (d.Ok ? 1 : 0) + "|" + d.Error + "|" + d.Name + "|" + (d.WithNames ? 1 : 0); foreach (var m in d.Mods) outp += "|" + m.WorkshopId + "," + m.Name + "," + (m.Enabled ? 1 : 0) + "," + (m.Local ? 1 : 0); break; }
                case "sha": outp = Release.Sha256Hex(Arg(1)); break;
                case "sani": outp = Util.SanitizeFileName(Arg(1)); break;
                case "trim": outp = Util.Trimmed(Arg(1)); break;
                case "snap": outp = ScriptTime.SnapToText(ScriptTime.SnapFromText(Arg(1))); break;
                case "fpc": outp = (Compare.FingerprintChanged(Arg(1), Arg(2)) ? 1 : 0).ToString(); break;
                case "steamid": outp = Names.SteamIdOf(Arg(1)) + "|" + (Playsets.ValidID(Arg(1)) ? 1 : 0); break;
                case "lower": outp = Util.Lower(Arg(1)); break;
                case "tr": outp = Lang.Trf(Arg(1), Arg(2), Arg(3)); break;
                default: continue;
            }
            W($"{fn}|{idx++}|{Esc(outp)}");
        }
    }

    static void Scenario(string root)
    {
        string opts = "";
        Util.ReadFile(Path.Combine(root, "options.txt"), out opts);
        string gameVer = "1.14.2"; var locked = new SortedSet<string>(StringComparer.Ordinal); var over = new Dictionary<string, int>(StringComparer.Ordinal);
        foreach (var line in opts.Split('\n'))
        {
            if (line.StartsWith("game ")) gameVer = line[5..];
            else if (line.StartsWith("lock ")) locked.Add(line[5..]);
            else if (line.StartsWith("cat ")) { int sp = line.LastIndexOf(' '); over[line.Substring(4, sp - 4)] = Util.Atoi(line[(sp + 1)..]); }
        }
        string dir = Path.Combine(root, "ck3");
        var mods = ModScan.ScanMods(dir);
        mods = mods.OrderBy(m => Util.Lower(m.Name), StringComparer.Ordinal).ThenBy(m => m.Id, StringComparer.Ordinal).ToList();
        W($"== mods {mods.Count}");
        foreach (var m in mods) W($"{Esc(m.Id)}|{Esc(m.Name)}|{Esc(m.Version)}|{Esc(m.Supported)}|{m.Source}|{Esc(m.Path)}|{m.ContentState}|{Join(m.Deps)}|{Join(m.ReplacePaths)}|{Join(m.Tags)}");
        Util.ReadFile(Path.Combine(root, "playset.json"), out var ptext);
        var pr = Playsets.Parse(ptext, mods);
        W($"== playset ok={(pr.Ok ? 1 : 0)} err={Esc(pr.Error)} invalid={pr.Invalid} unmatched={pr.Unmatched} name={Esc(pr.Playset.Name)}");
        if (!pr.Ok) return;
        var ps = pr.Playset;
        Playsets.Sync(ps, mods);
        var info = Playsets.InfoMap(mods);
        for (int i = 0; i < ps.Mods.Count; i++) W($"P{i}|{Esc(ps.Mods[i].Id)}|{(ps.Mods[i].Enabled ? 1 : 0)}|{Esc(ps.Mods[i].Name)}");
        W("== export"); W(Playsets.ExportLauncher(ps, info));
        var index = new SortedDictionary<string, ModFiles>(StringComparer.Ordinal); var defs = new SortedDictionary<string, ModDefs>(StringComparer.Ordinal);
        foreach (var m in mods)
        {
            if (m.ContentDir.Length == 0 || m.ContentState != 1) continue;
            var f = Walk.IndexModFiles(m.ContentDir);
            f.Files.Sort(StringComparer.Ordinal);
            var g = new ModFiles { Complete = f.Complete, Files = f.Files };
            index[m.Id] = g;
            defs[m.Id] = Scripts.IndexModDefs(m.ContentDir, g.Files);
        }
        W("== files");
        foreach (var kv in index) W($"F|{Esc(kv.Key)}|{(kv.Value.Complete ? 1 : 0)}|{kv.Value.Files.Count}|{Join(kv.Value.Files, ";")}");
        VanillaIndex? van = null;
        string exe = Path.Combine(root, "install", "binaries", "ck3.exe");
        string gdir = Vanilla.GameDir(exe);
        if (gdir.Length > 0) van = Vanilla.Index(gdir);
        W($"== vanilla {(gdir.Length == 0 ? "none" : "yes")} {(van?.Files.Count ?? 0)} {(van?.DlcFiles ?? 0)}");
        var issues = Analyze.AnalyzePlayset(ps, info, gameVer);
        var cr = Conflicts.Find(ps, info, index, van);
        W($"== conflicts valid={(cr.Valid ? 1 : 0)} idx={cr.ModsIndexed} files={cr.FilesIndexed} without={cr.ModsWithoutFiles} vchecked={(cr.VanillaChecked ? 1 : 0)}");
        foreach (var f in cr.Files) W($"C|{Esc(f.Path)}|{Ints(f.Mods)}|{f.Sev}|{(f.Vanilla ? 1 : 0)}");
        foreach (var p in cr.Pairs) { var a = new StringBuilder(); foreach (var x in p.Areas) a.Append(x.Key).Append(':').Append(x.Value).Append(';'); W($"PAIR|{p.A}|{p.B}|{p.Count}|{p.Sev}|{Esc(a.ToString())}"); }
        foreach (var w in cr.Wipes) W($"WIPE|{w.Replacer}|{w.Victim}|{w.Files}|{Esc(w.Folder)}");
        foreach (var v in cr.Vanilla) W($"VAN|{Esc(v.Path)}|{Ints(v.Mods)}|{v.Sev}");
        foreach (var w in cr.VanillaWipes) W($"VW|{w.Mod}|{w.Files}|{Esc(w.Folder)}");
        W($"LOSES|{Ints(cr.Loses)}"); W($"WINS|{Ints(cr.Wins)}"); W($"VC|{Ints(cr.VanillaCount)}");
        var sr = Scripts.FindScriptConflicts(ps, info, defs);
        W($"== script valid={(sr.Valid ? 1 : 0)} read={sr.ModsRead} missing={sr.ModsMissing}");
        foreach (var it in sr.Items)
        {
            var h = new StringBuilder();
            foreach (var x in it.Hits) h.Append(x.Mod).Append(':').Append(x.File).Append(':').Append(x.Flags).Append(';');
            W($"S|{it.Kind}|{it.Sev}|{it.Winner}|{Esc(it.Area)}|{Esc(it.Key)}|{Esc(it.Note)}|{Esc(h.ToString())}");
        }
        for (int i = 0; i < sr.PerMod.Length; i++) { var sb = new StringBuilder($"SPM|{i}"); for (int k = 0; k < DK.Kinds; k++) sb.Append('|').Append(sr.PerMod[i][k]); sb.Append('|').Append(sr.Unclear[i]); W(sb.ToString()); }
        foreach (var kv in defs)
        {
            var names = new List<string>();
            foreach (var d in kv.Value.Defs) names.Add($"{d.Kind}:{d.Flags}:{kv.Value.NameOf(d)}@{kv.Value.Files[(int)d.File]}");
            names.Sort(StringComparer.Ordinal);
            W($"D|{Esc(kv.Key)}|{kv.Value.BytesRead}|{Join(names, ";")}");
        }
        Analyze.AddConflictIssues(issues, cr, ps, info, gameVer);
        Scripts.AddScriptIssues(issues, sr, ps);
        W("== issues");
        for (int i = 0; i < issues.Length; i++) foreach (var iss in issues[i]) W($"I|{i}|{iss.Sev}|{Esc(iss.Text)}");
        var db = Known.Builtin();
        W($"== known {db.Count}");
        foreach (var m in mods) W($"K|{Esc(m.Id)}|{Known.Find(db, m)}");
        var plan = Sorter.Plan(ps, info, locked, over, cr, index, db);
        W($"== sort changed={(plan.Changed ? 1 : 0)} known={plan.KnownCount} links={plan.PatchLinks} choices={plan.ConflictChoices}");
        W($"ORDER|{Ints(plan.Order)}"); W($"NEWPOS|{Ints(plan.NewPos)}"); W($"CAT|{Ints(plan.Cat)}"); W($"TIER|{Ints(plan.PosTier)}");
        for (int i = 0; i < plan.Cat.Length; i++) W($"CW|{i}|{Esc(plan.CatWhy[i])}|{Esc(plan.Known[i])}");
        foreach (var m in plan.Moves) W($"MV|{m.From}|{m.To}|{Esc(m.Id)}|{Esc(m.Reason)}");
        foreach (var w in plan.Warnings) W($"WARN|{Esc(w)}");
        foreach (var b in plan.Broken) W($"BRK|{b.Mod}|{b.Other}|{b.Kind}|{Esc(b.Text)}");
        foreach (var e in plan.Edges) W($"EDGE|{e.From}|{e.To}|{e.Kind}|{e.Subj}|{Esc(e.Text)}");
        for (int p = 0; p < ps.Mods.Count; p++) { W($"EXPL|{p}"); W(Esc(Sorter.ExplainPosition(plan, ps, info, p, locked.Contains(ps.Mods[p].Id)))); }
        var hr = Health.Check(ps, info, gameVer, locked, over, cr, index, db);
        W("== health"); W(Esc(Health.Text(hr, ps.Name)));
        var ch = Known.FindCompatches(ps, info);
        W("== compat"); W(Esc(Known.CompatText(ch, ps.Name)));
        foreach (var h in ch) W($"CH|{h.State}|{Esc(h.Patch)}|{h.Id}|{Join(h.Parents)}|{h.PatchPos}");
        string c1 = Share.Make(ps, info, false), c2 = Share.Make(ps, info, true);
        W("== share"); W(c1); W(c2);
        foreach (var c in new[] { c1, c2 })
        {
            var d = Share.Decode(c);
            W($"DEC|{(d.Ok ? 1 : 0)}|{Esc(d.Name)}|{(d.WithNames ? 1 : 0)}|{d.Mods.Count}");
            foreach (var m in d.Mods) W($"DM|{m.WorkshopId}|{Esc(m.Name)}|{(m.Enabled ? 1 : 0)}|{(m.Local ? 1 : 0)}");
        }
        W("== report"); W(Esc(Release.BuildSortReport(ps, info, db, plan.Cat, locked, gameVer, 0)));
        var rev = ps.Clone(); rev.Mods.Reverse(); rev.Name = "rev";
        var pd = Compare.ComparePlaysets(ps, rev, info);
        W($"== compare shared={pd.Shared} same={(pd.SameOrder ? 1 : 0)}");
        W($"OA|{Join(pd.OnlyA)}"); W($"OB|{Join(pd.OnlyB)}"); W($"ED|{Join(pd.EnabledDiffers)}"); W($"MV|{Join(pd.Moved)}");
    }

    static int Main(string[] args)
    {
        if (args.Length < 2) { Console.Error.WriteLine("usage: oracle scenario <dir>"); return 2; }
        if (args[0] == "scenario") Scenario(args[1]);
        else if (args[0] == "funcs") Funcs(args[1]);
        using var so = Console.OpenStandardOutput();
        var b = new UTF8Encoding(false).GetBytes(O.ToString());
        so.Write(b, 0, b.Length);
        return 0;
    }
}
