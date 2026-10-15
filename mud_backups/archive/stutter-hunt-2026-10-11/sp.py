import sys;sys.path.insert(0,sys.argv[2]);exec(open(sys.argv[2]+'/an.py').read().split("p=sys.argv[1]")[0])
r=load(sys.argv[1]);s=series(r)
print('frame  t_ms   dt     spd   vis_m/s  cam_m/s  vyaw  cyaw   camrel_d  camrel(x,y,z)   cam_y')
for k,x in enumerate(s):
    if x['crel']>0.03 or abs(x['cyaw']-x['vyaw'])>15:
        b=r[k+1]
        print('%5d %6d %.4f %.2f %7.2f %7.2f %6.1f %7.1f %.3f  (%.2f %.2f %.2f) %.2f'%(x['i'],b['tick_ms']-r[0]['tick_ms'],x['dt'],x['spd'],x['vis'],x['cam'],x['vyaw'],x['cyaw'],x['crel'],b['camrel_x'],b['camrel_y'],b['camrel_z'],b['cam_y']))
