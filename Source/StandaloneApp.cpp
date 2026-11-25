/*  Applicazione Standalone di Riky Piano Man con lo stile MBStudio anche per finestra, menu e dialoghi.
    - al primo avvio abilita tutti gli ingressi MIDI collegati (tastiere e controller);
    - argomenti di prova: --sfz FILE.sfz  --install  --shot FILE.png  (salva un'immagine del pannello ed esce)        */
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
        juce::String shot;
        bool installing = args.contains ("--install");
        for (int i = 0; i + 1 < args.size(); ++i)
        {
            const auto a = args[i], v = args[i + 1].unquoted();
            if (a == "--sfz") proc->loadPiano (juce::File (v));
            else if (a == "--shot") shot = v;
            else if (a == "--install") installing = true;
            else if (a == "--profile") proc->midiMap.applyProfile (v.getIntValue());
            else if (a == "--learn") proc->midiMap.setLearnMode (true);
        }
        if (installing) proc->installPiano();
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
};

JUCE_CREATE_APPLICATION_DEFINE (RikyStandaloneApp)
