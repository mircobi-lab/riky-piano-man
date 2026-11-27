#include "EditChain.h"

namespace riky
{
using Coefs = juce::dsp::IIR::Coefficients<float>;

void EditChain::prepare (double sampleRate, int)
{
    sr = sampleRate;
    first = true;
    trim.reset (sampleRate, .05);
    tapeAmt.reset (sampleRate, .05);
    reset();
}

void EditChain::reset()
{
    for (auto& b : f) b.clear();
    env = 0.f; gainDbSm = 0.f; noiseEnv = 0.f;
    hissZ[0] = hissZ[1] = 0.f;
}

void EditChain::updateFilters (const EditSettings& s)
{
    const float nyq = (float) sr * .45f;
    auto g = [] (float db) { return juce::Decibels::decibelsToGain (db); };
    active[hp] = s.hpf > 20.5f;
    if (active[hp]) f[hp].set (*Coefs::makeHighPass (sr, s.hpf, .707f));
    active[lp] = s.lpf < 19900.f;
    if (active[lp]) f[lp].set (*Coefs::makeLowPass (sr, juce::jmin (s.lpf, nyq), .707f));
    active[low] = std::abs (s.eqLow) > .01f;
    if (active[low]) f[low].set (*Coefs::makeLowShelf (sr, 120.f, .707f, g (s.eqLow)));
    active[lmid] = std::abs (s.eqLMid) > .01f;
    if (active[lmid]) f[lmid].set (*Coefs::makePeakFilter (sr, s.eqLMidF, .8f, g (s.eqLMid)));
    active[hmid] = std::abs (s.eqHMid) > .01f;
    if (active[hmid]) f[hmid].set (*Coefs::makePeakFilter (sr, s.eqHMidF, .8f, g (s.eqHMid)));
    active[high] = std::abs (s.eqHigh) > .01f;
    if (active[high]) f[high].set (*Coefs::makeHighShelf (sr, juce::jmin (6000.f, nyq), .707f, g (s.eqHigh)));
    // nastro: rigonfiamento dei bassi intorno agli 80 Hz e acuti che scendono con la saturazione
    active[bump] = active[tapeLp] = s.tape > .001f;
    if (s.tape > .001f)
    {
        f[bump].set (*Coefs::makePeakFilter (sr, 80.f, .9f, g (2.5f * s.tape)));
        f[tapeLp].set (*Coefs::makeLowPass (sr, juce::jmin (nyq, 20000.f * std::pow (.55f, s.tape)), .6f));
    }
    applied = s;
}

void EditChain::process (juce::AudioBuffer<float>& buffer, const EditSettings& s) noexcept
{
    const int n = buffer.getNumSamples();
    const int chans = juce::jmin (2, buffer.getNumChannels());
    if (! s.on) { first = true; return; }

    if (first || s.hpf != applied.hpf || s.lpf != applied.lpf || s.eqLow != applied.eqLow || s.eqLMid != applied.eqLMid || s.eqLMidF != applied.eqLMidF
        || s.eqHMid != applied.eqHMid || s.eqHMidF != applied.eqHMidF || s.eqHigh != applied.eqHigh || s.tape != applied.tape)
    {
        if (first) reset();
        updateFilters (s);
        first = false;
    }

    // filtri ed EQ
    for (int c = 0; c < chans; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int k : { (int) hp, (int) lp, (int) low, (int) lmid, (int) hmid, (int) high })
            if (active[k])
                for (int i = 0; i < n; ++i) d[i] = f[k].run (d[i], c);
    }

    // compressore: picco con attacco e rilascio, ginocchio morbido di 6 dB, guadagno di compensazione automatico a metà
    if (s.compOn && s.compRatio > 1.01f)
    {
        const float aA = std::exp (-1.f / (float) (sr * s.compAtt * .001)), aR = std::exp (-1.f / (float) (sr * s.compRel * .001));
        const float makeup = -.5f * s.compThr * (1.f - 1.f / s.compRatio);
        const float knee = 6.f;
        auto* l = buffer.getWritePointer (0);
        auto* r = chans > 1 ? buffer.getWritePointer (1) : l;
        for (int i = 0; i < n; ++i)
        {
            const float x = juce::jmax (std::abs (l[i]), std::abs (r[i]));
            env = x > env ? aA * env + (1.f - aA) * x : aR * env + (1.f - aR) * x;
            const float lv = juce::Decibels::gainToDecibels (env, -120.f);
            const float over = lv - s.compThr;
            float red = 0.f;
            if (over > knee * .5f) red = over * (1.f - 1.f / s.compRatio);
            else if (over > -knee * .5f) { const float t = over + knee * .5f; red = (1.f - 1.f / s.compRatio) * t * t / (2.f * knee); }
            gainDbSm = makeup - red;
            const float gl = juce::Decibels::decibelsToGain (gainDbSm);
            l[i] *= gl;
            if (chans > 1) r[i] *= gl;
        }
    }

    // nastro: saturazione asimmetrica morbida con il livello compensato, poi rigonfiamento dei bassi e acuti morbidi
    tapeAmt.setTargetValue (s.tape);
    if (s.tape > .001f || tapeAmt.isSmoothing())
    {
        for (int i = 0; i < n; ++i)
        {
            const float t = tapeAmt.getNextValue();
            const float drive = 1.f + 2.f * t, bias = .12f * t;
            const float off = std::tanh (bias);
            const float norm = 1.f / (drive * (1.f - off * off));          // guadagno 1 sui segnali piccoli: comprime solo i picchi
            for (int c = 0; c < chans; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                const float x = d[i];
                const float y = (std::tanh (drive * x + bias) - off) * norm;
                d[i] = x + t * (y - x);
            }
        }
        for (int c = 0; c < chans; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            for (int k : { (int) bump, (int) tapeLp })
                if (active[k])
                    for (int i = 0; i < n; ++i) d[i] = f[k].run (d[i], c);
        }
    }

    // fruscio: rumore filtrato verso gli acuti, con un livello che segue il suono (tipo nastro con riduttore di rumore)
    if (s.noise > .001f)
    {
        const float level = juce::Decibels::decibelsToGain (-78.f + 30.f * s.noise);
        const float up = std::exp (-1.f / (float) (sr * .005)), down = std::exp (-1.f / (float) (sr * .6));
        auto* l = buffer.getWritePointer (0);
        auto* r = chans > 1 ? buffer.getWritePointer (1) : nullptr;
        for (int i = 0; i < n; ++i)
        {
            const float x = juce::jmax (std::abs (l[i]), r != nullptr ? std::abs (r[i]) : 0.f);
            noiseEnv = x > noiseEnv ? up * noiseEnv + (1.f - up) * x : down * noiseEnv + (1.f - down) * x;
            const float gate = juce::jlimit (0.f, 1.f, noiseEnv * 30.f);     // pieno sopra circa -30 dBFS
            for (int c = 0; c < chans; ++c)
            {
                const float w = rng.nextFloat() * 2.f - 1.f;
                const float h = w - hissZ[c];                                // differenza: più acuti che bassi
                hissZ[c] = w;
                buffer.getWritePointer (c)[i] += h * level * gate;
            }
        }
    }

    // livello del suono su misura
    trim.setTargetValue (juce::Decibels::decibelsToGain (s.trimDb));
    if (trim.isSmoothing() || std::abs (s.trimDb) > .01f)
        for (int i = 0; i < n; ++i)
        {
            const float g = trim.getNextValue();
            for (int c = 0; c < chans; ++c) buffer.getWritePointer (c)[i] *= g;
        }
}
}
