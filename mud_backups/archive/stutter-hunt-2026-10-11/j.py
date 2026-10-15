import sys,math,statistics as st;exec(open(sys.argv[2]+'/an.py').read().split("p=sys.argv[1]")[0])
r=load(sys.argv[1]);s=series(r)
def q(xs,p): xs=sorted(xs); return xs[min(len(xs)-1,int(p*len(xs)))]
mv=[k for k,x in enumerate(s) if x['spd']>0.5 and x['dt']<0.03]
# 1 visual step ratio vs local median
vstep=[x['vis']*x['dt'] for x in s]
rat={k:vstep[k]/max(1e-6,st.median(vstep[max(0,k-7):k+8])) for k in mv}
st_=[rat[k] for k in mv if s[k]['bstep']>0]; ns=[rat[k] for k in mv if s[k]['bstep']==0]
print(sys.argv[1].split('_run')[1][:1],'moving frames',len(mv),'| physics steps/s %.1f'%(sum(1 for x in s if x['bstep']>0)/sum(x['dt'] for x in s)))
print(' visual step / local median: all p1 %.2f p50 %.2f p99 %.2f | frames WITH body step n=%d p50 %.2f | WITHOUT n=%d p50 %.2f'%(q(list(rat.values()),.01),q(list(rat.values()),.5),q(list(rat.values()),.99),len(st_),st.median(st_),len(ns),st.median(ns) if ns else 0))
# visual step vs dt-scaled: speed jitter normalised by dt
vr=[(s[k]['vis'])/max(1e-6,st.median([s[j]['vis'] for j in range(max(0,k-7),min(len(s),k+8))])) for k in mv]
print(' visual speed (step/dt) / local median: p1 %.2f p5 %.2f p50 %.2f p95 %.2f p99 %.2f'%(q(vr,.01),q(vr,.05),q(vr,.5),q(vr,.95),q(vr,.99)))
# 2 body per physics step vs speed/60
bs=[s[k]['bstep']/(s[k]['spd']/60) for k in mv if s[k]['bstep']>0]
print(' body step / (speed/60): p1 %.2f p50 %.2f p99 %.2f max %.2f  (2.0 = two physics steps in one frame)'%(q(bs,.01),q(bs,.5),q(bs,.99),max(bs)))
# 3 camera rel second difference (judder = sign flip); mouse sweeps are smooth
cr=[(r[k]['camrel_x'],r[k]['camrel_y'],r[k]['camrel_z']) for k in range(len(r))]
d2=[math.dist([2*cr[k][i]-cr[k-1][i]-cr[k+1][i] for i in range(3)],[0,0,0]) for k in range(1,len(cr)-1)]
print(' camera-vs-car 2nd diff (m): p50 %.4f p99 %.4f max %.4f'%(q(d2,.5),q(d2,.99),max(d2)))
vp=[(r[k]['vis_x'],r[k]['vis_y'],r[k]['vis_z']) for k in range(len(r))]
d2v=[math.dist([2*vp[k][i]-vp[k-1][i]-vp[k+1][i] for i in range(3)],[0,0,0]) for k in range(1,len(vp)-1) if r[k]['speed']>0.5]
print(' visual pos 2nd diff (m): p50 %.4f p99 %.4f max %.4f'%(q(d2v,.5),q(d2v,.99),max(d2v)))
# 4 wheel rpm sign/size alternation per physics step
for w in range(6):
    rp=[r[k]['w%d_rpm'%w] for k in range(1,len(r)) if s[k-1]['bstep']>0 and r[k]['speed']>0.5]
    big=sum(1 for i in range(1,len(rp)-1) if abs(rp[i])>3*max(1,abs(rp[i-1])) and abs(rp[i])>3*max(1,abs(rp[i+1])))
    print('  w%d rpm p50 |%.1f| p95 |%.1f|  single-step spikes (>3x both neighbours) %d / %d steps'%(w,st.median([abs(x) for x in rp]),q([abs(x) for x in rp],.95),big,len(rp)))
