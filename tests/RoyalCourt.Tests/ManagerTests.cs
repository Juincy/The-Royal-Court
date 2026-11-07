using RoyalCourt.Core;
using Xunit;
namespace RoyalCourt.Tests;

public class ManagerTests
{
    static ModManager Make(TestEnv e, out string[] ids)
    {
        string a = e.P("c", "a"), b = e.P("c", "b"), c = e.P("c", "c");
        e.W("c/a/common/x.txt", new string('x', 3000)); e.W("c/a/events/y.txt", new string('y', 2000));
        e.W("c/b/common/z.txt", "z"); e.W("c/c/common/q.txt", "qq");
        e.W("ck3/mod/ugc_1.mod", $"name=\"Alpha\"\nversion=\"1\"\nsupported_version=\"1.99.*\"\npath=\"{a.Replace('\\', '/')}\"\n");
        e.W("ck3/mod/ugc_2.mod", $"name=\"Beta\"\npath=\"{b.Replace('\\', '/')}\"\n");
        e.W("ck3/mod/ugc_3.mod", $"name=\"Gamma\"\npath=\"{c.Replace('\\', '/')}\"\n");
        ids = new[] { "ugc_1.mod", "ugc_2.mod", "ugc_3.mod" };
        var mm = new ModManager { St = new Settings { Ck3Dir = e.Ck3 }, DocRoots = new List<string>() };
        mm.Reload();
        return mm;
    }

    [Fact]
    public void SizesAreMeasuredAndRemembered()
    {
        using var e = new TestEnv();
        var mm = Make(e, out var ids);
        var t = DateTime.UtcNow.AddSeconds(15);
        while (mm.SizeOf(ids[0]) < 0 || mm.SizeOf(ids[1]) < 0 || mm.SizeOf(ids[2]) < 0) { Assert.True(DateTime.UtcNow < t, "sizes never measured"); Thread.Sleep(20); }
        Assert.Equal(5000, mm.SizeOf(ids[0])); Assert.Equal(1, mm.SizeOf(ids[1])); Assert.Equal(2, mm.SizeOf(ids[2]));
        Assert.Equal("4 KB", mm.Rows.First(r => r.Id == ids[0]).Size);
        Assert.True(File.Exists(Path.Combine(Util.DataDir(), "sizes.txt")));
        var again = new ModSizes(); again.Load();
        Assert.Equal(5000, again.Map[ids[0]].bytes);
        Assert.Equal((0L, 0, 0), mm.EnabledSize());   // new mods start turned off
        foreach (var m in mm.Active()!.Mods) m.Enabled = true;
        var (bytes, measured, enabled) = mm.EnabledSize();
        Assert.Equal(3, enabled); Assert.Equal(3, measured); Assert.Equal(5003, bytes);
    }

    [Fact]
    public void NotesAreKeptShownAndSearchable()
    {
        using var e = new TestEnv();
        var mm = Make(e, out var ids);
        mm.SetNote(ids[1], "  check\nlater  ");
        Assert.Equal("check later", mm.NoteOf(ids[1]));
        Assert.Contains("\u270E check later", mm.Rows.First(r => r.Id == ids[1]).Note);
        Assert.DoesNotContain("\u270E", mm.Rows.First(r => r.Id == ids[0]).Note);
        Assert.True(mm.St.Save());
        Assert.Equal("check later", Settings.Load().Notes[ids[1]]);
        mm.Filter = "later"; mm.Populate();
        Assert.Single(mm.Shown);
        mm.Filter = ""; mm.Populate();
        Assert.Equal(300, ModManager.CleanNote(new string('x', 400)).Length);
        mm.SetNote(ids[1], "   ");
        Assert.Equal("", mm.NoteOf(ids[1])); Assert.Empty(mm.St.Notes);
    }

    [Fact]
    public void HandMadeMovesAreRememberedUntilForgotten()
    {
        using var e = new TestEnv();
        var mm = Make(e, out var ids);
        var ps = mm.Active()!; foreach (var m in ps.Mods) m.Enabled = true;
        string moved = ps.Mods[0].Id;
        Assert.True(mm.MoveMod(0, 2));
        Assert.Equal("a:" + ps.Mods[1].Id, mm.St.MyRules[moved]);            // now last: after the mod above it
        Assert.True(mm.HasMyRule(moved));
        Assert.Contains("loads after", mm.MyRulesText());
        string top = ps.Mods[2].Id;
        Assert.True(mm.MoveMod(2, 0));
        Assert.Equal("b:" + ps.Mods[1].Id, mm.St.MyRules[top]);              // dragged to the top: before the mod below it
        Assert.True(mm.St.Save()); Assert.Equal(mm.St.MyRules.Count, Settings.Load().MyRules.Count);
        mm.ForgetMyRule(top); Assert.False(mm.HasMyRule(top));
        mm.ToggleLearnMoves(); Assert.False(mm.St.LearnMoves);
        int before = mm.St.MyRules.Count;
        Assert.True(mm.MoveMod(1, 2)); Assert.Equal(before, mm.St.MyRules.Count);   // not learning any more
        mm.ToggleLearnMoves();
        mm.ToggleLock(new List<int> { 0 }, 0);                                // a locked mod is never remembered
        string lockedId = ps.Mods[0].Id; mm.St.MyRules.Remove(lockedId);
        Assert.True(mm.MoveMod(0, 1)); Assert.False(mm.St.MyRules.ContainsKey(lockedId));
    }

    [Fact]
    public void QuickFiltersNarrowTheListAndBlockDragging()
    {
        using var e = new TestEnv();
        var mm = Make(e, out var ids);
        while (ids.Any(i => mm.SizeOf(i) < 0)) Thread.Sleep(20);
        int all = mm.Shown.Count; Assert.Equal(3, all);
        Assert.False(mm.OrderLocked);
        mm.SetQuick(ModManager.QOutdated);                       // Alpha says 1.99.*, the others say nothing
        Assert.True(mm.OrderLocked);
        Assert.True(mm.Shown.Count <= 1);
        mm.SetQuick(ModManager.QUnknownOrder); Assert.Equal(3, mm.Shown.Count);   // none are in the database
        mm.SetQuick(ModManager.QLarge); Assert.Empty(mm.Shown);
        mm.SetQuick(ModManager.QOff); Assert.Equal(0, mm.Shown.Count(i => mm.Active()!.Mods[i].Enabled));
        mm.SetQuick(ModManager.QAll); Assert.Equal(all, mm.Shown.Count); Assert.False(mm.OrderLocked);
    }

    [Fact]
    public void SizeFormatting()
    {
        Assert.Equal("", ModSizes.Format(-1)); Assert.Equal("0 B", ModSizes.Format(0)); Assert.Equal("1023 B", ModSizes.Format(1023));
        Assert.Equal("1 KB", ModSizes.Format(1024)); Assert.Equal("1.0 MB", ModSizes.Format(1 << 20)); Assert.Equal("10 MB", ModSizes.Format(10L << 20));
        Assert.Equal("1.5 GB", ModSizes.Format(3L << 29)); Assert.Equal("12 GB", ModSizes.Format(12L << 30));
    }
}

public class CompatchTableTests
{
    [Fact]
    public void NewPatchesAreFoundWhenBothParentsAreOn()
    {
        var cs = new List<ModInfo> { H.Mk("1", "A Game of Thrones"), H.Mk("2", "IMHO"), H.Mk("3", "AGOT Nobility of Westeros"), H.Mk("4", "COW-AGOT: 3D Models & Bigger Castles"), H.Mk("5", "Dragons Alive") };
        var p = H.PS("x", cs); var ci = Playsets.InfoMap(cs);
        var got = Known.FindCompatches(p, ci).Select(h => h.Patch).ToHashSet();
        Assert.Contains("[AGOT][IMHO] Compatch", got);
        Assert.Contains("COW-AGOT: Nobility of Westeros Compatch", got);
        Assert.DoesNotContain("Dragons Alive - LOTR Compatch", got);     // Realms in Exile is missing
        Assert.DoesNotContain("AGOT Patch Architecture Expanded", got);
    }
}

public class BisectKeepTests
{
    [Fact]
    public void KeepOnListIsRememberedPerPlayset()
    {
        using var e = new TestEnv();
        e.W("ck3/mod/ugc_1.mod", "name=\"Alpha\"\n"); e.W("ck3/mod/ugc_2.mod", "name=\"Beta\"\n");
        var mm = new ModManager { St = new Settings { Ck3Dir = e.Ck3 }, DocRoots = new List<string>() };
        mm.Reload();
        foreach (var m in mm.Active()!.Mods) m.Enabled = true;
        mm.SetBisectKeep("ugc_1.mod", true);
        var ev = new BisectEvidence();
        var mods = mm.BisectMods(BisectMode.Errors, ev, out _);
        Assert.True(mods.First(m => m.Id == "ugc_1.mod").Keep); Assert.False(mods.First(m => m.Id == "ugc_2.mod").Keep);
        Assert.Contains("ugc_1.mod", Settings.Load().BisectKeep[mm.Active()!.Name]);       // saved to settings.json
        mm.SetBisectKeep("ugc_1.mod", false);
        Assert.False(Settings.Load().BisectKeep.ContainsKey(mm.Active()!.Name));            // an empty list is not kept
        Assert.False(mm.BisectMods(BisectMode.Errors, ev, out _).First(m => m.Id == "ugc_1.mod").Keep);
    }

    [Fact]
    public void KeepOnListRoundTripsThroughSettings()
    {
        using var e = new TestEnv();
        var st = new Settings(); st.BisectKeep["Main"] = new SortedSet<string> { "a.mod", "b.mod" };
        Assert.True(st.Save());
        var back = Settings.Load();
        Assert.Equal(new[] { "a.mod", "b.mod" }, back.BisectKeep["Main"].ToArray());
    }

    [Fact]
    public void BisectResultIsSavedAndShownInTheCrashHelper()
    {
        using var e = new TestEnv();
        e.W("ck3/mod/ugc_1.mod", "name=\"Alpha\"\n"); e.W("ck3/mod/ugc_2.mod", "name=\"Beta\"\n");
        var mm = new ModManager { St = new Settings { Ck3Dir = e.Ck3 }, DocRoots = new List<string>() };
        mm.Reload();
        foreach (var m in mm.Active()!.Mods) m.Enabled = true;
        mm.SaveBisectResult(new[] { "ugc_2.mod" }, new[] { "Beta" }, false);
        var back = Settings.Load().BisectLast[mm.Active()!.Name];
        Assert.Equal(new[] { "ugc_2.mod" }, back.Ids.ToArray()); Assert.Equal("Beta", back.Names[0]); Assert.False(back.TurnedOff); Assert.True(back.When > 1700000000);
        var st = new Settings(); st.BisectLast["Main"] = new Settings.BisectFind { Ids = { "a.mod" }, Names = { "A" }, When = 1790000000, TurnedOff = true };
        Assert.True(st.Save());
        var rt = Settings.Load().BisectLast["Main"]; Assert.True(rt.TurnedOff && rt.Ids[0] == "a.mod" && rt.When == 1790000000);
    }
}
