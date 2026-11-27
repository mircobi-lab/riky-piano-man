# Stima grezza delle note usate: salienza armonica su spettro, istogramma 21..108
import sys, json, numpy as np, soundfile as sf
from scipy import signal
import mido, random
x, sr = sf.read(sys.argv[1]); segs=json.load(open(sys.argv[2])); outdir=sys.argv[3]
midi_f = 440*2**((np.arange(21,109)-69)/12)
res={}
for name,(a,b) in segs.items():
    y=x[int((a+1)*sr):int((b-1)*sr)].mean(1)
    f,t,Z=signal.stft(y,sr,nperseg=8192,noverlap=8192-2048)
    M=np.abs(Z)
    sal=np.zeros((len(midi_f),M.shape[1]))
    for k,f0 in enumerate(midi_f):
        for h,w in zip(range(1,6),[1,.6,.4,.3,.2]):
            fh=f0*h
            if fh>5000: break
            i=int(round(fh/(f[1]))); sal[k]+=w*M[max(i-1,0):i+2].max(0)
    # sottrai armoniche: per frame tieni i picchi più forti
    hist=np.zeros(len(midi_f)); vel=[]
    frame_rms=np.sqrt((M**2).sum(0))
    for j in range(sal.shape[1]):
        s=sal[:,j].copy()
        if frame_rms[j]<frame_rms.max()*0.03: continue
        for _ in range(4):
            k=int(s.argmax()); 
            if s[k]<=0: break
            hist[k]+=s[k]
            for d in (0,12,19,24,28): 
                if k+d<len(s): s[k+d]=0
            if k-12>=0: s[k-12]*=0.5
    hist/=hist.sum()
    mean_note=float((hist*np.arange(21,109)).sum())
    res[name]=dict(mean_note=round(mean_note,1), hist=hist.tolist())
    # MIDI di riferimento: accordi casuali dalle note stimate, 24 s
    rnd=random.Random(1); mf=mido.MidiFile(ticks_per_beat=480); tr=mido.MidiTrack(); mf.tracks.append(tr)
    tr.append(mido.MetaMessage('set_tempo',tempo=500000))
    ev=[]; tick=0
    for c in range(48):
        notes=set(rnd.choices(range(21,109),weights=hist,k=3))
        v=max(20,min(120,int(rnd.gauss(72,18))))
        on=c*240; dur=rnd.choice([240,480,720])
        for n in notes: ev.append((on,'on',n,v)); ev.append((on+dur,'off',n,0))
        if c%8==0: ev.append((on+10,'cc',64,127))
        if c%8==7: ev.append((on+230,'cc',64,0))
    ev.append((48*240+1920,'off',60,0))
    ev.sort(key=lambda e:(e[0], e[1]!='off'))
    last=0
    for tk,typ,n,v in ev:
        d=tk-last; last=tk
        if typ=='on': tr.append(mido.Message('note_on',note=n,velocity=v,time=d))
        elif typ=='off': tr.append(mido.Message('note_off',note=n,velocity=0,time=d))
        else: tr.append(mido.Message('control_change',control=n,value=v,time=d))
    fn=outdir+'/'+name.replace(' ','_').replace('+','p').replace('*','x')+'.mid'; mf.save(fn)
    top=np.argsort(hist)[-5:][::-1]+21
    print(f"{name:24s} nota media {mean_note:5.1f}  note principali {list(top)}")
json.dump(res,open(outdir+'/notes.json','w'))
