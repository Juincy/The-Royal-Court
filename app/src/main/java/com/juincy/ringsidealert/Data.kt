package com.juincy.ringsidealert

import android.content.Context
import org.json.JSONObject
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.time.ZoneId
import java.time.ZonedDateTime

val COMPANIES = listOf("WWE", "AEW", "TNA", "ROH", "MLW", "NWA", "AAA", "CMLL", "Stardom", "NJPW")
val LEADS = listOf("At start", "15 min before", "30 min before", "1 hour before", "2 hours before", "Morning of the show", "1 day before")

data class Show(
    val id: String,
    val company: String,
    val brand: String,
    val name: String,
    val image: String,
    val network: String,
    val start: ZonedDateTime,
    val event: Boolean = false,
)

object Net {
    fun open(url: String): HttpURLConnection {
        val c = URL(url).openConnection() as HttpURLConnection
        c.connectTimeout = 15000
        c.readTimeout = 60000
        c.instanceFollowRedirects = true
        c.setRequestProperty("User-Agent", "RingsideAlert")
        return c
    }

    fun get(url: String): String {
        val c = open(url)
        try {
            return c.inputStream.bufferedReader().readText()
        } finally {
            c.disconnect()
        }
    }
}

object Repo {
    const val URL = "https://raw.githubusercontent.com/Juincy/Ringside-Alert/main/schedule.json"

    private fun file(ctx: Context) = File(ctx.filesDir, "schedule.json")

    fun parse(text: String): List<Show> {
        val arr = JSONObject(text).getJSONArray("shows")
        val zone = ZoneId.systemDefault()
        val out = ArrayList<Show>()
        for (i in 0 until arr.length()) {
            val o = arr.getJSONObject(i)
            out.add(
                Show(
                    o.getString("id"), o.getString("company"), o.getString("brand"),
                    o.getString("name"), o.optString("image", ""), o.optString("network", "Check listings"),
                    ZonedDateTime.parse(o.getString("startsAt")).withZoneSameInstant(zone),
                    o.optString("type", "weekly") == "event",
                )
            )
        }
        return out.sortedBy { it.start }
    }

    var lastError = ""

    fun refresh(ctx: Context): Boolean = try {
        val text = Net.get(URL)
        if (parse(text).isEmpty()) {
            lastError = "the schedule file is empty. The schedule job on GitHub has not filled it yet."
            false
        } else {
            file(ctx).writeText(text)
            ctx.getSharedPreferences("ra", 0).edit().putLong("last", System.currentTimeMillis()).apply()
            lastError = ""
            true
        }
    } catch (e: java.io.FileNotFoundException) {
        lastError = "the schedule file was not found on GitHub (error 404). Run the Update schedule job."
        false
    } catch (e: Exception) {
        lastError = e.message ?: "network error"
        false
    }

    fun cached(ctx: Context): List<Show> = try {
        parse(file(ctx).readText())
    } catch (e: Exception) {
        emptyList()
    }
}
