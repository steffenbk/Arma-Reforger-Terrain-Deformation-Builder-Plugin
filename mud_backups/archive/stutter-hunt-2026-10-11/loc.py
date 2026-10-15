import csv,glob,os,statistics as st,math
rows=[]
for f in sorted(glob.glob('MUD_framelog_client_run*.csv'),key=os.path.getmtime):
    r=list(csv.DictReader(open(f)))
    if len(r)<500: continue
    x=float(r[0]['body_x']);z=float(r[0]['body_z'])
    snaps=sum(1 for a in r if float(a['probe_corr'])>0.15)
    ys=[float(a['body_y']) for a in r]; p=[float(a['body_pitch']) for a in r]; ro=[float(a['body_roll']) for a in r]
    mv=[a for a in r if float(a['speed'])>1]
    ls=st.mean(abs(float(a['w%d_long'%w])) for a in mv for w in range(6)) if mv else 0
    lat=st.mean(abs(float(a['w%d_lat'%w])) for a in mv for w in range(6)) if mv else 0
    # pitch/roll frame-to-frame jitter
    dj=st.mean(abs(p[i]-p[i-1])+abs(ro[i]-ro[i-1]) for i in range(1,len(p)))
    zone='S' if (z<2560 and 1040<x<1095) else 'N'
    rows.append((zone,os.path.getmtime(f),x,z,snaps,max(ys)-min(ys),st.pstdev(p),st.pstdev(ro),dj,ls,lat))
import time
for zn in 'NS':
    R=[r for r in rows if r[0]==zn]
    print(zn,'runs',len(R),'snaps',sum(r[4] for r in R),'| mean yrange %.2f pitchsd %.2f rollsd %.2f jitter %.4f long %.3f lat %.3f'%tuple(st.mean(r[i] for r in R) for i in range(5,11)))
for r in rows:
    if r[0]=='S': print(time.strftime('%H:%M:%S',time.localtime(r[1])),'%.0f/%.0f snaps %d yr %.2f psd %.2f rsd %.2f jit %.4f long %.3f lat %.3f'%r[2:])
