# The Royal Court

**A fast, light mod manager for Crusader Kings III.** It sorts your load order for you, finds the conflicts between your mods before you start a game, and tells you what went wrong when the game crashes.

![license](https://img.shields.io/badge/license-MIT-blue) ![.NET 10](https://img.shields.io/badge/.NET-10-512bd4) ![platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey)

The program has three priorities, in this order:

1. **Performance and light weight.** One self-contained `TheRoyalCourt.exe` (about 49 MB, nothing to install). It reads a 400-mod, 60,000-file playset in about 2 seconds the first time and a third of a second after that, finds file conflicts in about 0.1 seconds, and stays under 100 MB of memory. Background work runs at low priority and pauses while the game is running.
2. **Auto Sort**, to the best of what can be known. A built-in database of load orders that the mods' own authors have stated, plus type rules, dependency order and conflict-aware tie-breaking.
3. **A conflict finder** that looks at files, at definitions inside the files, and at the base game.

It never edits your mod files, never merges anything behind your back, and never downloads mods. The program goes online only when you press **Updates**.

## Download

Get `TheRoyalCourt.exe` from the [latest release](../../releases/latest) (a `SHA256SUMS.txt` is attached). Put it anywhere and run it. It finds your Crusader Kings III folder (`Documents\Paradox Interactive\Crusader Kings III`) and the Steam Workshop mods by itself; if it cannot, type the folder (the one that contains `mod`) at the bottom of the window and press **Save folder**.

Requirements: Windows 10 or 11 (64-bit) and Crusader Kings III. Steam must be running to start the game from the program.

## What it does

### Playsets and load order
- Playsets are saved as Paradox Launcher playset files in the program's own folder, so nothing is locked in. Create, duplicate, rename, delete, export and import them.
- Tick a mod to turn it on or off; every change is saved at once. Drag rows or use **Up** / **Down**, with multi-select (Ctrl or Shift). **Lock position** keeps a mod where it is.
- A filter box and a **Show** menu narrow the list: turned off, turned on, with problems or warnings, may be outdated, load order not known, or large (1 GB or more).
- Columns: order number, mod, version, game version, source, type, **Order** (does the database know this mod's load order?), **Size** on disk, and a Notes column that flags missing mods, mods made for an older game, and conflicts. The status bar shows how much space the enabled mods take.
- **Mod pictures**, three themes (dark, light and a Halloween one), per-monitor DPI, and a window that remembers its position, size and column widths.
- Eleven languages: English, German, Spanish, French, Japanese, Korean, Polish, Portuguese (Brazil), Russian, Turkish and Chinese.

### Auto Sort
- Press **Auto Sort** to get a proposed order with a preview. Nothing changes until you press **Apply**, and **Undo order** puts the list back.
- Dependencies go first, then libraries, overhauls, content, graphics, interface, translations and patches. Total conversions always load first (A Game of Thrones stays at the very top), and mods turned off go to the bottom.
- The database holds **load-order rules taken only from what authors write on their own mod pages** (for example "place this below A Game of Thrones", "Rise and Fall goes at the very bottom") and from the load-order lists of well-known collections. It currently covers around 200 mods and families. Every rule shows its source in the mod's note.
- Right-click a mod to correct its type, lock it, or use **Explain this position**, which tells you which rules hold it where it is and what Auto Sort would do with it.
- **Compatch finder** lists compatibility patches that join two mods you use but have not subscribed to yet (a table of about 50 patches, mostly for AGOT and the common graphics and UI mods).
- **Report a sort problem** copies the details of a wrong sort so the database can be fixed.

### Conflicts
- **By mod pair or by file:** mods that ship the same file. Double-click a file to see every mod that ships it and who wins; right-click a row to let one mod win (the order is saved and checked again).
- **By definition:** mods that define the same trait, event, define or localization key inside different files.
- **Base game:** mods that replace a file of the game itself, which the game never merges.
- Every conflict has a severity (High, Medium, Low); type that word in the filter box to see only those.
- **Known conflicts** are warned about when the mod's author says two mods cannot be used together.

### When something goes wrong
- **Health check:** is the playset ready to play (nothing turned on, two total conversions, load-order warnings, serious file conflicts, known conflicts)? Can run automatically when you press Play.
- **Crash helper:** after a crash, says what kind it was and which of your mods the last log lines point at.
- **Game log:** every error and warning of the game grouped by the mod it comes from, with a copy button for the mod's author.
- **What changed since I last pressed Play:** mods that updated, mods turned on or off, a game update.
- **Bisect:** finds the mod causing a problem by testing halves of your list.
- **Script time:** which mod's scripts use the most game time, measured by the game itself, with a history across sessions and a report for a mod's author.
- **Compare playsets**, automatic **backups** (at startup and before Auto Sort) with restore, and **Clear the shader cache**.

### Play, share, update
- **Play** starts the game directly with your playset, without the Paradox Launcher. Your own `dlc_load.json` is kept next to it as `dlc_load.json.rc-original`. Per-playset launch options (for example `-debug_mode`) are under Advanced.
- **Share codes** turn a whole playset into one line of text; paste it to build the same playset on another machine.
- **Updates** checks this project's GitHub releases, shows what is new, downloads the exe, verifies it against the release's SHA-256 checksum, and restarts. It also fetches the shared known-mods list (`knownmods.json` in this repository). Nothing is contacted until you press the button.
- **Remove**, **Delete mod permanently** (refuses folders that contain links or junctions) and **Unsubscribe on Steam** are always confirmed first.

## Add your own load-order rules

Put a `knownmods.json` in the program's data folder (`%APPDATA%\TheRoyalCourt`, next to `settings.json`). Your entries win over the built-in and the shared ones. A broken file is ignored.

```json
{
  "format": 1,
  "mods": [
    {
      "names": ["my favourite mod"],
      "ids": ["1234567890"],
      "type": "Patch",
      "position": "last",
      "rank": 5,
      "after": ["a game of thrones", "some other mod"],
      "before": ["rise and fall"],
      "note": "Why it sits there (shown in the mod's note)"
    }
  ]
}
```

- `names` are matched ignoring case, punctuation, bracketed tags and trailing version numbers (`&` reads as "and", `+` as "plus"); `ids` are Steam Workshop ids. One of the two is required.
- `type` is one of Library, Overhaul, Content, Graphics, Interface, Translation, Patch.
- `position` is `top`, `first` or `last`; `rank` is -100 to 100; `after` and `before` name other mods.

## Build and test

```
dotnet build RoyalCourt.sln -c Release
cd tests/RoyalCourt.Tests && dotnet test
```

Release build (what the workflow runs):

```
dotnet publish src/RoyalCourt.App -c Release -r win-x64 --self-contained true \
  -p:PublishSingleFile=true -p:EnableCompressionInSingleFile=true \
  -p:IncludeNativeLibrariesForSelfExtract=true -p:DebugType=none -o out
```

Layout:

| Folder | What is in it |
|---|---|
| `src/RoyalCourt.Core` | Everything that is not a window: scanning, playsets, sorting, conflicts, the known-mods database, health check, crash and log helpers, translations (`lang/*.json`). No UI code. |
| `src/RoyalCourt.App` | The Avalonia 11 interface (hand-drawn controls, themes, tour, tool windows). |
| `tests/RoyalCourt.Tests` | xUnit tests: parsers, sorting properties, conflicts, playset files, the database (every name in a rule must match a real Workshop title) and the manager state. |
| `tests/RoyalCourt.Bench` | A synthetic playset benchmark: `dotnet run -c Release -- 400 150`. |
| `tests/RoyalCourt.Shots` | Renders the real windows headlessly for screenshots. |
| `tests/RoyalCourt.Oracle` | Differential testing against the original C++ implementation. |
| `tools` | `workshop_ledger.json` (which Workshop pages were read for load-order statements) and the scripts around it. |

Pushing to `main` runs the tests and builds the exe. Pressing **Run workflow** on the Actions tab also creates a draft release with `TheRoyalCourt.exe` and `SHA256SUMS.txt`.

## Contributing load orders

The most useful contribution is a correct rule. Open an issue (or a pull request against `src/RoyalCourt.Core/KnownData.cs`) with the mod's Workshop link and the sentence in which its author states where it should load. Rules that are guesses are not accepted: the database only holds what authors state. The test `EveryNameInARuleIsKnown` and its siblings catch names that cannot match.

## Status

See [PORT_STATUS.md](PORT_STATUS.md) for what is verified where, and [CHANGELOG.md](CHANGELOG.md) for every change since the first version.

## License

MIT. See [LICENSE](LICENSE).
