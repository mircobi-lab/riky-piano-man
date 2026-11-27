# Genera LayerSounds.cpp: ogni regione con il suo volume esplicito (in SFZ il volume della regione sostituisce quello della sezione)
import re, json, sys, os
HERE=os.path.dirname(os.path.abspath(__file__))
order=['fm_ep','tine_ep','warm_pad','synth_pad','fm_brass','felt_pad','synth_bass']
adj=json.load(open(sys.argv[1])) if len(sys.argv)>1 else {n:0 for n in order}
out=['#include "LayerSounds.h"','','namespace riky','{',
'// Suoni del secondo strato: strumenti SFZ originali scritti per Riky Piano Man, che usano gli oscillatori di sfizz',
'// (sine, saw, square, triangle) con inviluppi, filtri e LFO. Nessun campione di terzi, nessuna licenza da rispettare.',
'// Ogni regione ha il suo volume (in SFZ il volume della regione sostituisce quello di <global>, non si somma).',
'// Livelli: con il volume dello strato a 0 dB ogni suono è forte quanto il Maestro su "Riky\'s Theme".',
'juce::String layerSfzText (int index)','{','    switch (index)','    {']
for i,n in enumerate(order,1):
    lines=open(os.path.join(HERE,'layers',n+'.sfz')).read().strip().split('\n')
    g=[l for l in lines if l.startswith('<global>')][0]
    gv=float(re.search(r'volume=(-?[\d.]+)',g).group(1)) if 'volume=' in g else 0.0
    new=[]
    for l in lines:
        if l.startswith('<global>'): l=re.sub(r'\s*volume=-?[\d.]+','',l)
        elif l.startswith('<region>'):
            m=re.search(r'volume=(-?[\d.]+)',l); v=(float(m.group(1)) if m else gv)+adj[n]
            l=re.sub(r'\s*volume=-?[\d.]+','',l)+' volume=%g'%round(v,1)
        new.append(l)
    body='\\n"\n            "'.join(x.replace('"','\\"') for x in new)
    out.append(f'        case {i}: return "{body}\\n";')
out+=['        default: return {};   // 0: pianoforte con feltro, cioè lo strumento caricato con il feltro','    }','}','}','']
open(os.path.join(HERE,'..','..','Source','LayerSounds.cpp'),'w').write('\n'.join(out))
