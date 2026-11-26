#include "Cues.h"

namespace riky
{
void Cues::add (float freq, int startOffset, double seconds, float amp, bool click) noexcept
{
    for (auto& b : beeps)
        if (b.len == 0)
        {
            b.phase = 0;
            b.inc = juce::MathConstants<double>::twoPi * freq / sr;
            b.start = juce::jmax (0, startOffset);
            b.len = juce::jmax (1, (int) (seconds * sr));
            b.pos = 0;
            b.amp = amp;
            b.click = click;
            return;
        }
}

int Cues::scheduleBeats (double tStart, double tEnd, double bpm, bool metronomeOn, int n) noexcept
{
    const double beat = 60.0 / juce::jlimit (40.0, 240.0, bpm);
    int count = 0;
    for (auto b = (juce::int64) std::ceil (tStart / beat - 1e-9); (double) b * beat < tEnd - 1e-9; ++b)
    {
        const double tb = (double) b * beat;
        if (tb >= 0.0 && ! metronomeOn) break;   // solo conteggio
        click (juce::jlimit (0, n - 1, (int) std::llround ((tb - tStart) * sr)), b % 4 == 0);
        ++count;
    }
    return count;
}

void Cues::render (juce::AudioBuffer<float>& buffer, bool enabled) noexcept
{
    // frasi brevi e diverse tra loro: si riconoscono anche senza lettore di schermo
    if (const int p = pending.exchange (0); p != 0 && enabled)
    {
        const int ms = (int) (sr / 1000.0);
        if (p & recStart) { add (880.f, 0, .09, .18f); add (1320.f, 110 * ms, .14, .18f); }          // sale: si registra
        if (p & recStop)  { add (1320.f, 0, .09, .18f); add (880.f, 110 * ms, .16, .18f); }          // scende: fermo
        if (p & done)     { add (660.f, 0, .1, .15f); add (880.f, 110 * ms, .1, .15f); add (1320.f, 220 * ms, .22, .15f); }
        if (p & error)    { add (220.f, 0, .18, .2f); add (220.f, 260 * ms, .18, .2f); }             // due note gravi
        if (p & confirm)  { add (1760.f, 0, .05, .12f); }
    }
    const int n = buffer.getNumSamples();
    const int chans = juce::jmin (2, buffer.getNumChannels());
    for (auto& b : beeps)
    {
        if (b.len == 0) continue;
        if (! enabled && ! b.click) { b.len = 0; continue; }
        int i = b.start;
        b.start = juce::jmax (0, b.start - n);
        for (; i < n && b.pos < b.len; ++i, ++b.pos)
        {
            // inviluppo: attacco di 2 ms, poi discesa (il click del metronomo scende in fretta)
            const float t = (float) b.pos / (float) b.len;
            const float att = juce::jmin (1.f, (float) b.pos / (float) (sr * .002));
            const float env = att * (b.click ? std::exp (-6.f * t) : (1.f - t) * (1.f - t));
            const float v = b.amp * env * (float) std::sin (b.phase);
            b.phase += b.inc;
            for (int c = 0; c < chans; ++c) buffer.addSample (c, i, v);
        }
        if (b.pos >= b.len) b.len = 0;
    }
}
}
