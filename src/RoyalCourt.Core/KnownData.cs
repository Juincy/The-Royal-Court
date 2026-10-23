// Generated once by tools/conv_known.py from the C++ core.hpp; since 2026-10-22 maintained by hand in the C# line (new entries are marked below).
namespace RoyalCourt.Core;

public static partial class KnownData
{
    public static List<KnownMod> Builtin()
    {
        var v = new List<KnownMod>();
    { var k = new KnownMod(); k.Names = new() {"rise and fall"}; k.Ids = new() {"3554844335"}; k.Pos = 1; k.Note = K("Rise and Fall: its author says to place it at the very bottom of the load order"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"rui"}; k.After = new() {"rise and fall"}; k.Note = K("RUI: its page says to place it below Rise and Fall"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"unofficial patch", "ck3 unofficial patch"}; k.Ids = new() {"2871648329"}; k.Pos = -1;
      k.Note = K("Unofficial Patch: its page says it must be loaded at the very top of your mod list (only a total conversion goes above it)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"better barbershop", "better barbershop mod"}; k.Ids = new() {"2220326926"}; k.Cat = Cat.Graphics; k.Rank = 5;
      k.After = new() {"community flavor pack", "ethnicities and portraits expanded", "a game of thrones", "agot"}; k.Note = K("Better Barbershop: its page says to put it below CFP, EPE and AGOT"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"community flavor pack", "cfp"}; k.Ids = new() {"2220098919"}; k.Cat = Cat.Content; k.Note = K("Community Flavor Pack (known mod)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"ethnicities and portraits expanded", "epe"}; k.Ids = new() {"2507209632"}; k.Cat = Cat.Graphics; k.Note = K("Ethnicities & Portraits Expanded (known mod)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"better character ui"}; k.Ids = new() {"2222540784"}; k.Cat = Cat.Ui; k.Rank = 5; k.Note = K("Better Character UI: its page says it must be placed under other UI mods"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"a game of thrones", "agot"}; k.Ids = new() {"2962333032"}; k.Cat = Cat.Overhaul; k.Pos = -2;
      k.Note = K("A Game of Thrones: the AGOT Submod Core page says it must remain at the top of the load order"); v.Add(k); }
    // Total conversions replace the whole game setting, so they always go first. Their own Workshop pages give no load-order
    // sentence (checked), so this is the standard rule for total conversions rather than a quote.
    { var k = new KnownMod(); k.Names = new() {"lotr realms in exile", "realms in exile"}; k.Ids = new() {"2291024373"}; k.Cat = Cat.Overhaul; k.Pos = -2;
      k.Note = K("LotR: Realms in Exile is a total conversion, so it goes first (standard rule for total conversions)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"princes of darkness"}; k.Ids = new() {"2216659254"}; k.Cat = Cat.Overhaul; k.Pos = -2;
      k.Note = K("Princes of Darkness is a total conversion, so it goes first (standard rule for total conversions)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"elder kings 2"}; k.Ids = new() {"2887120253"}; k.Cat = Cat.Overhaul; k.Pos = -2;
      k.Note = K("Elder Kings 2 is a total conversion, so it goes first (standard rule for total conversions)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"godherja the dying world", "godherja"}; k.Ids = new() {"2326030123"}; k.Cat = Cat.Overhaul; k.Pos = -2;
      k.Note = K("Godherja: The Dying World is a total conversion, so it goes first (standard rule for total conversions)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"agot submod core"}; k.Cat = Cat.Library; k.Pos = -2; k.After = new() {"a game of thrones", "agot"};
      k.Note = K("AGOT Submod Core: its page says to load it immediately after A Game of Thrones"); v.Add(k); }
    foreach (var sub in new[] {"agot crowns of westeros", "armor of the kingsguard", "legacy of the dragon", "valyrian steel", "agot plus", "the golden company", "agot brightboar westerosi house flavor"}) {
        var k = new KnownMod(); k.Names = new() {sub}; k.After = new() {"agot submod core"};
        k.Note = K("AGOT submod: the AGOT Submod Core page says participating submods must load after AGOT Submod Core"); v.Add(k);
    }
    { var k = new KnownMod(); k.Names = new() {"more interactive vassals", "miv"}; k.Ids = new() {"2712590542"}; k.Pos = 1; k.Before = new() {"rise and fall"};
      k.Note = K("More Interactive Vassals: its page says to place it at the bottom of the load order (Rise and Fall still goes below it)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"battle graphics"}; k.Ids = new() {"3225355262"}; k.Cat = Cat.Graphics;
      k.After = new() {"community flavor pack", "ethnicities and portraits expanded"};
      k.Note = K("Battle Graphics: its page lists the order: other mods, CFP, EPE, the CFP + EPE patch, then Battle Graphics, then its own compatibility patches"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"dynamic family portrait"}; k.Ids = new() {"3608374693"}; k.Cat = Cat.Graphics;
      k.After = new() {"ethnicities and portraits expanded"}; k.Note = K("Dynamic Family Portrait: its page says to load it below EPE (above EPE disables its brighter portraits)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"visible disfigurement no more masks"}; k.Ids = new() {"3245958435"}; k.After = new() {"community flavor pack", "ethnicities and portraits expanded"};
      k.Note = K("Visible Disfigurement: its page says to load it after CFP / EPE"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"unique artifacts plus"}; k.Ids = new() {"3718999968"};
      k.Note = K("Unique Artifacts +: no load order is given, but its page warns that conflicts may occur with mods that modify adventure inspiration rewards, unique artifact generation, pilgrimage artifact rewards, legend-related artifact rewards or the same artifact creation effects; if another mod changes the same systems, a compatibility patch may be required"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"more lifestyles"}; k.Ids = new() {"3013259695"}; k.Cat = Cat.Content; k.Rank = 5;
      k.Note = K("More Lifestyles: its page says to put it lower in the load order"); v.Add(k); }
    // More recognised mods: the Workshop id and title were read from each mod's own Workshop page; the type is the one the mod's title and summary state.
    // No load-order sentence is recorded for these (none could be read), so only the type (and, for the total conversion, the standard first place) is set.
    { var k = new KnownMod(); k.Names = new() {"the bronze age maryannu", "bronze age maryannu"}; k.Ids = new() {"2216525506"}; k.Cat = Cat.Overhaul; k.Pos = -2; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"cities of wonders 2", "cities of wonders"}; k.Ids = new() {"2216789478"}; k.Cat = Cat.Content; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"viet events", "very immersive events and tales", "viet"}; k.Ids = new() {"2227658180"}; k.Cat = Cat.Content; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"nameplates"}; k.Ids = new() {"2220762808"}; k.Cat = Cat.Ui; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"culture expanded"}; k.Ids = new() {"2829397295"}; k.Cat = Cat.Content; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"daddy pikas cheat menu", "daddy pika's cheat menu"}; k.Ids = new() {"2216746850"}; k.Cat = Cat.Content; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"advanced cheat menu"}; k.Ids = new() {"2218867072"}; k.Cat = Cat.Content; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"immortality updated and revised", "immortality"}; k.Ids = new() {"2252368989"}; k.Cat = Cat.Content; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"seven more traits commander"}; k.Ids = new() {"2750682003"}; k.Cat = Cat.Content; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"petty inheritable traits"}; k.Ids = new() {"2394747105"}; k.Cat = Cat.Content; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"inheritable traits++", "inheritable traits"}; k.Ids = new() {"2588849555"}; k.Cat = Cat.Content; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"game master a new lifestyle trait", "game master"}; k.Ids = new() {"3197506469"}; k.Cat = Cat.Content; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"sawwhets legacies mod pack", "sawwhet's legacies mod pack"}; k.Ids = new() {"3000855939"}; k.Cat = Cat.Content; k.Pos = -1;
      k.Note = K("Sawwhet's Legacies Mod Pack: its page says to load it near the top of the load order"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"hiraeth dynasty legacies overhaul", "hiraeth"}; k.Ids = new() {"2697392271"}; k.Cat = Cat.Content; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"better battle window"}; k.Ids = new() {"3305890494"}; k.Cat = Cat.Ui; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"better battles"}; k.Ids = new() {"2749062980"}; k.Cat = Cat.Content; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"crusader wars warfare"}; k.Ids = new() {"2977969008"}; k.Cat = Cat.Content; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"more tenet slot and doctriness"}; k.Ids = new() {"2857204827"}; k.Cat = Cat.Content; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"dynamic ui themes"}; k.Ids = new() {"2815382549"}; k.Cat = Cat.Ui; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"show more traits"}; k.Ids = new() {"2221307044"}; k.Cat = Cat.Ui; v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"browse the bookstore"}; k.Ids = new() {"2751980550"}; k.Cat = Cat.Content; v.Add(k); }
    // From the mods' own Workshop pages (read and sent by the user): Renovatio requires EPE; the compatibility page lists Embers of Empire, Renovatio, then the patch.
    { var k = new KnownMod(); k.Names = new() {"renovatio"}; k.Cat = Cat.Graphics; k.After = new() {"ethnicities and portraits expanded", "embers of empire"};
      k.Note = K("Renovatio: its page says it needs Ethnicities & Portraits Expanded, and shows it loaded below it"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"embers of empire", "embers of empire a roman restoration"}; k.Note = K("Embers of Empire (known mod)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"eoe + renovatio", "eoe and renovatio"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"embers of empire", "renovatio"};
      k.Note = K("EoE + Renovatio: its page lists the load order Embers of Empire, Renovatio, then this patch last"); v.Add(k); }
    // Author-stated orders from the mods' own Workshop pages (read 2026-10-10). Only statements that the pages agree on are used.
    { var k = new KnownMod(); k.Names = new() {"more bookmarks plus", "mb plus", "more bookmarks plus mb plus"}; k.Ids = new() {"2216670956"};
      k.After = new() {"regional immersion and cultural enrichment", "community flavor pack", "ethnicities and portraits expanded", "unit packs expanded", "the catholic trinity", "culture expanded", "muslim enhancements", "unified ui", "rice plus epe compatibility patch"};
      k.Note = K("More Bookmarks+: its page says to load it after RICE, CFP, EPE, Culture Expanded and Unified UI, with its own compatches below it"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"mb plus epe cfp", "mb plus epecfp"}; k.Ids = new() {"2543865921"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"more bookmarks plus", "mb plus"};
      k.Note = K("A More Bookmarks+ compatch: its page says to load it after More Bookmarks+"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"mb plus ce"}; k.Ids = new() {"2829541200"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"more bookmarks plus", "mb plus", "culture expanded"};
      k.Note = K("A More Bookmarks+ compatch: its page says to load it after More Bookmarks+"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"mb plus no flavorized titles dont use with ctp", "mb plus no flavorized titles"}; k.Ids = new() {"2457619527"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"more bookmarks plus", "mb plus"};
      k.Note = K("A More Bookmarks+ compatch: its page says to load it after More Bookmarks+"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"regional immersion and cultural enrichment", "rice"}; k.Ids = new() {"2273832430"}; k.Before = new() {"ethnicities and portraits expanded", "epe"};
      k.Note = K("RICE: its RICE + EPE patch page says RICE loads before EPE"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"rice plus epe compatibility patch", "rice plus epe compatch", "rice plus epe"}; k.Ids = new() {"2553043828"}; k.Cat = Cat.Patch; k.Rank = 9;
      k.After = new() {"regional immersion and cultural enrichment", "rice", "ethnicities and portraits expanded", "epe"};
      k.Note = K("RICE + EPE patch: its page says to load it after both RICE and EPE"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"unit packs expanded"}; k.Ids = new() {"2818979385"}; k.After = new() {"community flavor pack", "ethnicities and portraits expanded"};
      k.Note = K("Unit Packs Expanded: its page says to load it after CFP and EPE (it requires EPE)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"fair lords expanded"}; k.Ids = new() {"2783343921"}; k.After = new() {"ethnicities and portraits expanded"};
      k.Note = K("Fair Lords / Fair Ladies Expanded: the page says to load it below EPE"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"fair ladies expanded"}; k.Ids = new() {"2783349674"}; k.After = new() {"ethnicities and portraits expanded"};
      k.Note = K("Fair Lords / Fair Ladies Expanded: the page says to load it below EPE"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"unified ui", "uniui"}; k.Ids = new() {"2768734977"}; k.Cat = Cat.Ui;
      k.After = new() {"the catholic trinity", "dynamic ui themes", "nameplates", "more tenets", "more religious doctrines", "more immersive doctrines", "show me your court", "show me your council", "factions explained", "stress bar for all", "classic laws", "enhanced dynasty viewer", "rename anyone", "show spoken language", "auction off artifact", "polytheism reborn"};
      k.Note = K("Unified UI: its page lists mods that load before it (Catholic Trinity, Nameplates, Dynamic UI Themes and others) and a few that load after it"); v.Add(k); }
    foreach (var sub in new[] {"reassembled character window", "rank and file", "name packs expanded"}) {
        var k = new KnownMod(); k.Names = new() {sub}; k.After = new() {"unified ui"}; k.Note = K("The Unified UI page says to load this below Unified UI"); v.Add(k);
    }
    { var k = new KnownMod(); k.Names = new() {"muslim enhancements"}; k.Ids = new() {"2241658518"}; k.Before = new() {"culture expanded", "community flavor pack", "ethnicities and portraits expanded"};
      k.Note = K("Muslim Enhancements: its page says to load it before Culture Expanded, CFP and EPE"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"community title project", "ctp"}; k.Ids = new() {"2216919487"};
      k.Note = K("Community Title Project: its page says mods that add cultures, change councils or edit bookmarks should load below it"); v.Add(k); }
    // ---- round of 2026-10-12: more author-stated orders from Workshop pages ----
    { var k = new KnownMod(); k.Names = new() {"character ui overhaul", "cuio"}; k.Ids = new() {"2519175282"}; k.Cat = Cat.Ui; k.After = new() {"better barbershop"};
      k.Note = K("Character UI Overhaul: its page says to load it below Better Barbershop"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"a special world special building compatch", "a special world"}; k.Ids = new() {"2875587269"}; k.Cat = Cat.Patch; k.Rank = 9;
      k.After = new() {"cities of wonder 3", "medieval arts", "unique building plus", "regional immersion and cultural enrichment", "rice", "more bookmarks plus", "africa plus", "muslim enhancements"};
      k.Note = K("A Special World compatch: its page says the special-building mods (Cities of Wonder 3, Medieval Arts, RICE, More Bookmarks+, Muslim Enhancements and others) load before it"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"more holding graphics", "mhg"}; k.Ids = new() {"2262523658"}; k.Cat = Cat.Graphics;
      k.After = new() {"rice", "community flavor pack", "ethnicities and portraits expanded", "culture expanded", "architect of estates", "architect of estates estates building expansion", "medieval arts"};
      k.Note = K("More Holding Graphics: its page lists the order RICE, CFP, EPE, Culture Expanded, Architect of Estates, Medieval Arts, then this mod"); v.Add(k); }
    foreach (var id_name in new (string, string)[] {("2509797306", "MHG + EPE Compatch"), ("2764329234", "MHG + CFP Compatch"), ("2947626935", "MHG + CE Compatch"), ("3186143971", "MHG + RICE + EPE Compatch"), ("", "MHG + RICE Compatch"), ("", "MHG + AOE Compatch"), ("", "MHG + MA Compatch")}) {
        var k = new KnownMod(); k.Names = new() {id_name.Item2}; if (id_name.Item1.Length > 0) k.Ids = new() {id_name.Item1}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"more holding graphics"};
        k.Note = K("A More Holding Graphics compatch: the More Holding Graphics page lists it below More Holding Graphics"); v.Add(k);
    }
    { var k = new KnownMod(); k.Names = new() {"customizable gui"}; k.Ids = new() {"2834463679"}; k.Cat = Cat.Library; k.Pos = -1; k.After = new() {"unofficial patch", "ck3 unofficial patch"};
      k.Note = K("Customizable GUI: its page says to place it above other mods (it does not work on its own)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"snat + the catholic trinity"}; k.Ids = new() {"2744600131"}; k.Cat = Cat.Patch; k.Rank = 9;
      k.After = new() {"the catholic trinity", "separate name and title ie", "seperate name and title ue", "seperate name and title", "separate name and title"};
      k.Note = K("SNAT + The Catholic Trinity: its page says to load it behind both mods"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"immersive chaste trait"}; k.Ids = new() {"2662352702"}; k.Before = new() {"travel the distance", "love marriage family"};
      k.Note = K("The Love Marriage Family compatch page lists this load order: Immersive Chaste Trait, Travel the Distance, Love Marriage Family, then the compatch"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"travel the distance"}; k.Ids = new() {"2743500919"}; k.After = new() {"immersive chaste trait"}; k.Before = new() {"love marriage family"};
      k.Note = K("The Love Marriage Family compatch page lists this load order: Immersive Chaste Trait, Travel the Distance, Love Marriage Family, then the compatch"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"love marriage family"}; k.Ids = new() {"3037969445"}; k.After = new() {"immersive chaste trait", "travel the distance"};
      k.Note = K("The Love Marriage Family compatch page lists this load order: Immersive Chaste Trait, Travel the Distance, Love Marriage Family, then the compatch"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"love marriage family travel the distance immersive chaste trait compatch"}; k.Ids = new() {"3049846007"}; k.Cat = Cat.Patch; k.Rank = 9;
      k.After = new() {"immersive chaste trait", "travel the distance", "love marriage family"};
      k.Note = K("The Love Marriage Family compatch page lists this load order: Immersive Chaste Trait, Travel the Distance, Love Marriage Family, then the compatch"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"dnasis character ui compatch"}; k.Ids = new() {"3058423279"}; k.Cat = Cat.Patch; k.Rank = 9;
      k.After = new() {"customizable gui", "the catholic trinity", "in my humble opinion", "rename anyone", "displayed birth and death dates", "seperate name and title ue", "seperate name and title", "separate name and title ie", "stress bar for all", "custom title form of address", "hometowns"};
      k.Note = K("Dnasis Character UI Compatch: its page says Customizable GUI goes at the top and this loads after the mods it supports"); v.Add(k); }
    // ---- round of 2026-10-17: AGOT search, last updated ----
    { var k = new KnownMod(); k.Names = new() {"grand remembrance agot compatibility submod", "grand remembrance agot"}; k.Ids = new() {"3683507542"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"a game of thrones", "grand remembrance"};
      k.Note = K("Grand Remembrance - AGOT Compatibility Submod: its page says the order is A Game of Thrones, Grand Remembrance, then this submod last"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"animal characters agot patch"}; k.Ids = new() {"3820075861"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"a game of thrones", "animal characters"};
      k.Note = K("Animal Characters - AGOT patch: its page says the order is A Game of Thrones, Animal Characters, then this patch"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"animal characters vanilla patch"}; k.Ids = new() {"3820076820"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"animal characters"};
      k.Note = K("Animal Characters - Vanilla patch: its page says to load it after Animal Characters"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"sightseer agot compatibility"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"a game of thrones", "sightseer"};
      k.Note = K("Sightseer AGOT Compatibility: its page says to keep it after A Game of Thrones and its submods"); v.Add(k); }
    // ---- round of 2026-10-21 (AGOT search) ----
    { var k = new KnownMod(); k.Names = new() {"principality immunity", "agot principality immunity", "\u516c\u56fd\u65e0\u60e9\u7f5a \u73a9\u5bb6\u4e13\u5c5e"}; k.Ids = new() {"3808336193"}; k.After = new() {"a game of thrones"};
      k.Note = K("Principality immunity (the Chinese-titled duchy-penalty mod): its page says to put it below A Game of Thrones so it overrides it"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"agot dragonkeepers turkce", "agot dragonkeepers t\u00fcrk\u00e7e"}; k.Ids = new() {"3808352309"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"a game of thrones", "agot dragonkeepers"};
      k.Note = K("AGOT - Dragonkeepers - Turkce: its page says to load it after Dragonkeepers"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"peace deals expanded agot compatibility"}; k.Ids = new() {"3771381436"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"a game of thrones", "peace deals expanded"};
      k.Note = K("Peace Deals Expanded - AGOT Compatibility: its page says A Game of Thrones, then Peace Deals Expanded, then this patch"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"agot dynamic aesthetics by hades and fred", "hades and fred s agot dynamic aesthetics"}; k.Ids = new() {"3793472366"}; k.After = new() {"a game of thrones", "agot submod core", "better barbershop"};
      k.Note = K("AGOT - Dynamic Aesthetics: its page recommends A Game of Thrones, AGOT Submod Core, any clothing mods, Better Barbershop, then this mod"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"hades and fred s agot dynamic aesthetics better barbershop compatch"}; k.Ids = new() {"3815803072"}; k.Cat = Cat.Patch; k.Rank = 9;
      k.After = new() {"a game of thrones", "better barbershop", "agot dynamic aesthetics by hades and fred", "hades and fred s agot dynamic aesthetics"};
      k.Note = K("Dynamic Aesthetics - Better Barbershop Compatch: its page says to load it after both Better Barbershop and Dynamic Aesthetics"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"dragon horn for adventurers"}; k.Ids = new() {"3810748783"}; k.After = new() {"a game of thrones"};
      k.Note = K("Dragon Horn For Adventurers: its page says to place it below A Game of Thrones and its dragon-egg mod"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"agot squire events \u0440\u0443\u0441\u0438\u0444\u0438\u043a\u0430\u0442\u043e\u0440"}; k.Ids = new() {"3800913445"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"a game of thrones", "agot squire events"};
      k.Note = K("AGOT - Squire Events (Russian): its page says to place it below Squire Events and below the Russian AGOT translation"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"agot dragonpit levels rus"}; k.Ids = new() {"3788886480"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"agot dragonpit levels"};
      k.Note = K("AGOT Dragonpit Levels (Russian add-on): its page says to load it below AGOT Dragonpit Levels"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"agot more actions \u0440\u0443\u0441\u0441\u043a\u0430\u044f \u043b\u043e\u043a\u0430\u043b\u0438\u0437\u0430\u0446\u0438\u044f"}; k.Ids = new() {"3792276316"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"a game of thrones", "agot more actions"};
      k.Note = K("AGOT: More Actions (Russian): its page says to load it after AGOT: More Actions"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"agot battlefield duel plus duel overlay compatch"}; k.Ids = new() {"3765555569"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"a game of thrones", "agot battlefield duel plus", "duel overlay"};
      k.Note = K("AGOT - Battlefield Duel + - Duel Overlay Compatch: its page says A Game of Thrones, Battlefield Duel +, Duel Overlay, then this patch"); v.Add(k); }
    // ---- round of 2026-10-13: Dynamic Trade Routes and Africa Plus ----
    { var k = new KnownMod(); k.Names = new() {"custom title form of address"}; k.Ids = new() {"2653885896"};
      k.After = new() {"title manager", "randomize coa", "ethnicities and portraits expanded", "community title project", "more cultural names", "community mods for historicity more cultural names", "submission to authority"};
      k.Note = K("Custom Title Form of Address: its page says it must load after Title Manager, Randomize CoA, Ethnicities & Portraits Expanded, Community Title Project, More Cultural Names and Submission to Authority"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"displayed birth and death dates", "displayed birth dates"}; k.Ids = new() {"2228178763"}; k.Cat = Cat.Ui;
      k.Before = new() {"better ui scaling", "better character ui", "show more traits", "show more traits better ui mod"};
      k.Note = K("Displayed Birth and Death Dates: its page says it should go before Better UI Scaling, Better Character UI and Show More Traits"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"seperate name and title ue", "separate name and title ue", "seperate name and title", "separate name and title"}; k.Ids = new() {"2970652165"}; k.Cat = Cat.Ui;
      k.After = new() {"in my humble opinion", "custom title form of address", "displayed birth and death dates"};
      k.Note = K("Separate Name and Title (UE): its page says In My Humble Opinion, Custom Title Form of Address and Displayed Birth and Death Dates go above it"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"dtr pow", "dynamic trade routes prisoners of war"}; k.Ids = new() {"2854343196"};
      k.After = new() {"dynamic trade routes", "beta dynamic trade routes", "prisoners of war"};
      k.Note = K("DTR PoW: its page says to load it after both Dynamic Trade Routes and Prisoners of War"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"holding sprawl dtr"}; k.Ids = new() {"2824465219"};
      k.After = new() {"dynamic trade routes", "beta dynamic trade routes"};
      k.Note = K("Holding Sprawl DTR: its page says to load it after Dynamic Trade Routes"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"immersive personalities", "1 20 immersive personalities"}; k.Ids = new() {"3596393244"}; k.Cat = Cat.Content;
      k.Note = K("Immersive Personalities: its author says any load order works, since it replaces no vanilla files (overhauls like Princes of Darkness still go first)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"elder kings 2 nostalgia music submod", "elder kings 2 nostalgia music"}; k.Ids = new() {"2882895497"}; k.After = new() {"elder kings 2"};
      k.Note = K("Elder Kings 2 - Nostalgia Music Submod: its page says to place it below Elder Kings 2"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"search and trade artifacts", "search trade artifacts"}; k.Ids = new() {"2962238514"}; k.After = new() {"unofficial patch", "ck3 unofficial patch"};
      k.Note = K("Search & Trade Artifacts: its page says to put it after the Unofficial Patch"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"agot expanded dragons"}; k.Ids = new() {"3466228580"}; k.After = new() {"agot more dragon eggs", "a game of thrones", "agot"};
      k.Note = K("AGOT Expanded - Dragons: its page says it works with AGOT More Dragon Eggs if it loads below it"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"better ruler designer"}; k.Ids = new() {"3275318194"}; k.Cat = Cat.Ui; k.After = new() {"better ui scaling"};
      k.Note = K("Better Ruler Designer: its page says to put it below Better UI Scaling and other mods that change the ruler designer window"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"agot armies of westeros", "armies of westeros"}; k.Ids = new() {"2966543745"};
      k.After = new() {"a game of thrones", "agot", "agot submod core", "agot plus", "legacy of valyria"};
      k.Note = K("AGOT - Armies of Westeros: its page gives the order AGOT, AGOT Submod Core, AGOT+, Legacy of Valyria, then this mod"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"cow agot 3d models and bigger castles", "cow agot 3d models bigger castles"}; k.Ids = new() {"2971198450"};
      k.After = new() {"a game of thrones", "agot", "agot submod core", "agot plus", "legacy of valyria", "agot crowns of westeros", "armor of the kingsguard", "legacy of the dragon", "valyrian steel", "agot armies of westeros"};
      k.Note = K("COW-AGOT 3D Models and Bigger Castles: its page says to always put it after AGOT and the other AGOT submods"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"rui character unofficial", "ruicharacter unofficial", "rui character"}; k.Ids = new() {"3239002020"}; k.Cat = Cat.Ui;
      k.After = new() {"hometowns", "separate name and title", "seperate name and title", "seperate name and title ue", "weight of crown", "historical bloodline", "oriental empires eastern etiquette", "elf destiny"};
      k.Note = K("RUI:Character Unofficial: its page lists mods (Hometowns, Separate Name and Title, Weight of Crown and others) that must load before it"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"knight manager"}; k.Ids = new() {"2293272317"}; k.After = new() {"unified ui"};
      k.Note = K("Knight Manager: its page says that if you use Unified UI it goes at the bottom of the load order"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"dynamic trade routes", "beta dynamic trade routes", "dtr"}; k.Ids = new() {"2821835496"}; k.After = new() {"more bookmarks plus"}; k.Before = new() {"dtr lite"};
      k.Note = K("DTR Lite page lists this load order: More Bookmarks+, Dynamic Trade Routes, DTR Lite, then the MB+DTR compatch"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"dtr lite", "not updated yet dtr lite"}; k.Ids = new() {"2828692797"}; k.After = new() {"more bookmarks plus", "dynamic trade routes"};
      k.Note = K("DTR Lite page lists this load order: More Bookmarks+, Dynamic Trade Routes, DTR Lite, then the MB+DTR compatch"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"mb plus dtr compatch", "mb plus dtr"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"more bookmarks plus", "dynamic trade routes", "dtr lite"};
      k.Note = K("DTR Lite page lists this load order: More Bookmarks+, Dynamic Trade Routes, DTR Lite, then the MB+DTR compatch"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"africa plus", "bap", "africa plus needs update"}; k.Ids = new() {"2552371897"}; k.After = new() {"muslim enhancements", "ibn battutas legacy 2"};
      k.Before = new() {"regional immersion and cultural enrichment", "ethnicities and portraits expanded", "culture expanded"};
      k.Note = K("Africa Plus: its page suggests Muslim Enhancements, Ibn Battuta's Legacy 2, Africa Plus, its compatches, then RICE, EPE and Culture Expanded"); v.Add(k); }
    foreach (var nm in new[] {"bap me compatch", "bap ibl compatch", "bap epe compatch"}) {
        var k = new KnownMod(); k.Names = new() {nm}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"africa plus"};
        k.Note = K("Africa Plus page: its compatches load right after Africa Plus"); v.Add(k);
    }
    // ---- added in the C# line from Workshop pages read on 2026-10-22 (only author-stated orders) ----
    { var k = new KnownMod(); k.Names = new() {"simple graphic pack"}; k.Ids = new() {"3255992492"}; k.Cat = Cat.Graphics;
      k.Note = K("Simple Graphic Pack: its page says it works fine above AGOT, but AGOT must stay at the very top, so no position is forced here"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"ck3 expanded compatch", "ck3 expanded"}; k.Ids = new() {"3798622429"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"a game of thrones", "agot"};
      k.Note = K("CK3 Expanded (Compatch): its page says any fantasy total conversion, then CK3 Expanded, then this compatch"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"immersive realm laws"}; k.Ids = new() {"3351630685"}; k.After = new() {"a game of thrones", "agot"};
      k.Note = K("Immersive Realm Laws: its page says to place it below total conversions"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"sis legacy poses"}; k.Ids = new() {"3805901603"}; k.After = new() {"set in stone court statues"};
      k.Note = K("SIS - Legacy Poses: its page says Set In Stone, then this mod, then any SIS patch"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"legend of the sacred spring"}; k.Ids = new() {"3381939779"}; k.After = new() {"a game of thrones", "agot"};
      k.Note = K("Legend of the Sacred Spring: its page says to place it below mods that change the interaction menu (example order: Divine Intervention, AGOT, then this)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"agot direwolves"}; k.Ids = new() {"3766320609"}; k.After = new() {"a game of thrones", "agot", "character ui overhaul"};
      k.Note = K("AGOT: DireWolves: its page says it must load after AGOT, UI mods and mods that overwrite 00_agot_character_triggers.txt (it names Character UI Overhaul)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"holding art by cafka agot", "holding art by cafka"}; k.Ids = new() {"3752156793"}; k.After = new() {"a game of thrones", "agot submod core", "agot plus"};
      k.Note = K("Holding Art by CaFka (AGOT): its page says AGOT, AGOT Submod Core, AGOT+, then this mod (COW-AGOT after it)"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"battle graphics agot compatibility patch"}; k.Ids = new() {"3235061780"}; k.Cat = Cat.Patch; k.Rank = 9; k.After = new() {"a game of thrones", "battle graphics"};
      k.Note = K("Battle Graphics AGOT Compatibility Patch: its page says AGOT, then Battle Graphics, then this patch"); v.Add(k); }
    { var k = new KnownMod(); k.Names = new() {"agot royal guards"}; k.Ids = new() {"3733779333"}; k.After = new() {"a game of thrones", "valyrian steel", "agot knighting ceremony"};
      k.Note = K("AGOT: Royal Guards: its page says required mods go above it, it goes below AGOT: Knighting Ceremony, and compatches go below both mods they patch"); v.Add(k); }
        foreach (var k in v) { k.Names = k.Names.Select(Names.Norm).ToList(); k.After = k.After.Select(Names.Norm).ToList(); k.Before = k.Before.Select(Names.Norm).ToList(); }
        return v;
    }

    public static readonly CompatPatch[] Patches =
    {
        new("EoE + Renovatio", "", new[] {new[] {"embers of empire"}, new[] {"renovatio"}}),
        new("MB Plus EPE CFP", "2543865921", new[] {new[] {"more bookmarks plus", "mb plus"}, new[] {"ethnicities and portraits expanded", "epe"}, new[] {"community flavor pack", "cfp"}}),
        new("MB Plus CE", "2829541200", new[] {new[] {"more bookmarks plus", "mb plus"}, new[] {"culture expanded"}}),
        new("RICE + EPE Compatibility Patch", "2553043828", new[] {new[] {"regional immersion and cultural enrichment", "rice"}, new[] {"ethnicities and portraits expanded", "epe"}}),
        new("MHG + EPE Compatch", "2509797306", new[] {new[] {"more holding graphics", "mhg"}, new[] {"ethnicities and portraits expanded", "epe"}}),
        new("MHG + CFP Compatch", "2764329234", new[] {new[] {"more holding graphics", "mhg"}, new[] {"community flavor pack", "cfp"}}),
        new("MHG + CE Compatch", "2947626935", new[] {new[] {"more holding graphics", "mhg"}, new[] {"culture expanded"}}),
        new("MHG + RICE + EPE Compatch", "3186143971", new[] {new[] {"more holding graphics", "mhg"}, new[] {"regional immersion and cultural enrichment", "rice"}, new[] {"ethnicities and portraits expanded", "epe"}}),
        new("MHG + RICE Compatch", "", new[] {new[] {"more holding graphics", "mhg"}, new[] {"regional immersion and cultural enrichment", "rice"}}),
        new("MHG + AOE Compatch", "", new[] {new[] {"more holding graphics", "mhg"}, new[] {"architect of estates"}}),
        new("MHG + MA Compatch", "", new[] {new[] {"more holding graphics", "mhg"}, new[] {"medieval arts"}}),
        new("SNAT + The Catholic Trinity", "2744600131", new[] {new[] {"separate name and title ue", "seperate name and title ue", "separate name and title", "seperate name and title"}, new[] {"the catholic trinity"}}),
        new("Love Marriage Family + Travel the Distance + Immersive Chaste Trait Compatch", "3049846007", new[] {new[] {"love marriage family"}, new[] {"travel the distance"}, new[] {"immersive chaste trait"}}),
        new("MB Plus DTR Compatch", "", new[] {new[] {"more bookmarks plus", "mb plus"}, new[] {"dynamic trade routes", "dtr lite"}}),
        new("BAP ME Compatch", "", new[] {new[] {"africa plus"}, new[] {"muslim enhancements"}}),
        new("BAP IBL Compatch", "", new[] {new[] {"africa plus"}, new[] {"ibn battutas legacy"}}),
        new("BAP EPE Compatch", "", new[] {new[] {"africa plus"}, new[] {"ethnicities and portraits expanded", "epe"}}),
        new("DTR PoW", "", new[] {new[] {"dynamic trade routes"}, new[] {"prisoners of war"}}),
        new("Peace Deals Expanded - AGOT Compatibility", "3771381436", new[] {new[] {"a game of thrones"}, new[] {"peace deals expanded"}}),
        new("Legacy of Valyria - AGOT 0.5.2.1 Compatch", "3788296332", new[] {new[] {"a game of thrones"}, new[] {"legacy of valyria"}}),
        new("Dynamic Aesthetics - Better Barbershop Compatch", "3815803072", new[] {new[] {"agot dynamic aesthetics by hades and fred", "hades and fred s agot dynamic aesthetics"}, new[] {"better barbershop"}}),
        new("AGOT - Battlefield Duel + - Duel Overlay Compatch", "3765555569", new[] {new[] {"agot battlefield duel plus"}, new[] {"duel overlay"}}),
        new("Elf Destiny Lite V2 Agot Compatch", "3571218992", new[] {new[] {"a game of thrones"}, new[] {"elf destiny lite"}}),
        new("CFP + EPE Compatibility Patch", "2996881191", new[] {new[] {"community flavor pack", "cfp"}, new[] {"ethnicities and portraits expanded", "epe"}}),
        new("AGOT - Better Executions Updated Compatibility", "3817963287", new[] {new[] {"a game of thrones"}, new[] {"better executions updated"}}),
        new("A Special World - Special Building Compatch", "2875587269", new[] {new[] {"a special world"}, new[] {"cities of wonders", "medieval arts", "unique buildings plus", "regional immersion and cultural enrichment", "rice", "more bookmarks plus", "mb plus", "africa plus", "muslim enhancements"}}),
        new("Grand Remembrance - AGOT Compatibility Submod", "3683507542", new[] {new[] {"grand remembrance"}, new[] {"a game of thrones"}}),
        new("Animal Characters - AGOT patch", "3820075861", new[] {new[] {"animal characters"}, new[] {"a game of thrones"}}),
        new("Battle Graphics AGOT Compatibility Patch", "3235061780", new[] {new[] {"a game of thrones"}, new[] {"battle graphics"}}),
        new("Sightseer AGOT Compatibility", "", new[] {new[] {"sightseer"}, new[] {"a game of thrones"}}),
    };
}
