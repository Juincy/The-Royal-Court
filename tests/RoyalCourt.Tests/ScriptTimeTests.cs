using RoyalCourt.Core;
using Xunit;
namespace RoyalCourt.Tests;

public class ScriptTimeTests
{
    static void Def(ModDefs md, int kind, string nm) { md.Defs.Add(new DefEntry { NameOff = (uint)md.Pool.Length, NameLen = (ushort)nm.Length, Kind = (byte)kind }); md.Pool += nm; }

    const string EvCsv = "#ID;IsValidToTrigger;MajorTrigger;Weight;ImmediateEffect;TriggerFailEffect;OptionEffect;OptionAi;AfterEffect;IsValidToTriggerCount;ImmediateEffectCount;ExecuteCount;Sum;%;\r\n"
        + "AAA_maint.100;0.5;0;0;60;0;0;0;0;1000;500;0;60.5;60.50%;\r\n"
        + "BBB_x.1;0.1;0;0;30;0;0;0;0;10;10;10;30.1;30.10%;\r\n"
        + "vanilla_event.0001;0;0;0;9;0;0;0;0;1;1;1;9.0;9.00%;\r\n"
        + "broken row without enough columns\r\n"
        + "AAA_maint.101;0.0;0;0;0.4;0;0;0;0;5;5;0;0.4;0.40%;\r\n";
    const string OaCsv = "#ID;IsValidToTrigger;ExecuteEffect;ExecuteEvents;ExecuteFirstValidEvent;ExecuteRandomEvent;ExecuteEventsTotal;ExecuteOnActions;ExecuteFirstValidOnAction;ExecuteRandomOnAction;ExecuteOnActionsTotal;ExecuteFallback;IsValidToTriggerCount;ExecuteCount;Sum;%;\n"
        + "on_death;0.1;2.0;5;0;0;5;3;0;0;3;0;10;10;10.1;50%;\n"
        + "AAA_on_death_extra;0;4.0;0;0;0;0;0;0;0;0;0;10;10;4.0;20%;\n"
        + "shared_pulse;0;1.0;0;0;0;0;0;0;0;0;0;10;10;1.0;5%;\n";

    [Fact]
    public void StatisticsFilesAndAttribution()
    {
        var ev = ScriptTime.ParseStatsCsv(EvCsv, false); var oa = ScriptTime.ParseStatsCsv(OaCsv, true);
        Assert.True(ev.Ok && ev.Rows.Count == 4 && oa.Ok && oa.Rows.Count == 3);
        Assert.True(ev.Rows[0].Id == "AAA_maint.100" && ev.Rows[0].Count == 1000 && Math.Abs(ev.Total - 100.0) < 1e-9);
        Assert.True(Math.Abs(ev.Rows[0].Share - 0.605) < 1e-9);
        double od = oa.Rows.First(r => r.Id == "on_death").Own;
        Assert.True(Math.Abs(od - 2.1) < 1e-9);      // 10.1 - 5 - 3
        Assert.False(ScriptTime.ParseStatsCsv("garbage\nmore", false).Ok); Assert.False(ScriptTime.ParseStatsCsv("", true).Ok); Assert.False(ScriptTime.ParseStatsCsv("#ID;Foo;\nx;1;", false).Ok);
        var defs = new Dictionary<string, ModDefs>();
        { var m = new ModDefs(); Def(m, DK.Event, "AAA_maint.100"); Def(m, DK.Event, "AAA_maint.101"); Def(m, DK.OnAction, "AAA_on_death_extra"); Def(m, DK.OnAction, "on_death"); Def(m, DK.OnAction, "shared_pulse"); defs["1"] = m; }
        { var m = new ModDefs(); Def(m, DK.Event, "BBB_x.1"); Def(m, DK.OnAction, "on_death"); Def(m, DK.OnAction, "shared_pulse"); defs["2"] = m; }
        var ps = new Playset { Name = "t" }; ps.Mods.Add(new ModRef("1", true, "")); ps.Mods.Add(new ModRef("2", true, ""));
        var tr = ScriptTime.AttributeTime(ev, oa, ps, defs);
        Assert.True(tr.PerMod.Count == 2 && tr.PerMod[0].Mod == 0);
        Assert.True(Math.Abs(tr.PerMod[0].Time - 64.9) < 1e-9 && tr.PerMod[0].Slowest == "AAA_maint.100");
        Assert.True(Math.Abs(tr.PerMod[1].Time - 30.1) < 1e-9);
        Assert.True(Math.Abs(tr.BaseEvents - 9.0) < 1e-9);
        Assert.True(Math.Abs(tr.SharedActions - 3.1) < 1e-9);
        Assert.True(Math.Abs(tr.All - (100.0 + 7.1)) < 1e-9);
        ps.Mods[0].Enabled = false;
        var t2 = ScriptTime.AttributeTime(ScriptTime.ParseStatsCsv(EvCsv, false), ScriptTime.ParseStatsCsv(OaCsv, true), ps, defs);
        Assert.True(t2.PerMod.Count == 1 && t2.PerMod[0].Mod == 1);
        string yr = "[08:02:53][I][jomini_log.cpp:23]: [AUTOMATION_STATS][living characters][Year][.]:[22520][1276]\n"
            + "[08:02:53][I][jomini_log.cpp:23]: [AUTOMATION_STATS][opinions][Year][.]:[53697][1276]\n"
            + "[09:39:49][I][jomini_log.cpp:23]: [AUTOMATION_STATS][living characters][Year][.]:[23758][1277]\n"
            + "[09:39:49][I][jomini_log.cpp:23]: [AUTOMATION_STATS][errors][Year][.]:[100000][1277]\n"
            + "[09:39:49][I][jomini_log.cpp:23]: something else\n";
        var ys = ScriptTime.ParseYearStats(yr);
        Assert.True(ys.Count == 2 && ys[0].Year == 1276 && ys[0].V["living characters"] == 22520 && ys[1].V["errors"] == 100000 && ys[1].V["living characters"] == 23758);
        Assert.Empty(ScriptTime.ParseYearStats(""));
    }

    [Fact]
    public void OnActionCreditHistoryAndAuthorReport()
    {
        using var e = new TestEnv();
        string oa = "#ID;ExecuteEventsTotal;ExecuteOnActionsTotal;ExecuteCount;Sum;\n" + "zeta_unique;0;0;5;8.0;\n" + "on_birth;0;0;5;2.0;\n" + "twin;0;0;5;1.0;\n";
        var defs = new Dictionary<string, ModDefs>();
        { var m = new ModDefs(); Def(m, DK.Event, "AAA_x.1"); Def(m, DK.OnAction, "zeta_unique"); Def(m, DK.OnAction, "on_birth"); Def(m, DK.OnAction, "twin"); defs["1"] = m; }
        { var m = new ModDefs(); Def(m, DK.Event, "BBB_x.1"); Def(m, DK.OnAction, "twin"); defs["2"] = m; }
        var ps = new Playset { Name = "h" }; ps.Mods.Add(new ModRef("1", true, "")); ps.Mods.Add(new ModRef("2", true, ""));
        var baseNames = new HashSet<string> { "on_birth", "on_death" };
        var noBase = ScriptTime.AttributeTime(new TimeTable(), ScriptTime.ParseStatsCsv(oa, true), ps, defs);
        Assert.True(noBase.PerMod.Count == 0 && Math.Abs(noBase.SharedActions - 11.0) < 1e-9);
        var withBase = ScriptTime.AttributeTime(new TimeTable(), ScriptTime.ParseStatsCsv(oa, true), ps, defs, baseNames);
        Assert.True(withBase.PerMod.Count == 1 && withBase.PerMod[0].Mod == 0 && Math.Abs(withBase.PerMod[0].Time - 8.0) < 1e-9 && withBase.ActionsByName == 1);
        Assert.True(Math.Abs(withBase.SharedActions - 3.0) < 1e-9);

        string gd = e.P("base_oa");
        e.W("base_oa/common/on_action/a.txt", "on_birth = {\n effect = { }\n}\non_death = { }\n"); e.W("base_oa/dlc/dlc001/common/on_action/b.txt", "dlc_pulse = { }\n");
        var names = ScriptTime.BaseOnActionNames(gd);
        Assert.True(names.Contains("on_birth") && names.Contains("on_death") && names.Contains("dlc_pulse") && !names.Contains("zeta_unique"));
        Assert.Empty(ScriptTime.BaseOnActionNames(""));

        SnapMod Sm(string id, string name, double t, double sh, long runs) => new() { Id = id, Name = name, Time = t, Share = sh, Runs = runs };
        var hv = new List<TimeSnap>();
        var s1 = new TimeSnap { When = 100, Total = 50, Events = 7, Playset = "h", Game = "1.20.1", Mods = { Sm("1", "Mod One", 40, 0.8, 9), Sm("*shared", "", 10, 0.2, 3) } };
        var s2 = new TimeSnap { When = 200, Total = 60, Events = 8, Playset = "h", Game = "1.20.1", Mods = { Sm("1", "Mod One", 30, 0.5, 9), Sm("2", "Mod\tTwo", 30, 0.5, 4) } };
        ScriptTime.SnapAdd(hv, s2); ScriptTime.SnapAdd(hv, s1); ScriptTime.SnapAdd(hv, s2);
        Assert.True(hv.Count == 2 && hv[0].When == 100 && hv[1].When == 200);
        var back = ScriptTime.SnapFromText(ScriptTime.SnapToText(hv));
        Assert.True(back.Count == 2 && back[1].Mods.Count == 2 && back[1].Mods[1].Name == "Mod Two" && back[0].Playset == "h" && Math.Abs(back[0].Mods[0].Share - 0.8) < 1e-9);
        Assert.Empty(ScriptTime.SnapFromText("junk\nM\tx\n"));
        var hr = ScriptTime.HistoryRows(back, out bool hb);
        Assert.True(hb && hr.Count == 3);
        foreach (var r in hr)
        {
            if (r.Id == "1") Assert.True(Math.Abs(r.Change - (-0.3)) < 1e-9 && r.Seen == 2);
            if (r.Id == "2") Assert.True(r.InNow && !r.InBefore && r.Seen == 1);
            if (r.Id == "*shared") Assert.True(!r.InNow && r.InBefore);
        }
        Assert.True(ScriptTime.TrendText(back, "1") == "80.0 · 50.0" && ScriptTime.TrendText(back, "2") == "- · 50.0" && ScriptTime.TrendText(back, "nope") == "- · -" && ScriptTime.TrendText(new List<TimeSnap>(), "1").Length == 0);
        Assert.Equal("50.0", ScriptTime.TrendText(back, "1", 1));
        string rep = ScriptTime.AuthorReport(withBase, 0, "Mod One", "123", "2.0", "1.20.1.1", "0.21.2", ScriptTime.TrendText(back, "1"), 8.0, 5);
        Assert.True(rep.Contains("\"Mod One\"") && rep.Contains("Workshop id 123") && rep.Contains("zeta_unique") && rep.Contains("80.0 · 50.0"));
        Assert.DoesNotContain("Slowest events", rep);
        string rep2 = ScriptTime.AuthorReport(withBase, 0, "Local", "", "", "", "0.21.2", "", 8.0, 5);
        Assert.True(!rep2.Contains("Workshop") && !rep2.Contains("Mod version"));
        for (int i = 0; i < 40; i++) ScriptTime.SnapAdd(hv, new TimeSnap { When = 1000 + i, Total = 50, Events = 7, Playset = "h", Game = "1.20.1" }, 30);
        Assert.Equal(30, hv.Count);
    }
}
