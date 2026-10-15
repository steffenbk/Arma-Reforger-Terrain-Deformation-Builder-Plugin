import sys;exec(open(sys.argv[2]+'/k.py').read().split("corr=[k")[0])
fr=[k for k in moved if r[k]['speed']>0.5]
cnt=0
for k in fr:
    prev=[j for j in moved if j<k][-30:]
    if sum(1 for j in prev if j in sp)>=10: cnt+=1
print(sys.argv[1][-22:],'share of moving physics-step frames with >=10/30 spikes in prev 30 steps: %.2f (%d/%d)'%(cnt/len(fr),cnt,len(fr)))
