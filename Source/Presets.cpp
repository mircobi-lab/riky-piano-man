#include "Presets.h"

namespace riky
{
namespace presets
{
// Valori: vedi docs/PRESET-CHARACTER-PIANOS.md per le misure da cui partono.
// 0 = layer FELT PIANO, 1 FM E.PIANO, 2 TINE E.PIANO, 3 WARM PAD, 4 SYNTH PAD, 5 FM BRASS, 6 FELT PAD, 7 SYNTH BASS
// RIKY_PRESET_TABLE_BEGIN
const std::vector<FactoryPreset>& factory()
{
    static const std::vector<FactoryPreset> list = {
        { "MAESTRO CONCERT GRAND", "Maestro Concert Grand", "Il pianoforte com'è registrato, senza modifiche.", {} },
        { "NASHVILLE C3", "Nashville C3", "Mezzacoda caldo passato su nastro.",
          { { id::eqLow, -0.5f }, { id::eqLMid, -6.f }, { id::eqLMidF, 1000.f }, { id::eqHMid, -2.f }, { id::tape, 0.35f }, { id::compOn, 1.f }, { id::compThr, -22.f }, { id::compRatio, 2.f }, { id::width, 1.2f }, { id::roomMix, 0.22f }, { id::trim, -3.5f } } },
        { "U1 UPRIGHT", "U1 verticale", "Pianoforte verticale: medi in avanti, molto largo, stanza piccola.",
          { { id::eqLMid, 10.f }, { id::eqLMidF, 1000.f }, { id::eqHigh, 9.5f }, { id::compOn, 1.f }, { id::compThr, -24.f }, { id::compRatio, 2.5f }, { id::width, 2.f }, { id::roomSize, 0.3f }, { id::roomMix, 0.16f }, { id::eqLow, 2.5f }, { id::eqHMid, 0.5f }, { id::trim, -12.f } } },
        { "FELT PIANO", "Felt Piano, pianoforte con feltro", "Verticale con il feltro: morbido e ovattato.",
          { { id::attack, 0.3f }, { id::eqLow, 7.f }, { id::eqLMid, 8.5f }, { id::eqLMidF, 1000.f }, { id::eqHMid, -10.f }, { id::width, 1.8f }, { id::roomSize, 0.6f }, { id::roomMix, 0.28f }, { id::felt, 0.25f }, { id::eqHigh, -4.f }, { id::trim, -3.5f } } },
        { "NASHVILLE C3 + DX EP", "Nashville C3 con piano elettrico FM", "Mezzacoda su nastro con un piano elettrico FM sotto.",
          { { id::eqLow, -8.5f }, { id::eqLMid, -4.5f }, { id::eqLMidF, 1000.f }, { id::eqHMid, -3.5f }, { id::tape, 0.35f }, { id::compOn, 1.f }, { id::compThr, -22.f }, { id::compRatio, 2.f }, { id::width, 1.6f }, { id::roomMix, 0.22f }, { id::eqHigh, 10.f }, { id::l2On, 1.f }, { id::l2Sound, 1.f }, { id::trim, -4.5f } } },
        { "NASHVILLE C3 BRIGHT", "Nashville C3 brillante", "Il mezzacoda su nastro con più presenza e aria.",
          { { id::eqLow, 0.5f }, { id::eqLMid, -8.f }, { id::eqLMidF, 1000.f }, { id::eqHMid, 8.5f }, { id::eqHigh, 10.f }, { id::tape, 0.3f }, { id::compOn, 1.f }, { id::compThr, -22.f }, { id::compRatio, 2.f }, { id::width, 1.2f }, { id::roomMix, 0.22f }, { id::trim, -4.f } } },
        { "U1 UPRIGHT BRIGHT", "U1 verticale brillante", "Il verticale con acuti più aperti.",
          { { id::eqLow, -4.5f }, { id::eqLMid, -4.f }, { id::eqLMidF, 1000.f }, { id::eqHigh, 10.f }, { id::compOn, 1.f }, { id::compThr, -24.f }, { id::compRatio, 2.5f }, { id::width, 2.f }, { id::roomSize, 0.3f }, { id::roomMix, 0.16f }, { id::eqHMid, -2.5f }, { id::trim, -5.f } } },
        { "FELT PIANO BRIGHT", "Felt Piano brillante", "Feltro più leggero: morbido ma più chiaro.",
          { { id::attack, 0.2f }, { id::eqLMid, -0.5f }, { id::eqLMidF, 1000.f }, { id::width, 1.5f }, { id::roomSize, 0.6f }, { id::roomMix, 0.26f }, { id::felt, 0.15f }, { id::eqLow, 2.5f }, { id::eqHigh, -0.5f }, { id::trim, -0.5f } } },
        { "FELT MONO-SP PIANO", "Felt Mono, feltro in mono con fruscio", "Feltro in mono con il fruscio del nastro, come un vecchio disco.",
          { { id::attack, 0.3f }, { id::eqLow, -6.f }, { id::eqLMid, 4.f }, { id::eqLMidF, 1000.f }, { id::eqHMid, -6.5f }, { id::width, 0.f }, { id::roomMix, 0.08f }, { id::felt, 0.3f }, { id::noise, 0.95f }, { id::eqHigh, 8.f }, { id::trim, 1.f } } },
        { "NASHVILLE C3 + SYNPAD", "Nashville C3 con pad", "Mezzacoda su nastro con un pad caldo.",
          { { id::eqLow, 1.5f }, { id::eqLMid, -6.f }, { id::eqLMidF, 1000.f }, { id::eqHMid, -9.f }, { id::tape, 0.35f }, { id::compOn, 1.f }, { id::compThr, -22.f }, { id::compRatio, 2.f }, { id::roomMix, 0.24f }, { id::eqHigh, -6.5f }, { id::l2On, 1.f }, { id::l2Sound, 3.f }, { id::l2Vol, -9.f }, { id::trim, -4.f } } },
        { "U1 UPRIGHT + RD EP", "U1 verticale con piano elettrico a lamelle", "Verticale con un piano elettrico a lamelle.",
          { { id::eqLMid, -0.5f }, { id::eqLMidF, 1000.f }, { id::eqHigh, 8.5f }, { id::compOn, 1.f }, { id::compThr, -24.f }, { id::compRatio, 2.5f }, { id::width, 1.85f }, { id::roomSize, 0.3f }, { id::roomMix, 0.16f }, { id::eqLow, 1.5f }, { id::eqHMid, -1.f }, { id::l2On, 1.f }, { id::l2Sound, 2.f }, { id::l2Vol, -5.f }, { id::trim, -7.5f } } },
        { "FELT PIANO PAD", "Felt Piano con pad", "Pianoforte con feltro e un pad morbido.",
          { { id::attack, 0.3f }, { id::eqLow, 10.f }, { id::eqLMid, 5.5f }, { id::eqLMidF, 1000.f }, { id::eqHMid, -10.f }, { id::roomSize, 0.6f }, { id::roomMix, 0.3f }, { id::felt, 0.25f }, { id::eqHigh, -10.f }, { id::l2On, 1.f }, { id::l2Sound, 6.f }, { id::l2Vol, -10.f }, { id::trim, -3.f } } },
        { "U1 UPRIGHT + FELT", "U1 verticale con Felt Piano", "Il verticale insieme alla sua versione con il feltro.",
          { { id::eqLMid, 8.5f }, { id::eqLMidF, 1000.f }, { id::eqHigh, 10.f }, { id::compOn, 1.f }, { id::compThr, -24.f }, { id::compRatio, 2.5f }, { id::width, 2.f }, { id::roomSize, 0.35f }, { id::roomMix, 0.18f }, { id::eqLow, 2.5f }, { id::eqHMid, -6.f }, { id::l2On, 1.f }, { id::l2Vol, -4.f }, { id::trim, -12.f } } },
        { "NASHC3 + SYNFMBRASS", "Nashville C3 con ottoni FM", "Mezzacoda su nastro con ottoni sintetici FM.",
          { { id::eqLow, 1.f }, { id::eqLMid, -6.f }, { id::eqLMidF, 1000.f }, { id::eqHMid, -2.f }, { id::tape, 0.35f }, { id::compOn, 1.f }, { id::compThr, -22.f }, { id::compRatio, 2.f }, { id::width, 1.45f }, { id::roomMix, 0.22f }, { id::eqHigh, 3.5f }, { id::l2On, 1.f }, { id::l2Sound, 5.f }, { id::l2Vol, -8.f }, { id::trim, -4.5f } } },
        { "NASHC3 + FELT PAD", "Nashville C3 con pad di feltro", "Mezzacoda su nastro con un pad morbido.",
          { { id::eqLow, 2.f }, { id::eqLMid, -6.f }, { id::eqLMidF, 1000.f }, { id::eqHMid, -10.f }, { id::tape, 0.35f }, { id::compOn, 1.f }, { id::compThr, -22.f }, { id::compRatio, 2.f }, { id::width, 0.75f }, { id::roomMix, 0.24f }, { id::eqHigh, -2.f }, { id::l2On, 1.f }, { id::l2Sound, 6.f }, { id::l2Vol, -10.f }, { id::trim, -4.f } } },
        { "FELT PIANO ARP*4", "Felt Piano con arpeggio", "Pianoforte con feltro e arpeggio in sedicesimi su due ottave, a 125 BPM.",
          { { id::attack, 0.3f }, { id::eqLow, 10.f }, { id::eqLMid, 2.f }, { id::eqLMidF, 1000.f }, { id::eqHMid, -10.f }, { id::width, 1.45f }, { id::roomSize, 0.6f }, { id::roomMix, 0.28f }, { id::felt, 0.25f }, { id::eqHigh, -10.f }, { id::arpOn, 1.f }, { id::arpMode, 2.f }, { id::arpOct, 2.f }, { id::arpGate, 0.6f }, { id::tempo, 125.f }, { id::trim, -2.5f } } },
        { "C3 BALLAD ARP", "C3 ballata con arpeggio", "Mezzacoda in arpeggio di ottavi con un pad che tiene l'accordo, a 86 BPM.",
          { { id::eqLow, 3.f }, { id::eqLMid, -3.5f }, { id::eqLMidF, 1000.f }, { id::eqHMid, -2.f }, { id::tape, 0.35f }, { id::compOn, 1.f }, { id::compThr, -22.f }, { id::compRatio, 2.f }, { id::width, 1.2f }, { id::roomMix, 0.26f }, { id::l2On, 1.f }, { id::l2Sound, 3.f }, { id::l2Vol, -11.f }, { id::arpOn, 1.f }, { id::arpRate, 1.f }, { id::arpGate, 0.9f }, { id::arpTarget, 1.f }, { id::tempo, 86.f }, { id::trim, -4.f } } },
    };
    return list;
}
// RIKY_PRESET_TABLE_END

juce::File folder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Riky Piano Man").getChildFile ("Presets");
}

juce::Array<juce::File> userFiles()
{
    auto files = folder().findChildFiles (juce::File::findFiles, false, "*.rikypreset");
    files.sort();
    return files;
}

int count() { return (int) factory().size() + userFiles().size(); }

juce::String name (int i)
{
    const auto& f = factory();
    if (i >= 0 && i < (int) f.size()) return f[(size_t) i].name;
    const auto u = userFiles();
    i -= (int) f.size();
    return i >= 0 && i < u.size() ? u[i].getFileNameWithoutExtension().toUpperCase() : juce::String();
}

juce::String spoken (int i)
{
    const auto& f = factory();
    if (i >= 0 && i < (int) f.size()) return juce::CharPointer_UTF8 (f[(size_t) i].spoken);
    const auto u = userFiles();
    i -= (int) f.size();
    return i >= 0 && i < u.size() ? u[i].getFileNameWithoutExtension() : juce::String();
}

juce::String about (int i)
{
    const auto& f = factory();
    if (i >= 0 && i < (int) f.size()) return juce::CharPointer_UTF8 (f[(size_t) i].about);
    return "Preset salvato da te.";
}

void applyValues (juce::AudioProcessorValueTreeState& st, const std::vector<std::pair<juce::String, float>>& values)
{
    for (auto& pid : soundParamIds())
        if (auto* p = st.getParameter (pid)) p->setValueNotifyingHost (p->getDefaultValue());
    for (auto& [pid, v] : values)
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (st.getParameter (pid))) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

bool apply (juce::AudioProcessorValueTreeState& st, int i)
{
    const auto& f = factory();
    std::vector<std::pair<juce::String, float>> values;
    if (i >= 0 && i < (int) f.size())
    {
        for (auto& [pid, v] : f[(size_t) i].values) values.push_back ({ pid, v });
        applyValues (st, values);
        return true;
    }
    const auto u = userFiles();
    i -= (int) f.size();
    if (i < 0 || i >= u.size()) return false;
    const auto xml = juce::XmlDocument::parse (u[i]);
    if (xml == nullptr || ! xml->hasTagName ("RIKYPRESET")) return false;
    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        values.push_back ({ e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value") });
    applyValues (st, values);
    return true;
}

juce::File saveUser (juce::AudioProcessorValueTreeState& st, const juce::String& nm)
{
    folder().createDirectory();
    const auto clean = juce::File::createLegalFileName (nm.trim().isNotEmpty() ? nm.trim() : juce::String ("Mio suono"));
    const auto file = folder().getChildFile (clean + ".rikypreset");
    juce::XmlElement xml ("RIKYPRESET");
    xml.setAttribute ("name", clean);
    for (auto& pid : soundParamIds())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (st.getParameter (pid)))
        {
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", pid);
            e->setAttribute ("value", p->convertFrom0to1 (p->getValue()));
        }
    return xml.writeTo (file) ? file : juce::File();
}
}
}
