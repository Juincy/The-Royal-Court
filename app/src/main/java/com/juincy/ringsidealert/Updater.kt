package com.juincy.ringsidealert

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageInstaller
import android.os.Build
import androidx.work.Constraints
import androidx.work.CoroutineWorker
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import org.json.JSONObject
import java.io.File
import java.security.MessageDigest
import java.util.concurrent.TimeUnit

object Updater {
    private const val API = "https://api.github.com/repos/Juincy/Ringside-Alert/releases/latest"

    data class Release(val version: String, val apkUrl: String, val shaUrl: String?)

    fun current(ctx: Context): String =
        ctx.packageManager.getPackageInfo(ctx.packageName, 0).versionName ?: "0"

    fun latest(): Release? {
        val o = JSONObject(Net.get(API))
        val version = o.getString("tag_name").removePrefix("v")
        val assets = o.getJSONArray("assets")
        var apk: String? = null
        var sha: String? = null
        for (i in 0 until assets.length()) {
            val a = assets.getJSONObject(i)
            val n = a.getString("name")
            if (n.endsWith(".apk")) apk = a.getString("browser_download_url")
            if (n.endsWith(".sha256")) sha = a.getString("browser_download_url")
        }
        return apk?.let { Release(version, it, sha) }
    }

    fun isNewer(a: String, b: String): Boolean {
        val x = a.split(".").map { it.toIntOrNull() ?: 0 }
        val y = b.split(".").map { it.toIntOrNull() ?: 0 }
        for (i in 0 until maxOf(x.size, y.size)) {
            val d = x.getOrElse(i) { 0 } - y.getOrElse(i) { 0 }
            if (d != 0) return d > 0
        }
        return false
    }

    fun download(ctx: Context, r: Release): File {
        val expected = r.shaUrl?.let { Net.get(it).trim().split(" ")[0].lowercase() }
            ?: throw IllegalStateException("release has no checksum file")
        val f = File(ctx.cacheDir, "update.apk")
        val c = Net.open(r.apkUrl)
        try {
            c.inputStream.use { ins -> f.outputStream().use { ins.copyTo(it) } }
        } finally {
            c.disconnect()
        }
        val md = MessageDigest.getInstance("SHA-256")
        f.inputStream().use { ins ->
            val buf = ByteArray(65536)
            while (true) {
                val n = ins.read(buf)
                if (n < 0) break
                md.update(buf, 0, n)
            }
        }
        val actual = md.digest().joinToString("") { "%02x".format(it) }
        if (actual != expected) {
            f.delete()
            throw IllegalStateException("checksum did not match")
        }
        return f
    }

    fun install(ctx: Context, f: File) {
        val pi = ctx.packageManager.packageInstaller
        val params = PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL)
        if (Build.VERSION.SDK_INT >= 31) {
            params.setRequireUserAction(PackageInstaller.SessionParams.USER_ACTION_NOT_REQUIRED)
        }
        val id = pi.createSession(params)
        pi.openSession(id).use { s ->
            f.inputStream().use { ins ->
                s.openWrite("update", 0, f.length()).use { out ->
                    ins.copyTo(out)
                    s.fsync(out)
                }
            }
            val intent = Intent(ctx, InstallReceiver::class.java)
            val pending = PendingIntent.getBroadcast(ctx, id, intent, PendingIntent.FLAG_MUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
            s.commit(pending.intentSender)
        }
    }
}

class InstallReceiver : BroadcastReceiver() {
    override fun onReceive(ctx: Context, i: Intent) {
        val status = i.getIntExtra(PackageInstaller.EXTRA_STATUS, -1)
        if (status == PackageInstaller.STATUS_PENDING_USER_ACTION) {
            val raw: Intent? = if (Build.VERSION.SDK_INT >= 33) {
                i.getParcelableExtra(Intent.EXTRA_INTENT, Intent::class.java)
            } else {
                @Suppress("DEPRECATION") i.getParcelableExtra(Intent.EXTRA_INTENT)
            }
            val confirm = raw ?: return
            confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            if (Store.foreground) {
                ctx.startActivity(confirm)
            } else {
                val pi = PendingIntent.getActivity(ctx, 7, confirm, PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
                Notifs.post(ctx, 7, Notifs.UPDATES, "Ringside Alert update ready", "Tap to finish installing.", pi)
            }
        }
    }
}

class ScheduleWorker(ctx: Context, p: WorkerParameters) : CoroutineWorker(ctx, p) {
    override suspend fun doWork(): Result {
        Repo.refresh(applicationContext)
        Alerts.reschedule(applicationContext)
        return Result.success()
    }
}

class UpdateWorker(ctx: Context, p: WorkerParameters) : CoroutineWorker(ctx, p) {
    override suspend fun doWork(): Result {
        val c = applicationContext
        if (!c.getSharedPreferences("ra", 0).getBoolean("au", true)) return Result.success()
        try {
            val r = Updater.latest() ?: return Result.success()
            if (!Updater.isNewer(r.version, Updater.current(c))) return Result.success()
            if (c.packageManager.canRequestPackageInstalls()) {
                Updater.install(c, Updater.download(c, r))
            } else {
                Notifs.post(c, 8, Notifs.UPDATES, "Update available", "Version ${r.version} is ready. Open Settings to install.")
            }
        } catch (e: Exception) {
            return Result.retry()
        }
        return Result.success()
    }
}

object Work {
    fun start(ctx: Context) {
        val wm = WorkManager.getInstance(ctx)
        val any = Constraints.Builder().setRequiredNetworkType(NetworkType.CONNECTED).build()
        wm.enqueueUniquePeriodicWork(
            "schedule", ExistingPeriodicWorkPolicy.KEEP,
            PeriodicWorkRequestBuilder<ScheduleWorker>(6, TimeUnit.HOURS).setConstraints(any).build(),
        )
        val wifi = ctx.getSharedPreferences("ra", 0).getBoolean("wf", true)
        val net = Constraints.Builder()
            .setRequiredNetworkType(if (wifi) NetworkType.UNMETERED else NetworkType.CONNECTED).build()
        wm.enqueueUniquePeriodicWork(
            "update", ExistingPeriodicWorkPolicy.UPDATE,
            PeriodicWorkRequestBuilder<UpdateWorker>(12, TimeUnit.HOURS).setConstraints(net).build(),
        )
    }
}
