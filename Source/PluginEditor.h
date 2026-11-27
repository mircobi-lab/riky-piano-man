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

// Gruppo invisibile per il lettore di schermo: una sezione del pannello con il suo nome e il suo posto
// nell'ordine di Tab (VoiceOver e NVDA annunciano "gruppo Sequencer" ecc.)
class A11yGroup : public juce::Component
{
public:
    explicit A11yGroup (const juce::String& title)
    {
        setTitle (title);
        setFocusContainerType (FocusContainerType::focusContainer);
        setInterceptsMouseClicks (false, true);
    }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::group);
    }
};

// Riquadro di testo leggibile con Tab: il display dello stato e quello del brano.
// Il titolo cambia con lo stato; Invio o clic fanno l'azione del riquadro.
class A11yText : public juce::Component
{
public:
    A11yText() { setWantsKeyboardFocus (true); }
    std::function<void()> onPress;
    void setText (const juce::String& t) { if (t != getTitle()) setTitle (t); }
    void mouseDown (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu() && onPress) onPress(); }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::returnKey && onPress) { onPress(); return true; }
        return false;
    }
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }
    void paint (juce::Graphics& g) override
    {
        if (hasKeyboardFocus (false))   // anello di fuoco visibile per chi aiuta Riky guardando lo schermo
        {
            g.setColour (juce::Colour (0xff3d8bff));
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.f), 4.f, 2.f);
        }
    }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::staticText,
            juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this] { if (onPress) onPress(); }));
    }
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

    static constexpr int baseW = 1240, baseH = 700;
    bool keyPressed (const juce::KeyPress&) override;
    void say (const juce::String& text, bool important = true);   // annuncio per il lettore di schermo
    void focusStart();                                               // il fuoco va sul display dello stato
    static void paintRikyLogo (juce::Graphics&, juce::Rectangle<float> area);   // marchio RIKY PIANO MAN
    void setPage (int newPage, bool announce);                       // 0 pianoforte, 1 modifica del suono

private:
    enum class Kind { knob, combo, toggle, fader, hslider };
    struct Ctl
    {
        Kind kind = Kind::knob;
        juce::String pid, label;
        int slot = -1;                       // SFZ CONTROLS: indice del posto
        juce::Component* group = nullptr;    // gruppo per il lettore di schermo
        juce::Slider slider;
        juce::ComboBox combo;
        juce::TextButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
        juce::Rectangle<int> cell;
        juce::Component& comp() { return kind == Kind::combo ? (juce::Component&) combo : kind == Kind::toggle ? (juce::Component&) button : (juce::Component&) slider; }
    };
    struct Section { juce::String title, scope; juce::Rectangle<int> area; int page = 0; };

    RikyProcessor& proc;
    std::vector<std::unique_ptr<Ctl>> ctls;
    std::vector<Section> sections;
    juce::TextButton installBtn { "INSTALL" }, libraryBtn { "LIBRARY" }, panicBtn { "PANIC" };
    juce::OwnedArray<juce::TextButton> switchBtns;
    A11yGroup gStatus { "Stato del pianoforte" }, gSeq { "Sequencer: demo, registrazione, esportazione" }, gPiano { "Suono del pianoforte" },
              gPedal { "Pedali" }, gSfz { "Controlli dello strumento" }, gArt { "Articolazioni" }, gRoom { "Stanza" }, gOut { "Uscita" },
              gEngine { "Motore" }, gTuning { "Accordatura" }, gMidi { "Controller MIDI" },
              gPages { "Pagine del pannello" }, gPreset { "Preset del suono" }, gInstr { "Feltro e attacco" }, gEq { "Equalizzatore" },
              gFilter { "Filtri" }, gTape { "Nastro" }, gLevel { "Livello del suono su misura" }, gComp { "Compressore" },
              gLayer { "Secondo strato" }, gArp { "Arpeggiatore" };
    // pagina 0: suona (pianoforte, pedali, motore...); pagina 1: modifica del suono (preset, EQ, compressore, strato, arpeggio)
    int page = 0;
    juce::TextButton pianoTab { "PIANO" }, editTab { "EDIT SOUND" };
    juce::ComboBox presetBox;
    juce::TextButton presetPrev { "<" }, presetNext { ">" }, presetSave { "SAVE" };
    int shownPreset = -1, shownPresetCount = -1, saidPreset = -2;
    bool shownEdited = false;
    void refreshPresetBox();
    void choosePreset (int index, bool announce);
    juce::String presetSpoken() const;          // "Preset 4 di 17: Felt Piano, modificato"
    void askPresetName();
    std::vector<juce::Component*> pageGroups (int pageIndex);
    A11yText statusView, songView;
    juce::TextButton helpBtn { "HELP" };
    juce::TextEditor commandBox;          // per la tastiera braille: si scrive "suona", "registra", "tempo 90"...
    juce::String commandReply;
    juce::uint32 commandReplyMs = 0;
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

    Ctl& add (Kind, const char* pid, const juce::String& label, juce::Component& group);
    void place (juce::Component&, juce::Rectangle<int> panelArea);   // posiziona dentro il gruppo, con coordinate del pannello
    void setupAccessibility();
    void announceChanges();
    juce::String statusText (bool full) const;
    juce::String songText() const;
    void announceHelp();
    void runCommand (const juce::String&);
    void quickExport (riky::Exporter::Format);
    void openGuide();
    void learnFocused (bool forget);
    // ultimo stato annunciato
    int saidEngine = -1, saidLib = -1, saidSeq = -1, saidSwitch = -2, saidProgress = -1;
    juce::String saidMidiStatus, lastSongName;
    bool saidExportBusy = false;
    int a11yTicks = 0;
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
    RikyPanel& getPanel() { return panel; }
private:
    mbstudio::ui::HardwareLookAndFeel lnf;
    RikyProcessor& proc;
    RikyPanel panel;
};
