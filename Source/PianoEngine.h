#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <array>
#include <atomic>
#include <memory>
#include <thread>

namespace sfz { class Sfizz; }

namespace riky
{
// Impostazioni lette a ogni blocco dal thread audio
struct EngineSettings
{
    float a4 = 440.f, cents = 0.f;
    int transpose = 0;
    int velCurve = 0;              // 0 NORMAL, 1 SOFT, 2 HARD, 3 FIXED
    bool pedalButton = false, softButton = false;
    float softAmount = .4f;
    float dynamics = 1.f, release = .575f, keyNoise = 1.f, pedNoise = .1f;   // CC del file SFZ (0..1)
    bool offline = false;          // bounce o render non in tempo reale: aspetta i campioni dal disco
};

// Motore del pianoforte: sfizz (BSD-2) suona il file SFZ del Maestro Concert Grand così com'è,
// con regioni di rilascio, rumori dei pedali, round robin e lettura dei campioni dal disco.
// Il caricamento avviene in un thread a parte su un'istanza nuova, poi viene scambiata con quella che suona.
class PianoEngine
{
public:
    enum class Status { empty, loading, ready, error };

    PianoEngine();
    ~PianoEngine();

    // thread dei messaggi
    void prepare (double sampleRate, int maxBlock);
    void loadAsync (const juce::File& sfz);
    void unload();
    Status status() const noexcept { return (Status) statusValue.load(); }
    juce::String errorText() const;
    juce::File currentFile() const;
    int numRegions() const noexcept { return regions.load(); }

    // thread audio
    void render (juce::AudioBuffer<float>&, const juce::MidiBuffer&, const EngineSettings&) noexcept;
    void requestPanic() noexcept { panicRequest.store (true); }

    // per il pannello
    int activeVoices() const noexcept { return voices.load(); }
    int lastNote() const noexcept { return lastNoteValue.load(); }
    bool pedalDown() const noexcept { return pedalState.load(); }
    bool softDown() const noexcept { return softState.load(); }
    std::function<void()> onStatusChanged;   // chiamata sul thread dei messaggi

    static float shapeVelocity (float v01, int curve, bool soft, float softAmount) noexcept;

private:
    juce::SpinLock swapLock;                 // protegge 'synth' durante lo scambio
    std::unique_ptr<sfz::Sfizz> synth;
    std::thread loader;
    std::atomic<int> generation { 0 };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    std::atomic<double> preparedRate { 44100.0 };
    std::atomic<int> preparedBlock { 512 };
    std::atomic<int> statusValue { (int) Status::empty };
    std::atomic<int> regions { 0 }, voices { 0 }, lastNoteValue { -1 };
    std::atomic<bool> pedalState { false }, softState { false }, panicRequest { false }, freshSynth { false };
    mutable juce::CriticalSection textLock;
    juce::String error;
    juce::File file;

    // stato del thread audio
    std::array<int, 128> playedAs {};        // nota suonata per ogni tasto premuto (la trasposizione può cambiare nel frattempo)
    int cc64 = 0, cc67 = 0, sentSustain = -1, sentLid = -1;
    float sentDyn = -1, sentRel = -1, sentKey = -1, sentPed = -1, sentTuning = -1;
    bool freewheel = false;
    std::vector<float> scratchL, scratchR;

    void finishLoad (std::unique_ptr<sfz::Sfizz>, const juce::File&, const juce::String& err, int gen, double loadedRate);
    void setStatus (Status, const juce::String& err = {});
    void joinLoader();
    void resetAudioState() noexcept;
};
}
