#!/usr/bin/env python3
"""Generates a random mod-folder scenario for the differential test: python3 gen_scen.py SEED OUTDIR"""
import os, random, re, sys, json, shutil

seed = int(sys.argv[1]); out = sys.argv[2]
R = random.Random(seed)
body = open('/tmp/known_body.cs', encoding='utf-8').read() if os.path.exists('/tmp/known_body.cs') else ''
known_names = re.findall(r'k\.Names = new\(\) \{([^}]*)\}', body)
KN = []
for g in known_names:
    for n in re.findall(r'"((?:[^"\\]|\\.)*)"', g):
        KN.append(n.encode().decode('unicode_escape') if '\\u' in n else n)
known_ids = re.findall(r'k\.Ids = new\(\) \{"(\d+)"', body)
WORDS = "dragon realm blood crown iron legacy saga ember frost holy empire guild feast tales lore stone river king queen duel plague trade road war peace mirror ember silver night dawn".split()
shutil.rmtree(out, ignore_errors=True)
os.makedirs(out + '/ck3/mod'); os.makedirs(out + '/ck3/content')

def decorate(n):
    r = R.random()
    if r < 0.08: n += ' ' + R.choice(['1.2', 'v3.1', '2.0.1'])
    elif r < 0.14: n += ' [' + R.choice(['1.14', 'Beta', 'Updated']) + ']'
    elif r < 0.2: n = n.upper()
    elif r < 0.26: n += R.choice([' Compatch', ' Patch', ' Compatibility Patch', ' Submod', ' Addon'])
    elif r < 0.3: n = n.replace(' and ', ' & ')
    elif r < 0.33: n = n.replace(' plus', ' +')
    return n

nmods = R.randint(2, 40)
if R.random() < 0.08: nmods = R.randint(60, 120)
mods = []
used_names = set()
for i in range(nmods):
    r = R.random()
    if r < 0.45 and KN:
        name = decorate(R.choice(KN))
    elif r < 0.55 and mods:
        base = R.choice(mods)['name']
        name = decorate(base) if R.random() < 0.6 else (base + ' ' + R.choice(['Plus', 'Fix']))
    else:
        name = ' '.join(R.sample(WORDS, R.randint(1, 4))).title()
        name = decorate(name)
    if R.random() < 0.05: name = name.replace('e', 'é', 1)
    wid = R.choice(known_ids) if (known_ids and R.random() < 0.2) else str(R.randint(2200000000, 3900000000))
    ident = ('ugc_%s.mod' % wid) if R.random() < 0.85 else ('local_%d.mod' % i)
    if any(m['id'] == ident for m in mods): continue
    mods.append({'id': ident, 'name': name})

pool_dirs = ['common/traits', 'common/culture/traditions', 'common/defines', 'common/on_action', 'common/script_values', 'common/decisions', 'events', 'localization/english', 'localization/replace/english', 'gfx/portraits', 'gui', 'history/provinces', 'map_data', 'common/genes', 'music', 'common/ai_war_stances']
shared_files = ['%s/%s.txt' % (d, R.choice(['00_main', 'zz_extra', 'a_core', 'misc'])) for d in pool_dirs[:12]]
shared_files += ['localization/english/shared_l_english.yml', 'gfx/portraits/shared.dds', 'gui/shared.gui']
shared_keys = ['shared_trait_%d' % i for i in range(6)]
shared_events = ['ns%d' % i for i in range(3)]

def script_for(path, mi):
    r = []
    bom = '﻿' if R.random() < 0.05 else ''
    nl = '\r\n' if R.random() < 0.2 else '\n'
    if path.startswith('events/'):
        ns = R.choice(shared_events) if R.random() < 0.3 else 'mod%d' % mi
        r.append('namespace = %s' % ns)
        for k in range(R.randint(1, 4)):
            body = ['\ttype = character_event', '\ttitle = %s.%d.t' % (ns, k)]
            if R.random() < 0.15: body.append('\tid_override_priority = 5')
            r.append('%s.%d = {' % (ns, R.randint(1, 6)) + nl + nl.join(body) + nl + '}')
    elif path.endswith('.yml'):
        r.append('l_english:')
        for k in range(R.randint(1, 8)):
            key = R.choice(shared_keys) if R.random() < 0.3 else 'mod%d_key_%d' % (mi, k)
            r.append(' %s:0 "Some text # not a comment"' % key)
        r.append(' # comment: "x"')
    elif 'defines' in path:
        for b in range(R.randint(1, 2)):
            r.append('N%s = {' % R.choice(['Game', 'Combat', 'Diplomacy']))
            for k in range(R.randint(1, 3)): r.append('\tVAL_%d = %d' % (R.choice([1, 2, 3]), R.randint(1, 9)))
            r.append('}')
    elif 'on_action' in path:
        for k in range(R.randint(1, 3)):
            nm = R.choice(['on_birthday', 'on_death', 'mod%d_on_tick' % mi, 'on_yearly_pulse'])
            parts = []
            if R.random() < 0.5: parts.append('\teffect = { add_gold = 5 }')
            if R.random() < 0.3: parts.append('\ttrigger = { always = yes }')
            if R.random() < 0.5: parts.append('\tevents = { ns0.1 }')
            r.append('%s = {' % nm + nl + nl.join(parts) + nl + '}')
    else:
        for k in range(R.randint(1, 6)):
            key = R.choice(shared_keys) if R.random() < 0.3 else 'mod%d_obj_%d' % (mi, k)
            inner = nl.join(['\tvalue = %d' % R.randint(0, 9), '\tnested = { a = 1 b = "q w" }', '\t# c = {'])
            if R.random() < 0.15: r.append('@var_%d = 5' % k)
            if R.random() < 0.1: r.append('%s = %d' % (key, R.randint(0, 9)))
            else: r.append('%s = {%s%s%s}' % (key, nl, inner, nl))
    return bom + nl.join(r) + nl

# vanilla
vroot = out + '/install'
if R.random() < 0.8:
    os.makedirs(vroot + '/binaries'); open(vroot + '/binaries/ck3.exe', 'w').write('x')
    vfiles = []
    for d in pool_dirs[:12]: vfiles.append('%s/00_vanilla.txt' % d)
    vfiles += shared_files[: R.randint(0, 6)]
    vfiles += ['gfx/vanilla/%d.dds' % i for i in range(R.randint(0, 8))]
    for f in vfiles:
        p = vroot + '/game/' + f; os.makedirs(os.path.dirname(p), exist_ok=True); open(p, 'w').write('x')
    open(vroot + '/game/readme.txt', 'w').write('root file')
    if R.random() < 0.5:
        for dlc in range(R.randint(1, 3)):
            for f in ['common/traits/dlc%d.txt' % dlc, 'gfx/dlc%d/a.dds' % dlc, 'events/dlc%d_events.txt' % dlc]:
                p = vroot + '/game/dlc/dlc%d/%s' % (dlc, f); os.makedirs(os.path.dirname(p), exist_ok=True); open(p, 'w').write('x')

names = [m['name'] for m in mods]
for i, m in enumerate(mods):
    cdir = 'content/' + m['id']
    desc = []
    desc.append('name="%s"' % m['name'].replace('"', ''))
    if R.random() < 0.8: desc.append('version="%s"' % R.choice(['1.0', '2.1.3', '0.9', 'v1.10', '1.9']))
    desc.append('supported_version="%s"' % R.choice(['1.14.*', '1.14.2', '1.12.*', '1.13', '*', '1.15.*', 'v1.14']))
    if R.random() < 0.4 and len(names) > 1:
        deps = [R.choice(names) if R.random() < 0.8 else 'Missing Dep %d' % R.randint(1, 9) for _ in range(R.randint(1, 3))]
        desc.append('dependencies={ ' + ' '.join('"%s"' % d.replace('"', '') for d in deps) + ' }')
    if R.random() < 0.12:
        desc.append('replace_path="%s"' % R.choice(['common/traits', 'events', 'common/on_action', 'history/provinces', 'gfx\\portraits', 'Common/Culture/Traditions/']))
    if R.random() < 0.3: desc.append('tags={ "%s" "Utilities" }' % R.choice(['Fixes', 'Translation', 'Interface', 'Graphics', 'Total Conversion', 'Gameplay']))
    if R.random() < 0.05: desc.append('# comment line "x"')
    missing = R.random() < 0.06
    absolute = R.random() < 0.1
    if R.random() < 0.05: desc.append('archive="%s.zip"' % m['id'])
    else: desc.append('path="%s"' % ((os.path.abspath(out + '/ck3/' + cdir)) if absolute else cdir))
    sep = '\r\n' if R.random() < 0.2 else '\n'
    txt = sep.join(desc) + sep
    if R.random() < 0.05: txt = '﻿' + txt
    open(out + '/ck3/mod/' + m['id'], 'w', encoding='utf-8', newline='').write(txt)
    if missing: continue
    files = set()
    for f in shared_files:
        if R.random() < 0.18: files.add(f)
    for d in pool_dirs:
        for k in range(R.randint(0, 3) if R.random() < 0.7 else 0):
            ext = '.yml' if d.startswith('localization') else ('.dds' if d.startswith('gfx') or d == 'music' else ('.gui' if d == 'gui' else '.txt'))
            files.add('%s/mod%d_%d%s' % (d, i, k, ext))
    if R.random() < 0.1:
        for k in range(R.randint(20, 80)): files.add('gfx/big%d/%d.dds' % (i, k))
    for f in files:
        p = out + '/ck3/' + cdir + '/' + f
        os.makedirs(os.path.dirname(p), exist_ok=True)
        if f.endswith(('.txt', '.yml')): open(p, 'w', encoding='utf-8', newline='').write(script_for(f, i))
        else: open(p, 'w').write('bin')
    open(out + '/ck3/' + cdir + '/descriptor.mod', 'w').write('name="x"\n')
    open(out + '/ck3/' + cdir + '/thumbnail.png', 'w').write('x')
    if R.random() < 0.2:
        os.makedirs(out + '/ck3/' + cdir + '/.git', exist_ok=True); open(out + '/ck3/' + cdir + '/.git/config', 'w').write('x')
        open(out + '/ck3/' + cdir + '/common/Thumbs.db', 'w') if os.path.isdir(out + '/ck3/' + cdir + '/common') else None

# playset
order = list(range(len(mods))); R.shuffle(order)
pm = []
for pos, idx in enumerate(order):
    m = mods[idx]
    e = {'displayName': m['name'], 'enabled': R.random() < 0.8, 'position': pos}
    sid = re.match(r'ugc_(\d+)\.mod', m['id'])
    if sid and R.random() < 0.9: e['steamId'] = sid.group(1)
    elif R.random() < 0.5: e['gameRegistryId'] = 'mod/' + m['id']
    pm.append(e)
for k in range(R.randint(0, 3)):
    pm.insert(R.randint(0, len(pm)), {'displayName': 'Ghost Mod %d' % k, 'enabled': R.random() < 0.7, 'position': 900 + k, **({'steamId': str(R.randint(1000, 9999))} if R.random() < 0.5 else {})})
open(out + '/playset.json', 'w', encoding='utf-8').write(json.dumps({'game': 'ck3', 'name': 'Test Playset', 'mods': pm}, ensure_ascii=False))
opts = ['game ' + R.choice(['1.14.2', '1.14.0.3', '1.12.1', '1.15.0'])]
for m in R.sample(mods, min(len(mods), R.randint(0, 3))): opts.append('lock ' + m['id'])
for m in R.sample(mods, min(len(mods), R.randint(0, 3))): opts.append('cat %s %d' % (m['id'], R.randint(0, 6)))
open(out + '/options.txt', 'w').write('\n'.join(opts) + '\n')
