import subprocess, numpy as np, soundfile as sf, json
from scipy import signal
R='/home/claude/riky-piano-man/build/riky_render_artefacts/Release/riky_render'; SFZ='/home/claude/ext/maestro/Maestro Concert Grand.sfz'; MID='/home/claude/work/montage/ref/demo12.mid'
def level(f):
    x,sr=sf.read(f); m=x.mean(1)
    b,a=signal.butter(2,[100/(sr/2),5000/(sr/2)],'band'); y=signal.lfilter(b,a,m)
    n=int(.4*sr); r=np.array([np.sqrt(np.mean(y[i:i+n]**2)) for i in range(0,len(y)-n,n//2)])
    return 20*np.log10(np.sqrt(np.mean(r[r>=np.percentile(r,50)]**2))+1e-12)
def render(args,out='/tmp/b.wav'):
    subprocess.run([R,SFZ,'--render',MID,out]+args,capture_output=True); return level(out)
combos={4:-7,9:-10,10:-6,11:-9,12:-5,13:-9,14:-10,16:-11}   # quanto sotto il pianoforte deve stare il secondo strato (dB)
res={}
for i,target in combos.items():
    piano=render([f'preset={i}','L2=0'])
    lay=render([f'preset={i}','SFZ1=0'])
    cur=json.load(open('final_presets.json'))[str(i)].get('L2VOL',-6)
    new=float(np.clip(round((cur+(piano+target-lay))*2)/2,-40,6))
    res[i]=new; print(i,'pianoforte %.1f  strato %.1f (L2VOL %.1f) -> L2VOL %.1f'%(piano,lay,cur,new))
json.dump(res,open('l2vol.json','w'))
