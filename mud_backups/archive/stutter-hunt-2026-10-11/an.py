import csv,sys,math,statistics as st
def load(p):
    r=list(csv.DictReader(open(p)))
    for d in r:
        for k,v in d.items():
            try:d[k]=float(v)
            except:d[k]=1.0 if v=='true' else 0.0 if v=='false' else v
    return r
def ang(a): return (a+180)%360-180
def series(r):
    out=[]
    for i in range(1,len(r)):
        a,b=r[i-1],r[i];dt=b['dt']
        def dist(p): return math.dist([a[p+'_x'],a[p+'_y'],a[p+'_z']],[b[p+'_x'],b[p+'_y'],b[p+'_z']])
        def hd(p): return math.dist([a[p+'_x'],a[p+'_z']],[b[p+'_x'],b[p+'_z']])
        o=dict(i=int(b['frame']),dt=dt,spd=b['speed'],
          bstep=dist('body'), vis=hd('vis')/dt, visy=(b['vis_y']-a['vis_y'])/dt, cam=hd('cam')/dt, camy=(b['cam_y']-a['cam_y'])/dt,
          vyaw=ang(b['vis_yaw']-a['vis_yaw'])/dt, cyaw=ang(b['cam_yaw']-a['cam_yaw'])/dt, byaw=ang(b['body_yaw']-a['body_yaw']),
          vpit=(b['vis_pitch']-a['vis_pitch'])/dt, vrol=(b['vis_roll']-a['vis_roll'])/dt,
          crel=math.dist([a['camrel_x'],a['camrel_y'],a['camrel_z']],[b['camrel_x'],b['camrel_y'],b['camrel_z']]),
          offz=b['off_z']-r[0]['off_z'], rpm=[b['w%d_rpm'%w] for w in range(6)], cy=[b['w%d_cy'%w] for w in range(6)])
        out.append(o)
    return out
def med(xs,i,k=7):
    w=xs[max(0,i-k):i+k+1];return st.median(w)
p=sys.argv[1];r=load(p);s=series(r)
moving=[x for x in s if x['spd']>0.5]
dts=[x['dt'] for x in s]
print(p.split('/')[-1],'frames',len(r),'dt mean %.4f max %.4f'%(st.mean(dts),max(dts)))
steps=sum(1 for x in s if x['bstep']>0); same=sum(1 for x in s if x['bstep']==0)
print('frames where body moved',steps,'frames body unchanged',same)
for key in ['vis','cam','vyaw','cyaw','visy','camy','vpit','vrol']:
    xs=[x[key] for x in s]
    dev=[abs(xs[i]-med(xs,i)) for i in range(len(xs)) if s[i]['spd']>0.5]
    ref=[abs(med(xs,i)) for i in range(len(xs)) if s[i]['spd']>0.5]
    print('%5s  median |rate| %.3f  dev from 15-frame median: p50 %.3f p95 %.3f p99 %.3f max %.3f'%(key,st.median(ref),st.median(dev),sorted(dev)[int(.95*len(dev))],sorted(dev)[int(.99*len(dev))],max(dev)))
crel=[x['crel'] for x in moving];print('camrel per-frame change: p50 %.4f p99 %.4f max %.4f m'%(st.median(crel),sorted(crel)[int(.99*len(crel))],max(crel)))
