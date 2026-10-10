#!/usr/bin/env python3
"""Workshop ledger: which Workshop pages we have already looked at for load-order rules.
  python3 tools/ledger.py stats            counts per status
  python3 tools/ledger.py todo [N]         pages still to read (unread, blocked, title-only), N at a time
  python3 tools/ledger.py seen ID          has this page been looked at? what did it say?
  python3 tools/ledger.py mark ID STATUS [FOUND] [NOTE...]   record a page (status: read, title-only, blocked, unread, collection)
  python3 tools/ledger.py check            ids in builtinKnownMods() that the ledger does not list
Steam blocks fast readers: read at most one page every ~6 s and stop at the first block."""
import json,re,sys,os,datetime,collections
P=os.path.join(os.path.dirname(os.path.abspath(__file__)),"workshop_ledger.json")
C=os.path.join(os.path.dirname(os.path.abspath(__file__)),"..","src","core.hpp")
L=json.load(open(P,encoding="utf-8")); I=L["items"]; by={e["id"]:e for e in I}
def save(): json.dump(L,open(P,"w",encoding="utf-8"),ensure_ascii=False,indent=1)
a=sys.argv[1:]; cmd=a[0] if a else "stats"
if cmd=="stats":
    for k,v in collections.Counter(e["status"] for e in I).most_common(): print(f"{k:16}{v}")
    print("total", len(I), "| order rules found:", sum(e["found"]=="order" for e in I), "| in the database:", sum(e["inDatabase"] for e in I))
elif cmd=="todo":
    n=int(a[1]) if len(a)>1 else 10
    q=[e for e in I if e["status"] in ("unread","blocked","title-only")]
    q.sort(key=lambda e:{"blocked":0,"unread":1,"title-only":2}[e["status"]])
    for e in q[:n]: print(e["id"],e["status"],e["title"])
    print(len(q),"left")
elif cmd=="seen":
    e=by.get(a[1]); print(json.dumps(e,ensure_ascii=False,indent=1) if e else "never looked at")
elif cmd=="mark":
    i,s=a[1],a[2]; f=a[3] if len(a)>3 else ""; n=" ".join(a[4:])
    e=by.setdefault(i,{"id":i,"title":"","inDatabase":False}); 
    if e not in I: I.append(e)
    e.update(status=s,found=f,note=n,date=datetime.date.today().isoformat(),source="Workshop page"); save(); print("saved",i,s)
elif cmd=="check":
    core=open(C,encoding="utf-8").read(); blk=core[core.find("builtinKnownMods() {"):]
    ids=set(x for m in re.findall(r'k\.ids\s*=\s*\{([^}]*)\}',blk) for x in re.findall(r'\d{9,10}',m))
    miss=sorted(ids-set(by)); print("missing from the ledger:",miss or "none")
