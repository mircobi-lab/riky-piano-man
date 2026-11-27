#include "Voicing.h"
#include "Params.h"
#include <algorithm>
#include <cstddef>
#include <limits>

namespace riky
{
void Voicing::prepare (double sampleRate)
{
    sr = sampleRate;
    reset();
}

void Voicing::reset()
{
    held.clear();
    held.reserve (128);
    sounding.clear();
    sounding.reserve (256);
    routeMain.fill (0);
    routeLayer.fill (0);
    toNextStep = 0.0;
    stepIndex = 0; direction = 1;
    running = wasArp = wasLayer = false;
}

bool Voicing::zoneMain (int key, const VoiceSettings& v) const noexcept
{
    if (! v.layerOn || v.zone == 0) return true;
    return v.zone == 1 ? key >= v.split : key < v.split;
}

bool Voicing::zoneLayer (int key, const VoiceSettings& v) const noexcept
{
    if (! v.layerOn) return false;
    if (v.zone == 0) return true;
    return v.zone == 1 ? key < v.split : key >= v.split;
}

std::vector<int> Voicing::pattern (const VoiceSettings& v) const
{
    // indici in 'held' con l'ottava: valore = indice + 1000 * ottava
    std::vector<int> base;
    for (int i = 0; i < (int) held.size(); ++i) base.push_back (i);
    if (v.arpMode != 4)   // tutti tranne "come suonate": dal grave all'acuto
        std::sort (base.begin(), base.end(), [this] (int a, int b) { return held[(size_t) a].key < held[(size_t) b].key; });
    std::vector<int> up;
    const int octs = juce::jlimit (1, 4, v.arpOctaves);
    for (int o = 0; o < octs; ++o)
        for (int i : base)
            if (held[(size_t) i].key + 12 * o <= 127) up.push_back (i + 1000 * o);
    if (v.arpMode == 1) std::reverse (up.begin(), up.end());
    else if (v.arpMode == 2 && up.size() > 2)
        for (int i = (int) up.size() - 2; i > 0; --i) up.push_back (up[(size_t) i]);   // su e giù senza ripetere gli estremi
    return up;
}

void Voicing::releaseArp (int pos, juce::MidiBuffer& mainOut, juce::MidiBuffer& layerOut)
{
    for (auto& s : sounding)
    {
        if (s.toMain) mainOut.addEvent (juce::MidiMessage::noteOff (1, s.note), pos);
        if (s.toLayer) layerOut.addEvent (juce::MidiMessage::noteOff (1, s.note), pos);
    }
    sounding.clear();
}

void Voicing::step (int pos, const VoiceSettings& v, juce::MidiBuffer& mainOut, juce::MidiBuffer& layerOut)
{
    const auto pat = pattern (v);
    if (pat.empty()) return;
    std::vector<int> chosen;
    if (v.arpMode == 5) chosen = pat;                                       // accordo ripetuto
    else if (v.arpMode == 3) chosen.push_back (pat[(size_t) rng.nextInt ((int) pat.size())]);
    else chosen.push_back (pat[(size_t) (stepIndex % (int) pat.size())]);
    ++stepIndex;

    const double stepLen = arpRateBeats (v.arpRate) * 60.0 / juce::jmax (20.0, v.bpm) * sr;
    for (int code : chosen)
    {
        const auto& h = held[(size_t) (code % 1000)];
        const int note = h.key + 12 * (code / 1000);
        Sounding s;
        s.note = note;
        s.toMain = arpMain (v) && zoneMain (h.key, v);
        s.toLayer = arpLayer (v) && zoneLayer (h.key, v);
        s.offIn = juce::jmax (1.0, stepLen * juce::jlimit (.1f, 1.f, v.arpGate) - 1.0);
        // la stessa nota che suona ancora: prima si stacca
        for (auto it = sounding.begin(); it != sounding.end();)
            if (it->note == note)
            {
                if (it->toMain) mainOut.addEvent (juce::MidiMessage::noteOff (1, note), pos);
                if (it->toLayer) layerOut.addEvent (juce::MidiMessage::noteOff (1, note), pos);
                it = sounding.erase (it);
            }
            else ++it;
        const auto on = juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, h.vel));
        if (s.toMain) mainOut.addEvent (on, pos);
        if (s.toLayer) layerOut.addEvent (on, pos);
        if (s.toMain || s.toLayer) sounding.push_back (s);
    }
}

void Voicing::advance (int from, int to, const VoiceSettings& v, juce::MidiBuffer& mainOut, juce::MidiBuffer& layerOut)
{
    constexpr double inf = std::numeric_limits<double>::max();
    const double stepLen = arpRateBeats (v.arpRate) * 60.0 / juce::jmax (20.0, v.bpm) * sr;
    double t = from;
    for (int guard = 0; guard < 4096; ++guard)
    {
        const double dtStep = running && v.arpOn ? toNextStep : inf;
        double dtOff = inf;
        size_t offIdx = 0;
        for (size_t i = 0; i < sounding.size(); ++i)
            if (sounding[i].offIn < dtOff) { dtOff = sounding[i].offIn; offIdx = i; }
        const double dt = juce::jmin (dtStep, dtOff);
        if (! (dt < inf) || t + dt >= to)
        {
            const double rest = to - t;
            if (running && v.arpOn) toNextStep -= rest;
            for (auto& s : sounding) s.offIn -= rest;
            return;
        }
        t += dt;
        if (running && v.arpOn) toNextStep -= dt;
        for (auto& s : sounding) s.offIn -= dt;
        const int pos = juce::jlimit (from, to - 1, (int) t);
        if (dtOff <= dtStep)
        {
            const auto s = sounding[offIdx];
            if (s.toMain) mainOut.addEvent (juce::MidiMessage::noteOff (1, s.note), pos);
            if (s.toLayer) layerOut.addEvent (juce::MidiMessage::noteOff (1, s.note), pos);
            sounding.erase (sounding.begin() + (std::ptrdiff_t) offIdx);
        }
        else
        {
            step (pos, v, mainOut, layerOut);
            toNextStep += stepLen;
        }
    }
}

void Voicing::process (const juce::MidiBuffer& in, int n, const VoiceSettings& v, juce::MidiBuffer& mainOut, juce::MidiBuffer& layerOut)
{
    // cambi di stato: strato spento o arpeggio acceso/spento mentre suonano delle note
    if (wasLayer && ! v.layerOn)
    {
        for (int k = 0; k < 128; ++k)
            if (routeLayer[(size_t) k]) { layerOut.addEvent (juce::MidiMessage::noteOff (1, k), 0); routeLayer[(size_t) k] = 0; }
        for (auto& s : sounding)
            if (s.toLayer) { layerOut.addEvent (juce::MidiMessage::noteOff (1, s.note), 0); s.toLayer = false; }
    }
    if (wasArp && ! v.arpOn) { releaseArp (0, mainOut, layerOut); running = false; }
    if (! wasArp && v.arpOn && ! held.empty())
    {
        // le note tenute passano all'arpeggio
        for (int k = 0; k < 128; ++k)
        {
            if (routeMain[(size_t) k] && arpMain (v)) { mainOut.addEvent (juce::MidiMessage::noteOff (1, k), 0); routeMain[(size_t) k] = 0; }
            if (routeLayer[(size_t) k] && arpLayer (v)) { layerOut.addEvent (juce::MidiMessage::noteOff (1, k), 0); routeLayer[(size_t) k] = 0; }
        }
        running = true; toNextStep = 0.0; stepIndex = 0;
    }
    wasLayer = v.layerOn;
    wasArp = v.arpOn;

    int cur = 0;
    for (const auto meta : in)
    {
        const int pos = juce::jlimit (0, juce::jmax (0, n - 1), meta.samplePosition);
        if (pos > cur) { advance (cur, pos, v, mainOut, layerOut); cur = pos; }
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const int key = m.getNoteNumber();
            for (auto it = held.begin(); it != held.end(); ++it) if (it->key == key) { held.erase (it); break; }
            held.push_back ({ key, m.getVelocity() });
            if (! arpMain (v) && zoneMain (key, v)) { mainOut.addEvent (m, pos); routeMain[(size_t) key] = 1; }
            if (v.layerOn && ! arpLayer (v) && zoneLayer (key, v)) { layerOut.addEvent (m, pos); routeLayer[(size_t) key] = 1; }
            if (v.arpOn && ! running)
            {
                running = true;
                stepIndex = 0;
                toNextStep = 0.0;
                if (v.hostPlaying)
                {
                    // prossimo passo sulla griglia dell'host
                    const double rate = arpRateBeats (v.arpRate);
                    const double beats = v.ppq + pos / sr * v.bpm / 60.0;
                    const double next = std::ceil (beats / rate - 1.0e-6) * rate;
                    toNextStep = (next - beats) * 60.0 / juce::jmax (20.0, v.bpm) * sr;
                }
                // il primo passo parte su questo campione, dopo le altre note dello stesso accordo
            }
        }
        else if (m.isNoteOff())
        {
            const int key = m.getNoteNumber();
            for (auto it = held.begin(); it != held.end(); ++it) if (it->key == key) { held.erase (it); break; }
            if (routeMain[(size_t) key]) { mainOut.addEvent (m, pos); routeMain[(size_t) key] = 0; }
            if (routeLayer[(size_t) key]) { layerOut.addEvent (m, pos); routeLayer[(size_t) key] = 0; }
            if (held.empty()) running = false;   // le note dell'arpeggio finiscono con il loro gate
        }
        else
        {
            if (m.isAllNotesOff() || m.isAllSoundOff()) { held.clear(); releaseArp (pos, mainOut, layerOut); running = false; }
            mainOut.addEvent (m, pos);
            if (v.layerOn) layerOut.addEvent (m, pos);
        }
    }
    if (cur < n) advance (cur, n, v, mainOut, layerOut);
}
}
