package com.juincy.ringsidealert

import android.content.Intent
import android.graphics.Typeface
import android.net.Uri
import android.provider.Settings
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.GridItemSpan
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.ArrowForward
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.Star
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.compositeOver
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.foundation.isSystemInDarkTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.time.format.TextStyle as DayStyle
import java.util.Locale

val Dsp = FontFamily(Typeface.create("sans-serif-condensed", Typeface.BOLD))
private val Accent = Color(0xFFD92D27)

val FIT = setOf("aaa", "njpw", "tna", "mlw", "roh", "cmll", "nwa", "stardom", "royalrumble", "worldsend", "wrestlepalooza")
fun fitBg(n: String): Color = when (n) {
    "roh", "cmll", "nwa", "stardom", "wrestlepalooza" -> Color.White
    "royalrumble" -> Color(0xFF1B0406)
    "worldsend" -> Color(0xFF131249)
    else -> Color.Black
}

fun watchUrl(net: String): String? {
    val n = net.lowercase()
    return when {
        "netflix" in n -> "https://www.netflix.com"
        "comet" in n -> "https://comettv.com/watch-live/"
        "stardom" in n -> "https://wwr-stardom.com/en/"
        "tubi" in n -> "https://tubitv.com"
        "fox deportes" in n -> "https://www.foxdeportes.com"
        "youtube" in n -> "https://www.youtube.com"
        "honorclub" in n -> "https://www.watchroh.com"
        "amc" in n -> "https://www.amc.com"
        "espn" in n -> "https://www.espn.com"
        "hbo" in n -> "https://www.hbomax.com"
        "tbs" in n -> "https://www.tbs.com"
        "usa network" in n -> "https://www.usanetwork.com"
        "cw" in n -> "https://www.cwtv.com"
        "njpw" in n -> "https://njpwworld.com"
        else -> null
    }
}

fun calendarIntent(s: Show): Intent = Intent(Intent.ACTION_INSERT)
    .setData(android.provider.CalendarContract.Events.CONTENT_URI)
    .putExtra(android.provider.CalendarContract.EXTRA_EVENT_BEGIN_TIME, s.start.toInstant().toEpochMilli())
    .putExtra(
        android.provider.CalendarContract.EXTRA_EVENT_END_TIME,
        s.start.plusHours(if (s.event) 4L else 2L).toInstant().toEpochMilli(),
    )
    .putExtra(android.provider.CalendarContract.Events.TITLE, s.name)
    .putExtra(android.provider.CalendarContract.Events.DESCRIPTION, "Watch on " + s.network)

fun tint(c: String): Color = when (c) {
    "WWE" -> Color(0xFFA32D2D)
    "AEW" -> Color(0xFF185FA5)
    "TNA" -> Color(0xFF534AB7)
    "NJPW" -> Color(0xFF0F6E56)
    "ROH" -> Color(0xFF8A6D1F)
    "MLW" -> Color(0xFF3D5A6C)
    "CMLL" -> Color(0xFF7A1F5A)
    "NWA" -> Color(0xFF6B4E00)
    "Stardom" -> Color(0xFFC2185B)
    else -> Color(0xFF854F0B)
}

@Composable
fun RingsideTheme(content: @Composable () -> Unit) {
    val dark = when (Store.theme) {
        "dark" -> true
        "light" -> false
        else -> isSystemInDarkTheme()
    }
    val cs = if (dark) {
        darkColorScheme(
            primary = Color(0xFFF0463F), background = Color(0xFF0B0C10), surface = Color(0xFF17181E),
            onBackground = Color(0xFFF4F3EF), onSurface = Color(0xFFF4F3EF), onSurfaceVariant = Color(0xFF9A98A0),
            outline = Color(0xFF2A2B33),
        )
    } else {
        lightColorScheme(
            primary = Accent, background = Color(0xFFF5F4F0), surface = Color.White,
            onBackground = Color(0xFF15161A), onSurface = Color(0xFF15161A), onSurfaceVariant = Color(0xFF6B6A70),
            outline = Color(0xFFE3E1DA),
        )
    }
    MaterialTheme(colorScheme = cs, content = content)
}

/** Raised, beveled surface: drop shadow, light-to-dark fill, and a bright top edge. */
@Composable
fun Modifier.bevel(r: Dp = 14.dp): Modifier {
    val s = RoundedCornerShape(r)
    val base = MaterialTheme.colorScheme.surface
    val top = Color.White.copy(alpha = 0.08f).compositeOver(base)
    val bottom = Color.Black.copy(alpha = 0.16f).compositeOver(base)
    return this
        .shadow(6.dp, s)
        .background(Brush.verticalGradient(listOf(top, bottom)), s)
        .border(1.dp, Brush.verticalGradient(listOf(Color.White.copy(alpha = 0.35f), Color.Black.copy(alpha = 0.35f))), s)
}

@Composable
fun Chip(t: String, on: Boolean, onClick: () -> Unit) {
    val cs = MaterialTheme.colorScheme
    Text(
        t, color = if (on) cs.background else cs.onSurface, fontSize = 13.sp,
        modifier = Modifier
            .bevel(20.dp)
            .background(if (on) cs.onSurface else Color.Transparent, RoundedCornerShape(20.dp))
            .clickable(onClick = onClick)
            .padding(horizontal = 14.dp, vertical = 7.dp),
    )
}

@Composable
fun Heading(small: String, big: String) {
    Column(Modifier.padding(start = 18.dp, end = 18.dp, top = 18.dp, bottom = 8.dp)) {
        Text(small, color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 13.sp)
        Text(big, fontFamily = Dsp, fontSize = 34.sp, color = MaterialTheme.colorScheme.onBackground)
    }
}

@Composable
fun Section(t: String) {
    Text(
        t, fontFamily = Dsp, fontSize = 22.sp, color = MaterialTheme.colorScheme.onBackground,
        modifier = Modifier.padding(top = 18.dp, bottom = 8.dp),
    )
}

@Composable
fun ToggleRow(label: String, on: Boolean, onChange: (Boolean) -> Unit) {
    Row(
        Modifier.fillMaxWidth().padding(bottom = 12.dp).bevel(14.dp).padding(horizontal = 14.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(label, Modifier.weight(1f), color = MaterialTheme.colorScheme.onSurface)
        Switch(checked = on, onCheckedChange = onChange)
    }
}

@Composable
fun BellButton(id: String, modifier: Modifier = Modifier) {
    val on = id in Store.alerts
    Box(
        modifier.size(34.dp).clip(CircleShape)
            .background(if (on) Color.White else Color(0x99000000))
            .clickable { Store.toggle(Store.alerts, id) },
        contentAlignment = Alignment.Center,
    ) {
        Icon(Icons.Filled.Notifications, "Toggle alert", Modifier.size(18.dp), tint = if (on) Accent else Color.White)
    }
}

@Composable
fun Poster(s: Show) {
    val ctx = LocalContext.current
    val res = remember(s.image) { ctx.resources.getIdentifier(s.image, "drawable", ctx.packageName) }
    val fit = s.image in FIT
    val shape = RoundedCornerShape(16.dp)
    Box(
        Modifier.fillMaxWidth().aspectRatio(0.75f).shadow(8.dp, shape).clip(shape)
            .background(if (fit) fitBg(s.image) else tint(s.company))
            .border(1.dp, Brush.verticalGradient(listOf(Color.White.copy(alpha = 0.4f), Color.Black.copy(alpha = 0.4f))), shape)
            .clickable { Store.open = s.id },
    ) {
        if (res != 0) {
            Image(painterResource(res), s.name, Modifier.fillMaxSize(), contentScale = if (fit) ContentScale.Fit else ContentScale.Crop)
        } else {
            Text(
                s.name, Modifier.align(Alignment.Center).padding(12.dp), color = Color.White,
                fontFamily = Dsp, fontSize = 30.sp, textAlign = TextAlign.Center,
            )
        }
        Text(
            s.company, color = Color.White, fontSize = 11.sp,
            modifier = Modifier.align(Alignment.TopStart).padding(8.dp)
                .background(Color(0x8C000000), RoundedCornerShape(10.dp)).padding(horizontal = 9.dp, vertical = 3.dp),
        )
        BellButton(s.id, Modifier.align(Alignment.TopEnd).padding(8.dp))
        Column(Modifier.align(Alignment.BottomStart).fillMaxWidth().background(Color(0xB0000000)).padding(10.dp)) {
            Text(s.name, color = Color.White, fontFamily = Dsp, fontSize = 18.sp, maxLines = 2, overflow = TextOverflow.Ellipsis)
            Text(
                whenText(s) + " · " + s.network,
                color = Color(0xFFE6E4DC), fontSize = 12.sp, maxLines = 1, overflow = TextOverflow.Ellipsis,
            )
        }
    }
}

fun whenText(s: Show): String {
    val mins = java.time.Duration.between(java.time.ZonedDateTime.now(), s.start).toMinutes()
    val clock = s.start.format(DateTimeFormatter.ofPattern("h:mm a"))
    return when {
        mins >= -180L && mins <= 0L -> "Live now"
        mins >= 1L && mins <= 360L -> "In " + (mins / 60) + "h " + (mins % 60) + "m · " + clock
        else -> clock
    }
}

private fun matches(s: Show) =
    (Store.company == "All" || s.company == Store.company) && (Store.brand == "All" || s.brand == Store.brand)

@Composable
fun ShowsScreen() {
    var menu by remember { mutableStateOf(false) }
    val ctx = LocalContext.current
    val ver = remember { Updater.current(ctx) }
    val list = Store.shows.filter { it.start.toLocalDate() == Store.day && matches(it) }
    val brands = if (Store.company == "All") emptyList<String>()
    else Store.shows.filter { it.company == Store.company }.map { it.brand }.distinct()
    LazyVerticalGrid(
        columns = GridCells.Fixed(2),
        contentPadding = PaddingValues(start = 18.dp, end = 18.dp, bottom = 24.dp),
        horizontalArrangement = Arrangement.spacedBy(14.dp),
        verticalArrangement = Arrangement.spacedBy(20.dp),
        modifier = Modifier.fillMaxSize(),
    ) {
        item(span = { GridItemSpan(maxLineSpan) }) {
            Column {
                Heading(LocalDate.now().format(DateTimeFormatter.ofPattern("EEEE, MMM d")), "Ringside Alert")
                val rel = Store.rel
                if (rel != null && Updater.isNewer(rel.version, ver)) {
                    Text(
                        "Update " + rel.version + " is available. Tap to install.", color = Color.White, fontSize = 13.sp,
                        modifier = Modifier.fillMaxWidth().padding(bottom = 10.dp).background(Accent, RoundedCornerShape(12.dp))
                            .clickable { Store.tab = 4 }.padding(12.dp),
                    )
                }
                Box(Modifier.padding(bottom = 10.dp)) {
                    Row(
                        Modifier.fillMaxWidth().bevel(24.dp).clickable { menu = true }.padding(horizontal = 16.dp, vertical = 11.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Text(if (Store.company == "All") "All companies" else Store.company, Modifier.weight(1f), color = MaterialTheme.colorScheme.onSurface)
                        Icon(Icons.Filled.KeyboardArrowDown, null, tint = MaterialTheme.colorScheme.onSurface)
                    }
                    DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
                        (listOf("All") + COMPANIES).forEach { c ->
                            DropdownMenuItem(
                                text = { Text(if (c == "All") "All companies" else c) },
                                onClick = { Store.company = c; Store.brand = "All"; menu = false },
                            )
                        }
                    }
                }
                if (brands.size > 1) {
                    Row(Modifier.horizontalScroll(rememberScrollState()).padding(bottom = 12.dp), horizontalArrangement = Arrangement.spacedBy(9.dp)) {
                        Chip("All", Store.brand == "All") { Store.brand = "All" }
                        brands.forEach { b -> Chip(b, Store.brand == b) { Store.brand = b } }
                    }
                }
                val weekStart = LocalDate.now().plusDays(7L * Store.weekOffset)
                Row(Modifier.fillMaxWidth().padding(bottom = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                    Icon(
                        Icons.Filled.ArrowBack, "Previous week",
                        Modifier.size(40.dp).clip(CircleShape).clickable(enabled = Store.weekOffset > 0) {
                            Store.weekOffset -= 1
                            Store.day = LocalDate.now().plusDays(7L * Store.weekOffset)
                        }.padding(8.dp),
                        tint = if (Store.weekOffset > 0) MaterialTheme.colorScheme.onSurface
                        else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.3f),
                    )
                    Text(
                        (if (Store.weekOffset == 0) "This week · " else "") +
                            weekStart.format(DateTimeFormatter.ofPattern("MMM d")) + " – " +
                            weekStart.plusDays(6).format(DateTimeFormatter.ofPattern("MMM d")),
                        Modifier.weight(1f), textAlign = TextAlign.Center, color = MaterialTheme.colorScheme.onSurface,
                    )
                    Icon(
                        Icons.Filled.ArrowForward, "Next week",
                        Modifier.size(40.dp).clip(CircleShape).clickable(enabled = Store.weekOffset < 5) {
                            Store.weekOffset += 1
                            Store.day = LocalDate.now().plusDays(7L * Store.weekOffset)
                        }.padding(8.dp),
                        tint = if (Store.weekOffset < 5) MaterialTheme.colorScheme.onSurface
                        else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.3f),
                    )
                }
                Row(Modifier.fillMaxWidth().padding(bottom = 6.dp)) {
                    for (i in 0..6) {
                        val d = weekStart.plusDays(i.toLong())
                        val on = d == Store.day
                        val has = Store.shows.any { it.start.toLocalDate() == d && matches(it) }
                        Column(
                            Modifier.weight(1f).padding(2.dp).clip(RoundedCornerShape(14.dp))
                                .background(if (on) Accent else Color.Transparent)
                                .clickable { Store.day = d }.padding(vertical = 8.dp),
                            horizontalAlignment = Alignment.CenterHorizontally,
                        ) {
                            val fg = if (on) Color.White else MaterialTheme.colorScheme.onSurfaceVariant
                            Text(d.dayOfWeek.getDisplayName(DayStyle.SHORT, Locale.getDefault()), color = fg, fontSize = 12.sp)
                            Text(d.dayOfMonth.toString(), color = if (on) Color.White else MaterialTheme.colorScheme.onBackground, fontFamily = Dsp, fontSize = 20.sp)
                            Box(Modifier.size(5.dp).clip(CircleShape).background(if (has) fg else Color.Transparent))
                        }
                    }
                }
            }
        }
        if (list.isEmpty()) {
            item(span = { GridItemSpan(maxLineSpan) }) {
                Text(
                    if (Store.shows.isNotEmpty()) "No shows on this day."
                    else if (Store.err.isEmpty()) "Loading the schedule…"
                    else "Could not load the schedule: " + Store.err,
                    Modifier.fillMaxWidth().padding(vertical = 48.dp), textAlign = TextAlign.Center,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        items(list, key = { it.id + it.start.toInstant() }) { Poster(it) }
    }
}

@Composable
fun DetailScreen(id: String) {
    val s = Store.next(id)
    if (s == null) { Store.open = null; return }
    val ctx = LocalContext.current
    val res = remember(s.image) { ctx.resources.getIdentifier(s.image, "drawable", ctx.packageName) }
    val fit = s.image in FIT
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
        Box(Modifier.fillMaxWidth().height(340.dp).background(if (fit) fitBg(s.image) else tint(s.company))) {
            if (res != 0) Image(painterResource(res), s.name, Modifier.fillMaxSize(), contentScale = if (fit) ContentScale.Fit else ContentScale.Crop)
            else Text(s.name, Modifier.align(Alignment.Center).padding(24.dp), color = Color.White, fontFamily = Dsp, fontSize = 40.sp, textAlign = TextAlign.Center)
            Box(
                Modifier.align(Alignment.TopStart).padding(16.dp).size(34.dp).clip(CircleShape).background(Color(0x99000000)).clickable { Store.open = null },
                contentAlignment = Alignment.Center,
            ) { Icon(Icons.Filled.ArrowBack, "Back", Modifier.size(18.dp), tint = Color.White) }
            val fav = id in Store.favs
            Box(
                Modifier.align(Alignment.TopEnd).padding(16.dp).size(34.dp).clip(CircleShape)
                    .background(if (fav) Color.White else Color(0x99000000)).clickable { Store.toggle(Store.favs, id) },
                contentAlignment = Alignment.Center,
            ) { Icon(Icons.Filled.Star, "Favorite", Modifier.size(18.dp), tint = if (fav) Accent else Color.White) }
        }
        Column(Modifier.padding(18.dp)) {
            Text(s.company, color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 13.sp)
            Text(s.name, fontFamily = Dsp, fontSize = 34.sp, color = MaterialTheme.colorScheme.onBackground)
            Text(
                s.start.format(DateTimeFormatter.ofPattern("EEEE, MMM d · h:mm a")),
                color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(top = 4.dp, bottom = 16.dp),
            )
            val url = watchUrl(s.network)
            Column(
                Modifier.fillMaxWidth().bevel(14.dp)
                    .clickable(enabled = url != null) {
                        url?.let { u -> runCatching { ctx.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(u))) } }
                    }
                    .padding(14.dp),
            ) {
                Text(
                    if (url != null) "Where to watch · tap to open" else "Where to watch",
                    color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp,
                )
                Text(s.network, fontFamily = Dsp, fontSize = 24.sp, color = MaterialTheme.colorScheme.onSurface)
            }
            Spacer(Modifier.height(18.dp))
            val on = id in Store.alerts
            Text(
                if (on) "Alert on · " + LEADS.filter { it in Store.leads }.joinToString(", ") else "Turn on alert",
                Modifier.fillMaxWidth().shadow(6.dp, RoundedCornerShape(14.dp)).clip(RoundedCornerShape(14.dp))
                    .background(Brush.verticalGradient(if (on) listOf(Color(0xFF3A3B44), Color(0xFF1E1F26)) else listOf(Color(0xFFFF625A), Color(0xFFB5231D))))
                    .clickable { Store.toggle(Store.alerts, id) }.padding(vertical = 14.dp),
                color = Color.White, fontFamily = Dsp, fontSize = 20.sp, textAlign = TextAlign.Center,
            )
            Spacer(Modifier.height(14.dp))
            Text(
                "Add to calendar",
                Modifier.fillMaxWidth().bevel(14.dp).clickable { runCatching { ctx.startActivity(calendarIntent(s)) } }
                    .padding(vertical = 14.dp),
                color = MaterialTheme.colorScheme.onSurface, fontFamily = Dsp, fontSize = 20.sp, textAlign = TextAlign.Center,
            )
        }
    }
}

@Composable
fun ShowRow(s: Show) {
    val ctx = LocalContext.current
    val res = remember(s.image) { ctx.resources.getIdentifier(s.image, "drawable", ctx.packageName) }
    Row(
        Modifier.fillMaxWidth().padding(bottom = 12.dp).bevel(14.dp).clickable { Store.open = s.id }.padding(10.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.size(44.dp, 58.dp).clip(RoundedCornerShape(8.dp)).background(tint(s.company))) {
            if (res != 0) Image(painterResource(res), null, Modifier.fillMaxSize(), contentScale = ContentScale.Crop)
        }
        Column(Modifier.padding(start = 12.dp)) {
            Text(s.name, fontFamily = Dsp, fontSize = 20.sp, color = MaterialTheme.colorScheme.onSurface)
            Text(
                s.start.format(DateTimeFormatter.ofPattern(if (s.event) "MMM d · h:mm a" else "EEE h:mm a")) + " · " + s.network,
                color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp,
            )
        }
    }
}

@Composable
fun EventRow(s: Show) {
    Row(
        Modifier.fillMaxWidth().padding(bottom = 12.dp).bevel(14.dp).clickable { Store.open = s.id }.padding(14.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            Text(s.company, color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp)
            Text(s.name, fontFamily = Dsp, fontSize = 22.sp, color = MaterialTheme.colorScheme.onSurface)
            Text(
                s.start.format(DateTimeFormatter.ofPattern("EEE, MMM d · h:mm a")),
                color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 13.sp,
            )
            Text(s.network, color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 13.sp)
        }
        BellButton(s.id)
    }
}

@Composable
fun EventsScreen() {
    val cutoff = java.time.ZonedDateTime.now().minusHours(3)
    val list = Store.shows.filter { it.event && it.start.isAfter(cutoff) }.sortedBy { it.start }
    LazyColumn(Modifier.fillMaxSize(), contentPadding = PaddingValues(horizontal = 18.dp)) {
        item { Heading("Pay-per-view and live events", "Events") }
        if (list.isEmpty()) item {
            Text("No upcoming events loaded yet.", color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(vertical = 24.dp))
        }
        items(list, key = { it.id }) { EventRow(it) }
    }
}

@Composable
fun AlertsScreen() {
    LazyColumn(Modifier.fillMaxSize(), contentPadding = PaddingValues(horizontal = 18.dp, vertical = 0.dp)) {
        item { Heading("Settings", "Alerts") }
        item {
            Section("Notify me")
            Text(
                "Pick more than one time to get several alerts for each show.",
                color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 13.sp, modifier = Modifier.padding(bottom = 8.dp),
            )
            Row(Modifier.horizontalScroll(rememberScrollState()).padding(bottom = 6.dp), horizontalArrangement = Arrangement.spacedBy(9.dp)) {
                LEADS.forEach { l -> Chip(l, l in Store.leads) { Store.toggleLead(l) } }
            }
            Section("Companies")
        }
        items(COMPANIES) { c -> ToggleRow(c, c in Store.cos) { Store.toggle(Store.cos, c) } }
        item { Section("Your alerts") }
        val mine = Store.alerts.mapNotNull { Store.next(it) }
        if (mine.isEmpty()) item {
            Text("No alerts yet. Tap the bell on any show.", color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(vertical = 12.dp))
        }
        items(mine) { ShowRow(it) }
    }
}

@Composable
fun FavoritesScreen() {
    LazyColumn(Modifier.fillMaxSize(), contentPadding = PaddingValues(horizontal = 18.dp)) {
        item { Heading("Saved", "Favorites") }
        val mine = Store.favs.mapNotNull { Store.next(it) }
        if (mine.isEmpty()) item {
            Text("Star a show on its details page to keep it here.", color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(vertical = 24.dp))
        }
        items(mine) { ShowRow(it) }
    }
}

@Composable
fun SettingsScreen() {
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    val cs = MaterialTheme.colorScheme
    val version = remember { Updater.current(ctx) }
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(horizontal = 18.dp)) {
        Heading("Preferences", "Settings")
        Section("Appearance")
        Row(horizontalArrangement = Arrangement.spacedBy(9.dp), modifier = Modifier.padding(bottom = 6.dp)) {
            listOf("system" to "System", "light" to "Light", "dark" to "Dark").forEach { (k, l) ->
                Chip(l, Store.theme == k) { Store.theme = k; Store.save() }
            }
        }
        Section("Schedule")
        Column(Modifier.fillMaxWidth().padding(bottom = 12.dp).bevel(14.dp).padding(14.dp)) {
            val lastText = if (Store.last == 0L) "never" else
                DateTimeFormatter.ofPattern("MMM d, h:mm a").format(Instant.ofEpochMilli(Store.last).atZone(ZoneId.systemDefault()))
            Text(Store.shows.size.toString() + " upcoming airings loaded", color = cs.onSurface)
            if (Store.err.isNotEmpty()) {
                Text("Problem: " + Store.err, color = Accent, fontSize = 13.sp, modifier = Modifier.padding(bottom = 4.dp))
            }
            Text("Last updated: " + lastText, color = cs.onSurfaceVariant, fontSize = 13.sp, modifier = Modifier.padding(top = 2.dp, bottom = 10.dp))
            Chip(if (Store.busy) "Working…" else "Refresh now", false) {
                if (Store.busy) return@Chip
                scope.launch {
                    Store.busy = true
                    val ok = withContext(Dispatchers.IO) { Repo.refresh(ctx) }
                    Store.err = if (ok) "" else Repo.lastError
                    Store.reload()
                    Store.busy = false
                }
            }
        }
        Section("Alert times")
        Text(
            "Pick more than one to get several alerts for each show.",
            color = cs.onSurfaceVariant, fontSize = 13.sp, modifier = Modifier.padding(bottom = 8.dp),
        )
        Row(Modifier.horizontalScroll(rememberScrollState()).padding(bottom = 6.dp), horizontalArrangement = Arrangement.spacedBy(9.dp)) {
            LEADS.forEach { l -> Chip(l, l in Store.leads) { Store.toggleLead(l) } }
        }
        Section("Testing alerts")
        Column(Modifier.fillMaxWidth().padding(bottom = 12.dp).bevel(14.dp).padding(14.dp)) {
            val tick = Store.tick
            val notifOn = remember(tick) { ctx.getSystemService(android.app.NotificationManager::class.java).areNotificationsEnabled() }
            Text(
                if (notifOn) "Notifications are on." else "Notifications are off for Ringside Alert, so alerts will not show.",
                color = cs.onSurface, modifier = Modifier.padding(bottom = 10.dp),
            )
            Row(horizontalArrangement = Arrangement.spacedBy(9.dp)) {
                Chip("Send test alert", false) { Alerts.test(ctx, 0) }
                Chip("Test in 1 minute", false) { Alerts.test(ctx, 60) }
            }
            if (!notifOn) {
                Spacer(Modifier.height(10.dp))
                Chip("Open notification settings", true) {
                    ctx.startActivity(
                        Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).putExtra(Settings.EXTRA_APP_PACKAGE, ctx.packageName)
                            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    )
                }
            }
        }
        Section("Updates")
        Column(Modifier.fillMaxWidth().padding(bottom = 12.dp).bevel(14.dp).padding(14.dp)) {
            Text("Version $version", fontFamily = Dsp, fontSize = 20.sp, color = cs.onSurface)
            Text(Store.upd, color = cs.onSurfaceVariant, fontSize = 13.sp, modifier = Modifier.padding(top = 2.dp, bottom = 10.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(9.dp)) {
                Chip(if (Store.busy) "Working…" else "Check for updates", false) {
                    if (Store.busy) return@Chip
                    scope.launch {
                        Store.busy = true
                        Store.upd = "Checking…"
                        val r = withContext(Dispatchers.IO) { runCatching { Updater.latest() }.getOrNull() }
                        Store.rel = r
                        Store.upd = when {
                            r == null -> "Could not check for updates."
                            Updater.isNewer(r.version, version) -> "Version ${r.version} is available."
                            else -> "You have the latest version."
                        }
                        Store.busy = false
                    }
                }
                val r = Store.rel
                if (r != null && Updater.isNewer(r.version, version)) {
                    Chip("Download and install", true) {
                        if (Store.busy) return@Chip
                        if (!ctx.packageManager.canRequestPackageInstalls()) {
                            ctx.startActivity(Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:" + ctx.packageName)))
                            Store.upd = "Allow installs from Ringside Alert, then tap again."
                            return@Chip
                        }
                        scope.launch {
                            Store.busy = true
                            Store.upd = "Downloading…"
                            val err = withContext(Dispatchers.IO) { runCatching { Updater.install(ctx, Updater.download(ctx, r)) }.exceptionOrNull() }
                            Store.upd = if (err == null) "Installing…" else "Update failed: " + (err.message ?: "unknown error")
                            Store.busy = false
                        }
                    }
                }
            }
        }
        ToggleRow("Auto-update", Store.au) { Store.au = it; Store.save(); Work.start(ctx) }
        ToggleRow("Update on Wi-Fi only", Store.wf) { Store.wf = it; Store.save(); Work.start(ctx) }
        Section("About")
        Text(
            "Show times come from TVmaze (CC BY-SA). Always check the network for last-minute changes.",
            color = cs.onSurfaceVariant, fontSize = 13.sp, modifier = Modifier.padding(bottom = 32.dp),
        )
    }
}
