#pragma once
#include "PluginProcessor.h"
#include "MBStudioLookAndFeel.h"
#include <juce_audio_utils/juce_audio_utils.h>

// Pannello frontale di Riky Piano Man: stesso telaio, sezioni e tastiera di MBStudio91 Campione,
// con il marchio RIKY PIANO MAN e i controlli dello strumento SFZ del Maestro Concert Grand.
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

    static constexpr int baseW = 1240, baseH = 484;
    static void paintRikyLogo (juce::Graphics&, juce::Rectangle<float> area);   // marchio RIKY PIANO MAN

private:
    enum class Kind { knob, combo, toggle, fader };
    struct Ctl
    {
        Kind kind = Kind::knob;
        juce::String pid, label;
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
    juce::TextButton installBtn { "INSTALL" }, folderBtn { "FOLDER" }, panicBtn { "PANIC" };
    mbstudio::ui::LedMeter meter { 14 };
    juce::MidiKeyboardComponent keyboard;
    mbstudio::midi::MidiControls midiButtons;
    mbstudio::midi::MidiLearnOverlay midiOverlay;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> displayArea, lcdArea;
    static constexpr int lcdChars = 16;
    juce::String lcdText;
    int lcdOffset = 0, lcdTicks = 0;
    juce::String touched, touchedValue;
    juce::uint32 touchedMs = 0;
    int panicFlash = 0;
    bool dragOver = false;

    Ctl& add (Kind, const char* pid, const juce::String& label);
    Ctl* find (const char* pid);
    void askInstall();
    void chooseFile();
    void showDisplayMenu();
    void updateLcd();
    void updateButtons();
    void paintLcd (juce::Graphics&);
    void paintDisplay (juce::Graphics&);
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
