using System.Text;
using RoyalCourt.Core;
using Xunit;
namespace RoyalCourt.Tests;

/// <summary>Import and Export through the real manager, with a stand-in for the file dialogs.</summary>
public class ImportExportTests
{
    sealed class FakeDlg : IDialogs
    {
        public string? Save, Open; public string Suggested = ""; public List<string> Infos = new();
        public Task Info(string text) { Infos.Add(text); return Task.CompletedTask; }
        public Task<bool> Confirm(string title, string text) => Task.FromResult(false);
        public Task<string?> Ask(string title, string prompt, string initial) => Task.FromResult<string?>(null);
        public Task<string?> PickGameExe() => Task.FromResult<string?>(null);
        public Task<string?> SaveFile(string suggestedName) { Suggested = suggestedName; return Task.FromResult(Save); }
        public Task<string?> OpenFile() => Task.FromResult(Open);
        public Task ShowText(string title, string text) => Task.CompletedTask;
        public Task<bool> SortPreview(SortPlan plan, Playset ps) => Task.FromResult(true);
        public Task<string> ClipboardGet() => Task.FromResult("");
        public Task<bool> ClipboardSet(string text) => Task.FromResult(true);
        public Task<int> PickItem(string title, IReadOnlyList<string> items) => Task.FromResult(-1);
        public Task<string?> PickBackup(string initialDir) => Task.FromResult<string?>(null);
        public Task<int> UpdateChoice(string head, string body, string notes, bool canInstall, bool hasPage) => Task.FromResult(0);
    }

    static ModManager Make(TestEnv e, out FakeDlg d)
    {
        e.BaseMods();
        d = new FakeDlg();
        var mm = new ModManager { St = new Settings { Ck3Dir = e.Ck3 }, DocRoots = new List<string>(), Dlg = d };
        mm.Reload();
        return mm;
    }

    [Fact]
    public async Task ExportThenImportGivesTheSamePlayset()
    {
        using var e = new TestEnv();
        var mm = Make(e, out var d);
        var ps = mm.Active()!;
        foreach (var m in ps.Mods) m.Enabled = true;
        string target = e.P("out", "My Great Playset.json");
        Directory.CreateDirectory(Path.GetDirectoryName(target)!);
        d.Save = target;
        await mm.ExportFile();
        Assert.True(File.Exists(target));
        Assert.Equal("Default.json", d.Suggested.Length > 0 ? d.Suggested : "Default.json");
        string text = File.ReadAllText(target);
        Assert.Contains("\"name\":\"My Great Playset\"", text);          // the name inside is the file name
        d.Open = target;
        int before = mm.Playsets.Count;
        await mm.ImportFile();
        Assert.Equal(before + 1, mm.Playsets.Count);
        var back = mm.Playsets.First(p => p.Name == "My Great Playset");
        Assert.Equal(ps.Mods.Where(m => m.Enabled).Select(m => m.Id), back.Mods.Where(m => m.Enabled).Select(m => m.Id));
        Assert.Equal("My Great Playset", mm.St.Active);
        Assert.Empty(d.Infos);
        // importing the same file again does not overwrite: a numbered copy is made
        await mm.ImportFile();
        Assert.Equal(before + 2, mm.Playsets.Count);
        Assert.Equal(mm.Playsets.Count, mm.Playsets.Select(p => p.Name.ToLowerInvariant()).Distinct().Count());
    }

    [Fact]
    public async Task CancelledDialogsChangeNothing()
    {
        using var e = new TestEnv();
        var mm = Make(e, out var d);
        d.Save = null; d.Open = null;
        await mm.ExportFile(); await mm.ImportFile();
        Assert.Single(mm.Playsets); Assert.Empty(d.Infos);
    }

    [Theory]
    [InlineData("utf8")] [InlineData("utf8bom")] [InlineData("utf16le")] [InlineData("utf16be")]
    public async Task FilesInAnyCommonTextEncodingAreImported(string enc)
    {
        using var e = new TestEnv();
        var mm = Make(e, out var d);
        string json = "{\"game\":\"ck3\",\"name\":\"Ünï cödé 名前\",\"mods\":[{\"displayName\":\"Rise and Fall\",\"enabled\":true,\"position\":0,\"steamId\":\"111\"},{\"displayName\":\"Sub Rosa\",\"enabled\":true,\"position\":1,\"steamId\":\"222\"}]}";
        byte[] bytes = enc switch
        {
            "utf8" => new UTF8Encoding(false).GetBytes(json),
            "utf8bom" => new UTF8Encoding(true).GetPreamble().Concat(new UTF8Encoding(false).GetBytes(json)).ToArray(),
            "utf16le" => Encoding.Unicode.GetPreamble().Concat(Encoding.Unicode.GetBytes(json)).ToArray(),
            _ => Encoding.BigEndianUnicode.GetPreamble().Concat(Encoding.BigEndianUnicode.GetBytes(json)).ToArray(),
        };
        string f = e.P("in", "Shared " + enc + ".JSON");
        Directory.CreateDirectory(Path.GetDirectoryName(f)!);
        File.WriteAllBytes(f, bytes);
        d.Open = f;
        await mm.ImportFile();
        Assert.Empty(d.Infos);
        var p = mm.Playsets.First(x => x.Name == "Shared " + enc);
        Assert.Equal(new[] { "ugc_111.mod", "ugc_222.mod" }, p.Mods.Where(m => m.Enabled).Select(m => m.Id).ToArray());
        Assert.True(File.Exists(Playsets.PlaysetPath(p.Name)));
    }

    [Theory]
    [InlineData("")] [InlineData("not json at all")] [InlineData("[1,2,3]")] [InlineData("{\"game\":\"ck3\"}")] [InlineData("{\"game\":\"eu4\",\"mods\":[]}")] [InlineData("{\"mods\":[1,\"x\",null,{}]}")]
    public async Task BadFilesGiveAMessageAndNeverThrow(string content)
    {
        using var e = new TestEnv();
        var mm = Make(e, out var d);
        string f = e.P("in", "bad.json");
        Directory.CreateDirectory(Path.GetDirectoryName(f)!);
        File.WriteAllText(f, content);
        d.Open = f;
        await mm.ImportFile();
        if (content.StartsWith("{\"mods\""))      // a list with only junk entries is a (blank) playset, not an error
        { Assert.Equal(2, mm.Playsets.Count); return; }
        Assert.Single(mm.Playsets);
        Assert.Single(d.Infos);
    }

    [Fact]
    public async Task MissingFileAndUnwritableTargetAreReported()
    {
        using var e = new TestEnv();
        var mm = Make(e, out var d);
        d.Open = e.P("nope", "missing.json");
        await mm.ImportFile();
        Assert.Single(d.Infos);
        d.Save = e.P("no such folder", "deeper", "x.json");
        await mm.ExportFile();
        Assert.True(File.Exists(d.Save) || d.Infos.Count == 2);      // the folder is created, or a message says it could not be written
    }

    [Fact]
    public async Task NamesWithOddCharactersSurviveExportAndImport()
    {
        using var e = new TestEnv();
        var mm = Make(e, out var d);
        string f = e.P("o", "Café – “quotes” & more [v2] 名前.json");
        Directory.CreateDirectory(Path.GetDirectoryName(f)!);
        d.Save = f; await mm.ExportFile(); Assert.True(File.Exists(f));
        d.Open = f; await mm.ImportFile();
        Assert.Contains(mm.Playsets, p => p.Name == "Café – “quotes” & more [v2] 名前");
    }

    [Fact]
    public void ShareCodeRoundTripsThroughTheManagerAndBack()
    {
        using var e = new TestEnv();
        var mm = Make(e, out var d);
        foreach (var m in mm.Active()!.Mods) m.Enabled = true;
        string code = Share.Make(mm.Active()!, mm.Info, true);
        var dec = Share.Decode("Look at this!\r\n" + code + "\r\nthanks");
        Assert.True(dec.Ok);
        var p = Share.ToPlayset(dec, mm.Mods);
        Assert.Equal(mm.Active()!.Mods.Count(m => m.Enabled), p.Mods.Count(m => m.Enabled));
    }
}
