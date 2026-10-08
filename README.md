# The Royal Court | CK3 Mod Manager  v0.9.1

Double-click `TheRoyalCourt.exe` (Windows 7 or newer, 64-bit). It is a normal desktop program: no installer, no browser, no internet access.

## What it does
- Finds your Crusader Kings III folder and lists installed mods.
- Playsets are Paradox Launcher playset files (one .json per playset in %APPDATA%\TheRoyalCourt\Playsets). The file name is the playset name.
- Tick mods on/off, filter by name, drag rows (or Move up/down) to set load order. Every change is saved to the file immediately.
- Export saves a playset file anywhere; Import reads playsets exported by the launcher. Use the launcher's own Import playset to bring one of ours into the launcher.
- Play starts Crusader Kings III directly, skipping the Paradox Launcher, using the playset selected in this app. It writes the playset's mods into the game's dlc_load.json first (your original is kept as dlc_load.json.rc-original). Steam must be running.
- The Changelog window lists every version as an expandable branch.

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
