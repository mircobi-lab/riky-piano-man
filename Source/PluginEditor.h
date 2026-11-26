#pragma once
#include "PluginProcessor.h"
#include "MBStudioLookAndFeel.h"
#include <juce_audio_utils/juce_audio_utils.h>

// Tastiera che mostra cosa fa il file SFZ: tasti che suonano, tasti spenti, keyswitch (ambra) e keyswitch attivo
class RikyKeyboard : public juce::MidiKeyboardComponent
{
public:
    RikyKeyboard (juce::MidiKeyboardState& s) : juce::MidiKeyboardComponent (s, horizontalKeyboard) {}
    std::shared_ptr<const riky::InstrumentInfo> info;
    int currentSwitch = -1;
protected:
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float>, bool isDown, bool isOver, juce::Colour line, juce::Colour text) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float>, bool isDown, bool isOver, juce::Colour fill) override;
private:
    bool used (int note) const { return info == nullptr || info->keys[(size_t) note]; }
    bool isSwitch (int note) const { return info != nullptr && info->switches[(size_t) note]; }
};

// Pannello frontale di Riky Piano Man: telaio, sezioni e tastiera di MBStudio91 Campione,
// marchio RIKY PIANO MAN e i controlli che il file SFZ dichiara.
class RikyPanel : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit RikyPanel (RikyProcessor&);
    ~RikyPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

    static constexpr int baseW = 1240, baseH = 660;
    static void paintRikyLogo (juce::Graphics&, juce::Rectangle<float> area);   // marchio RIKY PIANO MAN

private:
    enum class Kind { knob, combo, toggle, fader };
    struct Ctl
    {
        Kind kind = Kind::knob;
        juce::String pid, label;
        int slot = -1;                       // SFZ CONTROLS: indice del posto
        juce::Slider slider;
        juce::ComboBox combo;
        juce::TextButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
        juce::Rectangle<int> cell;
        juce::Component& comp() { return kind == Kind::combo ? (juce::Component&) combo : kind == Kind::toggle ? (juce::Component&) button : (juce::Component&) slider; }
    };
    struct Section { juce::String title, scope; juce::Rectangle<int> area; };

    RikyProcessor& proc;
    std::vector<std::unique_ptr<Ctl>> ctls;
    std::vector<Section> sections;
    juce::TextButton installBtn { "INSTALL" }, libraryBtn { "LIBRARY" }, panicBtn { "PANIC" };
    juce::OwnedArray<juce::TextButton> switchBtns;
    juce::TextButton demoBtn { "DEMO" }, loadBtn { "LOAD" }, saveBtn { "SAVE" }, recBtn { "REC" }, playBtn { "PLAY" }, stopBtn { "STOP" }, exportBtn { "EXPORT MP3 / WAV" };
    juce::Rectangle<int> seqArea;
    juce::String seqMessage;
    juce::File lastExport;
    bool exportPending = false;
    mbstudio::ui::LedMeter meter { 14 };
    RikyKeyboard keyboard;
    mbstudio::midi::MidiControls midiButtons;
    mbstudio::midi::MidiLearnOverlay midiOverlay;
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<const riky::InstrumentInfo> info;
    juce::Rectangle<int> displayArea, lcdArea, temperArea;
    static constexpr int lcdChars = 16;
    juce::String lcdText;
    int lcdOffset = 0, lcdTicks = 0, shownSwitch = -2;
    juce::String touched, touchedValue;
    juce::uint32 touchedMs = 0;
    int panicFlash = 0;
    bool dragOver = false;

    Ctl& add (Kind, const char* pid, const juce::String& label);
    Ctl* find (const char* pid);
    void askInstall();
    void chooseFile();
    void showLibraryMenu();
    void showDisplayMenu();
    void instrumentChanged();
    void layoutSwitches();
    void pressSwitch (int key);
    void updateLcd();
    void updateButtons();
    void paintLcd (juce::Graphics&);
    void paintDisplay (juce::Graphics&);
    void paintTemperament (juce::Graphics&);
    void paintSequencer (juce::Graphics&);
    void loadMidi();
    void saveMidi();
    void showExportMenu();
    void updateTransport();
    void timerCallback() override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
};

class RikyEditor : public juce::AudioProcessorEditor
{
public:
    explicit RikyEditor (RikyProcessor&);
    ~RikyEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    mbstudio::ui::HardwareLookAndFeel lnf;
    RikyProcessor& proc;
    RikyPanel panel;
};
