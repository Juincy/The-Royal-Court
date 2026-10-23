using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Avalonia.Platform;
using Avalonia.Threading;
using RoyalCourt.Core;
namespace RoyalCourt.Ui;

public partial class App : Application
{
    public override void Initialize() => AvaloniaXamlLoader.Load(this);
    public override void OnFrameworkInitializationCompleted()
    {
        try { MainWindow.AppIcon = new Avalonia.Controls.WindowIcon(AssetLoader.Open(new Uri("avares://TheRoyalCourt/Assets/royalcourt.ico"))); } catch { }
        Dispatcher.UIThread.UnhandledException += (_, e) => { try { Log.Line("UI error: " + e.Exception); } catch { } e.Handled = true; };
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime d)
        {
            var w = new MainWindow();
            w.Boot();
            d.MainWindow = w;
            var ev = Program.ShowEvent;
            if (ev != null)
            {
                var th = new Thread(() => { while (true) { try { ev.WaitOne(); } catch { return; } Dispatcher.UIThread.Post(() => { if (w.WindowState == Avalonia.Controls.WindowState.Minimized) w.WindowState = Avalonia.Controls.WindowState.Normal; w.Activate(); }); } }) { IsBackground = true };
                th.Start();
            }
        }
        base.OnFrameworkInitializationCompleted();
    }
}
