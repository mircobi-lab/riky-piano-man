import subprocess, json, sys, numpy as np, soundfile as sf
OUTJSON=sys.argv[1] if len(sys.argv)>1 else 'trims.json'
from scipy import signal
R='/home/claude/riky-piano-man/build/riky_render_artefacts/Release/riky_render'; SFZ='/home/claude/ext/maestro/Maestro Concert Grand.sfz'; REF='/home/claude/work/montage/ref'
mids=['Nashville_C3','Nashville_C3','U1_Upright','Felt_Piano','Nashville_C3pDX_EP','C3_Bright','U1_Upright_Bright','Felt_Piano_Bright','Felt_Mono-SP','Nashville_C3pSynPad','U1_Upright_p_Rd_EP','Felt_Piano_Pad','U1_Upright_p_Felt','NashC3_p_SynFMBrass','NashC3_p_Felt_Pad','Felt_Piano_ARPx4','Nashville_C3']
def level(f):
    x,sr=sf.read(f); m=x.mean(1)
    b,a=signal.butter(2,[100/(sr/2),5000/(sr/2)],'band'); y=signal.lfilter(b,a,m)
    # livello dei passaggi suonati: rms sui blocchi da 400 ms sopra il 50° percentile
    n=int(.4*sr); r=np.array([np.sqrt(np.mean(y[i:i+n]**2)) for i in range(0,len(y)-n,n//2)])
    return 20*np.log10(np.sqrt(np.mean(r[r>=np.percentile(r,50)]**2)))
out={}; peaks={}
for i in range(1,17):
    mid=f'{REF}/demo12.mid'
    subprocess.run([R,SFZ,'--render',mid,'/tmp/p0.wav','preset=0'],capture_output=True)
    r=subprocess.run([R,SFZ,'--render',mid,'/tmp/pi.wav',f'preset={i}'],capture_output=True,text=True)
    d=level('/tmp/p0.wav')-level('/tmp/pi.wav')
    out[str(i)]=float(np.clip(round(d*2)/2,-12,12)); peaks[i]=r.stdout.strip().split('picco')[-1]
    print(i, mids[i], 'trim %.1f'%out[str(i)], 'picco',peaks[i])
json.dump(out,open(OUTJSON,"w"),indent=1)
