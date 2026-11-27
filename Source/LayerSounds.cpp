#include "LayerSounds.h"

namespace riky
{
// Suoni del secondo strato: strumenti SFZ originali scritti per Riky Piano Man, che usano gli oscillatori di sfizz
// (sine, saw, square, triangle) con inviluppi, filtri e LFO. Nessun campione di terzi, nessuna licenza da rispettare.
// Ogni regione ha il suo volume (in SFZ il volume della regione sostituisce quello di <global>, non si somma).
// Livelli: con il volume dello strato a 0 dB ogni suono è forte quanto il Maestro su "Riky's Theme".
juce::String layerSfzText (int index)
{
    switch (index)
    {
        case 1: return "// FM E.PIANO - Riky Piano Man (originale, oscillatori di sfizz)\n"
            "<global> lokey=0 hikey=127 pitch_keycenter=60 ampeg_attack=0.002 ampeg_sustain=0 ampeg_release=0.45 amp_veltrack=80\n"
            "<region> sample=*sine ampeg_decay=5 volume=-19.4\n"
            "<region> sample=*sine transpose=12 ampeg_decay=1.6 volume=-32.4\n"
            "<region> sample=*sine transpose=45 tune=-14 ampeg_decay=0.28 amp_veltrack=100 amp_velcurve_64=0.12 amp_velcurve_127=1 volume=-35.4\n"
            "<region> sample=*sine transpose=31 ampeg_decay=0.5 amp_veltrack=100 volume=-41.4\n";
        case 2: return "// TINE E.PIANO - Riky Piano Man (originale, oscillatori di sfizz)\n"
            "<global> lokey=0 hikey=127 pitch_keycenter=60 ampeg_attack=0.003 ampeg_sustain=0 ampeg_release=0.4 amp_veltrack=85 amplfo_freq=4.2 amplfo_depth=1.2\n"
            "<region> sample=*sine ampeg_decay=6 volume=-19.2\n"
            "<region> sample=*sine transpose=12 ampeg_decay=2.5 volume=-36.2\n"
            "<region> sample=*saw fil_type=lpf_2p cutoff=250 fil_veltrack=4200 ampeg_decay=0.7 amp_veltrack=100 amp_velcurve_64=0.2 amp_velcurve_127=1 volume=-30.2\n"
            "<region> sample=*triangle transpose=28 ampeg_decay=0.15 amp_veltrack=100 volume=-46.2\n";
        case 3: return "// WARM PAD - Riky Piano Man (originale, oscillatori di sfizz)\n"
            "<global> lokey=0 hikey=127 pitch_keycenter=60 ampeg_attack=0.9 ampeg_sustain=100 ampeg_release=1.8 amp_veltrack=40 fil_type=lpf_2p cutoff=950 resonance=1 fil_veltrack=1200 fillfo_freq=0.15 fillfo_depth=300\n"
            "<region> sample=*saw tune=-8 pan=-45 volume=-40.1\n"
            "<region> sample=*saw tune=8 pan=45 volume=-40.1\n"
            "<region> sample=*triangle volume=-33.1\n";
        case 4: return "// SYNTH PAD - Riky Piano Man (originale, oscillatori di sfizz)\n"
            "<global> lokey=0 hikey=127 pitch_keycenter=60 ampeg_attack=0.45 ampeg_sustain=100 ampeg_release=1.3 amp_veltrack=50 fil_type=lpf_2p cutoff=2400 resonance=2 fillfo_freq=0.22 fillfo_depth=700\n"
            "<region> sample=*saw tune=-12 pan=-60 volume=-42.4\n"
            "<region> sample=*saw tune=12 pan=60 volume=-42.4\n"
            "<region> sample=*saw transpose=12 tune=3 volume=-33.4\n"
            "<region> sample=*square transpose=-12 volume=-35.4\n";
        case 5: return "// FM BRASS - Riky Piano Man (originale, oscillatori di sfizz)\n"
            "<global> lokey=0 hikey=127 pitch_keycenter=60 ampeg_attack=0.035 ampeg_sustain=100 ampeg_release=0.25 amp_veltrack=60 fil_type=lpf_2p cutoff=380 resonance=1.5 fil_veltrack=1500 fileg_attack=0.07 fileg_decay=0.6 fileg_sustain=45 fileg_depth=3400\n"
            "<region> sample=*saw tune=-5 pan=-25 volume=-35.4\n"
            "<region> sample=*saw tune=5 pan=25 volume=-35.4\n"
            "<region> sample=*square transpose=-12 volume=-33.4\n";
        case 6: return "// FELT PAD - Riky Piano Man (originale, oscillatori di sfizz)\n"
            "<global> lokey=0 hikey=127 pitch_keycenter=60 ampeg_attack=0.35 ampeg_sustain=100 ampeg_release=2.2 amp_veltrack=35 fil_type=lpf_2p cutoff=650 fil_veltrack=900 amplfo_freq=0.3 amplfo_depth=0.8\n"
            "<region> sample=*triangle tune=-4 pan=-30 volume=-37.7\n"
            "<region> sample=*triangle tune=4 pan=30 volume=-37.7\n"
            "<region> sample=*sine transpose=12 volume=-39.7\n";
        case 7: return "// SYNTH BASS - Riky Piano Man (originale, oscillatori di sfizz)\n"
            "<global> lokey=0 hikey=127 pitch_keycenter=60 ampeg_attack=0.003 ampeg_decay=0.8 ampeg_sustain=55 ampeg_release=0.12 amp_veltrack=70 fil_type=lpf_2p cutoff=260 resonance=3 fil_veltrack=1800 fileg_decay=0.25 fileg_sustain=0 fileg_depth=2600\n"
            "<region> sample=*saw volume=-34.5\n"
            "<region> sample=*square tune=7 volume=-28.5\n"
            "<region> sample=*sine transpose=-12 volume=-26.5\n";
        default: return {};   // 0: pianoforte con feltro, cioè lo strumento caricato con il feltro
    }
}
}
