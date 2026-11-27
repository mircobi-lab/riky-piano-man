#pragma once
#include <juce_core/juce_core.h>

namespace riky
{
// Testo SFZ del suono del secondo strato (indice del parametro L2SOUND). 0 = pianoforte con feltro: testo vuoto,
// si usa lo stesso file del pianoforte principale.
juce::String layerSfzText (int index);
inline constexpr float layerFeltAmount = .7f, layerFeltAttack = .25f;   // feltro del "pianoforte con feltro"
}
