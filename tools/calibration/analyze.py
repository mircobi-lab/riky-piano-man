import sys, json, numpy as np, soundfile as sf
from scipy import signal
x, sr = sf.read(sys.argv[1])
segs = json.load(open(sys.argv[2]))
bands = [63,125,250,500,1000,2000,4000,8000,12000]
def ltas(y):
    f, p = signal.welch(y, sr, nperseg=8192)
    out=[]
    for c in bands:
        lo, hi = c/2**0.5, c*2**0.5
        m=(f>=lo)&(f<hi); out.append(10*np.log10(np.mean(p[m])+1e-20))
    return np.array(out), f, p
res={}
for name,(a,b) in segs.items():
    y = x[int((a+1)*sr):int((b-1)*sr)]
    m = y.mean(1); s=(y[:,0]-y[:,1])/2
    L, f, p = ltas(m)
    L -= L[4]  # rel 1 kHz
    rms = np.sqrt(np.mean(m**2)); peak=np.max(np.abs(y))
    # short-term loudness range (400ms rms)
    n=int(0.4*sr); fr=np.array([np.sqrt(np.mean(m[i:i+n]**2)) for i in range(0,len(m)-n,n//2)])
    fr=20*np.log10(fr[fr>1e-5]); lra=np.percentile(fr,95)-np.percentile(fr,10)
    corr=np.corrcoef(y[:,0],y[:,1])[0,1]
    side=20*np.log10(np.sqrt(np.mean(s**2))/ (np.sqrt(np.mean(((y[:,0]+y[:,1])/2)**2))+1e-12))
    cen=np.sum(f*p)/np.sum(p)
    res[name]=dict(bands=[round(v,1) for v in L], crest=round(20*np.log10(peak/rms),1), lra=round(lra,1), corr=round(corr,3), side_db=round(side,1), centroid=int(cen), rms_db=round(20*np.log10(rms),1))
    print(f"{name:24s} "+" ".join(f"{v:6.1f}" for v in L)+f" | crest {res[name]['crest']:5.1f} lra {res[name]['lra']:5.1f} corr {corr:5.2f} side {side:6.1f} cen {int(cen)} rms {res[name]['rms_db']}")
json.dump(res, open(sys.argv[3],'w'), indent=1)
