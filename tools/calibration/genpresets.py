import json, re, sys
c1=json.load(open('cal1.json')); c6=json.load(open('cal6.json')); c2=json.load(open('cal2.json')); c3=json.load(open('cal3.json')); c5=json.load(open('cal5.json'))
def V(src,k): return dict(src[k]['vals'])
trims=json.load(open('trims.json')) if len(sys.argv)>1 and sys.argv[1]=='trim' else {}
C3=V(c1,'C3')
ballad=dict(C3); ballad.update({'EQLOW':3,'EQLMID':-3.5,'L2':1,'L2SOUND':3,'L2VOL':-11,'ROOMMIX':0.26,'ARP':1,'ARPMODE':0,'ARPRATE':1,'ARPOCT':1,'ARPGATE':0.9,'ARPTARGET':1,'TEMPO':86})
mono=V(c2,'MONO'); mono['ROOMMIX']=0.08
P=[
 ('MAESTRO CONCERT GRAND','Maestro Concert Grand',"Il pianoforte com'è registrato, senza modifiche.",{},None),
 ('NASHVILLE C3','Nashville C3','Mezzacoda caldo passato su nastro.',C3,'C3'),
 ('U1 UPRIGHT','U1 verticale','Pianoforte verticale: medi in avanti, molto largo, stanza piccola.',V(c1,'U1'),'U1'),
 ('FELT PIANO','Felt Piano, pianoforte con feltro','Verticale con il feltro: morbido e ovattato.',V(c2,'FELT'),'FELT'),
 ('NASHVILLE C3 + DX EP','Nashville C3 con piano elettrico FM','Mezzacoda su nastro con un piano elettrico FM sotto.',V(c6,'C3DX'),'C3DX'),
 ('NASHVILLE C3 BRIGHT','Nashville C3 brillante','Il mezzacoda su nastro con più presenza e aria.',V(c1,'C3B'),'C3B'),
 ('U1 UPRIGHT BRIGHT','U1 verticale brillante','Il verticale con acuti più aperti.',V(c1,'U1B'),'U1B'),
 ('FELT PIANO BRIGHT','Felt Piano brillante','Feltro più leggero: morbido ma più chiaro.',V(c2,'FELTB'),'FELTB'),
 ('FELT MONO-SP PIANO','Felt Mono, feltro in mono con fruscio','Feltro in mono con il fruscio del nastro, come un vecchio disco.',mono,'MONO'),
 ('NASHVILLE C3 + SYNPAD','Nashville C3 con pad','Mezzacoda su nastro con un pad caldo.',V(c6,'C3PAD'),'C3PAD'),
 ('U1 UPRIGHT + RD EP','U1 verticale con piano elettrico a lamelle','Verticale con un piano elettrico a lamelle.',V(c6,'U1RD'),'U1RD'),
 ('FELT PIANO PAD','Felt Piano con pad','Pianoforte con feltro e un pad morbido.',V(c6,'FELTPAD'),'FELTPAD'),
 ('U1 UPRIGHT + FELT','U1 verticale con Felt Piano','Il verticale insieme alla sua versione con il feltro.',V(c6,'U1FELT'),'U1FELT'),
 ('NASHC3 + SYNFMBRASS','Nashville C3 con ottoni FM','Mezzacoda su nastro con ottoni sintetici FM.',V(c6,'C3BRASS'),'C3BRASS'),
 ('NASHC3 + FELT PAD','Nashville C3 con pad di feltro','Mezzacoda su nastro con un pad morbido.',V(c6,'C3FPAD'),'C3FPAD'),
 ('FELT PIANO ARP*4','Felt Piano con arpeggio','Pianoforte con feltro e arpeggio in sedicesimi su due ottave, a 125 BPM.',V(c3,'FELTARP'),'FELTARP'),
 ('C3 BALLAD ARP','C3 ballata con arpeggio','Mezzacoda in arpeggio di ottavi con un pad che tiene l\'accordo, a 86 BPM.',ballad,None),
]
ids={'EDIT':'editOn','FELT':'felt','ATTACK':'attack','HPF':'hpf','LPF':'lpf','EQLOW':'eqLow','EQLMID':'eqLMid','EQLMIDF':'eqLMidF','EQHMID':'eqHMid','EQHMIDF':'eqHMidF','EQHIGH':'eqHigh',
 'COMP':'compOn','COMPTHR':'compThr','COMPRATIO':'compRatio','COMPATT':'compAtt','COMPREL':'compRel','TAPE':'tape','NOISE':'noise','TRIM':'trim','L2':'l2On','L2SOUND':'l2Sound','L2VOL':'l2Vol','L2OCT':'l2Oct','L2ZONE':'l2Zone','SPLIT':'split',
 'ARP':'arpOn','ARPMODE':'arpMode','ARPRATE':'arpRate','ARPOCT':'arpOct','ARPGATE':'arpGate','ARPTARGET':'arpTarget','TEMPO':'tempo','TONE':'tone','WIDTH':'width','ROOM':'roomOn','ROOMSIZE':'roomSize','ROOMDAMP':'roomDamp','ROOMMIX':'roomMix'}
defaults={'EQLOW':0,'EQLMID':0,'EQHMID':0,'EQHIGH':0,'EQLMIDF':500,'EQHMIDF':2500,'WIDTH':1,'ROOMMIX':0.2,'ROOMSIZE':0.5,'L2':0,'L2SOUND':0,'L2VOL':-6,'ARPTARGET':0,'ARPMODE':0,'ARPRATE':3,'ARPOCT':1,'ARPGATE':0.8,'TRIM':0}
def fmt(v):
    v=float(v); s=('%.3f'%v).rstrip('0').rstrip('.'); return (s if '.' in s else s+'.')+'f' if s not in ('0',) else '0.f'
lines=[]
out={}
for i,(name,spoken,about,vals,key) in enumerate(P):
    vals=dict(vals)
    for k in ['EQLOW','EQLMID','EQHMID','EQHIGH']:
        if k in vals: vals[k]=max(-10,min(10,vals[k]))   # tetto prudente: le misure dal video hanno un margine di qualche dB
    if str(i) in trims: vals['TRIM']=trims[str(i)]
    vals={k:v for k,v in vals.items() if not (k in defaults and abs(float(v)-defaults[k])<1e-6)}
    out[i]=vals
    items=', '.join('{ id::%s, %s }'%(ids[k],fmt(v)) for k,v in vals.items())
    lines.append('        { "%s", "%s", "%s",\n          { %s } },'%(name,spoken,about.replace('"','\\"'),items) if items else '        { "%s", "%s", "%s", {} },'%(name,spoken,about))
table='''const std::vector<FactoryPreset>& factory()
{
    static const std::vector<FactoryPreset> list = {
'''+'\n'.join(lines)+'''
    };
    return list;
}
'''
p='/home/claude/riky-piano-man/Source/Presets.cpp'; s=open(p).read()
a=s.index('// RIKY_PRESET_TABLE_BEGIN')+len('// RIKY_PRESET_TABLE_BEGIN\n'); b=s.index('// RIKY_PRESET_TABLE_END')
s=s[:a]+table+s[b:]
open(p,'w').write(s)
json.dump(out,open('final_presets.json','w'),indent=1)
print('scritti',len(P),'preset')
