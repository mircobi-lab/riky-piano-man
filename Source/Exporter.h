#pragma once
#include "PianoEngine.h"
#include "PostChain.h"
#include "Sequencer.h"
#include "Voicing.h"
#include <juce_core/juce_core.h>

namespace riky
{
// Export del brano del sequencer in un file audio, fuori tempo reale e in un thread a parte:
// un motore sfizz nuovo carica lo stesso SFZ e suona il brano alla qualità massima, con la stessa catena del pannello.
class Exporter : private juce::Thread
{
public:
    enum class Format { mp3_320, mp3_v0, wav24 };
    struct Job
    {
        std::vector<SeqEvent> events;
        juce::File sfz, dest;
        juce::String title;
        EngineSettings engine;
        PostSettings post;
        int temperament = 0, polyphony = 128;
        Format format = Format::mp3_320;
        double sampleRate = 44100.0;          // il Maestro è registrato a 44,1 kHz: nessun ricampionamento
        // secondo strato e arpeggiatore, come sul pannello
        VoiceSettings voices;
        juce::File layerSfz;                  // file (o percorso virtuale per i suoni interni)
        juce::String layerText;               // testo SFZ dei suoni interni
        EngineSettings layerEngine;
        float layerGain = 0.f;
    };
    struct Result { bool ok = false; juce::String text; juce::File file; float peakDb = -100.f; double seconds = 0; };

    Exporter() : juce::Thread ("Riky Piano Man export") {}
    ~Exporter() override { stopThread (20000); }

    bool start (Job);                         // false se un export è già in corso
    void cancel() { signalThreadShouldExit(); }
    bool busy() const { return isThreadRunning(); }
    float progress() const noexcept { return progressValue.load(); }
    Result result() const { const juce::ScopedLock sl (lock); return res; }
    std::function<void()> onFinished;         // thread dei messaggi

    static juce::String extension (Format f) { return f == Format::wav24 ? ".wav" : ".mp3"; }
    static Result renderNow (Job, std::function<bool (float)> progress);   // anche per le prove, senza thread

private:
    Job job;
    std::atomic<float> progressValue { 0.f };
    juce::CriticalSection lock;
    Result res;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    void run() override;
};
}
