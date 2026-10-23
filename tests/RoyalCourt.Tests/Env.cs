using RoyalCourt.Core;
using Xunit;
[assembly: CollectionBehavior(DisableTestParallelization = true)]
namespace RoyalCourt.Tests;

/// <summary>A fresh temp folder per test with RC_DATA_DIR pointing into it (the tests share process-wide state, so they run one at a time).</summary>
public sealed class TestEnv : IDisposable
{
    public readonly string T;
    public string Ck3 => Path.Combine(T, "ck3");
    public TestEnv()
    {
        T = Path.Combine(Path.GetTempPath(), "rc_xt_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(T, "ck3", "mod")); Directory.CreateDirectory(Path.Combine(T, "data"));
        Environment.SetEnvironmentVariable("RC_DATA_DIR", Path.Combine(T, "data"));
    }
    public string P(params string[] parts) => Path.Combine(new[] { T }.Concat(parts).ToArray());
    public void W(string rel, string text) { var f = P(rel.Split('/')); Directory.CreateDirectory(Path.GetDirectoryName(f)!); File.WriteAllText(f, text, new System.Text.UTF8Encoding(false)); }
    public string R(string rel) => File.ReadAllText(P(rel.Split('/')));
    public bool Ex(string rel) { var f = P(rel.Split('/')); return File.Exists(f) || Directory.Exists(f); }
    /// <summary>The three mods most scan tests start from.</summary>
    public List<ModInfo> BaseMods()
    {
        W("ck3/mod/ugc_111.mod", "name=\"Rise and Fall\"\r\nversion=\"1.2\"\r\nsupported_version=\"1.20.*\"\r\nnamespace=\"x\"\r\n");
        W("ck3/mod/local.mod", "  name = \"My Local Mod\"\nversion=\"0.3\"\n");
        W("ck3/mod/ugc_222.mod", "name=\"Sub Rosa\"\n");
        W("ck3/mod/readme.txt", "x");
        return ModScan.ScanMods(Ck3);
    }
    public static string Fx(string name) => File.ReadAllText(Path.Combine(AppContext.BaseDirectory, "fixtures", name));
    public void Dispose() { try { Directory.Delete(T, true); } catch { } Environment.SetEnvironmentVariable("RC_DATA_DIR", null); }
}
