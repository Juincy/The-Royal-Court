using RoyalCourt.Core;
using Xunit;
namespace RoyalCourt.Tests;

/// <summary>Seastar's AGOT Collection (pastebin tA8TJa0d, read 2026-10-26) is a working playset of 93 mods. Its order is not gospel, but every place
/// where one of our author-stated rules disagrees with it should be a known, reviewed difference (see the list below), not an accident.</summary>
public class CollectionTests
{
    static readonly string[] Reviewed =
    {
        // COW-AGOT 3D Models & Bigger Castles: its page says "always put after the AGOT mod and other AGOT sub mods"; the collection loads it earlier.
        "2971198450 after 2995674648", "2971198450 after 2966543745",
        // AGOT Expanded - Dragons: its page says it is compatible with More Dragon Eggs if placed below it; the collection loads it above.
        "3466228580 after 3388366564",
        // Seasons of Ice and Fire and More Interactive Vassals: pages recommend the bottom (or as close as you can); the collection keeps them higher.
        "3377641022 bottom", "2712590542 bottom",
    };

    [Fact]
    public void OurRulesAgreeWithTheCollectionExceptWhereReviewed()
    {
        var rows = File.ReadAllLines(Path.Combine(AppContext.BaseDirectory, "fixtures", "seastar_pastebin_tA8TJa0d.txt")).Where(l => !l.StartsWith('#') && l.Trim().Length > 0).Select(l => l.Split('\t')).ToList();
        var mods = rows.Select(r => new ModInfo { Id = "ugc_" + r[2] + ".mod", Name = r[3] }).ToList();
        var db = KnownData.Builtin(); int n = mods.Count;
        var norm = mods.Select(m => Names.Norm(m.Name)).ToArray();
        int Find(string name) { for (int i = 0; i < n; i++) if (Names.NameIs(norm[i], name)) return i; return -1; }
        var found = new List<string>();
        for (int i = 0; i < n; i++)
        {
            int ki = Known.Find(db, mods[i]); if (ki < 0) continue;
            var k = db[ki]; string id = rows[i][2];
            foreach (var a in k.After) { int j = Find(a); if (j > i) found.Add($"{id} after {rows[j][2]}"); }
            foreach (var b in k.Before) { int j = Find(b); if (j >= 0 && j < i) found.Add($"{id} before {rows[j][2]}"); }
            if (k.Pos == 1 && i != n - 1) found.Add($"{id} bottom");
        }
        var unexpected = found.Except(Reviewed).ToList();
        Assert.True(unexpected.Count == 0, "Rules that disagree with Seastar's collection and are not reviewed yet:\n  " + string.Join("\n  ", unexpected));
        var gone = Reviewed.Except(found).ToList();
        Assert.True(gone.Count == 0, "Reviewed differences that no longer exist (remove them from the list):\n  " + string.Join("\n  ", gone));
    }
}
