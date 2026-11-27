#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace riky
{
// Valori della sezione MODIFICA (suono su misura) che passano dal thread audio e dall'export
struct EditSettings
{
    bool on = true;
    float hpf = 20.f, lpf = 20000.f;                     // Hz; 20 e 20000 = spenti
    float eqLow = 0.f, eqLMid = 0.f, eqLMidF = 500.f, eqHMid = 0.f, eqHMidF = 2500.f, eqHigh = 0.f;
    bool compOn = false;
    float compThr = -18.f, compRatio = 2.5f, compAtt = 15.f, compRel = 200.f;
    float tape = 0.f, noise = 0.f;
    float trimDb = 0.f;
};

// Catena del suono su misura, prima di tono, larghezza e stanza:
// passa-alto, passa-basso, EQ a 4 bande, compressore, nastro (saturazione, rigonfiamento dei bassi, acuti più morbidi)
// e fruscio che segue il suono (sparisce nel silenzio, così la coda degli export finisce).
class EditChain
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (juce::AudioBuffer<float>&, const EditSettings&) noexcept;

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] {}, z2[2] {};
        void set (const juce::dsp::IIR::Coefficients<float>& c)
        {
            const auto* r = c.getRawCoefficients();   // b0 b1 b2 a1 a2, già divisi per a0
            b0 = r[0]; b1 = r[1]; b2 = r[2]; a1 = r[3]; a2 = r[4];
        }
        float run (float x, int ch) noexcept
        {
            const float y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return y;
        }
        void clear() { z1[0] = z1[1] = z2[0] = z2[1] = 0.f; }
    };
    enum { hp, lp, low, lmid, hmid, high, bump, tapeLp, numFilters };
    Biquad f[numFilters];
    bool active[numFilters] {};
    EditSettings applied;
    bool first = true;
    double sr = 44100.0;

    // compressore: rivelatore di picco per canale, collegato in stereo
    float env = 0.f, gainDbSm = 0.f;
    // nastro e fruscio
    juce::Random rng { 91 };
    float noiseEnv = 0.f, hissZ[2] {};
    juce::SmoothedValue<float> trim, tapeAmt;

    void updateFilters (const EditSettings&);
};
}
