# Status and verification

The Royal Court is a C# / .NET 10 / Avalonia program. Its UI-free core started as a line-by-line port of the original C++ v0.21.4 and was differential-tested against that build; since then it has grown well past it (the C++ comparison now differs mainly because the known-mods database is much bigger).

## Released
Versions 0.22.0 to 0.26.0 are published as GitHub releases (one self-contained `TheRoyalCourt.exe`, about 49 MB, plus `SHA256SUMS.txt`). The owner runs every release on real Windows.

## Verified automatically (Linux, headless; also run by the GitHub workflow on every push)
- 67 xUnit tests: parsers (with fuzzing), sorting properties, conflict finding, playset files, the known-mods database (every name used in a rule must match a real Workshop title in `tools/workshop_ledger.json`, or be on the reviewed list `tests/RoyalCourt.Tests/fixtures/external_names.txt`), size measuring, the Show filters and the compatch table.
- Benchmark (`tests/RoyalCourt.Bench`, synthetic playset, core only): 400 mods / 60k files: cold index about 2 s, warm 0.3 s, file conflicts 0.13 s, script conflicts 0.12 s, sort 0.07 s, peak memory 97 MB.
- Headless screenshots of the real windows (`tests/RoyalCourt.Shots`) in all three themes, used to catch layout problems.
- Function-level differential test against the C++ oracle: no differences.

## Needs real Windows (tested by hand by the owner)
Window chrome and dark title bar, DPI scaling, drag-reorder feel, popup menus, dialog focus, Steam unsubscribe, launching the game, single-instance raise, the in-place updater, performance on very large lists.

## Known limits
- The database only holds load orders that authors state on their own pages; mods without a statement sort by type, dependencies and conflicts only. The Order column shows which mods are covered.
- The Workshop ledger (`tools/workshop_ledger.json`) tracks which pages were read; reading continues as Steam allows (it rate-limits quick readers).
- Dropped on purpose: downloading Workshop mods, editing mod files, auto-merging conflicts.
