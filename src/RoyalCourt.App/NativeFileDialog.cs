using System.Runtime.InteropServices;
namespace RoyalCourt.Ui;

/// <summary>The classic Windows Open / Save dialog (comdlg32), the same one the first versions used. The newer dialog Avalonia opens fails on some PCs with "The parameter is incorrect" (0x80070057); this one does not.</summary>
public static class NativeFileDialog
{
    public enum Result { Picked, Cancelled, Failed }

    [StructLayout(LayoutKind.Sequential)]
    struct OPENFILENAME
    {
        public int lStructSize; public IntPtr hwndOwner, hInstance, lpstrFilter, lpstrCustomFilter;
        public int nMaxCustFilter, nFilterIndex; public IntPtr lpstrFile; public int nMaxFile; public IntPtr lpstrFileTitle; public int nMaxFileTitle;
        public IntPtr lpstrInitialDir, lpstrTitle; public int Flags; public short nFileOffset, nFileExtension; public IntPtr lpstrDefExt, lCustData, lpfnHook, lpTemplateName, pvReserved;
        public int dwReserved, FlagsEx;
    }
    [DllImport("comdlg32.dll", CharSet = CharSet.Unicode, EntryPoint = "GetOpenFileNameW")] static extern bool GetOpenFileName(ref OPENFILENAME o);
    [DllImport("comdlg32.dll", CharSet = CharSet.Unicode, EntryPoint = "GetSaveFileNameW")] static extern bool GetSaveFileName(ref OPENFILENAME o);
    [DllImport("comdlg32.dll")] static extern int CommDlgExtendedError();

    /// <param name="filters">pairs of (label, pattern), for example ("Playset (*.json)", "*.json")</param>
    public static Result Show(IntPtr owner, bool save, string title, (string label, string pattern)[] filters, string? initialDir, string? fileName, string? defExt, out string path)
    {
        path = "";
        if (!OperatingSystem.IsWindows()) return Result.Failed;
        IntPtr file = IntPtr.Zero, filter = IntPtr.Zero, ttl = IntPtr.Zero, dir = IntPtr.Zero, ext = IntPtr.Zero;
        try
        {
            const int MaxChars = 32768;
            file = Marshal.AllocHGlobal(MaxChars * 2);
            for (int i = 0; i < MaxChars; i++) Marshal.WriteInt16(file, i * 2, 0);
            if (!string.IsNullOrEmpty(fileName))
            {
                string n = fileName.Length > 250 ? fileName[..250] : fileName;
                for (int i = 0; i < n.Length; i++) Marshal.WriteInt16(file, i * 2, n[i]);
            }
            filter = Marshal.StringToHGlobalUni(string.Join("\0", filters.Select(f => f.label + "\0" + f.pattern)) + "\0\0");
            ttl = Marshal.StringToHGlobalUni(title);
            if (!string.IsNullOrEmpty(initialDir) && Directory.Exists(initialDir)) dir = Marshal.StringToHGlobalUni(initialDir);
            if (!string.IsNullOrEmpty(defExt)) ext = Marshal.StringToHGlobalUni(defExt);
            var o = new OPENFILENAME
            {
                lStructSize = Marshal.SizeOf<OPENFILENAME>(), hwndOwner = owner, lpstrFilter = filter, nFilterIndex = 1, lpstrFile = file, nMaxFile = MaxChars,
                lpstrInitialDir = dir, lpstrTitle = ttl, lpstrDefExt = ext,
                Flags = 0x80000 | 0x800 | 0x4 | 0x8 | (save ? 0x2 : 0x1000),   // explorer style, path must exist, hide read-only box, keep the working folder, overwrite prompt / file must exist
            };
            bool ok = save ? GetSaveFileName(ref o) : GetOpenFileName(ref o);
            if (!ok) return CommDlgExtendedError() == 0 ? Result.Cancelled : Result.Failed;
            path = Marshal.PtrToStringUni(file) ?? "";
            return path.Length > 0 ? Result.Picked : Result.Cancelled;
        }
        catch (Exception e) { RoyalCourt.Core.Log.Line("native file dialog: " + e.Message); return Result.Failed; }
        finally { foreach (var p in new[] { file, filter, ttl, dir, ext }) if (p != IntPtr.Zero) Marshal.FreeHGlobal(p); }
    }
}
