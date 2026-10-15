import sys,math,statistics as st;exec(open(sys.argv[2]+'/an.py').read().split("p=sys.argv[1]")[0])
r=load(sys.argv[1]);s=series(r)
def q(xs,p): xs=sorted(xs); return xs[min(len(xs)-1,int(p*len(xs)))]
mv=[k for k,x in enumerate(s) if x['spd']>0.5 and x['dt']<0.03 and 7<=k<len(s)-8]
vs=[x['vis']*x['dt'] for x in s]; dts=[x['dt'] for x in s]
a=[vs[k]/st.median(vs[k-7:k+8]) for k in mv]; b=[dts[k]/st.median(dts[k-7:k+8]) for k in mv]
ma,mb=st.mean(a),st.mean(b);cov=sum((x-ma)*(y-mb) for x,y in zip(a,b))/len(a)
print(sys.argv[1][-14:],'dt/median p5 %.2f p95 %.2f | vis step/median p5 %.2f p95 %.2f | corr %.2f | step/dt-ratio p5 %.2f p95 %.2f'%(q(b,.05),q(b,.95),q(a,.05),q(a,.95),cov/(st.pstdev(a)*st.pstdev(b)),q([x/y for x,y in zip(a,b)],.05),q([x/y for x,y in zip(a,b)],.95)))
