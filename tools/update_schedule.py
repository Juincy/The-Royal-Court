"""Builds schedule.json for the app.

Shows with a "weekly" slot in tools/shows.json are generated directly from that slot for the next 14 days
(regular weekly shows never need entering by hand). Shows without one are looked up on TVmaze
(https://www.tvmaze.com/api, CC BY-SA) and skipped if TVmaze has no upcoming airings.
Edit tools/shows.json to change a time, a channel, or to add a promotion.
"""
import json
import sys
import urllib.parse
import urllib.request
from datetime import datetime, time, timedelta, timezone
from zoneinfo import ZoneInfo

API = "https://api.tvmaze.com"
DAYS = 42


def get(path):
    req = urllib.request.Request(API + path, headers={"User-Agent": "RingsideAlert-schedule/1.0"})
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            return json.load(r)
    except Exception as e:
        print("request failed:", path, e)
        return None


def weekly(w, now, end):
    tz = ZoneInfo(w["tz"])
    hh, mm = (int(x) for x in w["time"].split(":")[:2])
    out = []
    for i in range(DAYS + 2):
        d = (now.astimezone(tz) + timedelta(days=i)).date()
        if d.strftime("%A") == w["day"]:
            t = datetime.combine(d, time(hh, mm), tzinfo=tz)
            if now - timedelta(hours=3) <= t <= end:
                out.append(t)
    return out


def from_tvmaze(c, now, end):
    show = get("/singlesearch/shows?q=" + urllib.parse.quote(c["query"]))
    if not show or c["must"].lower() not in show["name"].lower():
        print("skipped %s: TVmaze match was %r" % (c["id"], show and show["name"]))
        return [], c["watch"]
    net = show.get("webChannel") or show.get("network") or {}
    watch = c["watch"] if c["watch"] != "Check listings" else (net.get("name") or c["watch"])
    starts = []
    for e in get("/shows/%d/episodes" % show["id"]) or []:
        if e.get("airstamp"):
            t = datetime.fromisoformat(e["airstamp"].replace("Z", "+00:00"))
            if now - timedelta(hours=3) <= t <= end:
                starts.append(t)
    return starts, watch


def events(now):
    try:
        cfg = json.load(open("tools/events.json"))
    except Exception:
        return []
    out = []
    for e in cfg:
        try:
            t = datetime.fromisoformat(e["startsAt"])
            for key in ("id", "company", "name", "watch"):
                e[key]
        except Exception as err:
            print("skipped a bad entry in tools/events.json:", e, err)
            continue
        if now - timedelta(hours=3) <= t <= now + timedelta(days=400):
            out.append({
                "id": e["id"], "company": e["company"], "brand": "PPV", "name": e["name"],
                "image": e.get("image", ""), "network": e["watch"], "startsAt": t.isoformat(), "type": "event",
            })
    return out


def main():
    cfg = json.load(open("tools/shows.json"))
    now = datetime.now(timezone.utc)
    end = now + timedelta(days=DAYS)
    rows = []
    for c in cfg:
        if "weekly" in c:
            starts, watch = weekly(c["weekly"], now, end), c["watch"]
        else:
            starts, watch = from_tvmaze(c, now, end)
        print("%s: %d airings (%s)" % (c["id"], len(starts), watch))
        for t in sorted(set(starts)):
            rows.append({
                "id": c["id"], "company": c["company"], "brand": c["brand"], "name": c["name"],
                "image": c["image"], "network": watch, "startsAt": t.isoformat(),
            })
    rows += events(now)
    if not rows:
        print("no airings found; leaving schedule.json unchanged")
        return 1
    try:
        old = json.load(open("schedule.json")).get("shows")
    except Exception:
        old = None
    if old != rows:
        with open("schedule.json", "w") as f:
            json.dump({"updated": now.isoformat(timespec="seconds"), "shows": rows}, f, indent=1)
        print("wrote %d airings" % len(rows))
    else:
        print("schedule unchanged")
    return 0


if __name__ == "__main__":
    sys.exit(main())
