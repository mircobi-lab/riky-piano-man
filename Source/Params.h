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
}

inline constexpr int numSlots = 8;
// CC dei pedali: li gestisce la sezione PEDAL e non finiscono negli SFZ CONTROLS
namespace pedalCC
{
    inline constexpr int sustain = 64, sostenuto = 66, soft = 67;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
juce::String formatValue (const juce::String& paramId, float value);
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
