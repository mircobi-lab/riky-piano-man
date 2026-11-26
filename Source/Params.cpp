#include "Params.h"

namespace riky
{
namespace
{
using Float = juce::AudioParameterFloat;
using Range = juce::NormalisableRange<float>;

juce::String db (float d) { return d <= -59.9f ? "-INF DB" : juce::String (d, 1) + " DB"; }
juce::String pct (float v) { return juce::String (juce::roundToInt (v * 100.f)) + "%"; }

std::unique_ptr<Float> makeFloat (const char* pid, const juce::String& name, Range r, float def)
{
    return std::make_unique<Float> (juce::ParameterID { pid, 1 }, name, r, def,
                                    juce::AudioParameterFloatAttributes().withStringFromValueFunction ([pid] (float v, int) { return formatValue (pid, v); }));
}
}

juce::String accessibleValue (const juce::String& p, float v)
{
    // valori letti dal lettore di schermo: in italiano e senza abbreviazioni
    if (p == id::tone)
    {
        const int n = juce::roundToInt (std::abs (v) * 100.f);
        return n == 0 ? juce::String ("neutro") : (v < 0 ? "scuro " : "brillante ") + juce::String (n);
    }
    if (p == id::tune) { const int c = juce::roundToInt (v); return (c > 0 ? "più " : c < 0 ? "meno " : "") + juce::String (std::abs (c)) + " centesimi"; }
    if (p == id::transpose) { const int s = juce::roundToInt (v); return s == 0 ? juce::String ("nessuna") : (s > 0 ? "più " : "meno ") + juce::String (std::abs (s)) + (std::abs (s) == 1 ? " semitono" : " semitoni"); }
    if (p == id::volume) return v <= -59.9f ? juce::String ("spento") : (v < 0 ? "meno " : "") + juce::String (std::abs (v), 1).replaceCharacter ('.', ',') + " decibel";
    if (p == id::tempo) return juce::String (juce::roundToInt (v)) + " battiti al minuto";
    if (p.startsWith ("SFZ")) return juce::String (juce::roundToInt (v * 127.f)) + " su 127";
    if (p == id::width) return juce::String (juce::roundToInt (v * 100.f)) + " per cento";
    return juce::String (juce::roundToInt (v * 100.f)) + " per cento";
}

juce::StringArray temperamentNames()
{
    return { "EQUAL", "WERCKMEISTER III", "VALLOTTI", "KIRNBERGER III", "MEANTONE 1/4", "PYTHAGOREAN", "JUST 5-LIMIT" };
}

const double* temperamentCents (int index)
{
    static const double tables[7][12] = {
        { 0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100 },
        { 0, 90.225, 192.180, 294.135, 390.225, 498.045, 588.270, 696.090, 792.180, 888.270, 996.090, 1092.180 },
        { 0, 94.135, 196.090, 298.045, 392.180, 501.955, 592.180, 698.045, 796.090, 894.135, 1000.000, 1090.225 },
        { 0, 90.225, 193.157, 294.135, 386.314, 498.045, 590.224, 696.578, 792.180, 889.735, 996.090, 1088.269 },
        { 0, 76.049, 193.157, 310.265, 386.314, 503.422, 579.471, 696.578, 772.627, 889.735, 1006.843, 1082.892 },
        { 0, 113.685, 203.910, 294.135, 407.820, 498.045, 611.730, 701.955, 815.640, 905.865, 996.090, 1109.775 },
        { 0, 111.731, 203.910, 315.641, 386.314, 498.045, 590.224, 701.955, 813.686, 884.359, 1017.596, 1088.269 } };
    return tables[juce::jlimit (0, 6, index)];
}

std::string temperamentScala (int index)
{
    if (index <= 0) return {};
    const auto* c = temperamentCents (index);
    juce::String t;
    t << "! riky-" << index << ".scl\n" << temperamentNames()[index] << "\n 12\n!\n";
    for (int i = 1; i < 12; ++i) t << " " << juce::String (c[i], 3) << "\n";
    t << " 2/1\n";
    return t.toStdString();
}

juce::String formatValue (const juce::String& p, float v)
{
    if (p == id::tone)
    {
        const int n = juce::roundToInt (std::abs (v) * 100.f);
        return n == 0 ? juce::String ("FLAT") : (v < 0 ? "DARK " : "BRIGHT ") + juce::String (n);
    }
    if (p == id::tune) { const int c = juce::roundToInt (v); return (c > 0 ? "+" : "") + juce::String (c) + " CT"; }
    if (p == id::transpose) { const int s = juce::roundToInt (v); return (s > 0 ? "+" : "") + juce::String (s) + " ST"; }
    if (p == id::volume) return db (v);
    if (p == id::tempo) return juce::String (juce::roundToInt (v)) + " BPM";
    if (p.startsWith ("SFZ")) return juce::String (juce::roundToInt (v * 127.f));   // come un valore CC 0..127
    return pct (v);
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    l.add (makeFloat (id::tone, "Tone", Range (-1.f, 1.f), 0.f));
    l.add (makeFloat (id::tune, "Fine Tune", Range (-50.f, 50.f, .1f), 0.f));
    l.add (makeFloat (id::transpose, "Transpose", Range (-12.f, 12.f, 1.f), 0.f));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::a4ref, 1 }, "A4 Reference", juce::StringArray { "440 HZ", "432 HZ" }, 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::velCurve, 1 }, "Velocity Curve",
                                                         juce::StringArray { "NORMAL", "SOFT", "HARD", "FIXED" }, 0));

    // valori iniziali: al caricamento di un file prendono quelli scritti nel file (set_cc / set_hdcc)
    for (int i = 0; i < numSlots; ++i)
        l.add (makeFloat (id::sfzSlot (i), ("SFZ Control " + juce::String (i + 1)).toRawUTF8(), Range (0.f, 1.f), .5f));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::quality, 1 }, "Sample Quality", juce::StringArray { "STANDARD", "HIGH", "ULTRA" }, 1));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::poly, 1 }, "Polyphony", juce::StringArray { "64 VOICES", "128 VOICES", "256 VOICES" }, 1));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::temper, 1 }, "Temperament", temperamentNames(), 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::temperKey, 1 }, "Temperament Key",
                                                         juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::pedal, 1 }, "Sustain Pedal", false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::soft, 1 }, "Soft Pedal", false));
    l.add (makeFloat (id::softAmt, "Soft Amount", Range (0.f, 1.f), .4f));

    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::roomOn, 1 }, "Room", true));
    l.add (makeFloat (id::roomSize, "Room Size", Range (0.f, 1.f), .5f));
    l.add (makeFloat (id::roomDamp, "Room Damping", Range (0.f, 1.f), .45f));
    l.add (makeFloat (id::roomMix, "Room Mix", Range (0.f, 1.f), .2f));
    l.add (makeFloat (id::width, "Stereo Width", Range (0.f, 2.f), 1.f));

    Range vol (-60.f, 12.f, .1f); vol.setSkewForCentre (-6.f);
    l.add (makeFloat (id::volume, "Volume", vol, 0.f));

    l.add (makeFloat (id::tempo, "Tempo", Range (40.f, 240.f, 1.f), 100.f));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::metro, 1 }, "Metronome", false));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::countIn, 1 }, "Count-In", juce::StringArray { "OFF", "1 BAR", "2 BARS" }, 1));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::cues, 1 }, "Audio Cues", true));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::seqPlay, 1 }, "Song Play/Stop", false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::seqStop, 1 }, "Song Stop", false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::seqRec, 1 }, "Song Record", false));
    return l;
}
}
