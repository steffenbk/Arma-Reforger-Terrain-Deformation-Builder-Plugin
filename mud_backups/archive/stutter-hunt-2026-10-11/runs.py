import re,sys
w=open("wb_snap.log",errors="replace").read().splitlines()
c=open("cl_snap.log",errors="replace").read().splitlines()
runs=[]
for l in c:
    m=re.match(r'(\S+).*MUD robot (START|STOP) run (\d)',l)
    if m:
        if m.group(2)=='START': runs.append([m.group(1),None])
        else: runs[-1][1]=m.group(1)
wins=[("pre","14:19:41",runs[0][0])]
for i,(a,b) in enumerate(runs):
    wins.append((f"run{i+1}",a,b))
    nxt=runs[i+1][0] if i+1<len(runs) else "14:22:40"
    wins.append((f"after{i+1}",b,nxt))
def inw(t,a,b): return a<=t<b
# server decisions with field states
st={};e=p=None;dec=[]
for l in w:
    if 'NVMC' not in l: continue
    t=l.split()[0]
    m=re.search(r'Process Result (m_\w+) (\d)',l)
    if m: st[m.group(1)]=m.group(2); continue
    m=re.search(r'Engine (\d); Phys (\d)',l)
    if m: e,p=m.groups(); dec.append((t,'CHK',e,p,dict(st))); st={}; continue
    m=re.search(r'SERVER (CORRECTING|APPROVING) (\d+)',l)
    if m: dec.append((t,m.group(1),m.group(2),None,None)); continue
    if 'NO MOVE TO CONSUME' in l: dec.append((t,'NOMOVE',None,None,None))
    if 'discarding' in l: dec.append((t,'DISCARD',l.split('NVMC')[1][:60],None,None))
cc=[(l.split()[0],re.search(r'CORRECTING (\d+)',l).group(1)) for l in c if 'CLIENT CORRECTING' in l]
snaps=[(l.split()[0],re.search(r'body jump ([\d.]+)',l).group(1),re.search(r'mine (\d)',l).group(1)) for l in c if 'MUD corr: ' in l]
for name,a,b in wins:
    chks=[d for d in dec if d[1]=='CHK' and inw(d[0],a,b)]
    eng=sum(1 for d in chks if d[2]=='0'); ph=sum(1 for d in chks if d[3]=='0')
    phfail={}
    for d in chks:
        if d[3]=='0':
            for k,v in d[4].items():
                if k.startswith('m_fServer') and v=='0': phfail[k]=phfail.get(k,0)+1
    corr=[d[2] for d in dec if d[1]=='CORRECTING' and inw(d[0],a,b)]
    nm=[d[0] for d in dec if d[1]=='NOMOVE' and inw(d[0],a,b)]
    ccw=[x for x in cc if inw(x[0],a,b)]
    sn=[x for x in snaps if inw(x[0],a,b)]
    print(f"{name:7} {a}-{b} checks {len(chks)} engfail {eng} physfail {ph} {phfail} CORR {len(corr)} {corr} NOMOVE {len(nm)} {nm[:2]}..{nm[-1:] } clientCORR {len(ccw)} {[x[1] for x in ccw]} snaps {sn}")
T=lambda k: sum(1 for d in dec if d[1]==k and "14:19:41"<=d[0]<"14:22:40")
print("TOTAL <13:48:25 checks",T('CHK'),"CORR",T('CORRECTING'),"APPR",T('APPROVING'),"NOMOVE",T('NOMOVE'),"engfail",sum(1 for d in dec if d[1]=='CHK' and d[2]=='0' and "14:19:41"<=d[0]<"14:22:40"),"physfail",sum(1 for d in dec if d[1]=='CHK' and d[3]=='0' and "14:19:41"<=d[0]<"14:22:40"))
print("client CORR <13:48:25",sum(1 for x in cc if x[0]<"14:22:40"),"snaps",sum(1 for x in snaps if "14:19:41"<=x[0]<"14:22:40"))
