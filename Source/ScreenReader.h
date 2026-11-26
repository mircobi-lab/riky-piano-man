#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace riky
{
// Fa leggere un testo al lettore di schermo (voce e barra braille). Restituisce false se su questa
// piattaforma non c'è un modo diretto: allora si usa juce::AccessibilityHandler::postAnnouncement.
bool announceToScreenReader (juce::Component& anyComponentInTheWindow, const juce::String& text, bool important);

// Nome del tasto delle scorciatoie: Ctrl su Windows, Comando sul Mac
inline juce::String commandKeyName()
{
   #if JUCE_MAC
    return "Comando";
   #else
    return "Ctrl";
   #endif
}
// I testi sono scritti con "Ctrl" (Riky usa Windows): sul Mac diventano "Comando"
inline juce::String keys (const juce::String& s)
{
   #if JUCE_MAC
    return s.replace ("Ctrl ", "Comando ");
   #else
    return s;
   #endif
}
}
