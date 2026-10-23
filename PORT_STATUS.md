# The Royal Court - C# / Avalonia port (NOT pushed, needs real-Windows testing)

Core (UI-free) is a line-by-line port of the C++ v0.21.4, differential-tested against the C++ build:
scenario seeds 1-400 (only documented Unicode divergences) and function seeds 1-60 (0 diffs).
App: Avalonia 11.3.6, hand-drawn controls mirroring the owner-draw look; three themes, 420 ms slide, tour, conflicts, reports, changelog, sort preview.

Verified here (Linux, headless): builds with 0 warnings; main window in all 3 themes, tour, conflicts (CJK ok), script-time, changelog render; scripted enable-all -> conflicts -> sort plan runs.
NOT verified (needs the user on Windows): real window chrome/dark title bar, DPI scaling, drag-reorder feel, popup menus, dialogs focus/modal flow, Steam unsubscribe, launching CK3, single-instance raise, update installer, performance on big lists.
Not ported: test_core.cpp xUnit port (Core is covered by the differential oracle instead).
Target: .NET 10 (LTS). Release = ONE self-contained compressed single-file exe (~47 MB), no runtime install; updater asset name unchanged (TheRoyalCourt.exe). Re-verified on .NET 10: function seeds 1-30 0 diffs, scenario seeds 1-60 only the known Unicode seed 48.

## Audit results (2026-10-22, Linux, .NET 10)
- Tests: 51 xUnit tests (47 ported from test_core.cpp, plus sort property, parser fuzz, known-conflict pairs, bisect). Fuzz at 300,000 iterations: clean.
- Analyzers (latest-recommended): no correctness findings. Noise only: public fields (CA1051), culture overloads (CA1305), CancellationTokenSource/MemoryStream held without Dispose in short-lived job objects (harmless, no unmanaged handles).
- Speed (synthetic, core only; C++ figures in brackets): 400 mods / 60k files: cold index 0.86 s [0.8], warm 0.31 s [0.2], conflicts 65 ms [18], script conflicts 92 ms, sort 52 ms [6], working set 63 MB [51]. 1,200 mods / 144k files: cold 1.84 s [1.9], warm 0.54 s [0.6], conflicts 196 ms [44], sort 70 ms [20], working set 104 MB [122]. Indexing and memory match; conflict find and sort are slower but still well under a quarter second. Sort numbers come from a playset that needs no moves.
- Windows-only code read: mod-folder delete (refuses links/junctions, stays inside the mod folder), shader cache clear (deletes a link itself, never follows it), single copy (named Mutex + event), Recycle Bin call (struct layout matches x64), updater replace-and-restart. No changes needed. Not testable here: DPI, dark title bar, drag feel, Steam unsubscribe, game launch, updater end to end.
- Added: known-conflict warnings, huge-conflict-list note, bisect core. Bisect window still to build.
