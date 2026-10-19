# offline: CarveFloor first-pass rule under RUT_RAMP, one wheel, straight vs curve
import math
R=0.3; LEN=1.0; F=0.15; TOL=0.01; FC=0.125; STEP=0.5; CAP=0.35
def rampw(t,L): c=lambda v:min(1,max(0,v)); return c((t+LEN)/LEN)-c((t-L+LEN)/LEN)
def run(fixed, radius, speed):
    pts=[]; s=0
    while s<30:
        if radius: a=s/radius; pts.append((radius*math.sin(a), radius*(1-math.cos(a))))
        else: pts.append((s,0))
        s+=STEP
    per=max(1,round(speed*0.25/STEP))   # points per 250 ms op
    floor={}
    i=0
    while i+per < len(pts):
        segs=[(pts[j],pts[j+1]) for j in range(i,i+per)]
        L=STEP*per; best={}; rw={}
        def put(key,fall,w):
            if key not in best or fall>best[key]: best[key]=fall; rw[key]=w
        xs=[p[0] for sg in segs for p in sg]; zs=[p[1] for sg in segs for p in sg]
        for gi in range(int((min(xs)-2)/FC), int((max(xs)+2)/FC)+1):
            for gk in range(int((min(zs)-2)/FC)-1, int((max(zs)+2)/FC)+1):
                x=gi*FC; z=gk*FC
                for n,(a,b) in enumerate(segs):
                    dx=b[0]-a[0]; dz=b[1]-a[1]; l=math.hypot(dx,dz)
                    u=max(0,min(1,((x-a[0])*dx+(z-a[1])*dz)/(l*l)))
                    d=math.hypot(x-a[0]-u*dx, z-a[1]-u*dz)
                    if d<R:
                        w=rampw(n*STEP+u*l, L)
                        if w>0: put((gi,gk),(1-(d/R)**2)*w,w)
                    if n==0:  # RampBehind straight back
                        ux,uz=dx/l,dz/l; t=(x-a[0])*ux+(z-a[1])*uz
                        if -LEN<=t<0:
                            d=math.hypot(x-a[0]-ux*t, z-a[1]-uz*t)
                            if d<R:
                                w=rampw(t,L)
                                if w>0: put((gi,gk),(1-(d/R)**2)*w,w)
        for k,cf in best.items():
            w=rw[k]; dep=floor.get(k,0)
            f=F*cf/w if fixed else F*cf
            if dep < f-TOL: cut=f
            else: cut=min(0.03, max(0.08*cf/w, 0.4*(CAP*cf/w-dep)))
            cut*=w
            nd=min(CAP*cf/w, dep+cut); nd=round(nd*100)/100
            if nd>dep: floor[k]=nd
        i+=per
    # depth along the path centre (skip first/last 3 m)
    ds=[]
    for (x,z) in pts[6:-8]:
        ds.append(floor.get((round(x/FC),round(z/FC)),0))
    return min(ds), sum(ds)/len(ds), sum(1 for d in ds if d<0.09)/len(ds)
for fixed in (False,True):
    for rad,sp in ((0,10),(0,4),(7,4),(7,2)):
        mn,av,lo=run(fixed,rad,sp)
        print('fixed' if fixed else 'now  ', 'curve r=%s'%rad if rad else 'straight', 'speed',sp, 'min %.2f avg %.3f share<9cm %.0f%%'%(mn,av,lo*100))
