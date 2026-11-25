#include "Params.h"

namespace riky
{
namespace
{
using Float = juce::AudioParameterFloat;
using Range = juce::NormalisableRange<float>;

juce::String db (float d) { return d <= -59.9f ? "-INF DB" : juce::String (d, 1) + " DB"; }
juce::String pct (float v) { return juce::String (juce::roundToInt (v * 100.f)) + "%"; }

std::unique_ptr<Float> makeFloat (const char* pid, const char* name, Range r, float def)
{
    return std::make_unique<Float> (juce::ParameterID { pid, 1 }, name, r, def,
                                    juce::AudioParameterFloatAttributes().withStringFromValueFunction ([pid] (float v, int) { return formatValue (pid, v); }));
}
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

    // valori iniziali uguali a quelli del file SFZ (set_hdcc nella sezione <control>)
    l.add (makeFloat (id::dynamics, "Dynamics", Range (0.f, 1.f), 1.f));
    l.add (makeFloat (id::release, "Release", Range (0.f, 1.f), .575f));
    l.add (makeFloat (id::keyNoise, "Key Noise", Range (0.f, 1.f), 1.f));
    l.add (makeFloat (id::pedNoise, "Pedal Noise", Range (0.f, 1.f), .1f));

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
    return l;
}
}
