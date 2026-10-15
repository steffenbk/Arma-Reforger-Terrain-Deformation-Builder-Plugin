import csv,glob,os,statistics as st
A={'N':[],'S':[]}
for f in glob.glob('MUD_framelog_client_run*.csv'):
    r=list(csv.DictReader(open(f)))
    if len(r)<500: continue
    x=float(r[0]['body_x']);z=float(r[0]['body_z'])
    if not(1000<x<1130 and 2480<z<2640): continue
    zone='S' if (z<2560 and 1040<x<1095) else 'N'
    mv=[a for a in r if float(a['speed'])>1]
    if len(mv)<200: continue
    nw=sum(1 for k in r[0] if k.endswith('_contact'))
    noc=sum(1 for a in mv for w in range(nw) if a['w%d_contact'%w] in('0','false'))/(len(mv)*nw)
    ls=[float(a['w%d_long'%w]) for a in mv for w in range(nw)]
    big=sum(1 for v in ls if abs(v)>0.5)/len(ls)
    rpm=[[float(a['w%d_rpm'%w]) for a in mv] for w in range(nw)]
    drpm=st.mean(st.mean(abs(rr[i]-rr[i-1]) for i in range(1,len(rr))) for rr in rpm)
    sp=[float(a['speed']) for a in mv]
    yaw=[float(a['body_yaw']) for a in mv]
    yr=st.mean(abs(((yaw[i]-yaw[i-1]+180)%360)-180) for i in range(1,len(yaw)))
    cy=[float(a['w%d_cy'%w]) for a in mv for w in range(nw) if a['w%d_contact'%w] not in('0','false')]
    snaps=sum(1 for a in r if float(a['probe_corr'])>0.15)
    A[zone].append((snaps,noc,big,drpm,st.mean(sp),st.pstdev(sp),yr,st.pstdev(cy) if cy else 0,st.mean(abs(v) for v in ls)))
names='snaps/run nocontact bigslip>0.5 drpm/frame speed speedsd yaw/frame cy_sd |long|'.split()
for z in 'NS':
    R=A[z]; print(z,len(R),' '.join('%s %.4f'%(n,st.mean(r[i] for r in R)) for i,n in enumerate(names)))
