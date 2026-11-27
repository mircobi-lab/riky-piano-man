#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace riky
{
// Come le note arrivano ai due strati: zona della tastiera (split) e arpeggiatore
struct VoiceSettings
{
    bool layerOn = false;
    int zone = 0;              // 0 tutta la tastiera, 1 strato sotto lo split, 2 strato sopra lo split
    int split = 60;            // la nota di split appartiene alla zona alta
    bool arpOn = false;
    int arpMode = 0;           // 0 su, 1 giù, 2 su e giù, 3 casuale, 4 come suonate, 5 accordo
    int arpRate = 3;           // indice in arpRateBeats
    int arpOctaves = 1;
    float arpGate = .8f;
    int arpTarget = 0;         // 0 entrambi gli strati, 1 solo pianoforte, 2 solo secondo strato
    double bpm = 100.0;
    bool hostPlaying = false;  // con l'host in riproduzione l'arpeggio si allinea alla griglia dell'host
    double ppq = 0.0;
};

// Smista il MIDI tra pianoforte e secondo strato e suona l'arpeggiatore.
// Il sequencer registra le note prima di questo passaggio: un brano registrato con l'arpeggio acceso
// contiene gli accordi suonati, e l'arpeggio si risente uguale quando il brano suona.
class Voicing
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (const juce::MidiBuffer& in, int numSamples, const VoiceSettings&, juce::MidiBuffer& mainOut, juce::MidiBuffer& layerOut);
    int heldNotes() const noexcept { return (int) held.size(); }

private:
    struct Held { int key, vel; };
    struct Sounding { int note = -1; bool toMain = false, toLayer = false; double offIn = 0.0; };
    double sr = 44100.0;
    std::vector<Held> held;                  // tasti tenuti, nell'ordine in cui sono stati premuti
    std::array<signed char, 128> routeMain {}, routeLayer {};   // dove è andato ogni tasto suonato senza arpeggio
    std::vector<Sounding> sounding;          // note dell'arpeggio che suonano, con il tempo che manca allo stacco
    double toNextStep = 0.0;
    int stepIndex = 0, direction = 1;
    bool running = false, wasArp = false, wasLayer = false;
    juce::Random rng { 1991 };

    bool zoneMain (int key, const VoiceSettings& v) const noexcept;
    bool zoneLayer (int key, const VoiceSettings& v) const noexcept;
    bool arpMain (const VoiceSettings& v) const noexcept { return v.arpOn && v.arpTarget != 2; }
    bool arpLayer (const VoiceSettings& v) const noexcept { return v.arpOn && v.layerOn && v.arpTarget != 1; }
    void step (int pos, const VoiceSettings&, juce::MidiBuffer& mainOut, juce::MidiBuffer& layerOut);
    void advance (int from, int to, const VoiceSettings&, juce::MidiBuffer& mainOut, juce::MidiBuffer& layerOut);
    void releaseArp (int pos, juce::MidiBuffer& mainOut, juce::MidiBuffer& layerOut);
    std::vector<int> pattern (const VoiceSettings&) const;
};
}
