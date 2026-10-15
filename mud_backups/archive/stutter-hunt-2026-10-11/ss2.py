def load(p):
    f=[x.strip() for x in open(p,errors='replace').read().replace('\r','').split('\n')[1].split(',')]
    f=[x for x in f if x!='']
    rows=[f[i:i+11] for i in range(0,len(f)-10,11)]
    return rows
S=load("ArmaReforgerWorkbench/logs/logs_2026-10-11_12-39-23/VehicleWheeledStateDebug_1.txt")
C=load("PeerPlugin1/logs/logs_2026-10-11_12-54-46/VehicleWheeledStateDebug_0.txt")
print(len(S),len(C))
h="id SC Gen Corr Sim Steer Thr Brk Clutch Gear HB".split()
print(h)
from collections import Counter
print('server ids step',Counter(int(S[i+1][0])-int(S[i][0]) for i in range(len(S)-1)).most_common(5))
print('server corrected rows',[r[0] for r in S if r[3]=='1'])
print('client corrected rows',[r[0] for r in C if r[3]=='1'][:40])
sd={}
for r in S: sd.setdefault(r[0],r)
for target in [511,545,1044,1205]:
    print('--',target)
    for r in C:
        if target-6<=int(r[0])<=target+3: print('C',r)
    for r in S:
        if target-6<=int(r[0])<=target+3: print('S',r)
