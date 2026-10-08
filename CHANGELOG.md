# The Royal Court | CK3 Mod Manager - Changelog

Every change to the mod manager is logged here, newest first, starting 2026-10-06.
This file ships inside the EXE (Changelog button) and in the zip.

## [Unreleased]
- Nothing yet.

## [0.9.1] - 2026-10-07
### Fixed
- Crash when switching views in the Conflicts window (stale list rows). Views now switch safely.
- Changelog window now opens only once; clicking Changelog again brings the existing window to the front.

### Changed
- Removed the "replace_path wipes" view from the Conflicts window (it duplicated the Notes column warnings, which are kept).
- Header crown is now larger and drawn as a crisp vector crown.
- App/EXE icon redrawn to match the header crown (gold crown on a midnight tile).

## [0.9.0] - 2026-10-07
Conflict finder.

### Added
- **Conflicts** button (and window). The manager lists the files each enabled mod ships and finds where two or more mods ship the same file. The game keeps only the copy from the mod loaded last, so the others are silently ignored; this shows you exactly which ones.
  - **By mod pair**: which mods fight over files, how many, and in which areas (for example `common/traits x12, events x3`). The later mod wins.
  - **By file**: every conflicting file with its winner and the mods it also appears in. Double-click a pair to see only the files those two share.
  - **replace_path wipes**: mods whose `replace_path` removes everything in a folder that loaded before them, and whose files that destroys.
  - Search box to filter by mod name, file or folder; **Rescan files** to read the mod folders again.
- Conflict results appear in the **Notes** column: a warning when a mod's `replace_path` wipes files of an earlier mod (and on the mod that loses them), and "overrides N files / N files overridden" in the double-click details. The count line shows the number of file conflicts.
- Mod files are read in the background right after start (and when you enable a mod), with progress in the count line, so the window never freezes. Results are kept in memory, so reordering mods or toggling them recalculates conflicts instantly.

### Fixed
- The sun and moon in the theme switch looked blurry. They are now drawn as smooth vector shapes, and all buttons now have smooth rounded corners.

### Notes
- Conflicts are found by file path (the same file name in the same folder). Two mods that define the same game object (for example the same trait name) in differently named files are not detected yet; that needs reading the script files and is planned.
- File lists are refreshed when a mod's folder changes. If a Workshop mod updates in place without changing its folder timestamp, press Rescan files (or the resync button).

## [0.8.0] - 2026-10-07
Facelift release.

### Added
- **Dark and light themes** with a toggle switch (sun / moon) in the top right. The choice is remembered; on first start the app follows your Windows setting. Dark is "Midnight Court" (deep indigo and gold), light is "Parchment" (warm paper and royal purple). The title bar follows the theme too.
- **Royal header** with the app name and crown, and a gold accent line. Play is the one gold (dark) / purple (light) button.
- **Symbols on every button** (new, duplicate, rename, delete, export, import, play, advanced, changelog and more), with tooltips on the icon-only ones.
- **Resync button** (the circular arrow in the header): rescans the mod folder, for example after you subscribe to a mod while the app is open. New mods are added at the end of your playsets, disabled. The list also refreshes by itself when you switch back to the app and the mod folder has changed.
- **Green check mark** in the Notes column for enabled mods that passed all checks, so a clean mod no longer looks like an empty cell.
- Disabled mods are shown muted, with alternate row shading.

### Changed
- "Open playsets folder" moved into the Advanced menu to make room.
- The changelog window, name prompt and column headers follow the theme. The changelog no longer shows markdown marks (`**` and backticks).
- The list's last column now fills the full width, so the header has no empty strip on the right.

### Notes
- Windows' own message boxes and the Advanced drop-down menu stay in the standard Windows look.

## [0.7.0] - 2026-10-07
Descriptor reader: the manager now understands each mod's `.mod` file properly, which is the base for the conflict finder and auto sort.

### Added
- **Notes column** with the worst problem or warning for each enabled mod, in red (problem) or amber (warning). Checks:
  - the mod's files cannot be found (bad `path`),
  - a dependency is not installed, or installed but not enabled,
  - a dependency loads after the mod that needs it (it should be above),
  - two enabled mods have the same name,
  - the mod was made for another game version (major.minor).
- The count line now also shows how many enabled mods have problems, and updates as you tick or untick mods.
- **Double-click a mod** for its details: descriptor file, source, versions, files folder, dependencies, folders it replaces (`replace_path`), tags, and every check result including info notes.
- **Column sorting**: click a header to sort ascending, again for descending, a third time (or click `#`) to return to load order. Versions sort naturally (1.9 before 1.10), empty cells always go last. Sorting only changes the view, never the playset; moving mods is paused while a sort or filter is active.

### Changed
- `.mod` files are now read with a real parser instead of line matching: multi-line lists, repeated keys such as `replace_path`, comments and Windows line endings are handled.

## [0.6.2] - 2026-10-07
### Fixed
- Resizing the window all the way down and back up left half-drawn, overlapping buttons on the right side. The main window now clips its child controls, moves them all in a single batch and repaints once, and skips re-layout while minimized.

## [0.6.1] - 2026-10-07
### Added
- **Advanced...** menu (next to Play) for tools that go beyond mod management:
  - **Delete all saves...** removes everything in the CK3 `save games` folder. It shows how many saves and how much space first, asks for confirmation (default answer is No), refuses while the game is running, and moves the saves to the Recycle Bin (anything too large for the Recycle Bin is deleted permanently). The `save games` folder itself is kept.
  - **Open saves folder** and **Open game logs folder** shortcuts.

### Changed
- The window opens a little wider and cannot be made narrower than before, so the new Advanced button never overlaps the other buttons.

### Fixed
- Game Version warning was too strict: a mod built for `1.20.0.3` was flagged on game `1.20.0.4`. Only the major.minor part (`1.20`) is compared now, so patch differences no longer show as outdated.

## [0.6.0] - 2026-10-07
### Added
- New **Game Version** column: the game version each mod says it supports (`supported_version` in its .mod file, for example `1.14.*`). The existing **Version** column is still the mod author's own version.
- The installed Crusader Kings III version is shown at the bottom right. It is read from the game's own install folder (`launcher\launcher-settings.json` next to `binaries`); no Paradox Launcher data is touched. It shows "unknown" if the file cannot be read.
- A mod's Game Version turns amber when it does not match the installed game, and the count line shows how many enabled mods "may be outdated". Wildcards (`1.14.*`, `1.*`) and short forms (`1.14`) are understood; mods with no supported version are never flagged.

### Changed
- The Mod column gives up some width to make room for the new column.

## [0.5.1] - 2026-10-07
Code audit release: fixes and safety only, no new features.

### Fixed
- Loading playsets that contain many local mods was slow because of a quadratic lookup (about 350 ms for a 3000-mod playset, repeated for every playset at start-up). It is now about 10 ms.
- Renaming a playset so that only the capital letters change (for example "vanilla" to "Vanilla") now really renames the file. Before, the old spelling could come back after a restart.
- Playset names that differ only by capital letters, including non-English ones, are now recognised as the same name on Windows. Creating, duplicating, importing or renaming can no longer overwrite another playset's file by accident.
- Mod descriptors saved with a byte-order mark (some Windows editors add one) now show their real name instead of the file name.
- Play no longer overwrites your game's dlc_load.json when it cannot read it (in use, or unusually large). The file is left untouched and you get a message.
- A write that fails at the very end (for example a full disk) is now detected, so a cut-off file can never replace a good one.

### Added
- Only one copy of the app can run at a time. Starting a second copy brings the first one to the front. Two copies would have overwritten each other's changes.
- Play checks whether Crusader Kings III is already running and stops with a message before touching the mod list.
- If something unexpected goes wrong inside the window, you now get a message and the app keeps running instead of closing silently.

### Changed
- Build: unused code is removed (the EXE is smaller) and stack protection is switched on.
- Tests: every fix above has a test. Windows file behaviour (replacing files, capital letters in names) was also tested under Wine.

### Checked and left as it is
- Scanning the mod folder happens on the main thread. It is fast (about 40 ms for 3000 mods with the files already cached) but may feel slower on a cold disk with antivirus scanning.
- The list is rebuilt while you type in the filter box, which is fine for hundreds of mods and will be reworked for very large lists.
- The window uses system-wide display scaling, so it can look slightly soft when moved between screens with different scaling.
- Paths longer than 260 characters are not supported.
- Trailing text after a valid JSON file is accepted on purpose, so a slightly damaged file you own is not rejected.
- The two test-only environment settings (RC_CK3_DIR and RC_DATA_DIR) are still read by the release build.

## [0.5.0] - 2026-10-07
Play now skips the Paradox Launcher and uses the playset you have selected.

### Changed
- Play starts ck3.exe directly (from the "binaries" folder of your Crusader Kings III install) instead of going through Steam's launcher. The game starts with the playset that is selected in this app, not the one active in the Paradox Launcher.
- To make that work, Play writes the selected playset's enabled and installed mods, in load order, into the game's own mod list (dlc_load.json in your CK3 folder) just before starting the game. This happens only when you press Play; there is no separate button. Mods that are not installed are skipped, and the status line says how many.
- The game is found automatically through your Steam library. If it cannot be found (for example a non-Steam install), Play asks you once to point at ck3.exe and remembers it.
- Steam must be running. If it is not, Play tells you and does nothing.

### Added
- Your own dlc_load.json is copied once to dlc_load.json.rc-original before this app first changes it, so you can always get it back. Other settings in the file (such as disabled DLC) are kept.
- Tested end to end with a simulated Steam library and game: the game is found, the mod list is written in the right order, and the game is started from its own folder.

### Notes
- Not yet verified with the real game: that it loads the mods from dlc_load.json when started this way. If it starts without your mods, please tell me what you see.
- The launcher's own playsets are never read or changed. Next time you press Play inside the Paradox Launcher, it will write its own mod list again.
- Starting the game directly skips the launcher's own checks and settings screens.

## [0.4.0] - 2026-10-07
Play button, launcher-only playsets, and a changelog you can expand by version.

### Added
- Play button: starts Crusader Kings III through Steam. The game uses whichever playset is active in the Paradox Launcher, so import your playset there first.
- Changelog window is now a tree with one expandable branch per version (newest open, older ones collapsed), plus Expand all and Collapse all.
- "Open playsets folder" button, so you can see, back up or share the playset files directly.

### Changed
- Playsets are now Paradox Launcher playset files, one per playset, saved in %APPDATA%\TheRoyalCourt\Playsets. The file name is the playset name, so renaming a file by hand renames the playset, and exports and imports always keep the name you give them. There is no separate app database any more.
- Every change (ticking a mod, reordering, renaming) is saved straight into that playset's file. Files are written safely (to a temporary file first), so a crash cannot leave one half-written.
- Playsets from older versions are converted automatically on first start. The old playsets.json is kept as playsets.json.migrated.
- Only the launcher's own format is used. The old Royal Court export format is gone.
- Files larger than 5 MB are refused instead of being loaded into memory.
- Playset names that Windows does not allow as file names (for example CON, or names with / or ?) are adjusted automatically.

### Removed
- Everything to do with dlc_load.json: the "Write dlc_load.json" button, reading the file and its backups. The app never reads or writes the launcher's own data.

### Notes
- Mods that are not installed, and local mods the launcher can only describe by name, stay in the playset and are shown as "(not installed)" until the mod is available again.
- A playset file keeps the enabled mods in load order. Disabled mods that are installed are not stored (the app lists them as unchecked).
- Not yet verified on a real install: that Play behaves as expected with your Steam and launcher setup.

## [0.3.2] - 2026-10-07
Names and exact launcher file format, checked against a real launcher export.

### Fixed
- Exported playset files now keep the file name you choose: the playset name written inside the file is the file's name, so the launcher sees the same name either way.
- Importing a playset now names it after the file you pick instead of the name stored inside (if that name is already taken it becomes "Name (2)").
- Launcher exports are now byte-for-byte identical to the files the launcher writes itself (compact single line, same field order, no trailing newline). Verified with a real launcher export of an 18-mod playset: importing it into the app and exporting it again gives the identical file.

### Added
- A real launcher export is included as a test fixture, so this stays working in future versions.

## [0.3.1] - 2026-10-07
Compatibility with the Paradox Launcher's own playset files.

### Fixed
- Export now saves in the Paradox Launcher's playset format by default (game, playset name, and mods with displayName, enabled, position and steamId), so the launcher's Import playset can read it. The old Royal Court format is still available from the "Save as type" box.
- Import now reads playset files exported by the Paradox Launcher. Mods are matched by Workshop id (or the launcher's registry id when present), and local mods by name. Load order follows the file's positions. Entries that cannot be matched are skipped and counted in the status line. A playset for another game (for example Stellaris) is refused.
- The "Apply playset" button is now called "Write dlc_load.json" and says plainly what it does. The Paradox Launcher keeps playsets in its own database and rewrites dlc_load.json from its active playset when you press Play, so this button only affects starting the game without the launcher. To use a playset from this app with the launcher, Export it and use the launcher's Import playset.

### Notes
- The launcher format was built from how open-source tools read and write launcher playset files (since confirmed against a real launcher export in 0.3.2).
- Exports contain enabled mods only. Local mods have no Workshop id, so the launcher may only be able to match them by name.

## [0.3.0] - 2026-10-06
Playset import/export and a throne icon.

### Added
- Export...: saves the current playset (its enabled mods, in load order, with mod names) to a small .json file you can share or keep as a backup.
- Import...: loads a playset file as a new playset and switches to it. If the name is taken it becomes "Name (2)" and so on. Mods you don't have installed are shown as "(not installed)" with their real names, and are skipped on apply.
- Import checks the file first: wrong or broken files are rejected with a message, and unsafe or invalid entries (for example paths) are ignored and counted.
- Throne icon on the EXE, the window and the taskbar.
- Playsets now remember each mod's last known name, so a mod that was uninstalled still shows by name instead of a file name.

### Changed
- Toolbar rearranged to fit Export/Import; minimum window width is now wider.

### Known limitations
- Exported files contain only enabled mods; disabled mods are not part of a shared playset.
- Same limitations as 0.2.0 (Paradox Launcher may override Apply; launcher playsets not read; reorder disabled while filtering).

## [0.2.0] - 2026-10-06
Rebuilt as a real Windows desktop program.

### Changed
- Replaced the browser-based interface with a native Windows application (own window, native list, buttons and dialogs). It no longer starts a local web server or opens a browser.
- Rewritten in C++ (was Go). The EXE is built statically, so there is nothing to install; it only needs Windows 7 or newer, 64-bit.
- Mods that are enabled in a playset but no longer installed are now skipped when applying, and the status line says how many were skipped.
- The CK3 folder is now found through Windows' real Documents location first (handles moved or OneDrive Documents folders).
- Window layout scales with display scaling (DPI aware).

### Added
- File version and product details are embedded in the EXE.
- SHA256SUMS.txt in the zip so the EXE can be verified, and the full source in the src folder.

### Fixed / reported
- Reported: an antivirus flagged 0.1.0 as a trojan. 0.1.0 was an unsigned Go program that ran a local web server and launched a browser, a pattern heuristic scanners often flag. 0.2.0 no longer does either. It is still unsigned, so a scanner may still complain; see README.md for how to verify it.

### Known limitations
- Applying writes dlc_load.json. Starting the game from the Paradox Launcher may override it with the launcher's own playset. Not yet verified on a real install.
- The Paradox Launcher's own playsets (launcher-v2.sqlite) are not read or changed.
- Drag-to-reorder and the Move up/Move down buttons are disabled while the filter box has text.
- No conflict detection, log analysis, Workshop updates or import/export yet.

## [0.1.0] - 2026-10-06
First version. Core features only (browser-based interface; replaced in 0.2.0).

### Added
- Auto-detect of the CK3 user folder, installed mod list (Workshop and local) from the .mod files, playsets (create, duplicate, rename, delete), enable/disable, filter, load order, "Apply playset" to dlc_load.json with a timestamped backup, import of the current dlc_load.json on first run, and a Changelog view.
