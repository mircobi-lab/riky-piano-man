import json, numpy as np, itertools
v=json.load(open('/home/claude/work/montage/montage.json')); m=json.load(open('/home/claude/work/montage/ref/maestro.json'))
bands=np.array([63,125,250,500,1000,2000,4000,8000]); sr=44100
def biq(kind,f0,g,q=0.707):
    A=10**(g/40); w=2*np.pi*f0/sr; c=np.cos(w); s=np.sin(w); al=s/(2*q)
    if kind=='peak': b=[1+al*A,-2*c,1-al*A]; a=[1+al/A,-2*c,1-al/A]
    elif kind=='ls':
        sq=2*np.sqrt(A)*al; b=[A*((A+1)-(A-1)*c+sq),2*A*((A-1)-(A+1)*c),A*((A+1)-(A-1)*c-sq)]; a=[(A+1)+(A-1)*c+sq,-2*((A-1)+(A+1)*c),(A+1)+(A-1)*c-sq]
    elif kind=='hs':
        sq=2*np.sqrt(A)*al; b=[A*((A+1)+(A-1)*c+sq),-2*A*((A-1)+(A+1)*c),A*((A+1)+(A-1)*c-sq)]; a=[(A+1)-(A-1)*c+sq,2*((A-1)-(A+1)*c),(A+1)-(A-1)*c-sq]
    elif kind=='lp':
        b=[(1-c)/2,1-c,(1-c)/2]; a=[1+al,-2*c,1-al]
    return np.array(b),np.array(a)
def resp(ba,f):
    b,a=ba; z=np.exp(-1j*2*np.pi*np.asarray(f)/sr)
    return 20*np.log10(np.abs((b[0]+b[1]*z+b[2]*z*z)/(a[0]+a[1]*z+a[2]*z*z)))
def bandresp(ba):
    out=[]
    for c in bands:
        f=np.geomspace(c/2**.5,c*2**.5,16); out.append(10*np.log10(np.mean(10**(resp(ba,f)/10))))
    return np.array(out)
basis={'low':('ls',120),'lmid':('peak',400),'hmid':('peak',2500),'high':('hs',6000)}
res={}
for name in v:
    d=np.array(v[name]['bands'][:8])-np.array(m[name]['bands'][:8])
    best=None
    for lp,lmf in itertools.product([None,800,1000,1300,1700,2200,3000,4500],[250,400,700,1000]):
        B=np.stack([bandresp(biq(k,(lmf if n=='lmid' else f),1.0,0.8 if k=='peak' else .707)) for n,(k,f) in basis.items()],1)
        lr=bandresp(biq('lp',lp,0,0.6)) if lp else np.zeros(8)
        lr=lr-lr[4]
        t=d-lr
        # pesi: 8k meno affidabile
        w=np.array([.6,1,1,1,1,1,1,.5])
        X=(B-B[4])*w[:,None]; lam=0.35; A=np.vstack([X,np.sqrt(lam)*np.eye(4)]); y=np.concatenate([t*w,np.zeros(4)]); g,*_=np.linalg.lstsq(A,y,rcond=None); g=np.clip(g,-12,12)
        err=np.sqrt(np.mean(((X@g)-t*w)**2))+ (0.4 if lp else 0)
        if best is None or err<best[0]: best=(err,lp,g,lmf)
    err,lp,g,lmf=best
    width=None
    res[name]=dict(lmf=lmf,diff=[round(x,1) for x in d],lp=lp,gains=[round(x,1) for x in g],rms_err=round(err,2),corr_video=v[name]['corr'],corr_maestro=m[name]['corr'],side_video=v[name]['side_db'],side_maestro=m[name]['side_db'])
    print(f"{name:22s} diff {np.round(d,1)} -> LP {lp} LMf {lmf} gains L/LM/HM/H {np.round(g,1)} err {err:.2f} | side {v[name]['side_db']-m[name]['side_db']:+.1f} dB")
json.dump(res,open('/home/claude/work/montage/fit.json','w'),indent=1)
