using RoyalCourt.Core;
using Xunit;
namespace RoyalCourt.Tests;

public static class H
{
    public static ModInfo Mk(string id, string name, string[]? deps = null, string[]? tags = null) => new() { Id = id, Name = name, Deps = (deps ?? Array.Empty<string>()).ToList(), Tags = (tags ?? Array.Empty<string>()).ToList() };
    public static Playset PS(string name, IEnumerable<ModInfo> ms) { var p = new Playset { Name = name }; foreach (var m in ms) p.Mods.Add(new ModRef(m.Id, true, "")); return p; }
    public static readonly HashSet<string> NoLocks = new();
    public static readonly Dictionary<string, int> NoOv = new();
    public static SortPlan Plan(Playset ps, IReadOnlyDictionary<string, ModInfo> inf, ISet<string>? locked = null, IReadOnlyDictionary<string, int>? ov = null, IReadOnlyList<KnownMod>? db = null, ConflictReport? cr = null, IReadOnlyDictionary<string, ModFiles>? idx = null)
        => Sorter.Plan(ps, inf, locked ?? NoLocks, ov ?? NoOv, cr, idx, db);
    public static List<string> Ids(SortPlan pl, Playset ps) => pl.Order.Select(o => ps.Mods[o].Id).ToList();
    public static Dictionary<string, int> PosById(SortPlan pl, List<ModInfo> ms) { var d = new Dictionary<string, int>(); for (int i = 0; i < pl.Order.Length; i++) d[ms[pl.Order[i]].Id] = i; return d; }
    public static Dictionary<string, int> PosByName(SortPlan pl, List<ModInfo> ms) { var d = new Dictionary<string, int>(); for (int i = 0; i < pl.Order.Length; i++) d[ms[pl.Order[i]].Name] = i; return d; }
    public static ModFiles Files(bool complete, IEnumerable<string> f) => new() { Complete = complete, Files = f.ToList() };
    public static IReadOnlyList<KnownMod> Db = Known.Builtin();
}

public class SortTests
{
    [Fact]
    public void AutoSort()
    {
        var ms = new List<ModInfo> { H.Mk("p.mod", "Big Content Patch"), H.Mk("c1.mod", "Cool Events", new[] { "Lib Core Framework" }), H.Mk("ui.mod", "Better UI"), H.Mk("lib.mod", "Lib Core Framework"), H.Mk("g.mod", "Nice Portraits"), H.Mk("t.mod", "Foo", null, new[] { "Translation" }), H.Mk("c2.mod", "More Stuff") };
        var inf = Playsets.InfoMap(ms); var ps = H.PS("s", ms);
        Assert.True(Known.GuessCategory(inf["p.mod"]).Cat == Cat.Patch && Known.GuessCategory(inf["lib.mod"]).Cat == Cat.Library);
        Assert.True(Known.GuessCategory(inf["ui.mod"]).Cat == Cat.Ui && Known.GuessCategory(inf["g.mod"]).Cat == Cat.Graphics && Known.GuessCategory(inf["t.mod"]).Cat == Cat.Translation);
        Assert.True(Known.GuessCategory(inf["c1.mod"]).Cat == Cat.Content && Known.GuessCategory(H.Mk("x", "Compatibility for X")).Cat == Cat.Patch);
        Assert.Equal(Cat.Content, Known.GuessCategory(H.Mk("x", "Quiet Hill")).Cat);
        var plan = H.Plan(ps, inf); var ids = H.Ids(plan, ps);
        Assert.Equal(new[] { "lib.mod", "c1.mod", "c2.mod", "g.mod", "ui.mod", "t.mod", "p.mod" }, ids);
        Assert.True(plan.Changed && plan.Warnings.Count == 0 && plan.Moves.Count > 0);
        var sorted = new Playset { Name = "s2" }; foreach (var id in ids) sorted.Mods.Add(new ModRef(id, true, ""));
        Assert.False(H.Plan(sorted, inf).Changed);
        {   // turned-off mods are left alone
            var dp = H.PS("d", ms); dp.Mods[0].Enabled = false; dp.Mods[3].Enabled = false;
            var dpl = H.Plan(dp, inf);
            Assert.True(dpl.Order[0] == 0 && dpl.Order[3] == 3);
            foreach (var mv in dpl.Moves) Assert.True(mv.Id != "p.mod" && mv.Id != "lib.mod");
            Assert.Equal(5, dpl.Order.Count(o => dp.Mods[o].Enabled));
            Assert.Empty(dpl.Warnings);
        }
        var pl = H.Plan(ps, inf, new HashSet<string> { "p.mod" }); Assert.True(pl.Order[0] == 0 && pl.Changed);
        var po = H.Plan(ps, inf, null, new Dictionary<string, int> { ["c2.mod"] = Cat.Library });
        Assert.True(ps.Mods[po.Order[0]].Id == "c2.mod" || ps.Mods[po.Order[1]].Id == "c2.mod");
        var cy = new List<ModInfo> { H.Mk("a.mod", "A", new[] { "B" }), H.Mk("b.mod", "B", new[] { "A" }), H.Mk("c.mod", "C", new[] { "C" }) };
        var cp = H.PS("cy", cy); var pc = H.Plan(cp, Playsets.InfoMap(cy));
        Assert.True(pc.Order.Length == 3 && pc.Warnings.Count > 0); Assert.Equal(3, pc.Order.Distinct().Count());
        var lk = new List<ModInfo> { H.Mk("x.mod", "X", new[] { "Y" }), H.Mk("y.mod", "Y") };
        Assert.NotEmpty(H.Plan(H.PS("lk", lk), Playsets.InfoMap(lk), new HashSet<string> { "x.mod" }).Warnings);
        var cf = new List<ModInfo> { H.Mk("small.mod", "Small Tweaks"), H.Mk("big.mod", "Big Pack Stuff") };
        var fp = H.PS("cf", cf);
        var sm = H.Files(true, new[] { "common/a.txt", "common/b.txt", "common/c.txt" });
        var bg = H.Files(true, new[] { "common/a.txt", "common/b.txt", "common/c.txt" }.Concat(Enumerable.Range(0, 20).Select(i => $"events/e{i}.txt")));
        var fi = new Dictionary<string, ModFiles> { ["small.mod"] = sm, ["big.mod"] = bg };
        var fcr = Conflicts.Find(fp, Playsets.InfoMap(cf), fi);
        var pf = H.Plan(fp, Playsets.InfoMap(cf), null, null, null, fcr, fi);
        Assert.True(pf.ConflictChoices == 1 && fp.Mods[pf.Order[1]].Id == "small.mod");
        using var e = new TestEnv();
        var st = new Settings { Ck3Dir = "x" }; st.Locks["Default"] = new SortedSet<string> { "a.mod", "b.mod" }; st.Cats["a.mod"] = Cat.Patch;
        Assert.True(st.Save()); var back = Settings.Load();
        Assert.True(back.Locks["Default"].Count == 2 && back.Cats["a.mod"] == Cat.Patch);
    }

    [Fact]
    public void KnownModsPatchLinksAndNames()
    {
        Assert.True(Names.Norm("Rise & Fall [1.17] v2.3") == "rise and fall" && Names.Norm("Better Barbershop Mod 1.2.3") == "better barbershop mod");
        Assert.True(Names.NameIs("rise and fall", "rise and fall") && Names.NameIs("rise and fall extended", "rise and fall"));
        Assert.True(!Names.NameIs("rise and fall compatibility patch", "rise and fall") && !Names.NameIs("rise and fall of rome", "rise and fall"));
        Assert.True(Names.Acronym("community flavor pack") == "cfp" && Names.Acronym("ethnicities and portraits expanded") == "epe" && Names.Acronym("two words").Length == 0);
        var ms = new List<ModInfo> { H.Mk("ugc_3554844335.mod", "Anything at all"), H.Mk("rui.mod", "RUI"), H.Mk("x1.mod", "Some Content"), H.Mk("bb.mod", "Better Barbershop"), H.Mk("cfp.mod", "Community Flavor Pack"),
            H.Mk("epe.mod", "Ethnicities & Portraits Expanded"), H.Mk("up.mod", "Unofficial Patch"), H.Mk("x2.mod", "More Content"), H.Mk("pt.mod", "CFP + Some Content compatibility patch"), H.Mk("rf2.mod", "Rise and Fall Compatibility Patch") };
        var inf = Playsets.InfoMap(ms); var ps = H.PS("k", ms);
        var plan = H.Plan(ps, inf, null, null, H.Db); var ids = H.Ids(plan, ps);
        int Pos(string id) => ids.IndexOf(id);
        Assert.Equal(0, Pos("up.mod"));
        Assert.True(Pos("ugc_3554844335.mod") > Pos("pt.mod") && Pos("ugc_3554844335.mod") > Pos("rf2.mod") && Pos("ugc_3554844335.mod") > Pos("x2.mod"));
        Assert.True(Pos("rui.mod") == Pos("ugc_3554844335.mod") + 1 && Pos("rui.mod") == ids.Count - 1);
        Assert.True(Pos("bb.mod") > Pos("cfp.mod") && Pos("bb.mod") > Pos("epe.mod"));
        Assert.True(Pos("pt.mod") > Pos("cfp.mod") && Pos("pt.mod") > Pos("x1.mod") && plan.PatchLinks >= 2);
        Assert.True(plan.KnownCount >= 6);
        // the user can still lock a known mod; without the database nothing changes
        Assert.Equal(0, H.Plan(ps, inf, new HashSet<string> { "ugc_3554844335.mod" }, null, H.Db).Order[0]);
        Assert.Equal(0, H.Plan(ps, inf).KnownCount);
    }

    static void Check(List<ModInfo> ms, Action<Dictionary<string, int>> asserts, bool byName = true, bool noWarnings = false)
    {
        var ps = H.PS("w", ms); var pl = H.Plan(ps, Playsets.InfoMap(ms), null, null, H.Db);
        asserts(byName ? H.PosByName(pl, ms) : H.PosById(pl, ms));
        if (noWarnings) Assert.Empty(pl.Warnings);
    }

    [Fact]
    public void WorkshopRounds()
    {
        // 2026-10-12
        var ws = new List<ModInfo> { H.Mk("a.mod", "Love Marriage Family / Travel the Distance / Immersive Chaste Trait Compatch"), H.Mk("b.mod", "Love Marriage Family"), H.Mk("c.mod", "Travel the Distance"), H.Mk("d.mod", "Immersive Chaste Trait"), H.Mk("e.mod", "MHG + EPE Compatch"), H.Mk("f.mod", "More Holding Graphics"), H.Mk("g.mod", "Medieval Arts"), H.Mk("h.mod", "Ethnicities & Portraits Expanded"), H.Mk("i.mod", "Customizable GUI"), H.Mk("j.mod", "Unofficial Patch"), H.Mk("k.mod", "Character UI Overhaul"), H.Mk("l.mod", "Better Barbershop") };
        Check(ws, p => { }, false, true);
        var ps = H.PS("w", ws); var pl = H.Plan(ps, Playsets.InfoMap(ws), null, null, H.Db); var w = H.PosById(pl, ws);
        Assert.True(w["d.mod"] < w["c.mod"] && w["c.mod"] < w["b.mod"] && w["b.mod"] < w["a.mod"]);
        Assert.True(w["g.mod"] < w["f.mod"] && w["h.mod"] < w["f.mod"] && w["f.mod"] < w["e.mod"]);
        Assert.True(w["j.mod"] == 0 && w["i.mod"] == 1);
        Assert.True(w["l.mod"] < w["k.mod"] && pl.Warnings.Count == 0);
        // 2026-10-13
        ws = new List<ModInfo> { H.Mk("a.mod", "MB+DTR Compatch"), H.Mk("b.mod", "DTR Lite"), H.Mk("c.mod", "Dynamic Trade Routes"), H.Mk("d.mod", "More Bookmarks+"), H.Mk("e.mod", "Africa Plus"), H.Mk("f.mod", "Muslim Enhancements"), H.Mk("g.mod", "BAP-ME Compatch"), H.Mk("h.mod", "Culture Expanded") };
        Check(ws, _ => { }, false);
        w = H.PosById(H.Plan(H.PS("w", ws), Playsets.InfoMap(ws), null, null, H.Db), ws);
        Assert.True(w["d.mod"] < w["c.mod"] && w["c.mod"] < w["b.mod"] && w["b.mod"] < w["a.mod"]);
        Assert.True(w["f.mod"] < w["e.mod"] && w["e.mod"] < w["g.mod"] && w["e.mod"] < w["h.mod"]);
        // 2026-10-14
        ws = new List<ModInfo> { H.Mk("a.mod", "Knight Manager"), H.Mk("b.mod", "Unified UI"), H.Mk("c.mod", "Community Flavor Pack"), H.Mk("d.mod", "Godherja: The Dying World") };
        w = H.PosById(H.Plan(H.PS("w", ws), Playsets.InfoMap(ws), null, null, H.Db), ws);
        Assert.True(w["d.mod"] == 0 && w["b.mod"] < w["a.mod"]);
        var rs = new List<ModInfo> { H.Mk("a.mod", "RUI:Character Unofficial"), H.Mk("b.mod", "Hometowns"), H.Mk("c.mod", "Weight of Crown") };
        var rp = H.PS("r", rs); var rplan = H.Plan(rp, Playsets.InfoMap(rs), null, null, H.Db);
        Assert.Equal("a.mod", rp.Mods[rplan.Order[^1]].Id);
        var gs = new List<ModInfo> { H.Mk("a.mod", "AGOT - Armies of Westeros"), H.Mk("b.mod", "A Game of Thrones"), H.Mk("c.mod", "AGOT Submod Core"), H.Mk("d.mod", "COW-AGOT: 3D Models & Bigger Castles"), H.Mk("e.mod", "Better Ruler Designer"), H.Mk("f.mod", "Better UI Scaling") };
        var g = H.PosById(H.Plan(H.PS("g", gs), Playsets.InfoMap(gs), null, null, H.Db), gs);
        Assert.True(g["b.mod"] < g["c.mod"] && g["c.mod"] < g["a.mod"] && g["a.mod"] < g["d.mod"] && g["f.mod"] < g["e.mod"]);
        var ks = new List<ModInfo> { H.Mk("a.mod", "Elder Kings 2 - Nostalgia Music Submod"), H.Mk("b.mod", "Elder Kings 2"), H.Mk("c.mod", "Search & Trade Artifacts"), H.Mk("d.mod", "Unofficial Patch"), H.Mk("e.mod", "AGOT expanded - Dragons"), H.Mk("f.mod", "AGOT More Dragon Eggs") };
        var k = H.PosById(H.Plan(H.PS("k", ks), Playsets.InfoMap(ks), null, null, H.Db), ks);
        Assert.True(k["b.mod"] < k["a.mod"] && k["d.mod"] < k["c.mod"] && k["f.mod"] < k["e.mod"]);
        // 2026-10-15 (3)
        var us = new List<ModInfo> { H.Mk("1", "Separate Name and Title (UE)"), H.Mk("2", "Better Character UI"), H.Mk("3", "Displayed Birth and Death Dates"), H.Mk("4", "In My Humble Opinion"), H.Mk("5", "Better UI Scaling") };
        var u = H.PosByName(H.Plan(H.PS("u", us), Playsets.InfoMap(us), null, null, H.Db), us);
        Assert.True(u["Displayed Birth and Death Dates"] < u["Better UI Scaling"] && u["Displayed Birth and Death Dates"] < u["Better Character UI"]);
        Assert.True(u["Separate Name and Title (UE)"] > u["In My Humble Opinion"] && u["Separate Name and Title (UE)"] > u["Displayed Birth and Death Dates"]);
        // 2026-10-16
        var cs = new List<ModInfo> { H.Mk("1", "Custom Title Form of Address"), H.Mk("2", "Title Manager"), H.Mk("3", "Ethnicities & Portraits Expanded"), H.Mk("4", "Separate Name and Title (UE)") };
        var c = H.PosByName(H.Plan(H.PS("c", cs), Playsets.InfoMap(cs), null, null, H.Db), cs);
        Assert.True(c["Custom Title Form of Address"] > c["Title Manager"] && c["Custom Title Form of Address"] > c["Ethnicities & Portraits Expanded"] && c["Separate Name and Title (UE)"] > c["Custom Title Form of Address"]);
        // 2026-10-17
        cs = new List<ModInfo> { H.Mk("1", "Some Mod"), H.Mk("2", "Other Mod"), H.Mk("3000855939", "Sawwhet's Legacies Mod Pack") };
        var cpl = H.Plan(H.PS("s", cs), Playsets.InfoMap(cs), null, null, H.Db);
        Assert.Equal("Sawwhet's Legacies Mod Pack", cs[cpl.Order[0]].Name);
        // 2026-10-17 AGOT search
        cs = new List<ModInfo> { H.Mk("1", "Grand Remembrance - AGOT Compatibility Submod"), H.Mk("2", "Grand Remembrance"), H.Mk("3", "A Game of Thrones"), H.Mk("4", "Animal Characters - AGOT patch"), H.Mk("5", "Animal Characters (Horses & Dogs)"), H.Mk("6", "Sightseer AGOT Compatibility"), H.Mk("7", "Sightseer") };
        c = H.PosByName(H.Plan(H.PS("g", cs), Playsets.InfoMap(cs), null, null, H.Db), cs);
        Assert.True(c["Grand Remembrance - AGOT Compatibility Submod"] > c["Grand Remembrance"] && c["Grand Remembrance - AGOT Compatibility Submod"] > c["A Game of Thrones"]);
        Assert.True(c["Animal Characters - AGOT patch"] > c["A Game of Thrones"] && c["Animal Characters - AGOT patch"] > c["Animal Characters (Horses & Dogs)"]);
        Assert.True(c["Sightseer AGOT Compatibility"] > c["A Game of Thrones"] && c["Sightseer AGOT Compatibility"] > c["Sightseer"]);
        // 2026-10-21
        cs = new List<ModInfo> { H.Mk("3808352309", "AGOT - Dragonkeepers - T\u00fcrk\u00e7e"), H.Mk("2", "AGOT - Dragonkeepers"), H.Mk("3", "A Game of Thrones"), H.Mk("3808336193", "Principality immunity") };
        c = H.PosById(H.Plan(H.PS("g", cs), Playsets.InfoMap(cs), null, null, H.Db), cs);
        Assert.True(c["3808352309"] > c["2"] && c["3808352309"] > c["3"] && c["3808336193"] > c["3"]);
        // second AGOT batch
        cs = new List<ModInfo> { H.Mk("ugc_3771381436.mod", "Peace Deals Expanded - AGOT Compatibility"), H.Mk("2", "Peace Deals Expanded"), H.Mk("3", "A Game of Thrones"), H.Mk("4", "AGOT Submod Core"), H.Mk("5", "Better Barbershop"),
            H.Mk("ugc_3793472366.mod", "AGOT - Dynamic Aesthetics by Hades & Fred"), H.Mk("ugc_3815803072.mod", "Hades' & Fred's AGOT - Dynamic Aesthetics - Better Barbershop Compatch"),
            H.Mk("ugc_3800913445.mod", "AGOT - Squire Events | \u0420\u0443\u0441\u0438\u0444\u0438\u043a\u0430\u0442\u043e\u0440"), H.Mk("9", "AGOT - Squire Events"), H.Mk("ugc_3810748783.mod", "Dragon Horn For Adventurers") };
        var cp = H.PS("g", cs); var cinf = Playsets.InfoMap(cs);
        c = H.PosById(H.Plan(cp, cinf, null, null, H.Db), cs);
        Assert.True(c["ugc_3771381436.mod"] > c["2"] && c["ugc_3771381436.mod"] > c["3"]);
        Assert.True(c["ugc_3793472366.mod"] > c["3"] && c["ugc_3793472366.mod"] > c["4"] && c["ugc_3793472366.mod"] > c["5"]);
        Assert.True(c["ugc_3815803072.mod"] > c["ugc_3793472366.mod"] && c["ugc_3815803072.mod"] > c["5"]);
        Assert.True(c["ugc_3800913445.mod"] > c["9"] && c["ugc_3810748783.mod"] > c["3"]);
        Assert.DoesNotContain(Known.FindCompatches(cp, cinf), h => h.Patch == "Peace Deals Expanded - AGOT Compatibility");
        // 2026-10-22 Duel Overlay
        cs = new List<ModInfo> { H.Mk("ugc_3765555569.mod", "AGOT - Battlefield Duel + - Duel Overlay Compatch"), H.Mk("2", "Duel Overlay"), H.Mk("3", "AGOT - Battlefield Duel +"), H.Mk("4", "A Game of Thrones"), H.Mk("5", "Elf Destiny Lite V2") };
        cp = H.PS("g", cs); cinf = Playsets.InfoMap(cs);
        c = H.PosById(H.Plan(cp, cinf, null, null, H.Db), cs);
        Assert.True(c["ugc_3765555569.mod"] > c["2"] && c["ugc_3765555569.mod"] > c["3"] && c["ugc_3765555569.mod"] > c["4"]);
        var got = Known.FindCompatches(cp, cinf).Select(h => h.Patch).ToHashSet();
        Assert.True(got.Contains("Elf Destiny Lite V2 Agot Compatch") && !got.Contains("AGOT - Battlefield Duel + - Duel Overlay Compatch"));
        // 2026-10-15: DTR PoW and Holding Sprawl DTR
        var ds = new List<ModInfo> { H.Mk("1", "DTR PoW"), H.Mk("2", "Holding Sprawl DTR"), H.Mk("3", "Prisoners of War"), H.Mk("4", "[Beta] Dynamic Trade Routes") };
        var d = H.PosByName(H.Plan(H.PS("d", ds), Playsets.InfoMap(ds), null, null, H.Db), ds);
        Assert.True(d["DTR PoW"] > d["Prisoners of War"] && d["DTR PoW"] > d["[Beta] Dynamic Trade Routes"] && d["Holding Sprawl DTR"] > d["[Beta] Dynamic Trade Routes"]);
    }

    [Fact]
    public void CompatchFinderAndTable()
    {
        var cm = new List<ModInfo> { H.Mk("1", "More Holding Graphics"), H.Mk("2", "Ethnicities & Portraits Expanded"), H.Mk("3", "Culture Expanded") };
        var cpp = H.PS("c", cm); var cmi = Playsets.InfoMap(cm);
        var hits = Known.FindCompatches(cpp, cmi);
        bool epe = hits.Any(h => h.Patch == "MHG + EPE Compatch"), ce = hits.Any(h => h.Patch == "MHG + CE Compatch"), cfp = hits.Any(h => h.Patch == "MHG + CFP Compatch");
        Assert.True(epe && ce && !cfp);
        cm.Add(H.Mk("9", "MHG + EPE Compatch")); cmi = Playsets.InfoMap(cm); cpp.Mods.Add(new ModRef("9", true, ""));
        foreach (var h in Known.FindCompatches(cpp, cmi)) Assert.NotEqual("MHG + EPE Compatch", h.Patch);
        cpp.Mods[3].Enabled = false;
        Assert.Contains(Known.FindCompatches(cpp, cmi), h => h.Patch == "MHG + EPE Compatch" && h.State == 1);
        cpp.Mods[0].Enabled = false;
        Assert.Empty(Known.FindCompatches(cpp, cmi));
        Assert.Contains("2509797306", Known.CompatText(hits, "c"));

        var seen = new HashSet<string>();
        foreach (var cp in KnownData.Patches)
        {
            Assert.True(cp.Parents.Length >= 2);
            foreach (var g in cp.Parents) { Assert.NotEmpty(g); foreach (var n in g) Assert.Equal(n, Names.Norm(n)); }
            Assert.True(cp.Id.All(ch => ch >= '0' && ch <= '9'));
            Assert.True(seen.Add(cp.Name));
        }
        var cs = new List<ModInfo> { H.Mk("1", "A Game of Thrones"), H.Mk("2", "Better Executions Updated"), H.Mk("3", "A Special World"), H.Mk("4", "Medieval Arts"), H.Mk("5", "Community Flavor Pack"), H.Mk("6", "Ethnicities & Portraits Expanded") };
        var p = H.PS("x", cs); var ci = Playsets.InfoMap(cs);
        var got = Known.FindCompatches(p, ci).Select(h => h.Patch).ToHashSet();
        Assert.True(got.Contains("AGOT - Better Executions Updated Compatibility") && got.Contains("A Special World - Special Building Compatch") && got.Contains("CFP + EPE Compatibility Patch"));
        p.Mods[3].Enabled = false;
        Assert.DoesNotContain("A Special World - Special Building Compatch", Known.FindCompatches(p, ci).Select(h => h.Patch));
    }

    [Fact]
    public void ExplainThisPosition()
    {
        var cs = new List<ModInfo> { H.Mk("1", "Custom Title Form of Address"), H.Mk("2", "Title Manager"), H.Mk("3", "Plain Content Thing") };
        var ci = Playsets.InfoMap(cs); var ep = H.PS("e", cs);
        var epl = H.Plan(ep, ci, null, null, H.Db);
        string ex = Sorter.ExplainPosition(epl, ep, ci, 0, false);
        Assert.True(ex.Contains("Title Manager") && ex.Contains("not true yet") && ex.Contains("position 1 of 3"));
        string ex2 = Sorter.ExplainPosition(epl, ep, ci, 2, true);
        Assert.True(ex2.Contains("locked") && ex2.Contains("No rule ties it"));
        ep.Mods[2].Enabled = false;
        Assert.Contains("turned off", Sorter.ExplainPosition(H.Plan(ep, ci, null, null, H.Db), ep, ci, 2, false));
        Assert.Equal("", Sorter.ExplainPosition(epl, ep, ci, 9, false));
    }

    [Fact]
    public void WorkshopBatchNamesMatch()
    {
        var db = Known.Builtin();
        foreach (var (id, name) in new[] { ("ugc_3716251917.mod", "Commander+ 1.20"), ("ugc_3771234229.mod", "AGOT Aelyrion Dynasty Mod"), ("ugc_3771351073.mod", "MTS for AGoT - Ancient Birthright Patch"), ("ugc_3811591240.mod", "Adoption Options - AGOT BETA"), ("ugc_3776198575.mod", "Ck3 Archipelago randomization"), ("ugc_3232842149.mod", "BG Other Graphics: Zoom Scale Map"), ("ugc_3369079355.mod", "BG Other Graphics: Battleground Commanders"), ("ugc_2240104972.mod", "Extra More Building Slots(16 slots)"), ("ugc_2882431747.mod", "The Witcher's Realms"), ("ugc_3771827282.mod", "The Winter King"), ("ugc_3679787783.mod", "[1.19] New Era Old World"), ("ugc_3477963134.mod", "ATE Southern Land Release"), ("ugc_3376598776.mod", "Battle for Agatharia"), ("ugc_3337903092.mod", "Offical COW:AGOT - Legacy of Valyria Compatch"), ("ugc_2478202444.mod", "Ruler Designer Unlocked"), ("ugc_2882223898.mod", "Smaller World Map") })
            Assert.True(Known.Find(db, H.Mk(id, name)) >= 0, name);
    }

    static int Run(BisectSession b, Func<IReadOnlyList<int>, bool> broken)
    {
        int rounds = 0;
        while (!b.Done) { b.Report(broken(b.Off)); Assert.True(++rounds <= 20); }
        return rounds;
    }

    [Fact]
    public void BisectFindsCulprit()
    {
        for (int culprit = 0; culprit < 13; culprit++)
        {
            var b = new BisectSession(13, _ => 1);
            int rounds = Run(b, off => !off.Contains(culprit));
            Assert.Equal(culprit, b.Culprit);
            Assert.True(rounds <= 6, "rounds " + rounds);   // all-off check, 4 halvings, confirmation
        }
    }

    [Fact]
    public void BisectTriesHeavySuspectFirst()
    {
        var b = new BisectSession(40, i => i == 17 ? 100 : 1);
        b.Report(false);                                  // the all-off check: fixed
        Assert.Equal(new[] { 17 }, b.Group);              // the very suspicious mod is tested alone
        Assert.Equal(17, ((IReadOnlyList<int>)b.Off)[0]);
        b.Report(false);                                  // fixed with it off
        Assert.Equal(BisectPhase.Confirming, b.Phase);
        b.Report(false);
        Assert.Equal(17, b.Culprit); Assert.Equal(3, b.Rounds);
        var c = new BisectSession(40, i => i == 17 ? 100 : 1);   // a wrong guess still finishes
        Run(c, off => !off.Contains(30));
        Assert.Equal(30, c.Culprit);
    }

    [Fact]
    public void BisectKeepsDependenciesTogether()
    {
        var deps = new Dictionary<int, int[]> { [1] = new[] { 0 }, [2] = new[] { 1 }, [5] = new[] { 4 } };
        IEnumerable<int> D(int i) => deps.TryGetValue(i, out var d) ? d : Array.Empty<int>();
        for (int culprit = 0; culprit < 8; culprit++)
        {
            var b = new BisectSession(8, _ => 1, D);
            while (!b.Done)
            {
                var off = new HashSet<int>(b.Off);
                foreach (var kv in deps) if (!off.Contains(kv.Key)) foreach (var d in kv.Value) Assert.False(off.Contains(d), $"{d} is off while {kv.Key} is on");
                b.Report(!off.Contains(culprit));
            }
            Assert.Equal(culprit, b.Culprit);
        }
    }

    [Fact]
    public void BisectIsFastOnBigLists()
    {
        var sw = System.Diagnostics.Stopwatch.StartNew();
        var b = new BisectSession(1200, i => 1 + i % 7, i => i % 5 == 0 && i > 10 ? new[] { i - 10, i - 5 } : Array.Empty<int>());
        Run(b, off => !off.Contains(777));
        Assert.True(b.Culprit == 777);
        Assert.True(sw.ElapsedMilliseconds < 5000, sw.ElapsedMilliseconds + " ms");
    }

    [Fact]
    public void BisectFuzz()
    {
        var rnd = new Random(12345);
        for (int round = 0; round < 400; round++)
        {
            int n = rnd.Next(2, 60);
            var deps = new Dictionary<int, int[]>();
            for (int i = 1; i < n; i++) if (rnd.Next(4) == 0) deps[i] = Enumerable.Range(0, rnd.Next(1, 3)).Select(_ => rnd.Next(0, i)).Distinct().ToArray();
            var keep = Enumerable.Range(0, n).Where(_ => rnd.Next(10) == 0).ToList();
            var w = Enumerable.Range(0, n).Select(_ => rnd.NextDouble() * 5 + 0.1).ToArray();
            var b = new BisectSession(n, i => w[i], i => deps.TryGetValue(i, out var d) ? d : Array.Empty<int>(), keep);
            var cand = Enumerable.Range(0, n).Where(i => !b.Locked.Contains(i)).ToList();
            if (cand.Count == 0) { Assert.True(b.Done); continue; }
            int culprit = cand[rnd.Next(cand.Count)];
            int steps = 0;
            while (!b.Done)
            {
                var off = new HashSet<int>(b.Off);
                foreach (var k in b.Locked) Assert.DoesNotContain(k, off);                                                 // kept mods and what they need stay on
                foreach (var kv in deps) if (!off.Contains(kv.Key)) foreach (var d in kv.Value) Assert.DoesNotContain(d, off);   // nothing off under a mod that is on
                // the problem is gone when the culprit, or something it needs, is off
                bool gone = off.Contains(culprit);
                b.Report(!gone);
                Assert.True(++steps < 4 * Math.Log2(n) + 12, "too many steps " + steps + " of " + n);
            }
            Assert.Equal(BisectPhase.Found, b.Phase);
            // the answer is the culprit, or a mod it needs (turning that off also turns the culprit off)
            var found = b.Culprit!.Value;
            var needs = new HashSet<int> { culprit }; bool grew = true;
            while (grew) { grew = false; foreach (var x in needs.ToList()) if (deps.TryGetValue(x, out var d)) foreach (var y in d) if (needs.Add(y)) grew = true; }
            Assert.Contains(found, needs);
        }
    }

    [Fact]
    public void BisectKeepOnMultipleCulpritsAndNotMods()
    {
        var b = new BisectSession(10, _ => 1, null, new[] { 3 });
        Assert.DoesNotContain(3, b.Remaining);
        while (!b.Done) { Assert.DoesNotContain(3, b.Off); b.Report(!(b.Off.Contains(2) && b.Off.Contains(7))); }   // only turning off both fixes it
        Assert.Contains(2, b.AllCulprits); Assert.Contains(7, b.AllCulprits); Assert.Equal(2, b.AllCulprits.Count);
        var nm = new BisectSession(10, _ => 1); nm.Report(true);
        Assert.True(nm.Done && nm.NotMods && nm.Culprit == null);
        var u = new BisectSession(6, _ => 1); u.Report(false); int g = u.Group.Count; u.Report(true); Assert.True(u.CanUndo); u.Undo(); Assert.Equal(g, u.Group.Count);
    }

    [Fact]
    public void KnownConflictPairs()
    {
        var ms = new List<ModInfo> { H.Mk("ugc_3379545519.mod", "Barbershop For All Characters"), H.Mk("ugc_3383099613.mod", "AGOT Barbershop For All Characters"),
                                     H.Mk("ugc_3810761892.mod", "Set In Stone - Court Statues"), H.Mk("x.mod", "GG Terran's Statues of CK3") };
        var inf = Playsets.InfoMap(ms); var p = H.PS("k", ms);
        var hits = KnownConflicts.Find(p, inf);
        Assert.Equal(2, hits.Count);
        var r = Health.Check(p, inf, "1.20.0.4", H.NoLocks, H.NoOv, null, null, H.Db);
        Assert.True(r.Items.Count(i => i.Text.Contains("should not be used together")) == 2);
        p.Mods[1].Enabled = false; p.Mods[3].Enabled = false;
        Assert.Empty(KnownConflicts.Find(p, inf));
    }

    [Fact]
    public void TotalConversionsAndBottomMods()
    {
        var all = new List<ModInfo> { H.Mk("ugc_2478202444.mod", "Ruler Designer Unlocked"), H.Mk("ugc_3554844335.mod", "Rise and Fall"), H.Mk("x1.mod", "Some Content"), H.Mk("ugc_2882431747.mod", "The Witcher's Realms"),
                                     H.Mk("ugc_2962333032.mod", "A Game of Thrones"), H.Mk("ugc_3771827282.mod", "The Winter King"), H.Mk("ugc_2882223898.mod", "Smaller World Map"), H.Mk("ugc_2291024373.mod", "LOTR: Realms in Exile") };
        for (int rot = 0; rot < all.Count; rot++)    // every starting order: AGOT first, Rise and Fall last, content after the conversions
        {
            var ms = all.Skip(rot).Concat(all.Take(rot)).ToList();
            var inf = Playsets.InfoMap(ms); var ps = H.PS("t", ms);
            var ids = H.Ids(H.Plan(ps, inf, null, null, H.Db), ps);
            Assert.True(ids[0] == "ugc_2962333032.mod", string.Join(",", ids));
            Assert.Equal("ugc_3554844335.mod", ids[^1]);
            Assert.True(ids.IndexOf("x1.mod") > ids.IndexOf("ugc_2882431747.mod") && ids.IndexOf("x1.mod") > ids.IndexOf("ugc_2291024373.mod"));
        }
    }

    [Fact]
    public void ZoomScaleLiteConflicts()
    {
        var ms = new List<ModInfo> { H.Mk("ugc_3508281279.mod", "BG Other Graphics: Zoom Scale Map LITE"), H.Mk("ugc_3232842149.mod", "BG Other Graphics: Zoom Scale Map"), H.Mk("ugc_3369079355.mod", "BG Other Graphics: Battleground Commanders") };
        Assert.Equal(2, KnownConflicts.Find(H.PS("z", ms), Playsets.InfoMap(ms)).Count);
    }

    [Fact]
    public void HealthCheck()
    {
        ModInfo Mh(string id, string name, params string[] deps) { var m = H.Mk(id, name, deps); m.Supported = "1.20.*"; return m; }
        var hs = new List<ModInfo> { Mh("a.mod", "Lib Thing"), Mh("b.mod", "Needs Lib", "Lib Thing"), Mh("c.mod", "Needs Missing", "Not Installed Mod"), Mh("d.mod", "Old Mod") };
        hs[3].Supported = "1.14.*";
        var hi = Playsets.InfoMap(hs); var hp = H.PS("h", hs);
        HealthReport Chk(Playset p, Dictionary<string, ModInfo> inf) => Health.Check(p, inf, "1.20.0.4", H.NoLocks, H.NoOv, null, null, H.Db);
        var r1 = Chk(hp, hi);
        Assert.True(r1.Problems == 1 && r1.Warnings == 1 && !r1.Ok);
        string txt = Health.Text(r1, "h");
        Assert.True(txt.Contains("Not Installed Mod") && txt.Contains("Needs Missing"));
        hp.Mods[2].Enabled = false; hp.Mods[3].Enabled = false;
        var r2 = Chk(hp, hi);
        Assert.True(r2.Ok && r2.Enabled == 2 && Health.Text(r2, "h").Contains("ready to play"));
        hp.Mods[0].Enabled = false;
        Assert.Equal(1, Chk(hp, hi).Problems);
        var ws = new List<ModInfo> { Mh("x.mod", "Cool Events"), Mh("pod.mod", "Princes of Darkness"), Mh("agot.mod", "A Game of Thrones") };
        var wi = Playsets.InfoMap(ws); var wp = H.PS("w", ws);
        var r4 = Chk(wp, wi);
        Assert.True(r4.Warnings >= 3 && r4.OrderIssues);
        Assert.Contains(r4.Items, h => h.Text.Contains("total conversions"));
        var fix = H.Plan(wp, wi, null, null, H.Db);
        var sorted = new Playset { Name = "s" }; foreach (int o in fix.Order) sorted.Mods.Add(wp.Mods[o]);
        var r5 = Chk(sorted, wi);
        Assert.True(!r5.OrderIssues && r5.Warnings == 1);
        var r6 = Chk(new Playset { Name = "n" }, wi);
        Assert.True(r6.Enabled == 0 && r6.Items.Count == 1 && r6.Ok);
    }

    [Fact]
    public void FilesTellWhatAnUnnamedModIs()
    {
        Assert.Equal(Cat.Graphics, Known.GuessFromFiles(H.Files(true, Enumerable.Range(0, 20).Select(i => $"gfx/portraits/a{i}.dds"))).Cat);
        Assert.Equal(Cat.Translation, Known.GuessFromFiles(H.Files(true, Enumerable.Range(0, 20).Select(i => $"localization/english/a{i}.yml"))).Cat);
        Assert.Equal(Cat.Ui, Known.GuessFromFiles(H.Files(true, Enumerable.Range(0, 20).Select(i => $"gui/a{i}.gui"))).Cat);
        Assert.Equal(Cat.Content, Known.GuessFromFiles(H.Files(true, new[] { "common/traits/a.txt", "events/b.txt", "gfx/x.dds" })).Cat);
    }

    [Fact]
    public void UserAndOnlineKnownModLists()
    {
        using var e = new TestEnv();
        int Find(List<KnownMod> db, string nm) { for (int i = 0; i < db.Count; i++) if (db[i].Names.Contains(nm)) return i; return -1; }
        e.W("data/knownmods.json", "{\"mods\":[{\"names\":[\"My Cool Mod\"],\"type\":\"Graphics\",\"position\":\"last\",\"note\":\"mine\"},{\"names\":[\"Rise and Fall\"],\"position\":\"first\",\"note\":\"changed\"}]}");
        var udb = Known.Load(out _);
        int mine = Find(udb, "my cool mod"), rf = Find(udb, "rise and fall");
        Assert.True(mine >= 0 && udb[mine].Cat == Cat.Graphics && udb[mine].Pos == 1);
        Assert.True(rf >= 0 && udb[rf].Pos == -1 && udb[rf].Ids.Count == 0);
        e.W("data/knownmods.json", "{not json");
        Assert.Equal(Known.Builtin().Count, Known.Load(out _).Count);
        File.Delete(e.P("data", "knownmods.json"));
        string online = "{\"format\":1,\"revision\":4,\"mods\":[{\"names\":[\"Online Mod\"],\"type\":\"Interface\",\"position\":\"first\",\"note\":\"from github\"},{\"names\":[\"Rise and Fall\"],\"position\":\"last\",\"note\":\"online\"}]}";
        var oi = Known.CheckOnline(online); Assert.True(oi.Ok && oi.Revision == 4 && oi.Count == 2);
        Assert.False(Known.CheckOnline("").Ok); Assert.False(Known.CheckOnline("{not json").Ok);
        Assert.False(Known.CheckOnline("{\"format\":2,\"mods\":[]}").Ok); Assert.False(Known.CheckOnline("{\"mods\":[]}").Ok); Assert.False(Known.CheckOnline("{\"format\":1}").Ok);
        Assert.False(Known.CheckOnline(new string(' ', 600 * 1024)).Ok);
        File.WriteAllText(Known.OnlineFile(), online);
        var odb = Known.Load(out int rev); Assert.Equal(4, rev);
        int om = Find(odb, "online mod"); Assert.True(om >= 0 && odb[om].Cat == Cat.Ui && odb[om].Pos == -1);
        e.W("data/knownmods.json", "{\"mods\":[{\"names\":[\"Rise and Fall\"],\"position\":\"top\",\"note\":\"user\"}]}");
        var both = Known.Load(out rev); int rf2 = Find(both, "rise and fall");
        Assert.True(rf2 >= 0 && both[rf2].Note == "user" && both[rf2].Pos == -2);
        File.Delete(e.P("data", "knownmods.json"));
        File.WriteAllText(Known.OnlineFile(), "{\"format\":1,\"mods\":\"x\"}");
        Assert.True(Known.Load(out rev).Count == Known.Builtin().Count && rev == 0);
        File.Delete(Known.OnlineFile());
        string shipped = Path.Combine(AppContext.BaseDirectory, "fixtures", "knownmods.json");
        if (File.Exists(shipped)) { var t = File.ReadAllText(shipped); Assert.True(Known.CheckOnline(t).Ok && Known.CheckOnline(t).Revision >= 1); }
    }

    [Fact]
    public void SortReportKeepsPrivateThingsOut()
    {
        var a = new ModInfo { Id = "ugc_3554844335.mod", Name = "Rise and Fall", Path = "C:/secret/path" };
        var b = new ModInfo { Id = "mod/local.mod", Name = "Local Mod", Path = "D:/private/dir" };
        var inf2 = Playsets.InfoMap(new[] { a, b });
        var rp = new Playset { Name = "Test Set" }; rp.Mods.Add(new ModRef(a.Id, true, a.Name)); rp.Mods.Add(new ModRef(b.Id, false, b.Name)); rp.Mods.Add(new ModRef("ugc_999.mod", true, "Gone Mod"));
        string rep = Release.BuildSortReport(rp, inf2, Known.Builtin(), new[] { Cat.Content, Cat.Graphics, Cat.Content }, new HashSet<string> { b.Id }, "1.20.0.4", 4);
        Assert.True(rep.Contains("Program ") && rep.Contains("1.20.0.4") && rep.Contains("online revision 4"));
        Assert.Contains("1. [on] Rise and Fall (Steam 3554844335) | type Content | known mod", rep);
        Assert.Contains("2. [off] Local Mod | type Graphics | locked", rep);
        Assert.True(rep.Contains("3. [on] Gone Mod (Steam 999)") && rep.Contains("not installed"));
        Assert.True(!rep.Contains("secret") && !rep.Contains("private"));
    }

    [Fact]
    public void ShaderCacheScanAndClear()
    {
        using var e = new TestEnv(); string root = e.P("shader");
        Directory.CreateDirectory(Path.Combine(root, "ShaderCache", "sub")); Directory.CreateDirectory(Path.Combine(root, "save games")); Directory.CreateDirectory(Path.Combine(root, "mod"));
        File.WriteAllText(Path.Combine(root, "ShaderCache", "a.bin"), "12345"); File.WriteAllText(Path.Combine(root, "ShaderCache", "sub", "b.bin"), "123"); File.WriteAllText(Path.Combine(root, "save games", "keep.ck3"), "x");
        var sc = ModScan.ScanShaderCache(root);
        Assert.True(sc.Folders.Count == 1 && sc.Files == 2 && sc.Bytes == 8 && sc.Entries.Count == 2);
        Assert.Equal(0, ModScan.ClearShaderCache(sc));
        Assert.True(Directory.Exists(Path.Combine(root, "ShaderCache")) && !File.Exists(Path.Combine(root, "ShaderCache", "a.bin")) && File.Exists(Path.Combine(root, "save games", "keep.ck3")));
        Assert.True(ModScan.ScanShaderCache(root).Entries.Count == 0 && ModScan.ScanShaderCache("").Folders.Count == 0);
        Directory.Delete(Path.Combine(root, "ShaderCache"), true); Assert.Empty(ModScan.ScanShaderCache(root).Folders);
    }
}
