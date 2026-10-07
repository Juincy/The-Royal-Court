# The Royal Court | CK3 Mod Manager - Changelog

Every change to the mod manager is logged here, newest first, starting 2026-10-06.
This file ships inside the EXE (Changelog button) and in the zip.

## [Unreleased]
- Nothing yet.

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
