import csv,sys,glob,os
for n in range(1,17):
    fs=sorted(glob.glob(f"MUD_framelog_client_run{n}_*.csv"),key=os.path.getmtime)
    if not fs: continue
    f=fs[-1]
    if os.path.getmtime(f)<os.path.getmtime("diagmenu.save.bak_1455"): continue
    rows=list(csv.DictReader(open(f)))
    xs=[float(r['body_x']) for r in rows]; zs=[float(r['body_z']) for r in rows]
    slips=[abs(float(r['w0_long'])) for r in rows if float(r['speed'])>0.5]
    pc=sum(1 for r in rows if r['probe_corr'] not in ('0',''))
    ss=sum(1 for s in slips if s>0.2)/max(1,len(slips))
    print(f"run{n} {f} x {min(xs):.0f}-{max(xs):.0f} z {min(zs):.0f}-{max(zs):.0f} probe_corr {pc} frames {len(rows)} w0 long slip>0.2 share {ss:.2f}")
