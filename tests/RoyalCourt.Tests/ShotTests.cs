using Avalonia;
using Avalonia.Headless;
using Avalonia.Headless.XUnit;
using RoyalCourt.Core;
using RoyalCourt.Ui;
using Xunit;
[assembly: AvaloniaTestApplication(typeof(RoyalCourt.Tests.TestAppBuilder))]
namespace RoyalCourt.Tests;
public static class TestAppBuilder
{
    public static AppBuilder BuildAvaloniaApp() => AppBuilder.Configure<App>().UseSkia().UseHeadless(new AvaloniaHeadlessPlatformOptions { UseHeadlessDrawing = false }).WithInterFont();
}
public class ShotTests
{
    [AvaloniaFact]
    public void Screenshot()
    {
        var w = new MainWindow(); w.Show();
        var bmp = w.CaptureRenderedFrame();
        Directory.CreateDirectory("/tmp/shots");
        bmp!.Save("/tmp/shots/hello.png");
        Assert.True(File.Exists("/tmp/shots/hello.png"));
    }

    [AvaloniaFact]
    public void DiffWindowDraws()
    {
        var w = new MainWindow(); w.Show();
        var mm = w.mm;
        mm.Playsets.Clear();
        string[] names = { "A Game of Thrones", "AGOT Submod Core", "Better Barbershop", "Cool Hair", "Dragons Plus", "Old Mod" };
        for (int i = 0; i < names.Length; i++) mm.Info["ugc_" + (100 + i) + ".mod"] = new ModInfo { Id = "ugc_" + (100 + i) + ".mod", Name = names[i] };
        var a = new Playset { Name = "Vanilla+ V1.0" }; var b = new Playset { Name = "Test playset" };
        foreach (int i in new[] { 0, 1, 2, 3, 5 }) a.Mods.Add(new ModRef("ugc_" + (100 + i) + ".mod", true, ""));
        foreach (int i in new[] { 0, 1, 3, 2, 4 }) b.Mods.Add(new ModRef("ugc_" + (100 + i) + ".mod", i != 3, ""));
        mm.Playsets.Add(a); mm.Playsets.Add(b); mm.St.Active = "Vanilla+ V1.0";
        mm.SetNote("ugc_102.mod", "keep below Barbershop patch");
        DiffWindow.Open(w);
        var dw = w.SubWindows.OfType<DiffWindow>().First();
        var bmp = dw.CaptureRenderedFrame();
        Directory.CreateDirectory("/tmp/shots");
        bmp!.Save("/tmp/shots/diff.png");
        var bm = w.CaptureRenderedFrame(); bm!.Save("/tmp/shots/main_note.png");
        Assert.True(File.Exists("/tmp/shots/diff.png"));
    }
}
