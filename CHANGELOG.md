# The Royal Court | CK3 Mod Manager - Changelog

Every change to the mod manager is logged here, newest first, starting 2026-10-06.
This file ships inside the EXE (Changelog button) and in the zip.

## [Unreleased]
- Nothing yet.

## [0.17.0] - 2026-10-09
Find out which mod causes the errors, share a playset as one line of text, and see what changed since you last played.

### Added
- **Game log helper** (Advanced > "Game log: which mod causes the errors..."). Reads the game's own log (`logs/game.log`, and `error.log` on older builds) and shows which of your mods each error and warning comes from. The game names a file in its messages, not a mod, so the program looks the file up in your mods: the mod that loads last and has that file is the one the game used. When a message names no file, the names it mentions (an event, a trait, a script) are looked up among what your mods define; those rows say "matched by name". Two views: **By mod** (errors and warnings per mod, the most common message) and **All messages**, with a filter box. Double-click a mod to see only its messages. Right-click copies a line, or all of a mod's messages as a short text you can send to its author. The window also says when the log came from a different set of mods than you have enabled now (it reads the mod list the game printed into `debug.log`), because then the blame can land on the wrong mod.
- **Share codes.** Export > "Copy a share code" puts the whole playset on the clipboard as one line starting with `RC1:`. A short code is about 700 characters for 100 Workshop mods, so it fits in a chat message; the longer code also carries each mod's name so the other person can see what they are missing. Import > "Paste a share code" turns it into a new playset. Mods the other person does not have stay in the list as not installed, and you can copy their Workshop links. Local mods travel by name. A code that was cut or changed on the way is refused (it has a checksum). The playset file export and import are unchanged.
- **What changed since you last pressed Play** (Advanced menu). Lists the mods that were updated (and how: for example "3 files changed, 2 added (mostly common/traits)"), mods that are now older than your game version, mods turned on or off, mods that are gone, and whether the game itself updated. When something changed, the status line says so when the program starts. Pressing Play saves a small record of each enabled mod's files (16 bytes a file, in `%APPDATA%\TheRoyalCourt\seen`) so the next comparison can say what changed; delete the folder any time.
- Tests for the log reader (real message shapes, grouping, attribution, damaged and huge input), the share code (round trip, damage, cut-off, absurd sizes), the file records and the report, and fuzzing of all three readers.

### Fixed
- **The theme change paints faster.** The window background is painted once per frame and the controls are no longer erased a second time. Measured on Windows before this change: 10 frames in 360 ms, 33 ms to paint each. The log line for a theme change reports how long each frame took, and the source holds a measuring build (`RC_PROF`) that splits that time up.

## [0.16.0] - 2026-10-09
Conflicts against the base game, a conflicts window you can act from, and much faster starts with big playsets.

### Added
- **Base-game conflicts.** The program reads the game's own file list once (including DLC folders) and shows which mods **replace** a file of the base game. The game never merges such files: the mod's copy replaces the original completely. New **Base game** view in the conflicts window; the Notes column says "Replaces N file(s) of the base game"; and when that mod was made for an older game version the note turns into a warning (changes the game update made to those files are lost). A `replace_path` that removes whole folders of the base game is listed too.
- **Severity for every conflict** (High, Medium, Low), shown in colour and sorted most serious first. Replacing rules, events, history or map files is High; interface and text are Medium; graphics and sound are Low. Typing `high`, `medium` or `low` in the filter box shows only those.
- **Who wins**: double-click a file (By file or Base game view) to see every mod that ships it in load order, with the winner marked.
- **Fix a conflict from the conflicts window.** Right-click a row: "Let X win: move it below Y" or "move Y above it". The list is saved and re-checked at once, and the old button now called **Undo order** (it was "Undo sort") goes back to the order from before. "Copy file path" is there too.
- **Saved file index.** What the program learns about each mod (its event, trait, define and localization names) is saved in `%APPDATA%\TheRoyalCourt\cache` and only read again for a mod whose files changed. It is only a speed-up: deleting the folder is always safe, and a damaged file is simply ignored. In a test with 13 mods and 160,000 files a restart went from about 30 seconds to about half a second.
- Tests for the saved index (damaged, cut-off and mismatched files), the game file list, severity, vanilla replacement and `replace_path`, a Windows-only check that the new folder walker lists exactly what the old one does, and fuzzing of the cache files.
- The log (`royalcourt.log`) records how long each scan took and how many mods came from the saved index.

### Fixed
- **The smooth theme change was not smooth on Windows.** It faded a screenshot over the window, which did not show. The colours of the whole window are now blended frame by frame, so the change is visible and smooth.
- **Theme change runs at more frames per second.** Windows only fired the animation timer about every 31 ms, which capped it near 30 frames a second. The timer now runs at 1 ms resolution while the change plays, the title bar colour updates every other frame and the window frame is redrawn only at the end. The log line for a theme change now also says how long each frame took to paint.
- **Updates deep inside a mod could be missed.** A mod only counted as changed (the "Updated since you last pressed Play" note, and the conflict data) when its top folder changed, so a Steam update that only edited files in sub-folders went unnoticed. Every file's name, size and time now go into a fingerprint. It is recorded when you press Play, compared against the files on disk once they have been read, and checked again in the background when you switch back to the program or press **Rescan files** (at most every two minutes, silently). A harmless change of a folder's date no longer counts as an update.
- **A script conflict check could take almost a minute** when one name is defined thousands of times across mods (53 seconds in a test; now a quarter of a second).
- Conflicts are sorted by severity first, so the serious ones are not buried under a pair that shares many unimportant files.

### Changed
- **Listing a mod's files is several times faster on Windows** (the program's own folder walker, also used for the game's files); long paths over 260 characters are now handled.
- Large playsets (over about 150,000 files) compute their conflict report on a background thread, so the window never stops answering; small ones are still instant. Auto Sort itself still runs on the main thread.
- The conflicts window is wider by default and has a **Base game** button; the Undo button is called **Undo order**.

### Known and left for later
- The game's file list is refreshed when the game version or the game's folders change. If you change game files by hand, press **Rescan files** in the conflicts window.
- Where DLC files live is not confirmed on every install; the log says how many game files were found (`game files: N ...`).

## [0.15.0] - 2026-10-08
Auto Sort can now learn new mods without a new program version, and you can report a sort that looks wrong.

### Added
- **Shared known-mods list.** When you press **Updates**, the program also fetches `knownmods.json` from this project's GitHub (one fixed address, nothing else is contacted). The list is checked before it is kept (right format, sensible size) and a bad file is ignored. A newer list is saved as `knownmods.online.json` in `%APPDATA%\TheRoyalCourt` and used by the next Auto Sort. Layers: built into the program, then the shared list, then your own `knownmods.json`, which always wins.
- **Report a sort problem** (Advanced menu): copies the load order to the clipboard (mod names, Steam ids, types, which mods the program recognises, locked mods, game and program version) and offers to open the GitHub issues page to paste it. No file paths or personal information are included.
- `knownmods.json` in the repository, the file the shared list is read from, checked by the tests.
- Tests for the three list layers, broken or too-large downloads, and for the report content (including that no paths leak).

### Fixed (full audit, every item below was reproduced first)
- **Local mods could silently lose their place.** Mods that are not from the Workshop were stored by display name only, so renaming a mod, or two mods with the same name, demoted or dropped one. They are now stored by their own file name too (`gameRegistryId`, the field the launcher's own files use).
- **A mod folder that could not be read completely made the program rescan forever** and switched off the conflict finder for the whole playset. Such a mod is now skipped (logged), and tried again only when its folder changes.
- **Remove and Delete only backed up the open playset**, although the mod disappears from all of them. Every playset that contains the mod is backed up first. Deleting a playset now keeps a backup, and renaming a playset keeps its backups with it.
- **Auto Sort recognised short names too eagerly.** "AGOT Dothraki Rework" was treated as A Game of Thrones, "MIV Lite" as More Interactive Vassals and "Unofficial Patch Extras" as the Unofficial Patch. Short names and acronyms must now match exactly (or by Workshop id).
- **Elder Kings 2** was never recognised by name (its trailing "2" is treated as a version number); built-in names are now normalised the same way and a test checks every one.
- **Your own `knownmods.json` did not always win**: an entry that named a mod by Workshop id only was ignored in favour of the built-in one. Later layers now win, by name or by id.
- **Circular dependencies were broken at the wrong mod.** A mod that merely waited on a cycle could load before its own dependency. The break now happens inside the cycle.
- The Steam registration file (`ugc_<id>.mod`) is written all-or-nothing, so a crash cannot leave a half-written file that is then trusted.
- The old program file left by an update is deleted only after the new version has run for 20 seconds, so there is a way back if the new one crashes at startup.
- A game version with a wildcard in the installed version no longer throws.
- The small dialogs no longer swallow a "close" request from Windows (log off, shut down).
- A damaged `settings.json` is kept as `settings.json.bad` instead of being silently replaced, and absurd numbers in it (NaN, huge window sizes) are clamped.
- A playset file whose name is very long or has stray spaces is renamed once on load, instead of creating a second copy when saved.
- Deleting a mod's folder now also refuses folders that are links or junctions; text that is not valid UTF-8 in a descriptor no longer aborts the whole scan.
- The mod type of mods guessed from their files is remembered instead of recomputed on every redraw (a 300,000-file playset spent about 40 ms per redraw on it).

### Known and left for later (needs measuring on real Windows)
- "Updated since you last pressed Play" only notices a changed root folder of a mod, so an update that only edits files inside sub-folders can be missed. A fix means recording sizes and times during the background scan, which costs speed; it will be done with the on-disk index cache.
- Conflict finding, script conflicts and Auto Sort run on the main thread (measured at 0.2 to 0.4 seconds for 300 mods with 300,000 files).

### Changed
- **Smooth theme change**: the switch slides and its colours blend over a third of a second, while the whole window cross-fades from the old colours to the new ones instead of jumping.
- **New theme switch**: a raised 3D panel switch (glossy knob with a cast shadow, sunken track, gradient rim). Dark mode is a blue night track with the moon at the right, light mode a sunrise track with the sun at the left; the other symbol stays faintly visible. The sun and moon symbols themselves are unchanged.
- **Menus match the theme.** The right-click menu on mods, the Advanced menu and the playset picker of "Compare" are drawn in the program's colours (dark or parchment, gold tick marks, rounded highlight), and the menu frame follows the theme on Windows 10 (1903 and newer) and Windows 11.
- **Current CK3 version**: only the version number is shown in gold and bold, the words around it stay muted.
- A small "Author: Juincy" line under the title (small text, kept clear of the gold line under the banner).
- The Updates button tooltip mentions the known-mods list.
- `knownmods.json` entries are limited in size (note length, rank range) so a damaged file cannot cause trouble.

### Checked, nothing to add
- Steam pages of Mass Demand Conversion, Quando Sumus and Unique Artifacts + give no load-order instructions, so no new rules were added for them (only facts from a mod's own page are used).

## [0.14.1] - 2026-10-08
Subscribed Workshop mods now show up, plus a bug sweep.

### Added
- **Steam downloads show up before the launcher has set them up.** If you subscribe to a Workshop mod, the Paradox launcher only creates its entry (ugc_<id>.mod) the next time it runs, so the mod never appeared in the list after pressing Rescan. The app now also reads Steam's own Workshop download folder. Such mods are listed greyed out with the note "Downloaded from Steam. Tick the box to add it". Ticking the box adds the mod (the app writes the same entry the launcher would) and it becomes a normal mod. Enable shown and Play do the same for any that are ticked.
- Rescan now says why a mod may be missing: how many Steam downloads are waiting, how many mods are hidden because you removed them, or that Steam may still be downloading.
- The window watches Steam's Workshop folder too, so new downloads are picked up when you switch back to the app.
- Fuzz test for the file parsers (src/tests/fuzz_core.cpp), also run by the GitHub build.

### Fixed
- A rescan could start while a confirmation box or dialog was still open (when Steam or the launcher changed the mod folder at that moment), and the dialog's action then ran on stale data. Rescans and update messages now wait until no dialog is open.
- Play now warns if a Steam mod could not be added to your mod folder, instead of starting the game with a missing entry.
- Deleting mods no longer triggers a needless rescan right afterwards.
- The test-only update override (RC_UPDATE_DIR) is no longer compiled into the released exe.

## [0.14.0] - 2026-10-08
Updates from inside the app, plus a hardening pass.

### Added
- **Updates button** (top right): checks this project's GitHub releases. If a newer version exists you see what's new and can press **Update now**: the app downloads the new exe, checks it against the release's SHA-256 checksum, swaps itself and restarts. A file that does not match is thrown away and nothing changes. Or open the release page and update by hand. Nothing is contacted until you press the button; Advanced has an optional "Check for updates when the program starts" (off by default). Only downloads from this project's own GitHub releases are accepted.
- **Select several mods** (Ctrl/Shift-click): Up/Down moves the whole selection, and the right-click menu locks, sets the type of, removes or deletes all of them at once (one confirmation).
- **Diagnostics log**: Advanced > Open diagnostics log. A small royalcourt.log in %APPDATA%\TheRoyalCourt (startup info, removals, update checks, crashes) to attach to bug reports.
- **Tooltips** on every button.
- **Per-monitor DPI**: the window stays sharp and correctly sized when moved between monitors with different scaling, or when Windows scaling changes.
- GitHub Actions workflow: every push runs the tests and builds the exe; pushing a version tag creates a draft release with the exe, checksum file and zip attached.

### Changed
- The mod list now allows selecting more than one row.

## [0.13.3] - 2026-10-08
More known mods for Auto Sort. Rules come from each mod's own Workshop page, except total conversions, which follow the standard "total conversion loads first" rule.

### Added
- **Total conversions always load first**: A Game of Thrones, LotR: Realms in Exile, Princes of Darkness and Elder Kings 2 sit in a new top tier above everything else (including the Unofficial Patch). Their pages give no explicit order sentence; this is the standard rule.
- Unofficial Patch recognised by Workshop id (page: load at the very top, right below total conversions here). Visible Disfigurement loads after CFP and EPE. Unique Artifacts + carries its compatibility warning as a note.
- `knownmods.json` accepts position "top" (above "first").
- **A Game of Thrones family** (rules from the AGOT Submod Core page): AGOT stays at the top of the load order; **AGOT Submod Core** loads immediately after it; the participating submods (AGOT - Crowns of Westeros, Armor of the Kingsguard, Legacy Of The Dragon, Valyrian Steel, AGOT+, The Golden Company, AGOT: Brightboar - Westerosi House Flavor) load after the Submod Core.
- **More Interactive Vassals** goes at the bottom of the load order (Rise and Fall still goes below it, then RUI).
- **Battle Graphics** loads after Community Flavor Pack and Ethnicities & Portraits Expanded; its compatibility patches load after it.
- **Dynamic Family Portrait** loads below Ethnicities & Portraits Expanded.
- **More Lifestyles** is recognised and placed lower in its group, as its page asks.

### Fixed
- A "+" in a mod name is no longer ignored, so **AGOT+** is no longer mistaken for AGOT itself.
- Tests for all of the above, including two mods that both ask to be at the top.

## [0.13.2] - 2026-10-08
### Fixed
- **Settings were read from the wrong place at startup.** The app looked for settings.json before it knew your AppData folder, so it read (and created an empty) `TheRoyalCourt` folder next to wherever it was started, while saving to `%APPDATA%\TheRoyalCourt`. That is why your window size, theme, locked mods, chosen mod types and CK3 folder were not remembered after restarting. Settings now load from the same place they are saved. You can delete the empty `TheRoyalCourt` folder next to the exe.

### Changed
- **Delete mod permanently now also works for Workshop mods.** When you unsubscribe on Steam, the game leaves a stale entry behind; Delete erases that entry (its descriptor file) from your disk and from every playset for good. If the mod is still installed through Steam, the dialog tells you to unsubscribe first, because Steam would bring it back. The mod's Steam files are never touched.

## [0.13.1] - 2026-10-08
Permanent delete, window memory fix, audit and cleanup.

### Changed
- **Delete mod permanently** (local mods): the mod's descriptor and its folder in your CK3 mod folder are erased from disk, with no Recycle Bin. The dialog says it cannot be undone. The folder is only erased if it really lies inside your CK3 mod folder (never the mod folder itself, never a link). If a file is in use, nothing is half-removed and the mod stays in the list. Workshop mods keep "Remove from list" (unsubscribe on Steam to uninstall them).

### Fixed
- **Window size, position and column widths were not remembered.** They are now saved as soon as you finish moving or resizing, on maximize/restore, and on close, so they survive however the program ends.

### Audit and cleanup
- Static analysis (clang-tidy bugprone, performance and analyzer checks) and all compiler warnings reviewed: no real defects found. Fixed two confusing variable-name shadows, removed an unused function, tidied a stale comment.
- Update-detection records for mods that are no longer installed are forgotten.
- Re-checked speed and memory with a 100-mod, 100 MB script benchmark (about 3 s in the background, about 76 MB peak in this extreme case); no change needed.

## [0.13.0] - 2026-10-08
Remove mods, plus quality-of-life features.

### Added
- **Remove a mod from the list:** right-click a mod > *Remove from list...*. Nothing is deleted from disk; the mod disappears from every playset (a backup is made first). *Advanced > Show removed mods* brings them all back. For Workshop mods the dialog reminds you to unsubscribe on Steam to uninstall completely.
- **Delete mod files from disk:** for local mods only, the same menu has *Delete mod files from disk (Recycle Bin)...*. The descriptor and the mod's folder inside your CK3 mod folder go to the Recycle Bin, so you can still restore them.
- **Update detection:** pressing Play remembers the state of every enabled mod. Afterwards a mod whose files changed (Workshop update or edited local mod) shows "Updated since you last pressed Play" in Notes. Mods you have never played with are not flagged. It compares the mod folder's modified time and version, so a rare update that leaves both unchanged is not seen.
- **Compare playsets:** *Advanced > Compare with another playset...* lists mods enabled in only one, mods enabled in one and disabled in the other, and which mods sit at a different place in the shared load order (only the mods that actually moved are listed).
- **Game launch options per playset:** *Advanced > Game launch options for this playset...* (for example `-debug_mode`). Used when you press Play; each playset has its own.
- **Remembers window position, size, maximized state and column widths.**

## [0.12.1] - 2026-10-08
### Fixed
- Too many yellow warnings in the Notes column. Overlapping on_actions and definitions where the winner is unclear are normal (patches and compatibility mods do this on purpose), so they are now information only: they no longer show in the Notes column, but stay in the mod's details and in Conflicts > By definition. Only duplicate event IDs are still a warning.

## [0.12.0] - 2026-10-08
Script-level conflicts and automatic backups.

### Added
- **Script-level conflict detection.** Besides files that overwrite each other, the app now reads the mods' script files and finds the same thing defined twice: traits and other `common/` objects, event IDs, defines, on_actions and localization keys.
  - New **By definition** view in the Conflicts window, and warnings in the Notes column.
  - It says who wins only when file-name order and load order agree (CK3 rules here are disputed); otherwise it says plainly that it is unclear.
  - Localization overlaps are shown as information only.
- **Automatic playset backups** at startup and before every Auto Sort, kept in a Backups folder (newest 30).
- **Advanced menu:** Back up now, Restore a backup, Open backups folder.

### Changed
- The scan now also reads script definitions (still in the background).

## [0.11.0] - 2026-10-08
Smarter Auto Sort.

### Added
- **Known-mods list** built into the app, using only what the mods' own pages say:
  - **Rise and Fall** (Workshop 3554844335) goes at the very bottom of the load order; **RUI** goes below it (as its page says).
  - **Unofficial Patch** goes first.
  - **Better Barbershop** loads below Community Flavor Pack, Ethnicities & Portraits Expanded and AGOT.
  - **Better Character UI** loads after other interface mods.
  - Community Flavor Pack, Ethnicities & Portraits Expanded and A Game of Thrones are recognised and typed correctly.
  Recognition works by Workshop id or by name (version numbers, brackets and "&" vs "and" are ignored; "Rise and Fall Compatibility Patch" is not mistaken for Rise and Fall itself).
- **Your own entries:** put a `knownmods.json` file in the app's data folder (the same folder as settings.json) to add mods or change a built-in entry. A broken file is ignored. See the README for the format.
- **Patch linking:** a compatibility patch is now placed after the mods named in its title ("CFP + EPE Compatibility Patch" loads after Community Flavor Pack and Ethnicities & Portraits Expanded; acronyms such as CFP/EPE are understood).
- **File-based types:** when a mod's name and tags say nothing, the files it ships decide its type (mostly localization = Translation, mostly interface = Interface, mostly graphics/audio = Graphics, mod that rewrites the map and history = Overhaul). Mods that replace four or more vanilla folders are treated as Overhauls.
- The Auto Sort preview says how many mods were recognised and how many patches were linked, and the mod details window shows the known-mod note.
- Tests for names, the known-mods list, patch links, file-based types and knownmods.json.

### Changed
- The Type column now reflects the known-mods list and the file scan (it updates when the background file scan finishes).

## [0.10.0] - 2026-10-08
Auto Sort.

### Added
- **Auto Sort** button: works out a better load order and shows a preview first (every mod's new position, what moved, and why). Nothing changes until you press Apply. **Undo sort** puts the old order back.
- Sort rules, strongest first: a mod loads after the mods it depends on; mods are grouped by type (Library, Overhaul, Content, Graphics, Interface, Translation, Patch); when two mods in a group overwrite the same files, the smaller targeted mod loads last so it wins; otherwise your current order is kept. Circular dependencies are detected and reported instead of hanging.
- New **Type** column. Types are guessed from the mod's tags and name; right-click a mod to set its type yourself ("*" marks a type you set) or go back to automatic.
- **Lock position** (right-click a mod): Auto Sort never moves a locked mod (shown with a lock in the # column). Locks are saved per playset in settings.json, along with your type choices.
- Mod details now show the mod's type and whether it is locked.
- Core tests for the sorter (types, dependencies, cycles, locks, overrides, conflict tie-break, settings round trip).

### Changed
- The main window is a little wider by default (and has a larger minimum width) to fit the new buttons.

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
