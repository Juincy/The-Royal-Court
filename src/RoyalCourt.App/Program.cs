using Avalonia;
using RoyalCourt.Core;
namespace RoyalCourt.Ui;

internal static class Program
{
    static Mutex? single;
    static EventWaitHandle? showEvent;

    [STAThread]
    public static int Main(string[] args)
    {
        // Started by an update: wait for the old copy to finish closing, so the one-copy rule below does not turn the new one away.
        for (int i = 0; i + 1 < args.Length; i++)
            if (args[i] == "--wait-pid" && int.TryParse(args[i + 1], out int pid))
            {
                try { using var p = System.Diagnostics.Process.GetProcessById(pid); p.WaitForExit(15000); } catch { }
            }
        // Only one copy at a time: two copies would each keep their own picture of the playset files and overwrite each other's changes.
        single = new Mutex(false, "Local\\TheRoyalCourt.SingleInstance", out bool created);
        if (OperatingSystem.IsWindows())
        {
            try
            {
                showEvent = new EventWaitHandle(false, EventResetMode.AutoReset, "Local\\TheRoyalCourt.Show");
                if (!created && Environment.GetEnvironmentVariable("RC_ALLOW_SECOND") != "1") { showEvent.Set(); return 0; }
            }
            catch { }
        }
        else if (!created && Environment.GetEnvironmentVariable("RC_ALLOW_SECOND") != "1") return 0;
        AppDomain.CurrentDomain.UnhandledException += (_, e) => { try { Log.Line("CRASH: " + e.ExceptionObject); } catch { } };
        TaskScheduler.UnobservedTaskException += (_, e) => { try { Log.Line("task error: " + e.Exception); } catch { } e.SetObserved(); };
        return BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    }

    public static EventWaitHandle? ShowEvent => showEvent;
    public static AppBuilder BuildAvaloniaApp() => AppBuilder.Configure<App>().UsePlatformDetect().LogToTrace();
}
