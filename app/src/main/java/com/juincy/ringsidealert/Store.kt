package com.juincy.ringsidealert

import android.content.Context
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import java.time.LocalDate

object Store {
    lateinit var app: Context
    var foreground = false

    val shows = mutableStateListOf<Show>()
    val favs = mutableStateListOf<String>()
    val alerts = mutableStateListOf<String>()
    val cos = mutableStateListOf<String>()

    var tab by mutableIntStateOf(0)
    var open by mutableStateOf<String?>(null)
    var company by mutableStateOf("All")
    var brand by mutableStateOf("All")
    var day by mutableStateOf(LocalDate.now())
    val leads = mutableStateListOf<String>()
    var weekOffset by mutableIntStateOf(0)
    var theme by mutableStateOf("system")
    var au by mutableStateOf(true)
    var wf by mutableStateOf(true)
    var upd by mutableStateOf("Not checked yet")
    var rel by mutableStateOf<Updater.Release?>(null)
    var busy by mutableStateOf(false)
    var last by mutableStateOf(0L)
    var err by mutableStateOf("")
    var tick by mutableIntStateOf(0)

    private fun p() = app.getSharedPreferences("ra", 0)

    fun init(c: Context) {
        if (::app.isInitialized) return
        app = c.applicationContext
        val p = p()
        favs.addAll(p.getStringSet("favs", emptySet())!!)
        alerts.addAll(p.getStringSet("alerts", emptySet())!!)
        val off = p.getStringSet("off", emptySet())!!
        cos.addAll(COMPANIES.filter { it !in off })
        leads.addAll(p.getStringSet("leads", null) ?: setOf(p.getString("lead", "30 min before")!!))
        theme = p.getString("theme", theme)!!
        au = p.getBoolean("au", true)
        wf = p.getBoolean("wf", true)
        last = p.getLong("last", 0L)
        shows.addAll(Repo.cached(app))
    }

    fun save() {
        p().edit()
            .putStringSet("favs", favs.toSet())
            .putStringSet("alerts", alerts.toSet())
            .putStringSet("off", COMPANIES.filter { it !in cos }.toSet())
            .putStringSet("leads", leads.toSet())
            .putString("theme", theme)
            .putBoolean("au", au)
            .putBoolean("wf", wf)
            .apply()
        Alerts.reschedule(app)
    }

    fun toggleLead(l: String) {
        if (l in leads) {
            if (leads.size > 1) leads.remove(l)
        } else {
            leads.add(l)
        }
        save()
    }

    fun toggle(list: MutableList<String>, id: String) {
        if (!list.remove(id)) list.add(id)
        save()
    }

    fun reload() {
        val list = Repo.cached(app)
        shows.clear()
        shows.addAll(list)
        last = p().getLong("last", 0L)
        Alerts.reschedule(app)
    }

    fun next(id: String): Show? {
        val now = java.time.ZonedDateTime.now().minusHours(3)
        return shows.filter { it.id == id && it.start.isAfter(now) }.minByOrNull { it.start }
            ?: shows.firstOrNull { it.id == id }
    }
}
