using System.Diagnostics;
using System.Runtime.InteropServices;
namespace RoyalCourt.Core;

/// <summary>Everything that touches Windows itself: the registry, Steam's folders, running programs, the Steamworks library.
/// Each call is guarded so the same code also runs (doing nothing) on other systems, which is how the tests run.</summary>
public static class Platform
{
    public const string Ck3AppId = "1158310";

    static string RegString(bool machine, string sub, string name)
    {
        if (!OperatingSystem.IsWindows()) return "";
        try
        {
            using var k = (machine ? Microsoft.Win32.Registry.LocalMachine : Microsoft.Win32.Registry.CurrentUser).OpenSubKey(sub);
            return k?.GetValue(name) as string ?? "";
        }
        catch { return ""; }
    }

    /// <summary>Folders that can hold "Paradox Interactive\Crusader Kings III": Documents, the profile's Documents and OneDrive's Documents.</summary>
    public static List<string> DocRoots()
    {
        var v = new List<string>();
        void Add(string p) { if (p.Length > 0 && !v.Contains(p)) v.Add(p); }
        try { Add(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments)); } catch { }
        string up = Environment.GetEnvironmentVariable("USERPROFILE") ?? "";
        if (up.Length > 0) Add(Path.Combine(up, "Documents"));
        foreach (var e in new[] { "OneDrive", "OneDriveConsumer", "OneDriveCommercial" })
        {
            string o = Environment.GetEnvironmentVariable(e) ?? "";
            if (o.Length > 0) Add(Path.Combine(o, "Documents"));
        }
        return v;
    }

    /// <summary>Steam's install folder plus the extra library folders listed in libraryfolders.vdf.</summary>
    public static List<string> SteamLibraries()
    {
        string steam = RegString(false, @"Software\Valve\Steam", "SteamPath");
        if (steam.Length == 0) steam = RegString(true, @"SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath");
        var libs = new List<string>();
        if (steam.Length == 0) return libs;
        libs.Add(steam);
        if (Util.ReadFile(Path.Combine(steam, "steamapps", "libraryfolders.vdf"), out var vdf)) libs.AddRange(Steam.LibraryPaths(vdf));
        return libs;
    }

    /// <summary>Where Steam keeps subscribed CK3 Workshop mods (RC_WORKSHOP_DIR overrides it, for tests).</summary>
    public static List<string> WorkshopDirs()
    {
        string? e = Environment.GetEnvironmentVariable("RC_WORKSHOP_DIR");
        if (!string.IsNullOrEmpty(e)) return new List<string> { e };
        var o = new List<string>();
        foreach (var lib in SteamLibraries()) o.Add(Path.Combine(lib, "steamapps", "workshop", "content", Ck3AppId));
        return o;
    }

    public static string FindGameExeInSteam()
    {
        foreach (var lib in SteamLibraries())
        {
            string apps = Path.Combine(lib, "steamapps");
            if (!Util.ReadFile(Path.Combine(apps, "appmanifest_" + Ck3AppId + ".acf"), out var acf)) continue;
            string dir = Steam.AcfInstallDir(acf);
            if (dir.Length == 0) continue;
            string exe = Path.Combine(apps, "common", dir, "binaries", "ck3.exe");
            if (Util.FileExists(exe)) return exe;
        }
        return "";
    }

    /// <summary>Is a program with this name (without ".exe") running?</summary>
    public static bool ProcessRunning(string name)
    {
        try
        {
            if (name.EndsWith(".exe", StringComparison.OrdinalIgnoreCase)) name = name[..^4];
            var ps = Process.GetProcessesByName(name);
            bool any = ps.Length > 0;
            foreach (var p in ps) p.Dispose();
            return any;
        }
        catch { return false; }
    }

    public static bool StartProgram(string exe, string args, out string err)
    {
        err = "";
        try
        {
            var psi = new ProcessStartInfo(exe) { Arguments = args, WorkingDirectory = Path.GetDirectoryName(exe) ?? "", UseShellExecute = false };
            using var p = Process.Start(psi);
            return true;
        }
        catch (System.ComponentModel.Win32Exception w) { err = Lang.Trf("Windows error {0}", w.NativeErrorCode); return false; }
        catch (Exception e) { err = e.Message; return false; }
    }

    /// <summary>Opens a folder, file or address with whatever the system uses for it.</summary>
    public static void OpenShell(string target)
    {
        try
        {
            if (OperatingSystem.IsWindows()) Process.Start(new ProcessStartInfo(target) { UseShellExecute = true });
            else if (OperatingSystem.IsMacOS()) Process.Start("open", target);
            else Process.Start("xdg-open", target);
        }
        catch { }
    }

    // ---------- unsubscribe from Steam Workshop mods ----------
    // Steam has no command line for this. The only supported way is Valve's Steamworks library (steam_api64.dll, which ships with the game):
    // started with the game's app id it talks to the running Steam client and asks it to unsubscribe. Everything is optional: when the library,
    // Steam or the functions are not there, the Workshop page of the mod is opened in Steam instead. The program never touches the Paradox launcher.
    public static string SteamDllPath(string gameExe)
    {
        if (gameExe.Length == 0) gameExe = FindGameExeInSteam();
        if (gameExe.Length == 0) return "";
        string? d = Path.GetDirectoryName(gameExe);
        if (d == null) return "";
        string? up = Path.GetDirectoryName(d);
        var cands = new List<string> { Path.Combine(d, "steam_api64.dll") };
        if (up != null) { cands.Add(Path.Combine(up, "steam_api64.dll")); cands.Add(Path.Combine(up, "binaries", "steam_api64.dll")); }
        foreach (var c in cands) if (Util.FileExists(c)) return c;
        return "";
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate byte FInit();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int FInitFlat(IntPtr msg);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FVoid();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr FUgc();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate ulong FUnsub(IntPtr ugc, ulong id);

    static T? Sym<T>(IntPtr lib, string name) where T : Delegate
        => NativeLibrary.TryGetExport(lib, name, out var p) && p != IntPtr.Zero ? Marshal.GetDelegateForFunctionPointer<T>(p) : null;

    /// <summary>How many unsubscribe requests were handed to Steam; -1 when it could not be done this way (why: a short English reason for the log).</summary>
    public static int SteamUnsubscribe(string gameExe, IReadOnlyList<string> workshopIds, out string why)
    {
        why = "";
        string dll = SteamDllPath(gameExe);
        if (dll.Length == 0) { why = "steam_api64.dll not found"; return -1; }
        Environment.SetEnvironmentVariable("SteamAppId", Ck3AppId);
        Environment.SetEnvironmentVariable("SteamGameId", Ck3AppId);
        IntPtr lib;
        try { lib = NativeLibrary.Load(dll); } catch { why = "steam_api64.dll could not be loaded"; return -1; }
        try
        {
            var init = Sym<FInit>(lib, "SteamAPI_Init");
            var initFlat = Sym<FInitFlat>(lib, "SteamAPI_InitFlat");
            var shutdown = Sym<FVoid>(lib, "SteamAPI_Shutdown");
            var run = Sym<FVoid>(lib, "SteamAPI_RunCallbacks");
            var unsub = Sym<FUnsub>(lib, "SteamAPI_ISteamUGC_UnsubscribeItem");
            FUgc? ugcOf = null;
            for (int v = 40; v >= 8 && ugcOf == null; v--) ugcOf = Sym<FUgc>(lib, $"SteamAPI_SteamUGC_v{v:D3}");
            ugcOf ??= Sym<FUgc>(lib, "SteamAPI_SteamUGC");
            if ((init == null && initFlat == null) || shutdown == null || run == null || unsub == null || ugcOf == null) { why = "the Steamworks functions were not found"; return -1; }
            bool ok;
            if (initFlat != null)
            {
                IntPtr buf = Marshal.AllocHGlobal(1024);
                try
                {
                    for (int i = 0; i < 1024; i++) Marshal.WriteByte(buf, i, 0);
                    ok = initFlat(buf) == 0;
                    if (!ok) why = "Steam refused: " + (Marshal.PtrToStringAnsi(buf) ?? "");
                }
                finally { Marshal.FreeHGlobal(buf); }
            }
            else ok = init!() != 0;
            if (!ok) { if (why.Length == 0) why = "Steam did not accept the connection (is Steam running and signed in?)"; return -1; }
            IntPtr ugc = ugcOf();
            int sent = 0;
            if (ugc != IntPtr.Zero)
                foreach (var id in workshopIds) { if (ulong.TryParse(id, out var n)) { unsub(ugc, n); sent++; } }
            else why = "Steam's workshop service is not available";
            for (int i = 0; i < 30; i++) { run(); Thread.Sleep(80); }   // let Steam take the requests before the connection closes
            shutdown();
            return ugc != IntPtr.Zero ? sent : -1;
        }
        catch (Exception e) { why = "Steamworks call failed: " + e.Message; return -1; }
    }

    /// <summary>Lets the process's worker threads stay out of the way of the game: below-normal priority (the game running: background mode is not available through .NET, so the lowest normal level).</summary>
    public static void KeepOutOfTheWay()
    {
        try { Thread.CurrentThread.Priority = ProcessRunning("ck3") ? ThreadPriority.Lowest : ThreadPriority.BelowNormal; } catch { }
    }

    // ---------- the Recycle Bin ----------
    [StructLayout(LayoutKind.Sequential)]
    struct SHFILEOPSTRUCT
    {
        public IntPtr hwnd; public uint wFunc; public IntPtr pFrom; public IntPtr pTo; public ushort fFlags;
        [MarshalAs(UnmanagedType.Bool)] public bool fAnyOperationsAborted; public IntPtr hNameMappings; public IntPtr lpszProgressTitle;
    }
    [DllImport("shell32.dll", CharSet = CharSet.Unicode)] static extern int SHFileOperationW(ref SHFILEOPSTRUCT op);

    /// <summary>Moves files and folders to the Recycle Bin (anything too big for it is deleted for good, as Windows does). False when something was left.</summary>
    public static bool RecycleToBin(IEnumerable<string> paths)
    {
        if (!OperatingSystem.IsWindows()) return false;
        IntPtr from = IntPtr.Zero;
        try
        {
            string list = string.Join("\0", paths) + "\0\0";
            from = Marshal.StringToHGlobalUni(list);   // the whole text, embedded NULs included, plus the closing NUL
            var op = new SHFILEOPSTRUCT { wFunc = 3 /* FO_DELETE */, pFrom = from, fFlags = 0x0040 | 0x0010 | 0x0400 | 0x0004 /* ALLOWUNDO | NOCONFIRMATION | NOERRORUI | SILENT */ };
            int rc = SHFileOperationW(ref op);
            return rc == 0 && !op.fAnyOperationsAborted;
        }
        catch { return false; }
        finally { if (from != IntPtr.Zero) Marshal.FreeHGlobal(from); }
    }
}
