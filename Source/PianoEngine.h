#pragma once
#include "Params.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <array>
#include <atomic>
#include <bitset>
#include <map>
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
    std::array<float, numSlots> slots {};   // SFZ CONTROLS 0..1
    int quality = 4;               // qualità di ricampionamento dal vivo (sfizz 0..10)
    int temperKey = 0;             // tonica del temperamento (0 = Do)
    bool offline = false;          // bounce o render non in tempo reale: aspetta i campioni dal disco e usa la qualità massima
    // trasporto dell'host, per gli SFZ che usano tempo e posizione (LFO e inviluppi a tempo)
    bool hasTransport = false, playing = false;
    double bpm = 120.0, ppq = 0.0;
    int sigNum = 4, sigDen = 4;
};

// Quello che il file SFZ dichiara, letto da sfizz al caricamento
struct InstrumentInfo
{
    struct Control { int cc = 0; juce::String label; float defaultValue = 0.f; };
    juce::File file;
    juce::String name;
    int regions = 0, groups = 0, masters = 0, curves = 0, samples = 0;
    std::bitset<128> keys, switches;                    // tasti che suonano, tasti keyswitch
    std::map<int, juce::String> keyLabels, switchLabels;
    int defaultSwitch = -1;
    std::vector<Control> controls;                      // CC con label_cc, pedali esclusi, al massimo 8
    juce::String image;                                 // immagine dichiarata nel file (<control> image)
};

// Motore: sfizz (BSD-2) suona il file SFZ com'è, con regioni di rilascio, keyswitch, round robin, CC,
// curve, filtri, inviluppi e lettura dei campioni dal disco.
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
    void setPolyphony (int voices);
    void setTemperament (int index);
    Status status() const noexcept { return (Status) statusValue.load(); }
    juce::String errorText() const;
    juce::File currentFile() const;
    std::shared_ptr<const InstrumentInfo> info() const;
    int numRegions() const noexcept { return regions.load(); }

    // thread audio
    void render (juce::AudioBuffer<float>&, const juce::MidiBuffer&, const EngineSettings&) noexcept;
    void requestPanic() noexcept { panicRequest.store (true); }

    // per il pannello
    int activeVoices() const noexcept { return voices.load(); }
    int lastNote() const noexcept { return lastNoteValue.load(); }
    int currentSwitch() const noexcept { return switchValue.load(); }
    bool pedalDown() const noexcept { return pedalState.load(); }
    bool softDown() const noexcept { return softState.load(); }
    // un CC di uno slot arrivato dal MIDI: il processore lo riporta sul parametro (-1 = niente di nuovo)
    float takeIncomingSlot (int i) noexcept { return incoming[(size_t) i].exchange (-1.f); }
    std::function<void()> onStatusChanged;   // chiamata sul thread dei messaggi

    static float shapeVelocity (float v01, int curve, bool soft, float softAmount) noexcept;

private:
    juce::SpinLock swapLock;                 // protegge 'synth' durante lo scambio e le chiamate di controllo
    std::unique_ptr<sfz::Sfizz> synth;
    std::thread loader;
    std::atomic<int> generation { 0 };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    std::atomic<double> preparedRate { 44100.0 };
    std::atomic<int> preparedBlock { 512 };
    std::atomic<int> polyVoices { 128 }, temperament { 0 };
    std::atomic<int> statusValue { (int) Status::empty };
    std::atomic<int> regions { 0 }, voices { 0 }, lastNoteValue { -1 }, switchValue { -1 };
    std::atomic<bool> pedalState { false }, softState { false }, panicRequest { false }, freshSynth { false };
    std::array<std::atomic<int>, numSlots> slotCC;
    std::array<std::atomic<float>, numSlots> incoming;
    std::bitset<128> switchKeys;             // copia per il thread audio, scritta solo sotto swapLock
    int freshDefaultSwitch = -1;
    mutable juce::CriticalSection textLock;
    juce::String error;
    juce::File file;
    std::shared_ptr<const InstrumentInfo> infoPtr;

    // stato del thread audio
    std::array<int, 128> playedAs {};        // nota suonata per ogni tasto premuto (la trasposizione può cambiare nel frattempo)
    std::array<float, numSlots> seenParam {};
    int cc64 = 0, cc67 = 0, sentSustain = -1, sentLid = -1, sentQuality = -1, sentRootKey = -1;
    float sentTuning = -1;
    bool freewheel = false, wasPlaying = false;
    double sentBpm = -1;
    int sentNum = -1, sentDen = -1;
    std::vector<float> scratchL, scratchR;

    void finishLoad (std::unique_ptr<sfz::Sfizz>, std::shared_ptr<InstrumentInfo>, const juce::File&, const juce::String& err, int gen, double loadedRate);
    void setStatus (Status, const juce::String& err = {});
    void joinLoader();
    void resetAudioState() noexcept;
    static std::shared_ptr<InstrumentInfo> readInfo (sfz::Sfizz&, const juce::File&);
    static void applyTemperament (sfz::Sfizz&, int index);
};
}
