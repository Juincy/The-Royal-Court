using Avalonia;
using Avalonia.Controls;
using Avalonia.Headless;
using Avalonia.Threading;
using RoyalCourt.Core;
using RoyalCourt.Ui;

// Renders the real windows without a screen: dotnet RoyalCourt.Shots.dll SCENARIO_DIR OUT_DIR [what...]
class P
{
    static void Pump(int ms)
    {
        var end = DateTime.UtcNow.AddMilliseconds(ms);
        while (DateTime.UtcNow < end) { Dispatcher.UIThread.RunJobs(); AvaloniaHeadlessPlatform.ForceRenderTimerTick(); Thread.Sleep(10); }
    }
    static void Shot(Window w, string path)
    {
        Pump(60);
        var bmp = w.CaptureRenderedFrame();
        bmp?.Save(path);
        Console.WriteLine((bmp == null ? "FAILED " : "saved ") + path);
    }
    static int Main(string[] a)
    {
        string scen = a[0], outDir = a[1];
        Directory.CreateDirectory(outDir);
        string app = Path.Combine(outDir, "appdata"); Directory.CreateDirectory(app);
        Environment.SetEnvironmentVariable("APPDATA", app);
        Environment.SetEnvironmentVariable("RC_WORKSHOP_DIR", Path.Combine(scen, "content"));
        Environment.SetEnvironmentVariable("RC_ALLOW_SECOND", "1");
        var st = new Settings { Ck3Dir = Path.Combine(scen, "ck3"), TutorialDone = a.Contains("tour") ? false : true, Theme = "dark" };
        st.Save();
        AppBuilder.Configure<App>().UseSkia().UseHeadless(new AvaloniaHeadlessPlatformOptions { UseHeadlessDrawing = false }).WithInterFont().SetupWithoutStarting();
        var w = new MainWindow { Width = 1260, Height = 660 };
        w.Boot();
        w.Show();
        Pump(300);
        for (int i = 0; i < 300 && (w.mm.Scanning || w.mm.Rows.Count == 0); i++) Pump(50);
        Pump(500);
        Console.WriteLine($"mods {w.mm.Mods.Count} rows {w.mm.Rows.Count} scanning {w.mm.Scanning}");
        Shot(w, Path.Combine(outDir, "main_dark.png"));
        w.SetTheme(Look.ILight, false); Pump(100); Shot(w, Path.Combine(outDir, "main_light.png"));
        w.SetTheme(Look.IHalloween, false); Pump(100); Shot(w, Path.Combine(outDir, "main_halloween.png"));
        w.SetTheme(Look.IAgot, false); Pump(100); Shot(w, Path.Combine(outDir, "main_agot.png"));
        { var task = w.AdvancedMenu(); Pump(300); if (PopupMenu.Last != null) { Shot(PopupMenu.Last, Path.Combine(outDir, "advanced_agot.png")); PopupMenu.Last.Close(); } Pump(100); }
        w.SetTheme(Look.IDark, false); Pump(100);
        { var task = w.AdvancedMenu(); Pump(300); if (PopupMenu.Last != null) { Shot(PopupMenu.Last, Path.Combine(outDir, "advanced_dark.png")); PopupMenu.Last.Close(); } Pump(100); }
        if (a.Contains("tour")) { var t = TourWindow.Open(w); t.Width = 900; t.Height = 600; Pump(200); Shot(t, Path.Combine(outDir, "tour.png")); t.GoTo(2); Shot(t, Path.Combine(outDir, "tour3.png")); t.Close(); }
        if (a.Contains("conf")) { ConflictsWindow.Open(w); Pump(800); var cw = w.SubWindows.OfType<ConflictsWindow>().First(); Shot(cw, Path.Combine(outDir, "conflicts.png")); cw.Close(); }
        if (a.Contains("mini"))
        {
            ChangelogWindow.Open(w); Pump(300); var cw = w.SubWindows.OfType<ChangelogWindow>().First();
            foreach (var (ww, hh) in new[] { (0, 0), (1, 1), (0, 300), (300, 0), (100, 20), (780, 560) })
            {
                try { cw.MinWidth = 0; cw.MinHeight = 0; cw.Width = ww; cw.Height = hh; Pump(120); var b = cw.CaptureRenderedFrame(); Console.WriteLine($"size {ww}x{hh}: ok {(b != null)}"); }
                catch (Exception e) { Console.WriteLine($"size {ww}x{hh}: EXCEPTION {e}"); }
            }
            cw.Close();
        }
        if (a.Contains("log")) { ChangelogWindow.Open(w); Pump(300); var cw = w.SubWindows.OfType<ChangelogWindow>().First(); Shot(cw, Path.Combine(outDir, "changelog.png")); cw.Close(); }
        if (a.Contains("rep")) { ReportWindow.Open(w, 2); Pump(500); var cw = w.SubWindows.OfType<ReportWindow>().First(); Shot(cw, Path.Combine(outDir, "scripttime.png")); cw.Close(); }
        if (a.Contains("anim"))
        {
            w.SetTheme(Look.ILight, true); Console.WriteLine("moving during slide: " + Anim.Moving); Pump(900);
            Console.WriteLine("moving after slide: " + Anim.Moving);
            w.SetTheme(Look.IDark, true); w.SetTheme(Look.IHalloween, true); Pump(1200);
            Console.WriteLine("moving after quick switches: " + Anim.Moving + " theme " + Skin.Id);
        }
        if (a.Contains("thumbs"))
        {
            int ri = w.mm.Rows.FindIndex(r => r.Installed); w.list.Off = Math.Max(0, ri - 1) * RcList.RowH; w.list.Refresh(); Pump(800); w.list.Refresh(); Pump(300);
            Shot(w, Path.Combine(outDir, "thumbs.png"));
        }
        if (a.Contains("bisect"))
        {
            w.mm.SetShown(true); Pump(500);
            BisectWindow.Open(w); Pump(1500);
            var bw = w.SubWindows.OfType<BisectWindow>().First(); Shot(bw, Path.Combine(outDir, "bisect1.png"));
            bw.Test_Begin(); Pump(300); bw.Test_Report(false); Pump(300); Shot(bw, Path.Combine(outDir, "bisect2.png")); bw.Close();
        }
        if (a.Contains("e2e"))
        {
            var mm = w.mm;
            mm.SetShown(true); Pump(300);
            Console.WriteLine($"enabled: {mm.CountText}");
            w.SetTheme(Look.IDark, false);
            Shot(w, Path.Combine(outDir, "enabled.png"));
            ConflictsWindow.Open(w); Pump(1500);
            var cw = w.SubWindows.OfType<ConflictsWindow>().First(); Shot(cw, Path.Combine(outDir, "conflicts2.png")); cw.Close();
            var plan = mm.PlanSort(); Console.WriteLine($"plan: {plan.GetType().Name}");
            var t = mm.AutoSort(); Pump(300);
            Console.WriteLine($"autosort task done={t.IsCompleted}");
            Pump(500); Shot(w, Path.Combine(outDir, "sortdlg_main.png"));
        }
        w.Close();
        return 0;
    }
}
