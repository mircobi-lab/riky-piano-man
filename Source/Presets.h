#pragma once
#include "Params.h"

namespace riky
{
// Preset del suono su misura. Quelli di fabbrica imitano il colore delle Performance "Character Pianos"
// di Yamaha Montage/MODX partendo dal Maestro (nessun campione Yamaha): EQ, feltro, nastro, compressore,
// stanza e larghezza sono stati misurati sul video dimostrativo e poi regolati confrontando il risultato.
// Quelli dell'utente sono file .rikypreset nella cartella Musica/Riky Piano Man/Presets.
struct FactoryPreset
{
    const char* name;      // sul pannello
    const char* spoken;    // per il lettore di schermo
    const char* about;     // descrizione breve, italiano
    std::vector<std::pair<const char*, float>> values;   // solo quelli diversi dal valore iniziale del parametro
};

namespace presets
{
    const std::vector<FactoryPreset>& factory();
    juce::File folder();
    juce::Array<juce::File> userFiles();
    int count();                                   // fabbrica + utente
    juce::String name (int index);                 // nome visivo
    juce::String spoken (int index);               // nome da leggere
    juce::String about (int index);
    bool apply (juce::AudioProcessorValueTreeState&, int index);
    juce::File saveUser (juce::AudioProcessorValueTreeState&, const juce::String& name);   // file scritto (o vuoto)
    // tutti i parametri del suono ai valori iniziali, poi quelli del preset
    void applyValues (juce::AudioProcessorValueTreeState&, const std::vector<std::pair<juce::String, float>>& values);
}
}
