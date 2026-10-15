import sys
def load(p):
    t=open(p,encoding='utf-8',errors='replace').read()
    lines=t.replace('\r','').split('\n')
    hdr=[h.strip() for h in lines[0].split(',')]
    rows=[]
    for l in lines[1:]:
        f=[x.strip() for x in l.split(',')]
        if len(f)>5: rows.append(f)
    return hdr,rows
for p in sys.argv[1:]:
    h,r=load(p); print(p,len(h),'cols',len(r),'rows')
    from collections import Counter
    print(' lens',Counter(len(x) for x in r).most_common(4))
    print(' ids',r[0][0],r[-1][0])
    i=h.index('Engine_IsStalled') if 'Engine_IsStalled' in h else None
    if i is not None: print(' stalled',Counter(x[i] for x in r if len(x)>i))
