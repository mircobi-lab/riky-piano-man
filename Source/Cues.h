#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace riky
{
// Segnali sonori per chi non vede lo schermo: metronomo, inizio e fine registrazione, export finito, errore.
// Vanno all'uscita del plugin dopo la catena (tono, stanza, volume): non entrano nelle registrazioni né negli export.
class Cues
{
public:
    enum Cue { recStart = 1, recStop = 2, done = 4, error = 8, confirm = 16 };

    void prepare (double sampleRate) { sr = sampleRate; for (auto& b : beeps) b.len = 0; }
    void post (Cue c) noexcept { pending.fetch_or ((int) c); }             // da qualsiasi thread
    void click (int offset, bool accent) noexcept { add (accent ? 1568.f : 1047.f, offset, .03, accent ? .32f : .2f, true); }
    void render (juce::AudioBuffer<float>&, bool enabled) noexcept;
    // click per i battiti che cadono tra tStart e tEnd (secondi del sequencer, negativi durante il conteggio);
    // con metronomo spento solo quelli del conteggio. Restituisce quanti click ha messo.
    int scheduleBeats (double tStart, double tEnd, double bpm, bool metronomeOn, int numSamples) noexcept;

private:
    struct Beep { double phase = 0, inc = 0; int start = 0, len = 0, pos = 0; float amp = 0; bool click = false; };
    std::array<Beep, 24> beeps;
    std::atomic<int> pending { 0 };
    double sr = 44100.0;
    void add (float freq, int startOffset, double seconds, float amp, bool click = false) noexcept;
};
}
