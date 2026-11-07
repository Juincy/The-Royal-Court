using System.Text;
namespace RoyalCourt.Core;

// The user's own marks: a note on a mod, and the places they put mods by hand (Auto Sort keeps those places).
public sealed partial class ModManager
{
    // ---- notes ----
    public string NoteOf(string id) => St.Notes.TryGetValue(id, out var n) ? n : "";

    public static string CleanNote(string text)
    {
        var t = new StringBuilder();
        foreach (char c in text) t.Append(c == '\r' || c == '\n' || c == '\t' ? ' ' : c);
        string s = t.ToString().Trim();
        return s.Length > Settings.NoteMax ? s[..Settings.NoteMax] : s;
    }

    /// <summary>An empty text removes the note.</summary>
    public void SetNote(string id, string text)
    {
        text = CleanNote(text);
        if (text.Length == 0) St.Notes.Remove(id); else St.Notes[id] = text;
        SaveSettingsNow(); Populate();
    }

    public async Task EditNote(int idx)
    {
        var ps = Active();
        if (ps == null || Dlg == null || idx < 0 || idx >= ps.Mods.Count) return;
        string id = ps.Mods[idx].Id;
        var v = await Dlg.Ask(Tr("Note for this mod"), Trf("Your note for \"{0}\" (only you see it; leave it empty to remove it):", ShownName(ps, idx)), NoteOf(id));
        if (v == null) return;
        SetNote(id, v);
        Say(CleanNote(v).Length == 0 ? Tr("Note removed.") : Tr("Note saved. It shows in the Notes column."));
    }

    // ---- places the user chose ----
    /// <summary>Called after a hand-made move: the mod is remembered as "after the enabled mod above it" (or "before the one below it" at the very top). Moving it again replaces the old place.</summary>
    void LearnMove(Playset ps, int at)
    {
        if (!St.LearnMoves || at < 0 || at >= ps.Mods.Count) return;
        var m = ps.Mods[at];
        if (!m.Enabled || IsLocked(m.Id)) return;
        string rule = "";
        for (int i = at - 1; i >= 0 && rule.Length == 0; i--) if (ps.Mods[i].Enabled) rule = "a:" + ps.Mods[i].Id;
        for (int i = at + 1; i < ps.Mods.Count && rule.Length == 0; i++) if (ps.Mods[i].Enabled) rule = "b:" + ps.Mods[i].Id;
        if (rule.Length == 0) return;
        St.MyRules[m.Id] = rule;
    }

    public bool HasMyRule(string id) => St.MyRules.ContainsKey(id);

    /// <summary>One line per remembered place, with the names of the mods as they are installed now.</summary>
    public string MyRulesText()
    {
        var t = new StringBuilder();
        string Nm(string id) => Info.TryGetValue(id, out var mi) ? mi.Name : id;
        foreach (var kv in St.MyRules)
        {
            if (kv.Value.Length < 3) continue;
            t.Append(kv.Value[0] == 'a' ? Trf("\"{0}\" loads after \"{1}\"", Nm(kv.Key), Nm(kv.Value[2..])) : Trf("\"{0}\" loads before \"{1}\"", Nm(kv.Key), Nm(kv.Value[2..]))).Append("\r\n");
        }
        return t.ToString();
    }

    public void ForgetMyRule(string id)
    {
        if (!St.MyRules.Remove(id)) return;
        SaveSettingsNow(); Populate();
        Say(Tr("Forgot where you put this mod. Auto Sort will place it by the usual rules."));
    }

    public async Task ForgetMyRules()
    {
        if (Dlg == null) return;
        if (St.MyRules.Count == 0) { await Dlg.Info(Tr("No places are remembered yet. When you drag a mod to a new place in the list, the program remembers it and Auto Sort keeps it there.")); return; }
        if (!await Dlg.Confirm(Tr("My remembered places"), Trf("These {0} place(s) are remembered, and Auto Sort keeps them:", St.MyRules.Count) + "\r\n\r\n" + MyRulesText() + "\r\n" + Tr("Forget all of them? Auto Sort will then place those mods by the usual rules."))) return;
        int n = St.MyRules.Count;
        St.MyRules.Clear(); SaveSettingsNow(); Populate();
        Say(Trf("Forgot {0} remembered place(s).", n));
    }

    public void ToggleLearnMoves()
    {
        St.LearnMoves = !St.LearnMoves; SaveSettingsNow();
        Say(St.LearnMoves ? Tr("The program remembers where you drag mods, and Auto Sort keeps them there.") : Tr("The program no longer remembers where you drag mods. Places already remembered still apply until you forget them."));
    }
}
