import sys,math,statistics as st;exec(open(sys.argv[2]+'/an.py').read().split("p=sys.argv[1]")[0])
r=load(sys.argv[1])
B=lambda k:(r[k]['body_x'],r[k]['body_z']); V=lambda k:(r[k]['vis_x'],r[k]['vis_z'])
moved=[k for k in range(1,len(r)) if B(k)!=B(k-1)]
# spike flag per physics step (front wheels), indexed by frame
def spikes(w):
    rp=[(k,r[k]['w%d_rpm'%w]) for k in moved]; out=set()
    for i in range(1,len(rp)-1):
        a,b,c=abs(rp[i-1][1]),abs(rp[i][1]),abs(rp[i+1][1])
        if b>3*max(1,a) and b>3*max(1,c): out.add(rp[i][0])
    return out
sp=spikes(0)|spikes(1)
base=len([k for k in moved if k in sp])/len(moved)
corr=[k for k in range(len(r)) if r[k]['probe_corr']>0]
print(sys.argv[1][-22:],'corrections',len(corr),'| front-wheel spike rate per physics step, whole run %.2f'%base)
print(' frame  t_s   dt     spd  probe_m  body_steps  along  vis_step/med  vis_steps_next5    spikes in prev 30 steps  w0 rpm prev 6 steps')
for k in corr:
    d=(B(k)[0]-B(k-1)[0],B(k)[1]-B(k-1)[1]); ref=(B(k-1)[0]-B(k-6)[0],B(k-1)[1]-B(k-6)[1]); n=math.hypot(*ref) or 1
    along=(d[0]*ref[0]+d[1]*ref[1])/n
    stepm=r[k]['speed']/60
    vs=[math.dist(V(j),V(j-1)) for j in range(k-8,k+7)]; vm=st.median(vs)
    prev=[j for j in moved if j<k][-30:]
    w0=[round(r[j]['w0_rpm']) for j in prev[-6:]]
    print('%5d %5.2f %.4f %.2f %.3f   %5.2f   %+6.3f   %5.2f        %s   %2d/30 (%.2f)   %s'%(k,(r[k]['tick_ms']-r[0]['tick_ms'])/1000,r[k]['dt'],r[k]['speed'],r[k]['probe_corr'],math.dist(B(k),B(k-1))/stepm,along,math.dist(V(k),V(k-1))/vm,
      ' '.join('%.1f'%(math.dist(V(j),V(j-1))/vm) for j in range(k+1,k+6)),sum(1 for j in prev if j in sp),sum(1 for j in prev if j in sp)/30,w0))
