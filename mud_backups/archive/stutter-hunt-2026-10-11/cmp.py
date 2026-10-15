import csv,glob,os,time,statistics as st,sys
fs=[f for f in glob.glob("MUD_framelog_client_run*.csv")]
fs=[f for f in fs if time.strftime("%H:%M:%S",time.localtime(os.path.getmtime(f)))>="14:37:00"]
fs.sort(key=os.path.getmtime)
rows_out=[]
for f in fs:
    R=list(csv.DictReader(open(f)))
    if len(R)<500: continue
    t0=float(R[0]['world_ms'])
    dur=(float(R[-1]['world_ms'])-t0)/1000
    if dur<15: continue
    snaps=sum(1 for r in R if float(r['probe_corr'])>0)
    M=[r for r in R if float(r['world_ms'])-t0>3000]   # after robot start-up
    g=lambda k:[float(r[k]) for r in M]
    sp=g('speed'); dt=g('dt'); th=g('throttle'); stv=g('steer')
    slip=sum(1 for r in M if any(abs(float(r[f'w{i}_long']))>0.2 for i in range(6)))/len(M)
    lat=sum(1 for r in M if any(abs(float(r[f'w{i}_lat']))>0.2 for i in range(6)))/len(M)
    rpm=[abs(float(r['w0_rpm'])) for r in M]
    ss=sum(1 for a,b in zip(rpm,rpm[1:]) if abs(a-b)>30)/len(rpm)
    cont=sum(int(r[f'w{i}_contact']) for r in M for i in range(6))/(6*len(M))
    pitch=st.pstdev(g('body_pitch')); roll=st.pstdev(g('body_roll'))
    dts=sorted(dt)
    rows_out.append(dict(file=f,end=time.strftime("%H:%M:%S",time.localtime(os.path.getmtime(f))),snaps=snaps,dur=round(dur,1),
        speed=st.mean(sp),speed_sd=st.pstdev(sp),throttle=st.mean(th),steer=st.mean(stv),
        dt=st.mean(dt),dt_p99=dts[int(.99*len(dts))],dt_max=dts[-1],slip_long=slip,slip_lat=lat,rpm_jump=ss,contact=cont,pitch_sd=pitch,roll_sd=roll))
keys=['speed','speed_sd','throttle','steer','dt','dt_p99','dt_max','slip_long','slip_lat','rpm_jump','contact','pitch_sd','roll_sd']
print("end      snaps dur  "+" ".join(f"{k:>9}" for k in keys))
for r in rows_out:
    print(f"{r['end']} {r['snaps']:>4} {r['dur']:>5} "+" ".join(f"{r[k]:9.4f}" for k in keys))
Q=[r for r in rows_out if r['snaps']==0]; N=[r for r in rows_out if r['snaps']>=2]
print(f"\nquiet runs {len(Q)}  noisy runs (>=2) {len(N)}  other {len(rows_out)-len(Q)-len(N)}  total {len(rows_out)}")
for k in keys:
    q=[r[k] for r in Q]; n=[r[k] for r in N]
    print(f"{k:>9} quiet {st.mean(q):.4f} [{min(q):.4f}-{max(q):.4f}]  noisy {st.mean(n):.4f} [{min(n):.4f}-{max(n):.4f}]")
