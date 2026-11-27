#pragma once
#include "Exporter.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace riky
{
// Impostazioni dell'export lette dai parametri: le usa il plugin e le usa lo strumento di prova
// (così la taratura dei preset passa dallo stesso codice degli export).
Exporter::Job buildJob (juce::AudioProcessorValueTreeState&, const juce::File& sfz);
}
