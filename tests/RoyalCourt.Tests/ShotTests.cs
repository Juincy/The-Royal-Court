using Avalonia;
using Avalonia.Headless;
using Avalonia.Headless.XUnit;
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
}
