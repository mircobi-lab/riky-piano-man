/*  Applicazione Standalone di Riky Piano Man con lo stile MBStudio anche per finestra, menu e dialoghi.
    - al primo avvio abilita tutti gli ingressi MIDI collegati (tastiere e controller);
    - argomenti di prova: --sfz FILE.sfz  --install  --param ID=VALORE  --shot FILE.png  (salva un'immagine del pannello ed esce)        */
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

class RikyStandaloneApp final : public juce::JUCEApplication
{
public:
    RikyStandaloneApp()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = juce::CharPointer_UTF8 (JucePlugin_Name);
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        o.folderName = "~/.config";
       #endif
        appProperties.setStorageParameters (o);
    }

    const juce::String getApplicationName() override     { return juce::CharPointer_UTF8 (JucePlugin_Name); }
    const juce::String getApplicationVersion() override  { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override           { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    void initialise (const juce::String& commandLine) override
    {
        auto* settings = appProperties.getUserSettings();
        const bool firstRun = ! settings->containsKey ("rikyFirstRunDone");
        settings->setValue ("rikyFirstRunDone", true);

        juce::LookAndFeel::setDefaultLookAndFeel (&hardwareLook);
        mainWindow = std::make_unique<juce::StandaloneFilterWindow> (getApplicationName(), mbstudio::ui::colours::chassisBottom,
                                                                     settings, false);
        if (firstRun)
            for (auto& dev : juce::MidiInput::getAvailableDevices())
                mainWindow->pluginHolder->deviceManager.setMidiInputDeviceEnabled (dev.identifier, true);
        mainWindow->setVisible (true);
        // profilo del controller scelto dal nome della porta MIDI (solo se non ci sono gia' assegnazioni)
        if (auto* proc = dynamic_cast<RikyProcessor*> (mainWindow->getAudioProcessor()))
        {
            juce::StringArray ports;
            for (auto& dev : juce::MidiInput::getAvailableDevices())
                if (mainWindow->pluginHolder->deviceManager.isMidiInputDeviceEnabled (dev.identifier)) ports.add (dev.name);
            proc->midiMap.autoSelectFromPorts (ports);
        }
        applyCommandLine (commandLine);
    }

    void applyCommandLine (const juce::String& cmd)
    {
        auto args = juce::StringArray::fromTokens (cmd, true);
        auto* proc = dynamic_cast<RikyProcessor*> (mainWindow->getAudioProcessor());
        if (proc == nullptr) return;
        juce::String shot, a11yReport;
        bool installing = args.contains ("--install");
        for (int i = 0; i + 1 < args.size(); ++i)
        {
            const auto a = args[i], v = args[i + 1].unquoted();
            if (a == "--sfz") proc->loadPiano (juce::File (v));
            else if (a == "--shot") shot = v;
            else if (a == "--install") installing = true;
            else if (a == "--param")   // --param ID=valore (valore reale del parametro)
            {
                if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (proc->state.getParameter (v.upToFirstOccurrenceOf ("=", false, false))))
                    prm->setValueNotifyingHost (prm->convertTo0to1 (v.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
            }
            else if (a == "--a11y") a11yReport = v;
            else if (a == "--profile") proc->midiMap.applyProfile (v.getIntValue());
            else if (a == "--learn") proc->midiMap.setLearnMode (true);
        }
        if (installing) proc->installPiano();
        if (a11yReport.isNotEmpty())
            juce::Timer::callAfterDelay (3000, [this, a11yReport] { writeA11yReport (juce::File (a11yReport)); if (shotPath.isEmpty()) systemRequestedQuit(); });
        shotPath = shot;
        if (shot.isNotEmpty()) waitAndShoot (shot, installing, 0);
    }

    // --install: aspetta la fine del download (al massimo 15 minuti), poi scatta l'immagine del pannello
    void waitAndShoot (const juce::String& shot, bool installing, int waitedMs)
    {
        auto* proc = dynamic_cast<RikyProcessor*> (mainWindow->getAudioProcessor());
        using LS = riky::SampleLibrary::State;
        const bool busy = proc != nullptr && installing && (proc->library.state() == LS::downloading || proc->library.state() == LS::extracting);
        if (busy && waitedMs < 15 * 60 * 1000)
        {
            if (waitedMs % 10000 == 0)
            {
                std::printf ("install %s %d%%\n", proc->library.state() == LS::downloading ? "download" : "extract", juce::roundToInt (proc->library.progress() * 100.f));
                std::fflush (stdout);
            }
            if (waitedMs == 8000) snapshot (juce::File (shot).getSiblingFile (juce::File (shot).getFileNameWithoutExtension() + "_progress.png"));
            juce::Timer::callAfterDelay (500, [this, shot, installing, waitedMs] { waitAndShoot (shot, installing, waitedMs + 500); });
            return;
        }
        if (proc != nullptr && installing)
            std::printf ("install result: %s %s\n", proc->library.state() == LS::done ? "DONE" : "FAILED", proc->library.errorText().toRawUTF8());
        juce::Timer::callAfterDelay (4000, [this, shot]
        {
            snapshot (juce::File (shot));
            systemRequestedQuit();
        });
    }

    // --a11y FILE: albero di accessibilità nell'ordine di Tab, come lo vede un lettore di schermo
    void writeA11yReport (const juce::File& f)
    {
        auto* ed = mainWindow->getAudioProcessor()->getActiveEditor();
        if (ed == nullptr) return;
        juce::String out;
        int missing = 0, n = 0;
        std::function<void (juce::AccessibilityHandler&, int)> walk = [&] (juce::AccessibilityHandler& h, int depth)
        {
            for (auto* hh : h.getChildren())   // nell'ordine di navigazione del lettore di schermo
            {
                const auto role = hh->getRole();
                const bool silent = role == juce::AccessibilityRole::ignored || role == juce::AccessibilityRole::unspecified;
                if (! silent)
                {
                    ++n;
                    const auto title = hh->getTitle();
                    if (title.trim().isEmpty()) ++missing;
                    out << juce::String::repeatedString ("  ", depth) << "[" << (int) role << "] " << (title.isEmpty() ? juce::String ("(SENZA NOME)") : title);
                    if (auto* v = hh->getValueInterface()) out << "  = " << v->getCurrentValueAsString();
                    if (hh->getHelp().isNotEmpty()) out << "   | " << hh->getHelp();
                    out << "\n";
                }
                walk (*hh, silent ? depth : depth + 1);
            }
        };
        if (auto* root = ed->getTopLevelComponent()->getAccessibilityHandler())
        {
            out << "radice: " << root->getTitle() << ", figli " << (int) root->getChildren().size() << "\n";
            walk (*root, 0);
        }
        else out << "nessun gestore di accessibilità per l'editor\n";
        out = "elementi: " + juce::String (n) + ", senza nome: " + juce::String (missing) + "\n" + out;
        f.replaceWithText (out);
    }

    void snapshot (const juce::File& f)
    {
        if (auto* ed = mainWindow->getAudioProcessor()->getActiveEditor())
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.f);
            f.deleteFile();
            if (auto os = f.createOutputStream()) juce::PNGImageFormat().writeImageToStream (img, *os);
        }
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr) mainWindow->pluginHolder->savePluginState();
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
            juce::Timer::callAfterDelay (100, [] { if (auto app = JUCEApplicationBase::getInstance()) app->systemRequestedQuit(); });
        else
            quit();
    }

private:
    juce::ApplicationProperties appProperties;
    mbstudio::ui::HardwareLookAndFeel hardwareLook;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
    juce::String shotPath;
};

JUCE_CREATE_APPLICATION_DEFINE (RikyStandaloneApp)
