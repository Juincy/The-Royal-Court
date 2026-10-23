namespace RoyalCourt.Core;

public enum BisectPhase { Baseline, Testing, Confirming, Found, NotFound }

/// <summary>
/// Finds the mod behind a problem by turning mods off. Smarter than plain halving:
/// the candidates are put in order (most suspicious first, a mod always before the mods it needs, so nothing is turned off
/// while something that depends on it stays on) and the search finds the shortest set of leading mods whose removal fixes the problem.
/// The split balances suspicion, not count: a clearly guilty mod is tried alone first. An "everything off" check comes first,
/// the last candidate is confirmed on its own, and when the problem survives that, the culprit stays off and the search goes on for a second one.
/// Candidates are indexes 0..n-1 into a list the caller keeps.
/// </summary>
public sealed class BisectSession
{
    sealed class State
    {
        public List<int> O = new(), Always = new();
        public int Lo, Hi, Cut;            // O[0..Cut) is off in the current test; the culprit is in O[Lo..Hi)
        public BisectPhase Phase; public int Rounds; public bool SawFix;
        public State Copy() => new() { O = O, Always = new(Always), Lo = Lo, Hi = Hi, Cut = Cut, Phase = Phase, Rounds = Rounds, SawFix = SawFix };
    }
    readonly int n;
    readonly double[] w;
    readonly HashSet<int>[] needs;      // transitive: needs[a] = everything a depends on
    readonly List<int> locked = new();
    readonly List<int> initial;
    State st = new();
    readonly Stack<State> history = new();

    /// <param name="count">number of candidates</param>
    /// <param name="weight">suspicion of each candidate (&gt; 0, relative)</param>
    /// <param name="dependsOn">direct dependencies of each candidate (indexes), may be null</param>
    /// <param name="keepOn">candidates the user does not want turned off</param>
    public BisectSession(int count, Func<int, double> weight, Func<int, IEnumerable<int>>? dependsOn = null, IEnumerable<int>? keepOn = null)
    {
        n = count; w = new double[n]; needs = new HashSet<int>[n];
        for (int i = 0; i < n; i++) w[i] = Math.Max(0.01, weight(i));
        var direct = new List<int>[n];
        for (int i = 0; i < n; i++) direct[i] = dependsOn == null ? new() : dependsOn(i).Where(d => d >= 0 && d < n && d != i).Distinct().ToList();
        for (int i = 0; i < n; i++)
        {
            var seen = new HashSet<int>(); var stack = new Stack<int>(direct[i]);
            while (stack.Count > 0) { int d = stack.Pop(); if (!seen.Add(d)) continue; foreach (var e in direct[d]) if (e != i) stack.Push(e); }
            needs[i] = seen;
        }
        var pinned = new HashSet<int>(keepOn ?? Array.Empty<int>());
        foreach (var k in pinned.ToList()) if (k >= 0 && k < n) foreach (var d in needs[k]) pinned.Add(d);   // what a kept mod needs has to stay on too
        locked.AddRange(pinned.Where(p => p >= 0 && p < n).OrderBy(p => p));
        initial = Enumerable.Range(0, n).Where(i => !pinned.Contains(i)).ToList();
        var o = Ordered(initial);
        st = new State { O = o, Hi = o.Count, Cut = o.Count, Phase = o.Count == 0 ? BisectPhase.NotFound : BisectPhase.Baseline };
    }

    public BisectPhase Phase => st.Phase;
    public bool Done => st.Phase == BisectPhase.Found || st.Phase == BisectPhase.NotFound;
    /// <summary>True when the "everything off" check showed that no mod is involved.</summary>
    public bool NotMods { get; private set; }
    /// <summary>Candidates that may still hold the problem.</summary>
    public IReadOnlyList<int> Remaining => st.Phase == BisectPhase.Found || st.Phase == BisectPhase.NotFound ? Array.Empty<int>() : st.O.Skip(st.Lo).Take(st.Hi - st.Lo).ToList();
    /// <summary>The ones that must stay on (kept by the user, or needed by a kept mod).</summary>
    public IReadOnlyList<int> Locked => locked;
    /// <summary>Culprits found earlier in a multi-culprit search: they stay off.</summary>
    public IReadOnlyList<int> Culprits => st.Always;
    public IReadOnlyList<int> AllCulprits => st.Phase == BisectPhase.Found ? st.Always.Concat(new[] { st.O[st.Hi - 1] }).ToList() : st.Always;
    public int? Culprit => st.Phase == BisectPhase.Found && st.Always.Count == 0 ? st.O[st.Hi - 1] : null;
    /// <summary>The mods being turned off for this test (not counting those that must go with them).</summary>
    public IReadOnlyList<int> Group => Done ? Array.Empty<int>() : st.Phase == BisectPhase.Confirming ? new[] { st.O[st.Hi - 1] } : st.O.Take(st.Cut).ToList();
    /// <summary>Of those, the ones that are new in this test.</summary>
    public IReadOnlyList<int> NewInGroup => Done ? Array.Empty<int>() : st.Phase == BisectPhase.Confirming ? Group : st.O.Skip(st.Lo).Take(st.Cut - st.Lo).ToList();
    public int Rounds => st.Rounds;
    public bool CanUndo => history.Count > 0;
    public int RoundsLeft => Done ? 0 : st.Phase == BisectPhase.Confirming ? 1 : (st.Hi - st.Lo <= 1 ? 1 : (int)Math.Ceiling(Math.Log2(st.Hi - st.Lo)) + 1) + (st.Phase == BisectPhase.Baseline ? 1 : 0);

    /// <summary>Everything to turn off for this test: the group, anything that depends on it, and earlier culprits.</summary>
    public IReadOnlyList<int> Off
    {
        get
        {
            var off = new HashSet<int>(Group); foreach (var a in st.Always) off.Add(a);
            if (off.Count == 0) return Array.Empty<int>();
            for (int i = 0; i < n; i++) if (!off.Contains(i) && needs[i].Overlaps(off)) off.Add(i);
            return off.OrderBy(i => i).ToList();
        }
    }
    /// <summary>Turned off only because they need something in the group (not suspects themselves).</summary>
    public IReadOnlyList<int> Extras { get { var g = new HashSet<int>(Group); return Off.Where(i => !g.Contains(i) && !st.Always.Contains(i)).ToList(); } }

    /// <summary>Candidates in an order where anything that depends on another comes first, heaviest first among the free ones.</summary>
    List<int> Ordered(List<int> s)
    {
        var left = new HashSet<int>(s); var res = new List<int>(s.Count);
        // how many of the others still need each one (a loop between two counts for neither)
        var need = new Dictionary<int, int>();
        foreach (var i in s) need[i] = 0;
        foreach (var j in s) foreach (var i in needs[j]) if (i != j && need.ContainsKey(i) && !needs[i].Contains(j)) need[i]++;
        while (left.Count > 0)
        {
            int best = -1; double bw = -1;
            foreach (var i in left)
                if (need[i] == 0 && (w[i] > bw || (w[i] == bw && i < best))) { best = i; bw = w[i]; }
            if (best < 0) foreach (var i in left) if (w[i] > bw || (w[i] == bw && i < best)) { best = i; bw = w[i]; }   // a dependency loop
            res.Add(best); left.Remove(best);
            foreach (var i in needs[best]) if (left.Contains(i) && !needs[i].Contains(best)) need[i]--;
        }
        return res;
    }

    /// <summary>Skips the "everything off" check (it costs one launch).</summary>
    public void SkipBaseline() { if (st.Phase != BisectPhase.Baseline) return; history.Push(st.Copy()); Next(); }

    void Next()
    {
        if (st.Hi - st.Lo <= 1) { if (st.Hi <= st.Lo) { Finish(); return; } st.Phase = BisectPhase.Confirming; st.Cut = st.Hi; return; }
        double total = 0; for (int k = st.Lo; k < st.Hi; k++) total += w[st.O[k]];
        double acc = 0, bestDiff = double.MaxValue; int cut = st.Lo + 1;
        for (int k = st.Lo + 1; k < st.Hi; k++)
        {
            acc += w[st.O[k - 1]];
            double d = Math.Abs(acc - total / 2);
            if (d < bestDiff - 1e-9) { bestDiff = d; cut = k; }
        }
        st.Phase = BisectPhase.Testing; st.Cut = cut;
    }

    void Finish()
    {
        if (st.Always.Count > 0) { st.Phase = BisectPhase.Found; st.O = st.O.Concat(new[] { st.Always[^1] }).ToList(); st.Always.RemoveAt(st.Always.Count - 1); st.Hi = st.O.Count; }
        else st.Phase = BisectPhase.NotFound;
    }

    /// <summary>The test was run with <see cref="Off"/> turned off. stillBroken: the problem happened anyway.</summary>
    public void Report(bool stillBroken)
    {
        if (Done) return;
        history.Push(st.Copy());
        st.Rounds++;
        switch (st.Phase)
        {
            case BisectPhase.Baseline:
                if (stillBroken) { st.Phase = BisectPhase.NotFound; NotMods = true; return; }   // nothing the candidates did
                st.SawFix = true; Next(); return;
            case BisectPhase.Testing:
                if (stillBroken) st.Lo = st.Cut; else { st.Hi = st.Cut; st.SawFix = true; }
                Next(); return;
            case BisectPhase.Confirming:
            {
                int c = st.O[st.Hi - 1];
                if (!stillBroken) { st.Phase = BisectPhase.Found; return; }
                if (!st.SawFix) { st.Phase = BisectPhase.NotFound; return; }   // nothing ever fixed it: not a mod, or several that never went off together
                // it survives with this one off: this mod is part of the problem, but not all of it; look for another one before it
                st.Always.Add(c);
                st.O = st.O.Take(st.Hi - 1).ToList(); st.Lo = 0; st.Hi = st.O.Count; st.Cut = st.Hi;
                if (st.Hi == 0) { Finish(); return; }
                Next(); return;
            }
        }
    }

    public void Undo() { if (history.Count > 0) { st = history.Pop(); NotMods = false; } }
}
