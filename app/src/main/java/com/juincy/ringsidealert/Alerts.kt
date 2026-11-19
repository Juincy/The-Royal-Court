package com.juincy.ringsidealert

import android.Manifest
import android.app.AlarmManager
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import java.time.Duration
import java.time.LocalTime
import java.time.ZonedDateTime
import java.time.format.DateTimeFormatter

object Notifs {
    const val ALERTS = "alerts"
    const val UPDATES = "updates"

    fun channels(ctx: Context) {
        val nm = ctx.getSystemService(NotificationManager::class.java)
        nm.createNotificationChannel(NotificationChannel(ALERTS, "Show alerts", NotificationManager.IMPORTANCE_HIGH))
        nm.createNotificationChannel(NotificationChannel(UPDATES, "App updates", NotificationManager.IMPORTANCE_DEFAULT))
    }

    fun openApp(ctx: Context): PendingIntent {
        val i = ctx.packageManager.getLaunchIntentForPackage(ctx.packageName) ?: Intent()
        return PendingIntent.getActivity(ctx, 0, i, PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
    }

    fun post(ctx: Context, id: Int, channel: String, title: String, text: String, pi: PendingIntent? = null) {
        if (Build.VERSION.SDK_INT >= 33 &&
            ctx.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED
        ) return
        val b = Notification.Builder(ctx, channel)
            .setSmallIcon(R.drawable.ic_notif)
            .setContentTitle(title)
            .setContentText(text)
            .setAutoCancel(true)
        b.setContentIntent(pi ?: openApp(ctx))
        ctx.getSystemService(NotificationManager::class.java).notify(id, b.build())
    }
}

class AlertReceiver : BroadcastReceiver() {
    override fun onReceive(ctx: Context, i: Intent) {
        Notifs.post(
            ctx, i.getIntExtra("id", 1), Notifs.ALERTS,
            i.getStringExtra("title") ?: "Show tonight", i.getStringExtra("text") ?: "",
        )
    }
}

object Alerts {
    private fun pi(ctx: Context, code: Int, flags: Int, title: String = "", text: String = ""): PendingIntent? {
        val i = Intent(ctx, AlertReceiver::class.java).putExtra("id", code).putExtra("title", title).putExtra("text", text)
        return PendingIntent.getBroadcast(ctx, code, i, flags or PendingIntent.FLAG_IMMUTABLE)
    }

    fun test(ctx: Context, delaySec: Int) {
        val text = "This is a test alert. Alerts are working."
        if (delaySec <= 0) {
            Notifs.post(ctx, 99, Notifs.ALERTS, "Ringside Alert", text)
            return
        }
        val am = ctx.getSystemService(AlarmManager::class.java)
        val intent = pi(ctx, 99, PendingIntent.FLAG_UPDATE_CURRENT, "Ringside Alert", text) ?: return
        am.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, System.currentTimeMillis() + delaySec * 1000L, intent)
    }

    /** Reads everything from SharedPreferences and the cached schedule, so workers can call it too. */
    fun reschedule(ctx: Context) {
        val p = ctx.getSharedPreferences("ra", 0)
        val am = ctx.getSystemService(AlarmManager::class.java)
        for (c in p.getStringSet("codes", emptySet())!!) {
            pi(ctx, c.toInt(), PendingIntent.FLAG_NO_CREATE)?.let { am.cancel(it) }
        }
        val alerts = p.getStringSet("alerts", emptySet())!!
        val off = p.getStringSet("off", emptySet())!!
        val leads = p.getStringSet("leads", null) ?: setOf(p.getString("lead", "30 min before")!!)
        val now = ZonedDateTime.now()
        val horizon = now.plusDays(14)
        val fmt = DateTimeFormatter.ofPattern("h:mm a")
        val codes = HashSet<String>()
        for (s in Repo.cached(ctx)) {
            if (s.id !in alerts || s.company in off || s.start.isAfter(horizon)) continue
            for (lead in leads) {
                val at = when (lead) {
                    "At start" -> s.start
                    "15 min before" -> s.start.minus(Duration.ofMinutes(15))
                    "30 min before" -> s.start.minus(Duration.ofMinutes(30))
                    "1 hour before" -> s.start.minus(Duration.ofHours(1))
                    "2 hours before" -> s.start.minus(Duration.ofHours(2))
                    "1 day before" -> s.start.minus(Duration.ofDays(1))
                    else -> s.start.toLocalDate().atTime(LocalTime.of(9, 0)).atZone(s.start.zone)
                }
                if (!at.isAfter(now)) continue
                val code = (s.id + s.start.toInstant() + lead).hashCode()
                val text = "${s.name} starts at ${s.start.format(fmt)} on ${s.network}"
                val intent = pi(ctx, code, PendingIntent.FLAG_UPDATE_CURRENT, "Ringside Alert", text) ?: continue
                try {
                    am.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, at.toInstant().toEpochMilli(), intent)
                    codes.add(code.toString())
                } catch (e: Exception) {
                }
            }
        }
        p.edit().putStringSet("codes", codes).apply()
    }
}
