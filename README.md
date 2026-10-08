# The Royal Court | CK3 Mod Manager  v0.13.0

Double-click `TheRoyalCourt.exe` (Windows 10 or 11, 64-bit). It is a normal desktop program: no installer, no browser, no internet access.

## What it does
- Finds your Crusader Kings III folder and lists installed mods.
- Playsets are Paradox Launcher playset files (one .json per playset in %APPDATA%\TheRoyalCourt\Playsets). The file name is the playset name.
- Tick mods on/off, filter by name, drag rows (or Move up/down) to set load order. Every change is saved to the file immediately.
- Export saves a playset file anywhere; Import reads playsets exported by the launcher. Use the launcher's own Import playset to bring one of ours into the launcher.
- Play starts Crusader Kings III directly, skipping the Paradox Launcher, using the playset selected in this app. It writes the playset's mods into the game's dlc_load.json first (your original is kept as dlc_load.json.rc-original). Steam must be running.
- Columns for mod version, game version (checked against your installed CK3), source, type and notes; click a header to sort the view. The Notes column flags missing dependencies, wrong load order, outdated mods and file conflicts.
- Conflicts finds mods that overwrite the same files (by mod pair or by file) and mods that define the same trait, event ID, define, on_action or localization key (By definition). The mod loaded last usually wins; where that is unclear, the app says so.
- Your playset is backed up automatically at startup and before Auto Sort (Advanced menu: Back up now, Restore, Open backups folder).
- Auto Sort proposes a better load order (dependencies first, then Library, Overhaul, Content, Graphics, Interface, Translation, Patch; smaller targeted mods win file conflicts) and shows a preview before anything changes. Undo sort restores the old order. Right-click a mod to lock its position or set its type.
- Right-click a mod to remove it from the list (nothing is deleted; Advanced > Show removed mods undoes it) or, for local mods, send its files to the Recycle Bin.
- Mods that changed since you last pressed Play are flagged in Notes. Advanced has Compare with another playset and per-playset game launch options. The window remembers its size, position and column widths.
- Dark and light themes, and an Advanced menu (for example, delete all saves).
- The Changelog window lists every version as an expandable branch.

## Teaching Auto Sort about a mod
Auto Sort knows a few popular mods (for example Rise and Fall must be last). To add your own, create `knownmods.json` in `%APPDATA%\TheRoyalCourt` (next to settings.json):

```
{"mods":[
  {"names":["My Cool Mod"], "ids":["1234567890"], "type":"Graphics",
   "position":"first", "rank":0, "after":["Some Other Mod"], "before":[], "note":"why it goes here"}
]}
```
`names` are matched ignoring case, version numbers and brackets; `ids` are Steam Workshop ids; `type` is Library, Overhaul, Content, Graphics, Interface, Translation or Patch; `position` is "first" or "last" in the whole load order; `after`/`before` are mod names. An entry with the same name as a built-in one replaces it. Every field except `names` or `ids` is optional.

## About the antivirus warning
The EXE is not code-signed (certificates cost money), and brand-new unsigned programs are often flagged by heuristic scanners even when clean. You can check it yourself:
1. Compare the file's SHA-256 with SHA256SUMS.txt (PowerShell: `Get-FileHash .\TheRoyalCourt.exe`).
2. Upload it to https://www.virustotal.com and see how many engines flag it; one or two heuristic hits on an unsigned new file is a typical false positive.
3. The complete source is in the `src` folder; nothing is hidden. The program only reads your CK3 `mod` folder and writes `dlc_load.json` and its own playsets.json.
4. If Microsoft Defender flags it, you can report it as a false positive at https://www.microsoft.com/wdsi/filesubmission

## Rebuilding
On Linux: install g++-mingw-w64-x86-64 and binutils-mingw-w64-x86-64, then run `src/build.sh`. Tests for the core logic are in `src/tests`.

Every change is logged in CHANGELOG.md.

## License

MIT - see [LICENSE](LICENSE).
