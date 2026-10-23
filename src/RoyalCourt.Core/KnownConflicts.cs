namespace RoyalCourt.Core;

/// <summary>Pairs of mods whose own Workshop pages say they must not be run together. Only author statements go here.</summary>
public static class KnownConflicts
{
    public sealed class Pair
    {
        public string[] IdsA, NamesA, IdsB, NamesB; public string Text;
        public Pair(string[] idsA, string[] namesA, string[] idsB, string[] namesB, string text) { IdsA = idsA; NamesA = namesA; IdsB = idsB; NamesB = namesB; Text = text; }
    }
    public static readonly Pair[] List =
    {
        new(new[] { "3379545519" }, new[] { "barbershop for all characters" }, new[] { "3383099613" }, new[] { "agot barbershop for all characters" },
            "\"{0}\" and \"{1}\" should not be used together: the AGOT version of Barbershop For All Characters is standalone, according to its page."),
        new(new[] { "3810761892" }, new[] { "set in stone court statues" }, Array.Empty<string>(), new[] { "gg terran s statues of ck3" },
            "\"{0}\" and \"{1}\" should not be used together: Set In Stone replaces GG Terran's Statues of CK3, according to its page."),
        new(new[] { "3571438676" }, new[] { "zeitgeist situation and struggle" }, Array.Empty<string>(), new[] { "regional destiny" },
            "\"{0}\" and \"{1}\" should not be used together: the Zeitgeist page says it is not compatible with Regional Destiny."),
    };

    static bool Is(ModInfo m, string[] ids, string[] names)
    {
        string sid = Names.SteamIdOf(m.Id), nn = Names.Norm(m.Name);
        foreach (var i in ids) if (sid.Length > 0 && sid == i) return true;
        foreach (var n in names) if (Names.NameIs(nn, n)) return true;
        return false;
    }

    /// <summary>Position pairs (a, b) of enabled mods in the playset that match a known conflict, with the warning text.</summary>
    public static List<(int A, int B, string Text)> Find(Playset ps, IReadOnlyDictionary<string, ModInfo> info)
    {
        var res = new List<(int, int, string)>();
        foreach (var p in List)
        {
            int a = -1, b = -1;
            for (int i = 0; i < ps.Mods.Count; i++)
            {
                if (!ps.Mods[i].Enabled || !info.TryGetValue(ps.Mods[i].Id, out var m)) continue;
                if (a < 0 && Is(m, p.IdsA, p.NamesA)) a = i;
                else if (b < 0 && Is(m, p.IdsB, p.NamesB)) b = i;
            }
            if (a >= 0 && b >= 0) res.Add((a, b, string.Format(Tr(p.Text), info[ps.Mods[a].Id].Name, info[ps.Mods[b].Id].Name)));
        }
        return res;
    }
}
