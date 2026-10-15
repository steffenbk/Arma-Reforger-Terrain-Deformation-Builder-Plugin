import sys,math,statistics as st,collections;exec(open(sys.argv[2]+'/an.py').read().split("p=sys.argv[1]")[0])
r=load(sys.argv[1])
B=lambda k:(r[k]['body_x'],r[k]['body_z']); V=lambda k:(r[k]['vis_x'],r[k]['vis_z'])
mv=[k for k in range(8,len(r)-8) if r[k]['speed']>0.5]
corr=set(k for k in range(len(r)) if r[k]['probe_corr']>0)
h=collections.Counter(); hv=[]
for k in mv:
    n=math.dist(B(k),B(k-1))/(r[k]['speed']/60); h[round(n)]+=1
    vs=[math.dist(V(j),V(j-1)) for j in range(k-7,k+8)]; hv.append((math.dist(V(k),V(k-1))/st.median(vs),k))
big=[(round(x,2),k) for x,k in hv if x>1.8 and k not in corr]
print(sys.argv[1][-22:],'physics steps per frame (rounded):',dict(sorted(h.items())),'| visual step >1.8x median, non-snap frames:',len(big),'of',len(mv))
print('  first 15:',big[:15])
# consecutive pattern near first one
if big:
    k=big[0][1]
    for j in range(k-4,k+5): print('   fr %d dt %.4f body_steps %.2f vis/med %.2f off_z %.4f'%(j,r[j]['dt'],math.dist(B(j),B(j-1))/(r[j]['speed']/60),math.dist(V(j),V(j-1))/st.median([math.dist(V(i),V(i-1)) for i in range(j-7,j+8)]),r[j]['off_z']))
