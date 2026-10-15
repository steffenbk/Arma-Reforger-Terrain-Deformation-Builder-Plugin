import csv,glob,collections
pts=[];allp=collections.Counter()
for f in glob.glob('MUD_framelog_client_run*.csv'):
    r=list(csv.DictReader(open(f)))
    for a in r:
        x=float(a['body_x']);z=float(a['body_z'])
        k=(int(x//4)*4,int(z//4)*4)
        if float(a['speed'])>0.5: allp[k]+=1
        if float(a['probe_corr'])>0.15: pts.append((k,float(a['body_yaw']),float(a['speed']),float(a['throttle'])))
c=collections.Counter(p[0] for p in pts)
print('snaps',len(pts),'cells',len(c))
for k,n in c.most_common(25):
    print(k,'snaps',n,'frames-in-cell',allp[k],'rate per 1000 frames %.1f'%(1000*n/max(allp[k],1)))
