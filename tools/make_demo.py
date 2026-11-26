# Genera Resources/demo-riky-theme.mid: "Riky's Theme", brano originale per la demo di Riky Piano Man.
# Mib maggiore, 72 BPM, 4/4. Introduzione, tema A, sviluppo B piu' forte, ripresa e finale con arpeggio.
# Uso: python3 tools/make_demo.py   (serve il modulo mido)
import random, mido

TPB = 480
BPM = 72
rng = random.Random(1965)          # umanizzazione ripetibile

# accordi della mano sinistra: basso e quattro note per l'arpeggio di crome
CH = {
    'Eb':    [39, 46, 51, 55, 58],
    'Cm7':   [36, 43, 48, 51, 58],
    'Cm':    [36, 43, 48, 51, 55],
    'Abmaj7':[44, 51, 55, 60, 63],
    'Ab':    [44, 51, 56, 60, 63],
    'Bbsus': [46, 53, 58, 63, 65],
    'Bb':    [46, 53, 58, 62, 65],
    'Eb/G':  [43, 51, 55, 58, 63],
    'Eb/Bb': [46, 51, 55, 58, 63],
    'Fm7':   [41, 48, 51, 56, 60],
    'Bb7':   [46, 53, 56, 62, 65],
}
PATTERN = [0, 1, 2, 3, 4, 3, 2, 1]   # crome: basso, quinta, ottava, decima...

events = []   # (beat, tipo, nota/cc, valore)
def note(beat, dur, n, vel):
    v = max(1, min(127, int(vel + rng.uniform(-4, 4))))
    j = rng.uniform(-0.012, 0.012)
    events.append((beat + j, 'on', n, v))
    events.append((beat + j + dur * 0.97, 'off', n, 0))

def pedal(beat):
    # pedale "sincopato": su appena dopo il cambio di accordo, giu' subito dopo
    events.append((beat + 0.03, 'cc', 64, 0))
    events.append((beat + 0.18, 'cc', 64, 127))

def lh(bar, chords, vel):
    # chords: un accordo per battuta, oppure due (mezza battuta ciascuno)
    b0 = bar * 4
    if len(chords) == 1:
        c = CH[chords[0]]
        pedal(b0)
        for i, idx in enumerate(PATTERN):
            note(b0 + i * 0.5, 0.5 if i else 2.0, c[idx], vel + (6 if i == 0 else 0))
    else:
        for h, name in enumerate(chords):
            c = CH[name]
            pedal(b0 + 2 * h)
            for i, idx in enumerate([0, 1, 2, 3]):
                note(b0 + 2 * h + i * 0.5, 0.5 if i else 1.5, c[idx], vel + (6 if i == 0 else 0))

def rh(bar, notes, vel, octave=False):
    for beat, dur, n in notes:
        note(bar * 4 + beat, dur, n, vel)
        if octave:
            note(bar * 4 + beat, dur, n - 12, vel - 14)

A_CHORDS = [['Eb'], ['Cm7'], ['Abmaj7'], ['Bbsus', 'Bb'], ['Eb/G'], ['Abmaj7'], ['Fm7'], ['Bb7']]
A_MELODY = [
    [(0, 1.5, 70), (1.5, .5, 67), (2, 1, 70), (3, 1, 75)],
    [(0, 1.5, 74), (1.5, .5, 72), (2, 2, 70)],
    [(0, 1, 72), (1, 1, 75), (2, 1.5, 79), (3.5, .5, 77)],
    [(0, 2, 75), (2, 2, 74)],
    [(0, 1.5, 70), (1.5, .5, 67), (2, 1, 70), (3, 1, 79)],
    [(0, 1.5, 77), (1.5, .5, 75), (2, 2, 72)],
    [(0, 1, 68), (1, 1, 72), (2, 1, 75), (3, 1, 74)],
    [(0, 2, 74), (2, 2, 70)],
]
B_CHORDS = [['Cm'], ['Ab'], ['Eb/Bb'], ['Bb'], ['Cm7'], ['Abmaj7'], ['Fm7', 'Bb7'], ['Eb']]
B_MELODY = [
    [(0, 1.5, 79), (1.5, .5, 77), (2, 1, 75), (3, 1, 79)],
    [(0, 2, 84), (2, 1, 82), (3, 1, 80)],
    [(0, 1.5, 79), (1.5, .5, 75), (2, 1, 77), (3, 1, 79)],
    [(0, 3, 77), (3, 1, 74)],
    [(0, 1, 75), (1, 1, 79), (2, 1.5, 84), (3.5, .5, 82)],
    [(0, 1, 80), (1, 1, 79), (2, 1, 77), (3, 1, 75)],
    [(0, 1, 77), (1, 1, 80), (2, 2, 74)],
    [(0, 4, 75)],
]

bar = 0
# introduzione: solo arpeggi piano, note lunghe in alto
for i, ch in enumerate(A_CHORDS[:4]):
    lh(bar, ch, 44 + i * 2)
    rh(bar, [(0, 3.5, [87, 86, 84, 82][i])], 40)
    bar += 1
# tema A, mezzo piano
for ch, mel in zip(A_CHORDS, A_MELODY):
    lh(bar, ch, 52); rh(bar, mel, 72); bar += 1
# sviluppo B, forte, melodia in ottave
for i, (ch, mel) in enumerate(zip(B_CHORDS, B_MELODY)):
    lh(bar, ch, 60 + min(i, 4)); rh(bar, mel, 90 + min(i, 4) * 2, octave=True); bar += 1
# ripresa del tema, piu' piano
for i, (ch, mel) in enumerate(zip(A_CHORDS[:4], A_MELODY[:4])):
    lh(bar, ch, 48 - i * 2); rh(bar, mel, 66 - i * 3); bar += 1
# finale: arpeggio di Lab maj7, sospensione su Sib, accordo di Mib con la nona
lh(bar, ['Abmaj7'], 46)
for i, n in enumerate([68, 72, 75, 79, 80, 84, 87, 91]):
    note(bar * 4 + i * 0.25, 4 - i * 0.25, n, 50 + i * 2)
bar += 1
lh(bar, ['Bbsus', 'Bb'], 44)
rh(bar, [(0, 2, 70), (0, 2, 75), (0, 2, 77), (2, 2, 70), (2, 2, 74), (2, 2, 77)], 52)
bar += 1
pedal(bar * 4)
note(bar * 4, 8, 27, 58)            # Mib1
note(bar * 4 + .02, 8, 39, 52)      # Mib2
note(bar * 4 + .05, 8, 46, 48)      # Sib2
for i, n in enumerate([63, 67, 70, 77, 79, 82, 87]):   # accordo arpeggiato: Mib Sol Sib Fa Sol Sib Mib
    note(bar * 4 + .2 + i * .09, 8 - i * .09, n, 54 - i * 2)
end = bar * 4 + 8.5
events.append((end, 'cc', 64, 0))

# file MIDI tipo 0, un canale
mid = mido.MidiFile(type=0, ticks_per_beat=TPB)
tr = mido.MidiTrack(); mid.tracks.append(tr)
tr.append(mido.MetaMessage('track_name', name="Riky's Theme (MBStudio91)", time=0))
tr.append(mido.MetaMessage('set_tempo', tempo=mido.bpm2tempo(BPM), time=0))
tr.append(mido.MetaMessage('time_signature', numerator=4, denominator=4, time=0))
order = {'off': 0, 'cc': 1, 'on': 2}
evs = sorted(((max(0, b), k, n, v) for b, k, n, v in events), key=lambda e: (round(e[0] * TPB), order[e[1]]))
last = 0
for b, k, n, v in evs:
    t = int(round(b * TPB)); d = t - last; last = t
    if k == 'on': tr.append(mido.Message('note_on', note=n, velocity=v, time=d))
    elif k == 'off': tr.append(mido.Message('note_off', note=n, velocity=64, time=d))
    else: tr.append(mido.Message('control_change', control=n, value=v, time=d))
tr.append(mido.MetaMessage('end_of_track', time=TPB))
mid.save('Resources/demo-riky-theme.mid')
print('battute', bar + 3, 'durata', round(mid.length, 1), 's', 'note', sum(1 for e in evs if e[1] == 'on'))
