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
    // MAESTRO: controlli dello strumento SFZ (CC definiti nel file del Maestro Concert Grand)
    inline constexpr const char* dynamics  = "DYNAMICS";  // CC 99  veltrack
    inline constexpr const char* release   = "RELEASE";   // CC 72  durata del rilascio
    inline constexpr const char* keyNoise  = "KEYNOISE";  // CC 80  rumore dei tasti al rilascio
    inline constexpr const char* pedNoise  = "PEDNOISE";  // CC 81  rumore dei pedali
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

// CC del file "Maestro Concert Grand.sfz" (righe #define in cima al file)
namespace maestroCC
{
    inline constexpr int sustain = 64, sostenuto = 66, lid = 67, release = 72, keyNoise = 80, pedNoise = 81, veltrack = 99;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
juce::String formatValue (const juce::String& paramId, float value);
inline float a4Frequency (int choice) { return choice == 1 ? 432.f : 440.f; }
}
