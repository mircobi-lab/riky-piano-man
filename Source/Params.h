#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Parametri di Riky Piano Man. Gli ID restano stabili: sono salvati nei progetti e nelle mappe MIDI Learn.
namespace riky
{
namespace id
{
    // PIANO
    inline constexpr const char* tone      = "TONE";      // -1 scuro .. +1 brillante (inclinazione EQ)
    inline constexpr const char* tune      = "TUNE";      // centesimi -50 .. +50
    inline constexpr const char* transpose = "TRANSP";    // semitoni -12 .. +12
    inline constexpr const char* a4ref     = "A4REF";     // 440 Hz / 432 Hz
    inline constexpr const char* velCurve  = "VELCURVE";  // NORMAL / SOFT / HARD / FIXED
    // SFZ CONTROLS: 8 posti automatizzabili, collegati ai CC che il file SFZ dichiara (label_cc)
    inline const char* sfzSlot (int i) { static const char* ids[] = { "SFZ1", "SFZ2", "SFZ3", "SFZ4", "SFZ5", "SFZ6", "SFZ7", "SFZ8" }; return ids[i]; }
    // ENGINE
    inline constexpr const char* quality   = "QUALITY";   // ricampionamento dal vivo: STANDARD / HIGH / ULTRA
    inline constexpr const char* poly      = "POLY";      // polifonia 64 / 128 / 256
    // TUNING
    inline constexpr const char* temper    = "TEMPER";    // temperamento
    inline constexpr const char* temperKey = "TEMPKEY";   // tonica del temperamento
    // PEDAL
    inline constexpr const char* pedal     = "PEDAL";     // sustain dal pannello (si somma al CC 64)
    inline constexpr const char* soft      = "SOFT";      // una corda dal pannello (si somma al CC 67)
    inline constexpr const char* softAmt   = "SOFTAMT";
    // ROOM
    inline constexpr const char* roomOn    = "ROOM";
    inline constexpr const char* roomSize  = "ROOMSIZE";
    inline constexpr const char* roomDamp  = "ROOMDAMP";
    inline constexpr const char* roomMix   = "ROOMMIX";
    inline constexpr const char* width     = "WIDTH";     // larghezza stereo (mid/side)
    // OUTPUT
    inline constexpr const char* volume    = "VOLUME";    // dB
    // SEQUENCER: tempo, metronomo, conteggio, segnali sonori di conferma
    inline constexpr const char* tempo     = "TEMPO";     // BPM per metronomo, conteggio e file MIDI salvati
    inline constexpr const char* metro     = "METRO";     // metronomo durante la registrazione
    inline constexpr const char* countIn   = "COUNTIN";   // conteggio prima di registrare: OFF / 1 BAR / 2 BARS
    inline constexpr const char* cues      = "CUES";      // segnali sonori (inizio e fine registrazione, export finito, errori)
    // comandi del sequencer, da assegnare ai tasti del controller con MIDI LEARN (scattano quando vanno su ON)
    inline constexpr const char* seqPlay   = "SEQPLAY";
    inline constexpr const char* seqStop   = "SEQSTOP";
    inline constexpr const char* seqRec    = "SEQREC";
    // MODIFICA: suono su misura (strato dello strumento ed effetti, uguali nel plugin e negli export)
    inline constexpr const char* editOn    = "EDIT";      // tutta la sezione accesa o esclusa
    inline constexpr const char* felt      = "FELT";      // feltro: filtro per ogni nota che segue la dinamica
    inline constexpr const char* attack    = "ATTACK";    // attacco ammorbidito
    inline constexpr const char* hpf       = "HPF";       // passa-alto (Hz)
    inline constexpr const char* lpf       = "LPF";       // passa-basso (Hz)
    inline constexpr const char* eqLow     = "EQLOW";     // scaffale basso 120 Hz (dB)
    inline constexpr const char* eqLMid    = "EQLMID";    // medio-basso (dB)
    inline constexpr const char* eqLMidF   = "EQLMIDF";   // frequenza medio-basso (Hz)
    inline constexpr const char* eqHMid    = "EQHMID";    // medio-alto (dB)
    inline constexpr const char* eqHMidF   = "EQHMIDF";   // frequenza medio-alto (Hz)
    inline constexpr const char* eqHigh    = "EQHIGH";    // scaffale alto 6 kHz (dB)
    inline constexpr const char* compOn    = "COMP";
    inline constexpr const char* compThr   = "COMPTHR";   // soglia (dB)
    inline constexpr const char* compRatio = "COMPRATIO";
    inline constexpr const char* compAtt   = "COMPATT";   // ms
    inline constexpr const char* compRel   = "COMPREL";   // ms
    inline constexpr const char* tape      = "TAPE";      // saturazione tipo nastro
    inline constexpr const char* noise     = "NOISE";     // fruscio del nastro, segue il suono
    inline constexpr const char* trim      = "TRIM";      // livello del suono su misura (dB)
    // SECONDO STRATO
    inline constexpr const char* l2On      = "L2";
    inline constexpr const char* l2Sound   = "L2SOUND";
    inline constexpr const char* l2Vol     = "L2VOL";     // dB
    inline constexpr const char* l2Oct     = "L2OCT";     // ottave
    inline constexpr const char* l2Zone    = "L2ZONE";    // tutta la tastiera / sotto lo split / sopra lo split
    inline constexpr const char* split     = "SPLIT";     // nota di divisione (la nota appartiene alla zona alta)
    // ARPEGGIATORE
    inline constexpr const char* arpOn     = "ARP";
    inline constexpr const char* arpMode   = "ARPMODE";
    inline constexpr const char* arpRate   = "ARPRATE";
    inline constexpr const char* arpOct    = "ARPOCT";
    inline constexpr const char* arpGate   = "ARPGATE";
    inline constexpr const char* arpTarget = "ARPTARGET"; // entrambi / pianoforte / secondo strato
}

// parametri che un preset del suono su misura imposta (gli altri, come volume, accordatura e sequencer, restano)
const std::vector<const char*>& soundParams();
const juce::StringArray& soundParamIds();
juce::StringArray layerSoundNames();      // nomi visivi
juce::StringArray layerSoundNamesIt();    // nomi per il lettore di schermo
juce::StringArray arpModeNames();
juce::StringArray arpRateNames();
inline double arpRateBeats (int i) { static const double r[] = { 1.0, .5, 1.0 / 3.0, .25, 1.0 / 6.0, .125 }; return r[juce::jlimit (0, 5, i)]; }

inline constexpr int numSlots = 8;
// CC dei pedali: li gestisce la sezione PEDAL e non finiscono negli SFZ CONTROLS
namespace pedalCC
{
    inline constexpr int sustain = 64, sostenuto = 66, soft = 67;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
juce::String formatValue (const juce::String& paramId, float value);
juce::String accessibleValue (const juce::String& paramId, float value);   // per il lettore di schermo, in italiano
inline float a4Frequency (int choice) { return choice == 1 ? 432.f : 440.f; }
inline int sampleQuality (int choice) { return choice == 2 ? 10 : choice == 1 ? 4 : 2; }   // Hermite, sinc 12 punti, massima (come nei bounce)
inline int polyphony (int choice) { return choice == 0 ? 64 : choice == 2 ? 256 : 128; }

// Temperamenti: centesimi sopra la tonica per i 12 gradi.
// Werckmeister III e Kirnberger III dalle tabelle di Wikipedia; Vallotti dagli scarti di Wikipedia riportati a Do = 0.
// Mesotonico 1/4 di comma, pitagorico e naturale calcolati dai rapporti (catena di quinte da Mib a Sol#).
juce::StringArray temperamentNames();
const double* temperamentCents (int index);        // 12 valori
std::string temperamentScala (int index);           // testo .scl per sfizz (vuoto = equabile)
}
