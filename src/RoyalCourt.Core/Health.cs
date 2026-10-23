using System.Text;
namespace RoyalCourt.Core;

public sealed class HealthItem { public int Sev, Pos; public string Id = "", Name = "", Text = ""; }
public sealed class HealthReport
{
    public List<HealthItem> Items = new();
    public int Enabled, Problems, Warnings;
    public bool OrderIssues;
    public bool Ok => Problems == 0 && Warnings == 0;
}

public static class Health
{
    public static HealthReport Check(Playset ps, IReadOnlyDictionary<string, ModInfo> info, string gameVersion, ISet<string> locked,
                                     IReadOnlyDictionary<string, int> overrides, ConflictReport? cr, IReadOnlyDictionary<string, ModFiles>? index, IReadOnlyList<KnownMod>? db)
    {
        var hr = new HealthReport();
        int n = ps.Mods.Count;
        foreach (var m in ps.Mods) if (m.Enabled) hr.Enabled++;
        string NameOf(int i) => info.TryGetValue(ps.Mods[i].Id, out var mi) ? mi.Name : (ps.Mods[i].Name.Length == 0 ? ps.Mods[i].Id : ps.Mods[i].Name);
        void Add(int sev, int pos, string text)
        {
            var h = new HealthItem { Sev = sev, Pos = pos };
            if (pos >= 0 && pos < n) { h.Id = ps.Mods[pos].Id; h.Name = NameOf(pos); }
            h.Text = text; hr.Items.Add(h);
        }
        if (hr.Enabled == 0) { Add(0, -1, Tr("No mods are turned on.")); return hr; }
        var issues = Analyze.AnalyzePlayset(ps, info, gameVersion);
        for (int i = 0; i < n; i++)
        {
            if (!ps.Mods[i].Enabled) continue;
            foreach (var iss in issues[i]) if (iss.Sev >= 1) Add(iss.Sev, i, iss.Text);
        }
        var plan = Sorter.Plan(ps, info, locked, overrides, cr, index, db);
        foreach (var b in plan.Broken)
        {
            if (b.Mod < 0 || b.Mod >= n) continue;
            Add(1, b.Mod, b.Text);
            hr.OrderIssues = true;
        }
        var tcs = new List<int>();
        for (int i = 0; i < n; i++) if (ps.Mods[i].Enabled && plan.PosTier[i] == -2 && plan.Cat[i] == Cat.Overhaul) tcs.Add(i);
        if (tcs.Count >= 2) Add(1, tcs[1], Trf("Two total conversions are turned on (\"{0}\" and \"{1}\"). A total conversion replaces the whole game setting, so they usually cannot be combined.", NameOf(tcs[0]), NameOf(tcs[1])));
        foreach (var kc in KnownConflicts.Find(ps, info)) Add(1, kc.B, kc.Text);
        if (cr != null && cr.Valid)
        {
            int high = 0;
            foreach (var p in cr.Pairs) if (p.Sev >= (int)Sev.High) high++;
            if (high > 0) Add(1, -1, Trf("{0} serious file conflict(s) between mods. Open Conflicts to see who wins.", high));
            if (cr.Files.Count >= 100000) Add(0, -1, Trf("The conflict list is very large ({0} files). Checking mods in smaller groups makes it easier to read.", cr.Files.Count.ToString("N0")));
        }
        hr.Items = hr.Items.Select((x, i) => (x, i)).OrderBy(t => -t.x.Sev).ThenBy(t => t.x.Pos).ThenBy(t => t.i).Select(t => t.x).ToList();
        foreach (var h in hr.Items) { if (h.Sev >= 2) hr.Problems++; else if (h.Sev == 1) hr.Warnings++; }
        return hr;
    }

    public static string Text(HealthReport hr, string playsetName)
    {
        var t = new StringBuilder(Trf("Health check for \"{0}\": {1} mod(s) turned on", playsetName, hr.Enabled) + "\n\n");
        if (hr.Ok && hr.Enabled > 0) t.Append(Tr("No problems found. This playset looks ready to play.")).Append('\n');
        if (hr.Enabled == 0) t.Append(Tr("No mods are turned on.")).Append('\n');
        void Section(int sev, string head)
        {
            bool any = false;
            foreach (var h in hr.Items)
            {
                if (h.Sev != sev) continue;
                if (!any) { t.Append(head).Append('\n'); any = true; }
                t.Append(h.Pos >= 0 ? Trf("  #{0}  {1}: {2}", h.Pos + 1, h.Name, h.Text) : "  " + h.Text);
                t.Append('\n');
            }
            if (any) t.Append('\n');
        }
        Section(2, Trf("Problems ({0}): the game will probably not start properly", hr.Problems));
        Section(1, Trf("Warnings ({0}): worth fixing", hr.Warnings));
        int notes = 0; foreach (var h in hr.Items) if (h.Sev == 0) notes++;
        if (notes > 0) Section(0, Tr("Notes"));
        if (hr.OrderIssues) t.Append(Tr("Tip: the Auto Sort button can fix the load-order warnings above. You see a preview first, and Undo order puts everything back.")).Append('\n');
        return t.ToString();
    }

    /// <summary>Natural order for versions and names: digit runs compare as numbers ("1.9" &lt; "1.10"), letters ignore case.</summary>
    public static int NaturalCompare(string a, string b)
    {
        int i = 0, j = 0;
        while (i < a.Length && j < b.Length)
        {
            bool da = a[i] >= '0' && a[i] <= '9', db = b[j] >= '0' && b[j] <= '9';
            if (da && db)
            {
                int si = i, sj = j;
                while (si < a.Length && a[si] == '0') si++;
                while (sj < b.Length && b[sj] == '0') sj++;
                int ei = si, ej = sj;
                while (ei < a.Length && a[ei] >= '0' && a[ei] <= '9') ei++;
                while (ej < b.Length && b[ej] >= '0' && b[ej] <= '9') ej++;
                if (ei - si != ej - sj) return ei - si < ej - sj ? -1 : 1;
                int c = string.CompareOrdinal(a, si, b, sj, ei - si);
                if (c != 0) return c < 0 ? -1 : 1;
                i = ei; j = ej;
            }
            else
            {
                char x = a[i] is >= 'A' and <= 'Z' ? (char)(a[i] + 32) : a[i];
                char y = b[j] is >= 'A' and <= 'Z' ? (char)(b[j] + 32) : b[j];
                if (x != y) return x < y ? -1 : 1;
                i++; j++;
            }
        }
        if (i < a.Length) return 1;
        if (j < b.Length) return -1;
        return 0;
    }
}
