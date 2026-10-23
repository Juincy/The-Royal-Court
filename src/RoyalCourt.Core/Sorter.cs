using System.Text;
namespace RoyalCourt.Core;

public sealed class SortMove { public int From, To; public string Id = "", Reason = ""; }
public sealed class SortViolation { public int Mod, Other = -1, Kind; public string Text = ""; }
public sealed class SortEdge { public int From, To, Kind, Subj; public string Text = ""; }
public sealed class SortPlan
{
    public List<SortEdge> Edges = new();
    public int[] NewPos = Array.Empty<int>();
    public int[] Order = Array.Empty<int>();
    public List<SortMove> Moves = new();
    public List<string> Warnings = new();
    public int[] Cat = Array.Empty<int>();
    public string[] CatWhy = Array.Empty<string>();
    public string[] Known = Array.Empty<string>();
    public int[] PosTier = Array.Empty<int>();
    public List<SortViolation> Broken = new();
    public int ConflictChoices, KnownCount, PatchLinks;
    public bool Changed;
}

public static class Sorter
{
    sealed class Edge { public int From, To, Kind; public string Text = "", Note = "", Other = ""; public bool After; public int Subj; }

    public static SortPlan Plan(Playset ps, IReadOnlyDictionary<string, ModInfo> info, ISet<string> locked,
                                IReadOnlyDictionary<string, int> overrides, ConflictReport? cr, IReadOnlyDictionary<string, ModFiles>? index,
                                IReadOnlyList<KnownMod>? db = null)
    {
        var plan = new SortPlan();
        int n = ps.Mods.Count;
        plan.Cat = new int[n]; plan.CatWhy = new string[n]; plan.Known = new string[n]; plan.PosTier = new int[n];
        for (int i = 0; i < n; i++) { plan.Cat[i] = Core.Cat.Content; plan.CatWhy[i] = ""; plan.Known[i] = ""; }
        var tier = new int[n]; var rank = new int[n]; var kidx = new int[n];
        Array.Fill(kidx, -1);
        var norm = new string[n];
        for (int i = 0; i < n; i++) norm[i] = "";
        var byName = new Dictionary<string, int>(StringComparer.Ordinal);
        for (int i = 0; i < n; i++)
        {
            if (!info.TryGetValue(ps.Mods[i].Id, out var mi)) { plan.CatWhy[i] = Tr("not installed"); continue; }
            norm[i] = Names.Norm(mi.Name);
            var g = Known.GuessCategory(mi);
            if (g.Why == "default" && index != null && index.TryGetValue(ps.Mods[i].Id, out var mf)) g = Known.GuessFromFiles(mf);
            if (db != null && ps.Mods[i].Enabled)
            {
                int k = Known.Find(db, mi);
                if (k >= 0)
                {
                    var km = db[k];
                    kidx[i] = k; plan.Known[i] = Known.NoteText(km); plan.KnownCount++;
                    if (km.Cat >= 0) g = new(km.Cat, plan.Known[i]);
                    tier[i] = km.Pos; rank[i] = km.Rank;
                }
            }
            if (overrides.TryGetValue(ps.Mods[i].Id, out int ov) && ov >= 0 && ov < Core.Cat.Count) g = new(ov, Tr("set by you"));
            plan.Cat[i] = g.Cat; plan.CatWhy[i] = g.Why;
            if (ps.Mods[i].Enabled) byName.TryAdd(Util.Lower(Util.Trimmed(mi.Name)), i);
        }
        bool IsLocked(int i) => locked.Contains(ps.Mods[i].Id) || !ps.Mods[i].Enabled;
        string NameOf(int i) => info.TryGetValue(ps.Mods[i].Id, out var m) ? m.Name : ps.Mods[i].Id;

        var edges = new List<Edge>();
        var outE = new List<int>[n];
        for (int i = 0; i < n; i++) outE[i] = new List<int>();
        void AddEdge(int a, int b, int kind, string text, string note = "", string other = "", bool after = false)
        {
            if (!ps.Mods[a].Enabled || !ps.Mods[b].Enabled) return;
            outE[a].Add(edges.Count);
            edges.Add(new Edge { From = a, To = b, Kind = kind, Text = text, Note = note, Other = other, After = after, Subj = kind == 2 ? (after ? b : a) : b });
        }
        for (int i = 0; i < n; i++)
        {
            if (!info.TryGetValue(ps.Mods[i].Id, out var mi) || !ps.Mods[i].Enabled) continue;
            string self = Util.Lower(Util.Trimmed(mi.Name));
            foreach (var d in mi.Deps)
            {
                string dn = Util.Lower(Util.Trimmed(d));
                if (!byName.TryGetValue(dn, out int f) || f == i || dn == self) continue;
                AddEdge(f, i, 0, Trf("Needs \"{0}\" to load first", NameOf(f)));
            }
        }
        bool Reaches(int from, int target)
        {
            var seen = new bool[n];
            var st = new Stack<int>(); st.Push(from);
            while (st.Count > 0)
            {
                int u = st.Pop();
                if (u == target) return true;
                if (seen[u]) continue;
                seen[u] = true;
                foreach (int e in outE[u]) st.Push(edges[e].To);
            }
            return false;
        }
        if (db != null)
        {
            for (int i = 0; i < n; i++)
            {
                if (kidx[i] < 0) continue;
                var km = db[kidx[i]];
                void Link(string other, bool iAfter)
                {
                    for (int j = 0; j < n; j++)
                    {
                        if (j == i || norm[j].Length == 0 || !ps.Mods[j].Enabled) continue;
                        bool same = Names.NameIs(norm[j], other);
                        if (!same && kidx[j] >= 0) foreach (var kn in db[kidx[j]].Names) if (kn == other) same = true;
                        if (!same) continue;
                        int a = iAfter ? j : i, b = iAfter ? i : j;
                        if (Reaches(b, a))
                        {
                            plan.Warnings.Add(iAfter ? Trf("Could not put \"{0}\" below \"{1}\" (it would contradict a dependency).", NameOf(i), NameOf(j)) : Trf("Could not put \"{0}\" above \"{1}\" (it would contradict a dependency).", NameOf(i), NameOf(j)));
                            continue;
                        }
                        string note = Known.NoteText(km);
                        AddEdge(a, b, 2, iAfter ? (note.Length == 0 ? Trf("loads after \"{0}\"", NameOf(j)) : Trf("{0}: loads after \"{1}\"", note, NameOf(j)))
                                                : (note.Length == 0 ? Trf("loads before \"{0}\"", NameOf(j)) : Trf("{0}: loads before \"{1}\"", note, NameOf(j))), note, NameOf(j), iAfter);
                    }
                }
                foreach (var o in km.After) Link(o, true);
                foreach (var o in km.Before) Link(o, false);
            }
        }
        {
            var acr = new string[n];
            for (int j = 0; j < n; j++) acr[j] = Names.Acronym(norm[j]);
            for (int p = 0; p < n; p++)
            {
                if (plan.Cat[p] != Core.Cat.Patch || norm[p].Length == 0 || !ps.Mods[p].Enabled) continue;
                for (int j = 0; j < n; j++)
                {
                    if (j == p || plan.Cat[j] == Core.Cat.Patch || norm[j].Length < 5 || !ps.Mods[j].Enabled) continue;
                    bool hit = false;
                    string padded = " " + norm[p] + " ";
                    if (padded.Contains(" " + norm[j] + " ", StringComparison.Ordinal)) hit = true;
                    else if (acr[j].Length >= 3 && padded.Contains(" " + acr[j] + " ", StringComparison.Ordinal)) hit = true;
                    if (!hit || Reaches(p, j)) continue;
                    AddEdge(j, p, 3, Trf("Is a patch for \"{0}\", so it loads after it", NameOf(j)));
                    plan.PatchLinks++;
                }
            }
        }
        if (cr != null && cr.Valid && index != null)
        {
            long SizeOf(int i) => index.TryGetValue(ps.Mods[i].Id, out var f) ? f.Files.Count : 0;
            int budget = 3000;
            foreach (var p in cr.Pairs)
            {
                if (budget-- <= 0 || p.Count < 3) continue;
                int a = p.A, b = p.B;
                if (a < 0 || b < 0 || a >= n || b >= n || IsLocked(a) || IsLocked(b) || plan.Cat[a] != plan.Cat[b]) continue;
                if (tier[a] != tier[b] || rank[a] != rank[b]) continue;
                long sa = SizeOf(a), sb = SizeOf(b);
                if (sb <= sa * 2 || sa == 0) continue;
                if (Reaches(a, b)) continue;
                AddEdge(b, a, 1, "loads after \"" + NameOf(b) + "\" (they overwrite " + p.Count + " of the same files; the smaller, more targeted mod goes last so it wins)");
                plan.ConflictChoices++;
            }
        }
        // Kahn's algorithm over the movable mods, always taking the ready mod with the lowest (first/last, type, rank, current position)
        (int, int, int, int) KeyOf(int i) => (tier[i], plan.Cat[i], rank[i], i);
        var indeg = new int[n];
        var done = new bool[n];
        foreach (var e in edges) if (!IsLocked(e.From) && !IsLocked(e.To)) indeg[e.To]++;
        var ready = new SortedSet<(int, int, int, int)>();
        int freeCount = 0;
        for (int i = 0; i < n; i++) { if (IsLocked(i)) continue; freeCount++; if (indeg[i] == 0) ready.Add(KeyOf(i)); }
        var seq = new List<int>();
        bool cycleWarned = false;
        while (seq.Count < freeCount)
        {
            int pick = -1;
            if (ready.Count > 0) { var m = ready.Min; pick = m.Item4; ready.Remove(m); }
            else
            {
                for (int i = 0; i < n; i++) if (!IsLocked(i) && !done[i] && (pick < 0 || KeyOf(i).CompareTo(KeyOf(pick)) < 0)) pick = i;
                int WaitsFor(int u) { foreach (var e in edges) if (e.To == u && !IsLocked(e.From) && !IsLocked(e.To) && !done[e.From]) return e.From; return -1; }
                var where = new int[n]; Array.Fill(where, -1);
                var path = new List<int>();
                for (int u = pick; u >= 0;)
                {
                    if (where[u] >= 0)
                    {
                        int best = -1;
                        for (int q = where[u]; q < path.Count; q++) if (best < 0 || KeyOf(path[q]).CompareTo(KeyOf(best)) < 0) best = path[q];
                        if (best >= 0) pick = best;
                        break;
                    }
                    where[u] = path.Count; path.Add(u);
                    u = WaitsFor(u);
                }
                if (!cycleWarned) { plan.Warnings.Add(Trf("Circular dependency around \"{0}\"; the dependency was ignored.", NameOf(pick))); cycleWarned = true; }
            }
            if (pick < 0 || done[pick]) continue;
            done[pick] = true;
            seq.Add(pick);
            foreach (int e in outE[pick])
            {
                int to = edges[e].To;
                if (IsLocked(to) || IsLocked(pick) || done[to]) continue;
                if (--indeg[to] == 0) ready.Add(KeyOf(to));
            }
        }
        plan.PosTier = (int[])tier.Clone();
        foreach (var e in edges) plan.Edges.Add(new SortEdge { From = e.From, To = e.To, Kind = e.Kind, Subj = e.Subj, Text = e.Kind == 1 ? "" : e.Text });
        foreach (var e in edges)
            if (e.Kind != 0 && e.Kind != 1 && e.From > e.To) plan.Broken.Add(new SortViolation { Mod = e.To, Other = e.From, Kind = e.Kind, Text = e.Text });
        for (int i = 0; i < n; i++)
        {
            if (tier[i] >= 0 || !ps.Mods[i].Enabled) continue;
            int before = 0, firstOther = -1;
            for (int j = 0; j < i; j++) if (ps.Mods[j].Enabled && tier[j] > tier[i]) { before++; if (firstOther < 0) firstOther = j; }
            if (before > 0) plan.Broken.Add(new SortViolation { Mod = i, Other = firstOther, Kind = 4, Text = Trf("{0} Right now {1} other mod(s) load before it.", plan.Known[i], before) });
        }
        plan.Order = new int[n];
        int kk = 0;
        for (int p = 0; p < n; p++) plan.Order[p] = IsLocked(p) ? p : seq[kk++];
        var newPos = new int[n];
        for (int p = 0; p < n; p++) newPos[plan.Order[p]] = p;
        plan.NewPos = newPos;
        foreach (var e in edges)
        {
            if (e.Kind == 1 || e.Kind == 3 || newPos[e.From] < newPos[e.To]) continue;
            if (IsLocked(e.From) || IsLocked(e.To))
            {
                if (e.Kind == 0) plan.Warnings.Add(Trf("\"{0}\" needs \"{1}\" to load first, but a locked mod prevents it.", NameOf(e.To), NameOf(e.From)));
                else if (e.Note.Length == 0) plan.Warnings.Add(e.After ? Trf("\"{0}\" loads after \"{1}\", but a locked mod prevents it.", NameOf(e.To), e.Other) : Trf("\"{0}\" loads before \"{1}\", but a locked mod prevents it.", NameOf(e.To), e.Other));
                else plan.Warnings.Add(e.After ? Trf("\"{0}\" {1}: loads after \"{2}\", but a locked mod prevents it.", NameOf(e.To), e.Note, e.Other) : Trf("\"{0}\" {1}: loads before \"{2}\", but a locked mod prevents it.", NameOf(e.To), e.Note, e.Other));
            }
        }
        for (int p = 0; p < n; p++)
        {
            int old = plan.Order[p];
            if (old != p) plan.Changed = true;
            if (old == p) continue;
            var mv = new SortMove { From = old, To = p, Id = ps.Mods[old].Id };
            string why = "";
            foreach (var e in edges) if (e.To == old && e.Kind != 1 && e.From > old && why.Length == 0) why = e.Text;
            if (why.Length == 0 && tier[old] > 0) why = plan.Known[old];
            if (why.Length == 0 && tier[old] < 0) why = plan.Known[old];
            if (why.Length == 0) foreach (var e in edges) if (e.To == old && e.Kind == 1) { why = Trf("Loads after \"{0}\" (they overwrite the same files; this is the smaller mod, so it wins)", NameOf(e.From)); break; }
            if (why.Length == 0) why = plan.CatWhy[old].Length == 0 || plan.CatWhy[old] == "default" ? Trf("{0} group", Core.Cat.Name(plan.Cat[old])) : Trf("{0} group ({1})", Core.Cat.Name(plan.Cat[old]), plan.CatWhy[old]);
            mv.Reason = why;
            plan.Moves.Add(mv);
        }
        return plan;
    }

    /// <summary>"Why is this mod here?": every rule that ties the mod at OLD position pos to a place, and whether the current order already follows it.</summary>
    public static string ExplainPosition(SortPlan plan, Playset ps, IReadOnlyDictionary<string, ModInfo> info, int pos, bool locked)
    {
        int n = ps.Mods.Count;
        if (pos < 0 || pos >= n || plan.NewPos.Length != n) return "";
        string NameOf(int i) => info.TryGetValue(ps.Mods[i].Id, out var m) ? m.Name : ps.Mods[i].Id;
        var t = new StringBuilder(Trf("\"{0}\" is at position {1} of {2}.", NameOf(pos), pos + 1, n) + "\n");
        if (!ps.Mods[pos].Enabled) return t + Tr("It is turned off: Auto Sort ignores it and no rule about it is applied.") + "\n";
        if (locked) t.Append(Tr("You locked it: Auto Sort never moves this mod.")).Append('\n');
        else if (plan.NewPos[pos] != pos) t.Append(Trf("Auto Sort would move it to position {0}.", plan.NewPos[pos] + 1)).Append('\n');
        else t.Append(Tr("Auto Sort would leave it where it is.")).Append('\n');
        string grp = plan.CatWhy[pos].Length == 0 || plan.CatWhy[pos] == "default" ? Trf("{0} group", Core.Cat.Name(plan.Cat[pos])) : Trf("{0} group ({1})", Core.Cat.Name(plan.Cat[pos]), plan.CatWhy[pos]);
        t.Append(Tr("Group:")).Append(' ').Append(grp).Append('\n');
        if (plan.Known[pos].Length > 0) t.Append(Tr("Known mod:")).Append(' ').Append(plan.Known[pos]).Append('\n');
        if (plan.PosTier[pos] == -2) t.Append(Tr("It belongs at the very top of the load order.")).Append('\n');
        else if (plan.PosTier[pos] == -1) t.Append(Tr("It belongs near the top of the load order.")).Append('\n');
        else if (plan.PosTier[pos] == 1) t.Append(Tr("It belongs at the very bottom of the load order.")).Append('\n');
        var must = new StringBuilder(); var tie = new StringBuilder();
        foreach (var e in plan.Edges)
        {
            if (e.From != pos && e.To != pos) continue;
            int other = e.From == pos ? e.To : e.From;
            bool ok = e.From < e.To;
            string tail = ok ? Trf("already true (position {0})", other + 1) : Trf("not true yet (it is at position {0})", other + 1);
            if (e.Kind == 1)
            {
                if (e.To == pos) tie.Append("  ").Append(Trf("Loads after \"{0}\": they overwrite the same files and this is the smaller mod, so it wins", NameOf(other))).Append(" - ").Append(tail).Append('\n');
                else tie.Append("  ").Append(Trf("\"{0}\" loads after it: they overwrite the same files and \"{0}\" is the smaller mod, so it wins", NameOf(other))).Append(" - ").Append(tail).Append('\n');
                continue;
            }
            string line = e.Subj == pos ? e.Text : NameOf(e.Subj) + ": " + e.Text;
            must.Append("  ").Append(line).Append(" - ").Append(tail).Append('\n');
        }
        if (must.Length > 0) t.Append('\n').Append(Tr("Rules that fix its place:")).Append('\n').Append(must);
        if (tie.Length > 0) t.Append('\n').Append(Tr("Overlap with other mods (a tie-break, not a rule):")).Append('\n').Append(tie);
        if (must.Length == 0 && tie.Length == 0) t.Append('\n').Append(Tr("No rule ties it to a particular place. Auto Sort keeps it inside its group, in its current order.")).Append('\n');
        return t.ToString();
    }
}
