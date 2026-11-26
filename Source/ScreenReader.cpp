// Annunci per il lettore di schermo.
// Su Windows JUCE farebbe parlare la voce SAPI di Windows, una seconda voce sopra NVDA o JAWS e niente in braille.
// Qui invece si alza una notifica UI Automation (UiaRaiseNotificationEvent, Windows 10 1709 e successivi):
// NVDA, JAWS e l'Assistente vocale la leggono con la loro voce e la mostrano sulla barra braille.
#include "ScreenReader.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <ole2.h>
 #include <UIAutomation.h>

namespace
{
    // valori dell'SDK di Windows (uiautomationcoreapi.h, enum NotificationKind e NotificationProcessing)
    constexpr int kindOther = 4;                    // NotificationKind_Other
    constexpr int processingImportantAll = 0;       // NotificationProcessing_ImportantAll
    constexpr int processingMostRecent = 3;         // NotificationProcessing_MostRecent

    using HostFromHwnd = HRESULT (WINAPI*) (HWND, IRawElementProviderSimple**);
    using RaiseNotification = HRESULT (WINAPI*) (IRawElementProviderSimple*, int, int, BSTR, BSTR);

    struct Uia
    {
        HMODULE lib = LoadLibraryW (L"UIAutomationCore.dll");
        HostFromHwnd host = lib != nullptr ? (HostFromHwnd) GetProcAddress (lib, "UiaHostProviderFromHwnd") : nullptr;
        RaiseNotification raise = lib != nullptr ? (RaiseNotification) GetProcAddress (lib, "UiaRaiseNotificationEvent") : nullptr;
    };
}

bool riky::announceToScreenReader (juce::Component& c, const juce::String& text, bool important)
{
    static Uia uia;
    if (uia.host == nullptr || uia.raise == nullptr) return false;          // Windows troppo vecchio
    auto* peer = c.getPeer();
    if (peer == nullptr) return false;
    IRawElementProviderSimple* provider = nullptr;
    if (FAILED (uia.host ((HWND) peer->getNativeHandle(), &provider)) || provider == nullptr) return false;
    BSTR msg = SysAllocString (text.toWideCharPointer());
    BSTR activity = SysAllocString (important ? L"riky-important" : L"riky-info");
    const HRESULT hr = uia.raise (provider, kindOther, important ? processingImportantAll : processingMostRecent, msg, activity);
    SysFreeString (msg);
    SysFreeString (activity);
    provider->Release();
    return SUCCEEDED (hr);
}

#else

bool riky::announceToScreenReader (juce::Component&, const juce::String&, bool)
{
    return false;   // Mac e Linux: si usa l'annuncio di JUCE, che su Mac va a VoiceOver
}

#endif
