#include "PostChain.h"

namespace riky
{
void PostChain::prepare (double sampleRate, int maxBlock, const PostSettings& s)
{
    sr = sampleRate;
    editChain.prepare (sampleRate, maxBlock);
    const int block = juce::jmax (maxBlock, 32);
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) block, 1 };
    for (int c = 0; c < 2; ++c)
    {
        lowShelf[c].coefficients = new juce::dsp::IIR::Coefficients<float> (1, 0, 0, 1, 0, 0);
        highShelf[c].coefficients = new juce::dsp::IIR::Coefficients<float> (1, 0, 0, 1, 0, 0);
        lowShelf[c].prepare (spec);
        highShelf[c].prepare (spec);
    }
    appliedTone = 1.0e9f;
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    reverbCleared = false;
    dcR = (float) (1.0 - juce::MathConstants<double>::twoPi * 10.0 / sampleRate);   // passa-alto a 10 Hz
    for (int c = 0; c < 2; ++c) dcX[c] = dcY[c] = 0.f;
    gain.reset (sampleRate, .05);
    wet.reset (sampleRate, .05);
    widthSm.reset (sampleRate, .05);
    gain.setCurrentAndTargetValue (gainFor (s.volumeDb));
    wet.setCurrentAndTargetValue (s.roomOn ? s.roomMix : 0.f);
    widthSm.setCurrentAndTargetValue (s.width);
    dry.setSize (2, block);
}

void PostChain::updateTone (float t)
{
    // inclinazione: scaffale basso a 250 Hz e alto a 2,5 kHz in senso opposto, fino a 9 dB (come Campione)
    const float dbAmount = 9.f * t;
    const auto lo = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf (sr, 250.f, .707f, juce::Decibels::decibelsToGain (-dbAmount));
    const auto hi = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (sr, 2500.f, .707f, juce::Decibels::decibelsToGain (dbAmount));
    for (int c = 0; c < 2; ++c)
    {
        *lowShelf[c].coefficients = lo;
        *highShelf[c].coefficients = hi;
    }
    appliedTone = t;
}

void PostChain::process (juce::AudioBuffer<float>& buffer, const PostSettings& s) noexcept
{
    const int n = buffer.getNumSamples();
    const int chans = juce::jmin (2, buffer.getNumChannels());

    // suono su misura
    editChain.process (buffer, s.edit);

    // tono
    if (! juce::exactlyEqual (s.tone, appliedTone)) updateTone (s.tone);
    if (std::abs (appliedTone) > .001f)
        for (int c = 0; c < chans; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i) d[i] = highShelf[c].processSample (lowShelf[c].processSample (d[i]));
        }

    // larghezza stereo: mid/side, 100% = come registrato (coppia X/Y)
    widthSm.setTargetValue (s.width);
    if (chans == 2 && (widthSm.isSmoothing() || std::abs (widthSm.getTargetValue() - 1.f) > .001f))
    {
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float w = widthSm.getNextValue();
            const float m = .5f * (l[i] + r[i]), sd = .5f * (l[i] - r[i]) * w;
            l[i] = m + sd; r[i] = m - sd;
        }
    }

    // stanza: riverbero solo bagnato, miscelato con un livello che scorre senza scatti
    juce::Reverb::Parameters rp;
    rp.roomSize = .3f + .68f * s.roomSize;
    rp.damping = s.roomDamp;
    rp.wetLevel = 1.f; rp.dryLevel = 0.f; rp.width = 1.f; rp.freezeMode = 0.f;
    reverb.setParameters (rp);
    wet.setTargetValue (s.roomOn ? s.roomMix : 0.f);
    if (wet.isSmoothing() || wet.getTargetValue() > 0.f)
    {
        reverbCleared = false;
        for (int start = 0; start < n;)
        {
            const int len = juce::jmin (n - start, dry.getNumSamples());
            for (int c = 0; c < chans; ++c) dry.copyFrom (c, 0, buffer, c, start, len);
            if (chans == 2) reverb.processStereo (buffer.getWritePointer (0, start), buffer.getWritePointer (1, start), len);
            else reverb.processMono (buffer.getWritePointer (0, start), len);
            // il riverbero di JUCE (Freeverb) nei filtri con retroazione può restare su un livello di continua
            // minuscolo (circa -90 dBFS) che non si spegne mai: un passa-alto a 10 Hz lo toglie
            for (int i = 0; i < len; ++i)
            {
                const float w = wet.getNextValue(), d = 1.f - .5f * w;
                for (int c = 0; c < chans; ++c)
                {
                    auto* o = buffer.getWritePointer (c, start);
                    const float x = o[i];
                    const float y = x - dcX[c] + dcR * dcY[c];
                    dcX[c] = x; dcY[c] = y;
                    o[i] = d * dry.getSample (c, i) + w * y;
                }
            }
            start += len;
        }
    }
    else if (! reverbCleared)
    {
        reverb.reset();
        for (int c = 0; c < 2; ++c) dcX[c] = dcY[c] = 0.f;
        reverbCleared = true;
    }

    // volume
    gain.setTargetValue (gainFor (s.volumeDb));
    float pk = 0.f;
    for (int i = 0; i < n; ++i)
    {
        const float g = gain.getNextValue();
        for (int c = 0; c < chans; ++c)
        {
            auto* o = buffer.getWritePointer (c);
            o[i] *= g;
            pk = juce::jmax (pk, std::abs (o[i]));
        }
    }
    for (int c = chans; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    lastPeak = pk;
}
}
