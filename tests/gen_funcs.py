import random, sys, json, base64
seed=int(sys.argv[1]); R=random.Random(seed)
def esc(s): return s.replace('\\','\\\\').replace('\n','\\n').replace('\r','\\r').replace('\t','\\t')
def line(*a): print('\t'.join(esc(x) for x in a))
W=["name","version","supported_version","path","tags","dependencies","replace_path","remote_file_id","picture","a","B"," ","\"","{","}","=","#","\n","\r\n","\t","x y","1.2.3","1.*","v1.14","\"Mod A\"","\"B\"","1.14.2","*","ugc_123.mod","Ünï","\ufeff"]
def rw(n): return ''.join(R.choice(W)+R.choice(['',' ','=','\n','']) for _ in range(n))
def rver(): return R.choice(["1.14.2","1.14.*","1.*","v1.14","1.14.2.1","","*","1.x","0.9","1.15.0","2","1.14.2 (abc)","1.14","1.9.10","1.10.2"])
def rname():
    return ''.join(R.choice(["The ","AGOT","[x]","  ","Mod","UI","+","-","_","'","3.0","v2"," "," 2","(",")","&","!","Plus","Lite","Ab"]) for _ in range(R.randint(0,6)))
for i in range(60):
    line("desc", rw(R.randint(0,14)))
    line("desc", 'name="%s"\nversion="%s"\nsupported_version="%s"\ntags={ "A" "B" }\ndependencies={ "%s" }\nreplace_path="common/x"\nremote_file_id="%d"\n'%(rname(),rver(),rver(),rname(),R.randint(0,99999999)))
    line("ver", rver(), rver())
    line("gv", json.dumps({"rawVersion":rver()}) if R.random()<.7 else rw(5))
    line("nat", rname(), rname())
    line("norm", rname())
    line("nameis", rname().lower(), rname().lower())
    line("trim", rname())
    line("sani", rname()+R.choice(['','.','..',':','CON','a/b','x\\y','?*']))
    line("lower", rname())
    line("steamid", R.choice(["ugc_123.mod","mod/ugc_9.mod","local_x.mod","ugc_.mod","ugc_12a.mod","ugc_%d.mod"%R.randint(1,10**14),"x.mod",""]))
    line("cmpver", R.choice(["0.21.4","v0.21.4","0.21.10","1.0","0.9","","abc","1.2.3.4"]), R.choice(["0.21.4","v0.21.5","0.21.10","1.0","0.9","","1.2.3"]))
    line("tr","{0} of {1} mods","a","b"); line("tr","{x} {0}{","q","r")
    line("sha", rname())
# script key scanner
SC=["key = {","\n","}","# c","a.1 = { b = 2 }","@var = 3","namespace = x","x.0001 = {","\"q\"","=",":=","scripted_trigger","\t","effect = {","}","a_b_c","1","0.5","\"str # not comment\"","?=","<=",">="," ","\r\n"]
for i in range(60):
    t=''.join(R.choice(SC)+R.choice([' ','','\n']) for _ in range(R.randint(0,30)))
    line("keys", str(R.randint(0,6)), t)
    line("loc", ''.join(R.choice([" a_b:0 \"x\"\n"," k:1 \"y # z\"\n","l_english:\n","#c\n"," bad\n"," q:\"v\"\n","\ufeff"," a:0 \"\\\"q\\\"\"\n"]) for _ in range(R.randint(0,8))))
for rel in ["common/traits/a.txt","events/x.txt","gui/y.gui","localization/english/a_l_english.yml","localization/english/replace/a_l_english.yml","common/on_action/z.txt","common/scripted_effects/s.txt","gfx/a.dds","history/titles/t.txt","common/defines/a.txt","common/script_values/q.txt","events/dlc/x.txt","common/traits/sub/a.txt","map_data/provinces.png","common/bookmarks/bookmarks/a.txt","common/localization","localization/russian/a_l_russian.yml","README.txt","common/a.txt"]:
    line("fk", rel)
# json
JW=['{','}','[',']',':',',','"a"','"b\\n"','1','-2.5','1e3','true','false','null','"\\u00e9"','0.1','"x y"',' ','\n','1E+2','"\\ud83d\\ude00"','12345678901234567890','"\\q"','-','.5','01']
for i in range(150):
    line("json", ''.join(R.choice(JW) for _ in range(R.randint(0,14))))
line("json",'{"a":[1,2,{"b":null}],"c":"d","e":1.5e-3}'); line("json",'{"a":{"b":{"c":[]}},"z":{}}'); line("json",'[1,2,3]')
# csv
for i in range(60):
    hdr=R.choice(["Name,Count,Sum,Own,Share","name;count;time","id,Time,Count",""])
    rows=''.join("%s,%s,%s,%s\n"%(R.choice(["ev.1","on_x","a.b.0001","","x y"]),R.choice(["1","12","x","-3",""]),R.choice(["0.5","1.25e2","abc","","-1","nan","inf"]),R.choice(["0","3.2",""])) for _ in range(R.randint(0,6)))
    t=hdr+"\n"+rows
    line("csvev",t); line("csvoa",t)
    line("year",''.join("%s%s,%s,%s\n"%(R.choice(["","#",""]),R.choice(["867","1066","abc","0","-5","1200"]),R.choice(["a","b","Gold","x y"]),R.choice(["1","2.5","zz",""])) for _ in range(R.randint(0,6))))
# game log
LOGL=["[12:00:01][E][jomini_script_system.cpp:123]: Script system error!\n    Error: unexpected token\n    Script location: common/traits/a.txt: 12\n",
 "[12:00:01][W][x.cpp:1]: warn text\n","[12:00:02][E][pdx_gui.cpp:5]: bad gui in gui/a.gui:44\n","random line\n","[E][nobrackets]\n","[12:00:03][E][mod.cpp:1]: Mod/Some Mod/events/x.txt line 4\n","\r\n","[12:00:04][I][x]: info\n",
 "[12:00:05][E][x.cpp:1]: Failed to open file mod/ugc_1/gfx/x.dds\n"]
for i in range(60):
    line("log", ''.join(R.choice(LOGL) for _ in range(R.randint(0,10))))
    line("loaded", R.choice(['{"mods":[{"name":"A","id":"ugc_1.mod","enabled":true}]}','[{"displayName":"X","gameRegistryId":"mod/ugc_2.mod"}]','Loaded mod: A (ugc_1)\n','', 'garbage']))
# crash
CL=["Exception: Access violation\n","ExceptionCode: C0000005\n","0x00007FF6 in ck3.exe\n","Version: 1.14.2\n","OS: Windows 11\n","Stack:\n"," at foo\n","Reason: Out of memory\n","GPU: NVIDIA\n","Assertion failed: x\n","\n"]
for i in range(60):
    t=''.join(R.choice(CL) for _ in range(R.randint(0,10)))
    line("cex",t); line("cmeta",t); line("ckind",t); line("tail",t,str(R.randint(0,5)))
    line("cname",R.choice(["crash_2026.10.22_14.30.05","crash_2026-10-22_14-30-05","crash_x","","2026.10.22_14.30.05","crash_2026.13.45_99.99.99"]))
# vdf, regdesc
for i in range(40):
    v=''.join(R.choice(['"path"\t\t"C:\\\\Games"\n','"0"\n{\n','}\n','"x" "y"\n','// c\n','"path" "D:/Steam"\n','{','"a\\"b" "c"\n']) for _ in range(R.randint(0,8)))
    line("vdf",v,"path")
    line("regdesc",'name="%s"\nversion="1"\npath="C:/x"\nremote_file_id="5"\n'%rname() if R.random()<.5 else rw(6),str(R.randint(1,99999)),R.choice(["C:/Steam/workshop/content/1158310/12","D:\\w\\x",""]))
# release
for i in range(40):
    d={"tag_name":R.choice(["v0.22.0","0.21.4","","x"]),"name":rname(),"body":R.choice(["notes","a\nb",""]),"html_url":"https://github.com/x","assets":[{"name":R.choice(["TheRoyalCourt.exe","SHA256SUMS.txt","z"]),"browser_download_url":"https://github.com/Juincy/x/releases/download/v/a.exe","size":R.randint(0,10**9)} for _ in range(R.randint(0,3))]}
    if R.random()<.15: d.pop("tag_name")
    line("rel",json.dumps(d) if R.random()<.9 else rw(4))
    h=''.join(R.choice("0123456789abcdef") for _ in range(64))
    line("sum","%s  TheRoyalCourt.exe\n%s *SHA256SUMS.txt\n%s  other\n"%(h,h[::-1],h), R.choice(["TheRoyalCourt.exe","other","missing"]))
# share
def vi(v):
    o=bytearray()
    while v>=0x80: o.append(0x80|(v&0x7f)); v>>=7
    o.append(v); return bytes(o)
def crc(b):
    import zlib; return zlib.crc32(b)&0xffffffff
for i in range(60):
    nm=rname().encode()[:40]; wn=R.random()<.5
    body=bytes([1,1 if wn else 0])+vi(len(nm))+nm
    n=R.randint(0,8); body+=vi(n); 
    for _ in range(n):
        en=R.randint(0,1)
        if R.random()<.7:
            body+=vi((R.randint(1,10**12)<<2)|en)
            if wn: s=rname().encode()[:30]; body+=vi(len(s))+s
        else:
            s=rname().encode()[:30]; body+=vi((len(s)<<2)|2|en)+s
    if R.random()<.2: body=body[:R.randint(0,len(body))]
    full=body+(crc(body)).to_bytes(4,'little')
    if R.random()<.1: full=full[:-1]+bytes([full[-1]^1])
    code="RC1:"+base64.urlsafe_b64encode(full).decode().rstrip('=')
    line("share",R.choice(["",""," see ",""])+code+R.choice([""," tail","\nmore"]))
line("share","nothing"); line("share","RC1:"); line("share","RC1:A")
# snapshots, fingerprints
for i in range(40):
    line("snap",''.join("%s\t%s\t%s\n"%(R.choice(["1700000000","x","0"]),R.choice(["ugc_1.mod","a"]),R.choice(["1.5","zz","-2"])) for _ in range(R.randint(0,5))))
    fp=lambda: ';'.join("%s:%d"%(R.choice(["ugc_1.mod","ugc_2.mod","local_x"]),R.randint(0,3)) for _ in range(R.randint(0,4)))
    line("fpc",fp(),fp())
