#include "JobBuilder.h"
#include "LayerSounds.h"

namespace riky
{
Exporter::Job buildJob (juce::AudioProcessorValueTreeState& st, const juce::File& sfz)
{
    auto v = [&st] (const char* pid) { return st.getRawParameterValue (pid)->load(); };
    Exporter::Job j;
    j.sfz = sfz;
    auto& e = j.engine;
    e.a4 = a4Frequency ((int) v (id::a4ref));
    e.cents = v (id::tune);
    e.transpose = juce::roundToInt (v (id::transpose));
    e.velCurve = (int) v (id::velCurve);
    e.pedalButton = false;                 // il pedale è già nel brano
    e.softButton = v (id::soft) > .5f;
    e.softAmount = v (id::softAmt);
    for (int i = 0; i < numSlots; ++i) e.slots[(size_t) i] = v (id::sfzSlot (i));
    e.quality = sampleQuality ((int) v (id::quality));
    e.temperKey = (int) v (id::temperKey);
    const bool edit = v (id::editOn) > .5f;
    if (edit) { e.felt = v (id::felt); e.attack = v (id::attack); }

    auto& p = j.post;
    p.tone = v (id::tone);
    p.width = v (id::width);
    p.roomOn = v (id::roomOn) > .5f;
    p.roomSize = v (id::roomSize);
    p.roomDamp = v (id::roomDamp);
    p.roomMix = v (id::roomMix);
    p.volumeDb = v (id::volume);
    auto& ed = p.edit;
    ed.on = edit;
    ed.hpf = v (id::hpf); ed.lpf = v (id::lpf);
    ed.eqLow = v (id::eqLow); ed.eqLMid = v (id::eqLMid); ed.eqLMidF = v (id::eqLMidF);
    ed.eqHMid = v (id::eqHMid); ed.eqHMidF = v (id::eqHMidF); ed.eqHigh = v (id::eqHigh);
    ed.compOn = v (id::compOn) > .5f;
    ed.compThr = v (id::compThr); ed.compRatio = v (id::compRatio); ed.compAtt = v (id::compAtt); ed.compRel = v (id::compRel);
    ed.tape = v (id::tape); ed.noise = v (id::noise); ed.trimDb = v (id::trim);

    j.temperament = (int) v (id::temper);
    j.polyphony = polyphony ((int) v (id::poly));

    auto& vs = j.voices;
    vs.layerOn = v (id::l2On) > .5f;
    vs.zone = (int) v (id::l2Zone);
    vs.split = juce::roundToInt (v (id::split));
    vs.arpOn = v (id::arpOn) > .5f;
    vs.arpMode = (int) v (id::arpMode);
    vs.arpRate = (int) v (id::arpRate);
    vs.arpOctaves = juce::roundToInt (v (id::arpOct));
    vs.arpGate = v (id::arpGate);
    vs.arpTarget = (int) v (id::arpTarget);
    vs.bpm = v (id::tempo);
    if (vs.layerOn)
    {
        const int sound = (int) v (id::l2Sound);
        j.layerSfz = sound == 0 ? sfz : juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Riky Layer " + layerSoundNames()[sound] + ".sfz");
        j.layerText = sound == 0 ? juce::String() : layerSfzText (sound);
        j.layerEngine = e;
        j.layerEngine.transpose += 12 * juce::roundToInt (v (id::l2Oct));
        j.layerEngine.felt = sound == 0 ? layerFeltAmount : 0.f;
        j.layerEngine.attack = sound == 0 ? layerFeltAttack : 0.f;
        j.layerEngine.slotsFromFile = true;
        j.layerGain = v (id::l2Vol) > -39.9f ? juce::Decibels::decibelsToGain (v (id::l2Vol)) : 0.f;
    }
    return j;
}
}
