#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <bitset>
#include <vector>

namespace riky
{
struct SeqEvent
{
    double time = 0.0;              // secondi dall'inizio
    juce::MidiMessage msg;
};

// Sequencer semplice a una traccia: suona un brano MIDI (la demo o un file .mid) e registra quello che si suona,
// dalla tastiera MIDI, dalla tastiera del pannello e dal pulsante SUSTAIN.
// Il thread audio legge il brano sotto uno SpinLock e scrive la registrazione in una coda senza blocchi.
class Sequencer
{
public:
    enum class State { stopped, playing, recording, countIn };

    // ---- thread dei messaggi
    void setEvents (std::vector<SeqEvent>, const juce::String& name);
    const std::vector<SeqEvent>& events() const noexcept { return take; }
    juce::String name() const { return takeName; }
    double length() const noexcept;              // secondi (fino all'ultimo evento)
    int numNotes() const noexcept;
    bool isEmpty() const noexcept { return take.empty(); }

    void play();
    void stop();
    // nuova ripresa: sostituisce quella in memoria quando si ferma. Con metronomo o conteggio ("aligned")
    // il tempo della ripresa resta allineato alle battute e il file MIDI salvato ha quel tempo
    void record (double bpm = 120.0, bool aligned = false);
    void update();                               // da un timer: raccoglie la registrazione e chiude la ripresa
    State state() const noexcept { return (State) stateValue.load(); }
    double position() const noexcept { return positionSeconds.load(); }   // negativa durante il conteggio
    double bpm() const noexcept { return takeBpm; }
    std::function<void()> onChange;              // brano cambiato o fine della registrazione

    static bool readMidi (juce::InputStream&, std::vector<SeqEvent>& out);
    bool loadMidiFile (const juce::File&);
    bool saveMidiFile (const juce::File&) const;
    juce::MemoryBlock toMidiData() const;
    static juce::MidiFile toMidiFile (const std::vector<SeqEvent>&, const juce::String& name, double bpm = 120.0);

    // ---- thread audio
    void processPlayback (juce::MidiBuffer& midi, int numSamples, double sampleRate, double countInSeconds = 0.0) noexcept;    // prima della tastiera del pannello
    void processRecording (const juce::MidiBuffer& midi, int numSamples, double sampleRate, bool pedalButton) noexcept;

private:
    enum Request { none, reqPlay, reqStop, reqRecord };
    std::atomic<int> request { none }, stateValue { (int) State::stopped };
    std::atomic<double> positionSeconds { 0.0 };
    std::atomic<bool> recordingFinished { false };

    std::vector<SeqEvent> take;                  // thread dei messaggi
    juce::String takeName;
    int takeCounter = 0;
    double takeBpm = 120.0, recBpm = 120.0;
    bool recAligned = false;

    juce::SpinLock playLock;
    std::vector<SeqEvent> playData;              // copia letta dal thread audio

    // coda della registrazione
    struct RecEvent { double time; juce::uint8 data[3]; int size; };
    static constexpr int fifoSize = 16384;
    juce::AbstractFifo fifo { fifoSize };
    std::array<RecEvent, fifoSize> fifoData {};
    std::vector<SeqEvent> recording;             // thread dei messaggi

    // stato del thread audio
    juce::int64 pos = 0, countLeft = 0;
    size_t cursor = 0;
    std::bitset<128> sounding;
    bool sustainDown = false, lastPedalButton = false, pedalKnown = false;

    void startState (State) noexcept;
    void silence (juce::MidiBuffer&) noexcept;
    void push (double time, const juce::MidiMessage&) noexcept;
    void drain();
    void finishRecording();
};
}
