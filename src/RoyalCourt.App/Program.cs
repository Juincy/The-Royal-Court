using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Controls;
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
        if (OperatingSystem.IsWindows()) InstallFaultLogger();
        // a UI program: avoid blocking full garbage collections (they show up as stutter); the memory is small anyway
        try { System.Runtime.GCSettings.LatencyMode = System.Runtime.GCLatencyMode.SustainedLowLatency; } catch { }
        return BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    }

    public static EventWaitHandle? ShowEvent => showEvent;
    /// <summary>Drawing with the processor is the default: it never depends on the graphics driver, which on some PCs closed the program without a word when a window was minimised or restored. Advanced > "Draw with the graphics card" turns the card back on.</summary>
    static bool UseGpu()
    {
        string? e = Environment.GetEnvironmentVariable("RC_GPU");
        if (e == "1") return true; if (e == "0") return false;
        try { return Settings.Load().Gpu; } catch { return false; }
    }
    public static AppBuilder BuildAvaloniaApp()
    {
        var b = AppBuilder.Configure<App>().UsePlatformDetect().LogToTrace();
        if (OperatingSystem.IsWindows())
            b = b.With(new Win32PlatformOptions { RenderingMode = UseGpu() ? new[] { Win32RenderingMode.AngleEgl, Win32RenderingMode.Wgl, Win32RenderingMode.Software } : new[] { Win32RenderingMode.Software } });
        return b;
    }

    // ---- a note in the log when Windows itself ends the program (a native fault cannot be caught by normal error handling) ----
    delegate int VectoredHandler(IntPtr info);
    static VectoredHandler? vh;
    [DllImport("kernel32.dll")] static extern IntPtr SetUnhandledExceptionFilter(VectoredHandler h);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] static extern bool GetModuleHandleExW(uint flags, IntPtr addr, out IntPtr module);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] static extern uint GetModuleFileNameW(IntPtr module, char[] name, uint size);
    static int logged;
    static void InstallFaultLogger()
    {
        try
        {
            vh = info =>
            {
                try
                {
                    IntPtr rec = Marshal.ReadIntPtr(info);                     // EXCEPTION_POINTERS.ExceptionRecord
                    uint code = (uint)Marshal.ReadInt32(rec);
                    if (Interlocked.Increment(ref logged) <= 3)
                    {
                        IntPtr addr = Marshal.ReadIntPtr(rec, 16);               // ExceptionAddress
                        string mod = "?"; long off = 0;
                        if (GetModuleHandleExW(4, addr, out IntPtr m) && m != IntPtr.Zero)
                        {
                            var buf = new char[520]; uint n = GetModuleFileNameW(m, buf, 520);
                            mod = new string(buf, 0, (int)n); off = addr.ToInt64() - m.ToInt64();
                        }
                        Log.Line("PROGRAM ENDED BY WINDOWS: code 0x" + code.ToString("X8") + " in " + mod + " +0x" + off.ToString("X") + " (Windows ended the program)");
                    }
                }
                catch { }
                return 0;                                                       // EXCEPTION_CONTINUE_SEARCH
            };
            SetUnhandledExceptionFilter(vh);   // called only when Windows is about to end the program, so it cannot disturb normal running
        }
        catch { }
    }
}
