# Taratura dei preset: render con riky_render, AAC 128k come il video, misura e correzione di EQ e larghezza
import json, subprocess, sys, os, numpy as np, soundfile as sf
from scipy import signal
R='/home/claude/riky-piano-man/build/riky_render_artefacts/Release/riky_render'
SFZ='/home/claude/ext/maestro/Maestro Concert Grand.sfz'
REF='/home/claude/work/montage/ref'; OUT='/home/claude/work/out/cal'; os.makedirs(OUT,exist_ok=True)
video=json.load(open('/home/claude/work/montage/montage.json'))
bands=[63,125,250,500,1000,2000,4000,8000]; sr=44100
def biq(kind,f0,g,q=0.707):
    A=10**(g/40); w=2*np.pi*f0/sr; c=np.cos(w); s=np.sin(w); al=s/(2*q)
    if kind=='peak': b=[1+al*A,-2*c,1-al*A]; a=[1+al/A,-2*c,1-al/A]
    elif kind=='ls':
        sq=2*np.sqrt(A)*al; b=[A*((A+1)-(A-1)*c+sq),2*A*((A-1)-(A+1)*c),A*((A+1)-(A-1)*c-sq)]; a=[(A+1)+(A-1)*c+sq,-2*((A-1)+(A+1)*c),(A+1)+(A-1)*c-sq]
    else:
        sq=2*np.sqrt(A)*al; b=[A*((A+1)+(A-1)*c+sq),-2*A*((A-1)+(A+1)*c),A*((A+1)+(A-1)*c-sq)]; a=[(A+1)-(A-1)*c+sq,2*((A-1)-(A+1)*c),(A+1)-(A-1)*c-sq]
    return np.array(b),np.array(a)
def bandresp(ba):
    b,a=ba; out=[]
    for cc in bands:
        f=np.geomspace(cc/2**.5,cc*2**.5,16); z=np.exp(-1j*2*np.pi*f/sr)
        h=20*np.log10(np.abs((b[0]+b[1]*z+b[2]*z*z)/(a[0]+a[1]*z+a[2]*z*z))); out.append(10*np.log10(np.mean(10**(h/10))))
    return np.array(out)
def measure(wav):
    m4a=wav.replace('.wav','.m4a'); dec=wav.replace('.wav','.aac.wav')
    subprocess.run(['ffmpeg','-v','error','-y','-i',wav,'-c:a','aac','-b:a','128k',m4a],check=True)
    subprocess.run(['ffmpeg','-v','error','-y','-i',m4a,'-ar','44100',dec],check=True)
    x,_=sf.read(dec); x=x[:int(13*sr)]; m=x.mean(1); s=(x[:,0]-x[:,1])/2
    f,p=signal.welch(m,sr,nperseg=8192)
    L=np.array([10*np.log10(np.mean(p[(f>=c/2**.5)&(f<c*2**.5)])+1e-20) for c in bands]); L-=L[4]
    side=20*np.log10(np.sqrt(np.mean(s**2))/(np.sqrt(np.mean(m**2))+1e-12))
    # livello percepito grezzo: rms 100 Hz - 5 kHz
    bb,aa=signal.butter(2,[100/(sr/2),5000/(sr/2)],'band'); y=signal.lfilter(bb,aa,m)
    return L, side, 20*np.log10(np.sqrt(np.mean(y**2))+1e-12)
def render(mid,vals,out):
    args=[R,SFZ,'--render',mid,out,'preset=0']+[f'{k}={v}' for k,v in vals.items()]
    r=subprocess.run(args,capture_output=True,text=True)
    if r.returncode: print(r.stdout,r.stderr); sys.exit(1)
def calibrate(vname, midname, vals, iters=3, lam=0.4, fitEq=True):
    mid=f'{REF}/{midname}.mid'; target=np.array(video[vname]['bands'][:8]); sideT=video[vname]['side_db']
    vals=dict(vals)
    for it in range(iters+1):
        out=f'{OUT}/{midname}_{it}.wav'; render(mid,vals,out); L,side,lev=measure(out)
        res=target-L; w=np.array([.6,1,1,1,1,1,1,.5])
        err=np.sqrt(np.mean((res*w)**2))
        print(f'  {vname:22s} it{it} errore {err:4.2f} dB  residuo {np.round(res,1)}  side {side:5.1f}/{sideT:5.1f}  width {vals.get("WIDTH",1):.2f}')
        if it==iters or not fitEq: break
        lmf=vals.get('EQLMIDF',500); hmf=vals.get('EQHMIDF',2500)
        B=np.stack([bandresp(biq('ls',120,1)),bandresp(biq('peak',lmf,1,.8)),bandresp(biq('peak',hmf,1,.8)),bandresp(biq('hs',6000,1))],1)
        X=(B-B[4])*w[:,None]; A=np.vstack([X,np.sqrt(lam)*np.eye(4)]); y=np.concatenate([res*w,np.zeros(4)])
        g,*_=np.linalg.lstsq(A,y,rcond=None)
        for k,dg in zip(['EQLOW','EQLMID','EQHMID','EQHIGH'],g):
            vals[k]=float(np.clip(round((vals.get(k,0)+dg)*2)/2,-12,12))
        if 'WIDTH' in vals and vals['WIDTH']>0:
            vals['WIDTH']=float(np.clip(round(vals['WIDTH']*10**((sideT-side)/20)*20)/20,0,2))
    return vals, err, lev
if __name__=='__main__':
    jobs=json.load(open(sys.argv[1])); res={}
    import itertools
    for j in jobs:
        grid=j.get('grid',{})
        keys=list(grid); best=None
        for combo in itertools.product(*[grid[k] for k in keys]) if keys else [()]:
            vals=dict(j['vals']); vals.update(dict(zip(keys,combo)))
            print('  prova',dict(zip(keys,combo)))
            v,e,lev=calibrate(j['video'],j['mid'],vals,j.get('iters',3),fitEq=j.get('fit',True))
            if best is None or e<best[1]: best=(v,e,lev)
        res[j['key']]=dict(vals=best[0],err=best[1],level=best[2])
        print('  MIGLIORE',j['key'],round(best[1],2),best[0])
    json.dump(res,open(sys.argv[2],'w'),indent=1)
