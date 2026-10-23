namespace RoyalCourt.Core;

/// <summary>Finds the mod behind a crash by halving: the caller turns off the mods in <see cref="Off"/>, the user tests, and reports whether the problem is still there.</summary>
public sealed class Bisect
{
    readonly List<int> suspects;          // positions that may hold the culprit
    readonly List<int> fixedOn = new();   // positions that stay on (cleared)
    List<int> off = new();
    public Bisect(IEnumerable<int> enabledPositions) { suspects = enabledPositions.ToList(); Next(); }

    public IReadOnlyList<int> Suspects => suspects;
    public IReadOnlyList<int> Off => off;
    public bool Done => suspects.Count <= 1;
    public int? Culprit => Done && suspects.Count == 1 ? suspects[0] : null;
    public int Steps => suspects.Count <= 1 ? 0 : (int)Math.Ceiling(Math.Log2(suspects.Count));

    void Next() { off = suspects.Count > 1 ? suspects.Take(suspects.Count / 2).ToList() : new(); }

    /// <summary>The test was run with <see cref="Off"/> turned off. stillBroken: the problem happened anyway.</summary>
    public void Report(bool stillBroken)
    {
        if (Done) return;
        if (stillBroken) { suspects.RemoveRange(0, off.Count); }                 // the culprit is among those that were still on
        else { fixedOn.AddRange(suspects.Skip(off.Count)); suspects.RemoveRange(off.Count, suspects.Count - off.Count); }   // the culprit was in the group turned off
        Next();
    }

    /// <summary>Positions to turn off for the next test: the current group plus nothing else (cleared mods stay on).</summary>
    public IReadOnlyList<int> Cleared => fixedOn;
}
