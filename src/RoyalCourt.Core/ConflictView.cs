using System.Text;
namespace RoyalCourt.Core;

/// <summary>What the Conflicts window shows: four views of the same report, a filter, and the "let X win" moves.</summary>
public sealed class ConflictView
{
    public const int ByPair = 0, ByFile = 1, ByDef = 2, ByBase = 3;
    readonly ModManager m;
    public int View;
    public int PairA = -1, PairB = -1;           // set when a pair row was opened: the file list shows only their shared files
    public string Filter = "";
    public List<int> Rows = new();               // visible rows: indexes into pairs / files / definitions of the open view
    public string StatusText = "";
    public ConflictView(ModManager mm) { m = mm; }

    public sealed record Col(string Title, int Width);
    public List<Col> Columns()
    {
        string T(string s) => Tr(s);
        return View switch
        {
            ByPair => new() { new(T("Severity"), 80), new(T("Loses (loads earlier)"), 270), new(T("Wins (loads later)"), 270), new(T("Files"), 60), new(T("Where"), 400) },
            ByFile => new() { new(T("Severity"), 80), new(T("File"), 440), new(T("Winner (loads last)"), 270), new(T("Also in"), 380), new(T("Base game"), 90) },
            ByBase => new() { new(T("Severity"), 80), new(T("Base-game file that mods replace"), 470), new(T("Replaced by (load order, last one is used)"), 520) },
            _ => new() { new(T("Severity"), 80), new(T("Type"), 110), new(T("Area"), 160), new(T("Name"), 240), new(T("Defined by (load order)"), 340), new(T("What happens"), 400) },
        };
    }

    public string ModLabel(int pos)
    {
        var ps = m.Active();
        if (ps == null || pos < 0 || pos >= ps.Mods.Count) return "?";
        return "#" + (pos + 1) + "   " + (m.Info.TryGetValue(ps.Mods[pos].Id, out var mi) ? mi.Name : ps.Mods[pos].Id);
    }

    public string ShortName(int pos)
    {
        var ps = m.Active();
        if (ps == null || pos < 0 || pos >= ps.Mods.Count) return "?";
        string n = m.Info.TryGetValue(ps.Mods[pos].Id, out var mi) ? mi.Name : ps.Mods[pos].Id;
        if (n.Length > 38) n = n[..37] + "…";
        return n;
    }

    public void SetView(int v) { Rows.Clear(); View = v; PairA = PairB = -1; Rebuild(); }

    public void Rebuild()
    {
        Rows = new();
        string f = Util.Lower(Filter);
        var ps = m.Active();
        var names = new List<string>();   // lower-case mod names by playset position, for filtering
        if (ps != null && f.Length > 0) foreach (var pm in ps.Mods) names.Add(Util.Lower(m.Info.TryGetValue(pm.Id, out var mi) ? mi.Name : pm.Id));
        string Nm(int p) => p >= 0 && p < names.Count ? names[p] : "";
        bool SevHit(int sev) => f.Length > 0 && Util.Lower(Conflicts.SevName(sev)).Contains(f, StringComparison.Ordinal);
        var conf = m.Conf; var script = m.Script;
        if (conf.Valid)
        {
            if (View == ByPair)
            {
                for (int i = 0; i < conf.Pairs.Count; i++)
                {
                    var p = conf.Pairs[i];
                    if (f.Length > 0)
                    {
                        bool hit = SevHit(p.Sev) || Nm(p.A).Contains(f, StringComparison.Ordinal) || Nm(p.B).Contains(f, StringComparison.Ordinal);
                        foreach (var a in p.Areas) if (!hit && a.Key.Contains(f, StringComparison.Ordinal)) hit = true;
                        if (!hit) continue;
                    }
                    Rows.Add(i);
                }
            }
            else if (View == ByFile)
            {
                for (int i = 0; i < conf.Files.Count; i++)
                {
                    var fc = conf.Files[i];
                    if (PairA >= 0 && (!fc.Mods.Contains(PairA) || !fc.Mods.Contains(PairB))) continue;
                    if (f.Length > 0 && !SevHit(fc.Sev) && !fc.Path.Contains(f, StringComparison.Ordinal) && !Nm(fc.Mods[^1]).Contains(f, StringComparison.Ordinal)) continue;
                    Rows.Add(i);
                }
            }
            else if (View == ByBase)
            {
                for (int i = 0; i < conf.VanillaWipes.Count; i++)   // replace_path folders first: they remove whole folders of the base game
                {
                    var vw = conf.VanillaWipes[i];
                    if (f.Length > 0 && !SevHit((int)Sev.High) && !vw.Folder.Contains(f, StringComparison.Ordinal) && !Nm(vw.Mod).Contains(f, StringComparison.Ordinal) && !"replace_path".Contains(f, StringComparison.Ordinal)) continue;
                    Rows.Add(conf.Vanilla.Count + i);
                }
                for (int i = 0; i < conf.Vanilla.Count; i++)
                {
                    var vf = conf.Vanilla[i];
                    if (f.Length > 0 && !SevHit(vf.Sev) && !vf.Path.Contains(f, StringComparison.Ordinal))
                    {
                        bool hit = false;
                        foreach (int mm in vf.Mods) if (Nm(mm).Contains(f, StringComparison.Ordinal)) { hit = true; break; }
                        if (!hit) continue;
                    }
                    Rows.Add(i);
                }
            }
        }
        if (View == ByDef && script.Valid)
        {
            for (int i = 0; i < script.Items.Count; i++)
            {
                var sc = script.Items[i];
                if (f.Length > 0)
                {
                    bool hit = SevHit(sc.Sev != 0 ? (int)Sev.Med : (int)Sev.Low) || Util.Lower(sc.Key).Contains(f, StringComparison.Ordinal) || Util.Lower(sc.Area).Contains(f, StringComparison.Ordinal) || Util.Lower(Scripts.DefKindName(sc.Kind)).Contains(f, StringComparison.Ordinal);
                    for (int x = 0; x < sc.Hits.Count && !hit; x++) hit = Nm(sc.Hits[x].Mod).Contains(f, StringComparison.Ordinal);
                    if (!hit) continue;
                }
                Rows.Add(i);
            }
        }
        StatusText = Status();
    }

    string Status()
    {
        var conf = m.Conf; var script = m.Script;
        if (m.Scanning) return Trf("Reading mod files... {0}/{1}", m.ScanDone, m.ScanTotal);
        if (m.ConflictsRunning) return Tr("Calculating conflicts...");
        if (View == ByDef)
        {
            if (!script.Valid) return Tr("No script data yet. Enable some mods that have a files folder, then press Rescan files.");
            var cnt = new int[DK.Kinds];
            foreach (var sc in script.Items) if (sc.Kind >= 0 && sc.Kind < DK.Kinds) cnt[sc.Kind]++;
            string st = Trf("{0} overlaps", script.Items.Count) + "  |  " + Trf("{0} event IDs", cnt[DK.Event]) + "  |  " + Trf("{0} definitions/defines", cnt[DK.Common] + cnt[DK.Define]) +
                        "  |  " + Trf("{0} on_actions", cnt[DK.OnAction]) + "  |  " + Trf("{0} localization keys.", cnt[DK.Loc]) + "  " + Tr("Double-click a row for the files.");
            if (script.ModsMissing > 0) st += "  " + Trf("({0} mods not read)", script.ModsMissing);
            return st;
        }
        if (!conf.Valid) return Tr("No file data yet. Enable some mods that have a files folder, then press Rescan files.");
        if (View == ByBase)
        {
            if (!conf.VanillaChecked) return Tr("The game's own files could not be read (is the game installed? Play once, or set the game in Advanced), so mods cannot be compared with them.");
            int mods = conf.VanillaCount.Count(c => c > 0);
            string st = conf.VanillaWipes.Count == 0 ? Trf("{0} base-game files are replaced by {1} mods.", conf.Vanilla.Count, mods)
                : Trf("{0} base-game files are replaced by {1} mods", conf.Vanilla.Count, mods) + "  |  " + Trf("{0} replace_path folders remove base-game files.", conf.VanillaWipes.Count);
            return st + "  " + Tr("Double-click a row for who wins, right-click to reorder.");
        }
        string s = Trf("{0} conflicting files", conf.Files.Count) + "  |  " + Trf("{0} mod pairs", conf.Pairs.Count) + "  |  " + Trf("{0} mods checked", conf.ModsIndexed);
        if (conf.ModsWithoutFiles > 0) s += " " + Trf("({0} skipped: no readable files folder)", conf.ModsWithoutFiles);
        if (View == ByFile && PairA >= 0) s = Trf("Files shared by {0}  and  {1}  (press \"By file\" to see all)", ModLabel(PairA), ModLabel(PairB)) + "  |  " + Trf("{0} files", Rows.Count);
        else if (View == ByPair) s += ".  " + Tr("The later mod wins. Double-click a pair for its files, right-click to reorder.");
        else s += ".  " + Tr("Double-click a file for who wins.");
        return s;
    }

    /// <summary>Severity of a row of the open view (-1: none).</summary>
    public int RowSev(int row)
    {
        if (row < 0 || row >= Rows.Count) return -1;
        int idx = Rows[row];
        var conf = m.Conf; var script = m.Script;
        if (View == ByPair) return idx < conf.Pairs.Count ? conf.Pairs[idx].Sev : -1;
        if (View == ByFile) return idx < conf.Files.Count ? conf.Files[idx].Sev : -1;
        if (View == ByBase) return idx < conf.Vanilla.Count ? conf.Vanilla[idx].Sev : idx - conf.Vanilla.Count < conf.VanillaWipes.Count ? (int)Sev.High : -1;
        return idx < script.Items.Count ? (script.Items[idx].Sev != 0 ? (int)Sev.Med : (int)Sev.Low) : -1;
    }

    public string Cell(int row, int sub)
    {
        if (row < 0 || row >= Rows.Count) return "";
        int idx = Rows[row];
        var conf = m.Conf; var script = m.Script;
        if (sub == 0) { int sv = RowSev(row); return sv < 0 ? "" : Conflicts.SevName(sv); }
        sub--;
        var t = new StringBuilder();
        if (View == ByPair)
        {
            if (idx >= conf.Pairs.Count) return "";
            var p = conf.Pairs[idx];
            if (sub == 0) return ModLabel(p.A);
            if (sub == 1) return ModLabel(p.B);
            if (sub == 2) return p.Count.ToString();
            for (int i = 0; i < p.Areas.Count && i < 4; i++) t.Append(i > 0 ? ",  " : "").Append(p.Areas[i].Key).Append(" ×").Append(p.Areas[i].Value);
            if (p.Areas.Count > 4) t.Append("  ...");
        }
        else if (View == ByDef)
        {
            if (idx >= script.Items.Count) return "";
            var sc = script.Items[idx];
            if (sub == 0) return Scripts.DefKindName(sc.Kind);
            if (sub == 1) return sc.Area;
            if (sub == 2) return sc.Key;
            if (sub == 3)
            {
                var seen = new HashSet<int>();
                foreach (var h in sc.Hits) if (seen.Add(h.Mod)) t.Append(t.Length == 0 ? "" : ";  ").Append(ModLabel(h.Mod));
            }
            else
            {
                t.Append(sc.Note);
                if (sc.Winner >= 0) t.Append("  (").Append(ModLabel(sc.Winner)).Append(')');
            }
        }
        else if (View == ByBase)
        {
            if (idx >= conf.Vanilla.Count)
            {
                int wi = idx - conf.Vanilla.Count;
                if (wi >= conf.VanillaWipes.Count) return "";
                var vw = conf.VanillaWipes[wi];
                return sub == 0 ? vw.Folder + "/   " + Trf("(the whole folder: {0} base-game files removed)", vw.Files) : ModLabel(vw.Mod) + "   (replace_path)";
            }
            var vf = conf.Vanilla[idx];
            if (sub == 0) return vf.Path;
            for (int i = 0; i < vf.Mods.Count && i < 5; i++) t.Append(i > 0 ? ";  " : "").Append(ModLabel(vf.Mods[i]));
            if (vf.Mods.Count > 5) t.Append("  ").Append(Trf("+{0} more", vf.Mods.Count - 5));
        }
        else
        {
            if (idx >= conf.Files.Count) return "";
            var fc = conf.Files[idx];
            if (sub == 0) return fc.Path;
            if (sub == 1) return ModLabel(fc.Mods[^1]);
            if (sub == 2)
            {
                for (int i = 0; i + 1 < fc.Mods.Count && i < 4; i++) t.Append(i > 0 ? ";  " : "").Append(ModLabel(fc.Mods[i]));
                if (fc.Mods.Count > 5) t.Append("  ").Append(Trf("+{0} more", fc.Mods.Count - 5));
            }
            else return fc.Vanilla ? Tr("yes") : "";
        }
        return t.ToString();
    }

    /// <summary>"who wins" for a row: the mods that ship it, in load order (the last one's copy is used).</summary>
    public List<int> RowMods(int row, out string what)
    {
        what = "";
        var v = new List<int>();
        if (row < 0 || row >= Rows.Count) return v;
        int idx = Rows[row];
        var conf = m.Conf; var script = m.Script;
        if (View == ByPair && idx < conf.Pairs.Count) { v = new() { conf.Pairs[idx].A, conf.Pairs[idx].B }; what = Tr("Files both mods ship"); }
        else if (View == ByFile && idx < conf.Files.Count) { v = new(conf.Files[idx].Mods); what = conf.Files[idx].Path; }
        else if (View == ByBase && idx < conf.Vanilla.Count) { v = new(conf.Vanilla[idx].Mods); what = conf.Vanilla[idx].Path; }
        else if (View == ByBase && idx - conf.Vanilla.Count >= 0 && idx - conf.Vanilla.Count < conf.VanillaWipes.Count) { var vw = conf.VanillaWipes[idx - conf.Vanilla.Count]; v = new() { vw.Mod }; what = vw.Folder + "/"; }
        else if (View == ByDef && idx < script.Items.Count)
        {
            var seen = new HashSet<int>();
            foreach (var h in script.Items[idx].Hits) if (seen.Add(h.Mod)) v.Add(h.Mod);
            v.Sort();
            what = script.Items[idx].Key;
        }
        return v;
    }

    /// <summary>Moves one mod right next to another so the mod that should win loads last. The old order is kept for the Undo button.</summary>
    public void MoveFor(int mover, int target)
    {
        var ps = m.Active();
        if (ps == null || mover == target || mover < 0 || target < 0 || mover >= ps.Mods.Count || target >= ps.Mods.Count) return;
        m.UndoIds = ps.Mods.Select(x => x.Id).ToList();
        m.UndoPlayset = ps.Name;
        string a = ShortName(mover), b = ShortName(target);
        bool below = mover < target;
        m.MoveMod(mover, target);   // erased first, then inserted at the target's position: right after it when moving down, right before it when moving up
        PairA = PairB = -1;
        m.Say(below ? Trf("Moved \"{0}\" below \"{1}\". Press Undo to go back.", a, b) : Trf("Moved \"{0}\" above \"{1}\". Press Undo to go back.", a, b));
        if (View == ByFile) View = ByPair;
        Rebuild();
    }

    public sealed record MenuItem(int Id, string Text);
    public const int MnBelow = 900, MnAbove = 901, MnFiles = 902, MnCopy = 903;
    public List<MenuItem> ContextMenu(int row, out int lo, out int hi, out string what)
    {
        var mods = RowMods(row, out what);
        lo = hi = -1;
        var items = new List<MenuItem>();
        if (row < 0 || row >= Rows.Count) return items;
        if (mods.Count >= 2) { lo = mods[^2]; hi = mods[^1]; }   // the runner-up and the mod that wins now
        if (lo >= 0 && hi >= 0 && lo != hi)
        {
            items.Add(new(MnBelow, Trf("Let \"{0}\" win: move it below \"{1}\"", ShortName(lo), ShortName(hi))));
            items.Add(new(MnAbove, Trf("Let \"{0}\" win: move \"{1}\" above it", ShortName(lo), ShortName(hi))));
        }
        if (View == ByPair) items.Add(new(MnFiles, Tr("Show the files they share")));
        if (View == ByFile || View == ByBase) items.Add(new(MnCopy, Tr("Copy file path")));
        return items;
    }

    /// <summary>The pair row's files: switch to the file view limited to the two mods.</summary>
    public void ShowPairFiles(int row)
    {
        if (View != ByPair || row < 0 || row >= Rows.Count) return;
        int idx = Rows[row];
        if (idx >= m.Conf.Pairs.Count) return;
        int a = m.Conf.Pairs[idx].A, b = m.Conf.Pairs[idx].B;
        Rows.Clear(); View = ByFile; PairA = a; PairB = b; Rebuild();
    }

    /// <summary>The detail text for a double-click on a file / base-game row; null when the row has none.</summary>
    public string? WinnerText(int row, out string title)
    {
        title = "";
        var mods = RowMods(row, out string what);
        if (mods.Count < 2 && View != ByBase) return null;
        var conf = m.Conf;
        string t;
        if (View == ByBase && Rows[row] >= conf.Vanilla.Count)
        {
            var vw = conf.VanillaWipes[Rows[row] - conf.Vanilla.Count];
            title = "replace_path";
            return what + "\n\n" + Trf("{0} uses replace_path for this folder. While it is enabled, none of the {1} files the base game has there are loaded (and neither are files of mods that load before it). Total conversions do this on purpose; for any other mod it is worth a look.", ModLabel(vw.Mod), vw.Files);
        }
        if (View == ByBase) t = what + "\n\n" + Tr("This is a file of the base game. A mod's copy REPLACES it completely: the game does not merge them.\n\nShipped by (load order, the last one's copy is used):\n");
        else
        {
            bool van = View == ByFile && Rows[row] < conf.Files.Count && conf.Files[Rows[row]].Vanilla;
            t = what + "\n\n" + (van ? Tr("This file also exists in the base game, so the winner's copy replaces the game's.\n\n") : "") + Tr("Load order (the last mod's copy is used):\n");
        }
        var ps = m.Active();
        var sb = new StringBuilder(t);
        for (int k = 0; k < mods.Count; k++)
        {
            sb.Append("  ").Append(ModLabel(mods[k]));
            if (ps != null && mods[k] >= 0 && mods[k] < ps.Mods.Count && m.Info.TryGetValue(ps.Mods[mods[k]].Id, out var mi))
                if (View == ByBase && ModScan.MatchGameVersion(mi.Supported, m.GameVer) == VerMatch.Mismatch) sb.Append("   ").Append(Tr("(made for an older game version)"));
            sb.Append("     ").Append(k + 1 == mods.Count ? Tr("← wins") : Tr("loses")).Append('\n');
        }
        if (View == ByBase && mods.Count == 1) sb.Append('\n').Append(Tr("The game's own copy is not used while this mod is enabled."));
        sb.Append('\n').Append(Tr("To change who wins, right-click the row and move one of the mods."));
        title = View == ByBase ? Tr("Base-game file") : Tr("Who wins");
        return sb.ToString();
    }

    public string? ScriptDetail(int row)
    {
        if (View != ByDef || row < 0 || row >= Rows.Count || Rows[row] >= m.Script.Items.Count) return null;
        var sc = m.Script.Items[Rows[row]];
        var t = new StringBuilder(Scripts.DefKindName(sc.Kind) + ": " + sc.Key + "\n" + sc.Area + "\n\n" + sc.Note + "\n\n");
        t.Append(Tr("Defined in (load order):\n"));
        foreach (var hh in sc.Hits) t.Append("  ").Append(ModLabel(hh.Mod)).Append("\n      ").Append(hh.File).Append('\n');
        return t.ToString();
    }

    /// <summary>"Rescan files": forgets every file list and reads them again.</summary>
    public void Rescan()
    {
        m.ForgetIndexes();
        m.MaybeScan(false);
        Rebuild();
        m.RefreshNotes();
    }
}
