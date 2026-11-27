#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "EditChain.h"

namespace riky
{
// Valori della catena dopo il motore: tono, larghezza stereo, stanza, volume
struct PostSettings
{
    float tone = 0.f, width = 1.f;
    bool roomOn = true;
    float roomSize = .5f, roomDamp = .45f, roomMix = .2f;
    float volumeDb = 0.f;
    EditSettings edit;                         // suono su misura, prima di tono, larghezza e stanza
};

// La stessa catena serve al plugin e all'export: così il file esportato suona come il pannello
class PostChain
{
public:
    // guadagno fisso: il file SFZ del Maestro suona basso (CC 7 a 100, picchi di circa -11 dBFS a velocity 127)
    static constexpr float makeupDb = 6.f;

    void prepare (double sampleRate, int maxBlock, const PostSettings& initial);
    void process (juce::AudioBuffer<float>&, const PostSettings&) noexcept;   // restituisce in peak() il picco del blocco
    float peak() const noexcept { return lastPeak; }

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    Filter lowShelf[2], highShelf[2];
    float appliedTone = 1.0e9f;
    double sr = 44100.0;
    juce::Reverb reverb;
    bool reverbCleared = false;
    juce::SmoothedValue<float> gain, wet, widthSm;
    juce::AudioBuffer<float> dry;
    float lastPeak = 0.f;
    EditChain editChain;
    float dcR = .997f, dcX[2] {}, dcY[2] {};   // filtro anti-continua sull'uscita del riverbero

    void updateTone (float t);
    static float gainFor (float db) { return db <= -59.9f ? 0.f : juce::Decibels::decibelsToGain (db + makeupDb); }
};
}
