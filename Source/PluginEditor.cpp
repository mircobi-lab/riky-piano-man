#include "PluginEditor.h"
#include "LcdFont.h"

namespace ui = mbstudio::ui;
namespace C = mbstudio::ui::colours;
using namespace riky;
using Status = PianoEngine::Status;
using LibState = SampleLibrary::State;

namespace
{
constexpr int margin = 14, lineY = 112, chipY = 124, cellY = 144, cellH = 88, knobW = 60, step = 64;
constexpr int row2LabelY = 236, row2Y = 252, row2H = 28;
// seconda fila: pedali, motore, accordatura, articolazioni
constexpr int lineB = 292, chipB = 304, cellB = 324, comboB = 342, comboH = 26;
// terza fila: sequencer
constexpr int lineC = 424, chipC = 434, seqY = 454, footerY = 494, keyboardY = 516;
constexpr int lcdY = chipY, lcdH = 16, lcdW = 136;
constexpr float lcdPitch = 1.32f;
const juce::Colour lcdGlass (0xff07142e), lcdDotOff (0xff10285a);

const char* noteName (int n)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[(n % 12 + 12) % 12];
}
}

// ================================================================================================ marchio
void RikyPanel::paintRikyLogo (juce::Graphics& g, juce::Rectangle<float> r)
{
    // simbolo: sette tasti di pianoforte in un riquadro, poi RIKY in grande e PIANO MAN nel badge contornato
    const float h = r.getHeight();
    const juce::Rectangle<float> mark (r.getX(), r.getY() + h * .08f, h * .84f, h * .84f);
    g.setColour (C::print);
    g.fillRoundedRectangle (mark, h * .08f);
    {
        const auto keys = mark.reduced (h * .07f);
        const float kw = keys.getWidth() / 5.f;
        g.setColour (C::chipText);
        for (int i = 1; i < 5; ++i)
            g.fillRect (keys.getX() + kw * (float) i - .6f, keys.getY(), 1.2f, keys.getHeight());
        for (int i : { 1, 2, 4 })
            g.fillRoundedRectangle (keys.getX() + kw * (float) i - kw * .32f, keys.getY(), kw * .64f, keys.getHeight() * .58f, 1.f);
        ui::paintLed (g, { mark.getRight() - h * .14f, mark.getBottom() - h * .14f }, h * .05f, C::ledBlue, 1.f);
    }

    auto big = ui::Fonts::display (h * 1.02f).withExtraKerningFactor (.06f);
    const float x = mark.getRight() + h * .26f;
    const float rw = juce::GlyphArrangement::getStringWidth (big, "RIKY");
    g.setColour (C::print);
    g.setFont (big);
    g.drawText ("RIKY", juce::Rectangle<float> (x, r.getY() + h * .04f, rw + 4.f, h), juce::Justification::centredLeft, false);

    auto bf = ui::Fonts::display (h * .5f).withExtraKerningFactor (.14f);
    const float bw = juce::GlyphArrangement::getStringWidth (bf, "PIANO MAN") + h * .5f;
    const juce::Rectangle<float> badge (x + rw + h * .22f, r.getCentreY() - h * .3f, bw, h * .6f);
    g.drawRoundedRectangle (badge.reduced (.75f), 3.f, 1.6f);
    g.setFont (bf);
    g.drawText ("PIANO MAN", badge.translated (h * .02f, h * .03f), juce::Justification::centred, false);
}

// ================================================================================================ pannello
RikyPanel::RikyPanel (RikyProcessor& p)
    : proc (p), keyboard (p.keyboardState), midiButtons (p.midiMap), midiOverlay (p.midiMap)
{
    sections = { { "SFZ CONTROLS", "FROM THE SFZ FILE", { 246, chipY, 512, 160 } },
                 { "ROOM", {}, { 778, chipY, 192, 160 } },
                 { "OUTPUT", {}, { 992, chipY, 234, 160 } },
                 { "PEDAL", {}, { margin, chipB, 212, 110 } },
                 { "ENGINE", {}, { 246, chipB, 250, 110 } },
                 { "TUNING", {}, { 516, chipB, 242, 110 } },
                 { "ARTICULATION", "KEYSWITCHES", { 778, chipB, 448, 110 } },
                 { "SEQUENCER", "MIDI PLAYER AND RECORDER", { margin, chipC, baseW - 2 * margin, 50 } } };

    // PIANO
    add (Kind::knob, id::tone, "TONE").cell = { margin, cellY, knobW, cellH };
    add (Kind::knob, id::tune, "TUNE").cell = { margin + step, cellY, knobW, cellH };
    add (Kind::knob, id::transpose, "TRANSP").cell = { margin + 2 * step, cellY, knobW, cellH };
    add (Kind::combo, id::a4ref, "A4 REF").cell = { margin, row2Y, 92, row2H };
    add (Kind::combo, id::velCurve, "VEL CURVE").cell = { margin + 100, row2Y, 104, row2H };
    // SFZ CONTROLS: i nomi arrivano dal file caricato
    for (int i = 0; i < numSlots; ++i)
    {
        auto& c = add (Kind::knob, id::sfzSlot (i), "-");
        c.slot = i;
        c.cell = { 246 + i * step, cellY, knobW, cellH };
    }
    // ROOM
    add (Kind::knob, id::roomSize, "SIZE").cell = { 778, cellY, knobW, cellH };
    add (Kind::knob, id::roomDamp, "DAMP").cell = { 778 + step, cellY, knobW, cellH };
    add (Kind::knob, id::roomMix, "MIX").cell = { 778 + 2 * step, cellY, knobW, cellH };
    add (Kind::toggle, id::roomOn, "ROOM").cell = { 778, row2Y, 84, row2H };
    // OUTPUT
    add (Kind::fader, id::volume, "VOLUME").cell = { 998, cellY, 50, 136 };
    add (Kind::knob, id::width, "WIDTH").cell = { 1070, cellY, knobW, cellH };
    // PEDAL
    add (Kind::knob, id::softAmt, "SOFT AMT").cell = { margin, cellB, knobW, cellH };
    add (Kind::toggle, id::pedal, "SUSTAIN").cell = { 94, cellB + 6, 132, 28 };
    add (Kind::toggle, id::soft, "SOFT").cell = { 94, cellB + 46, 132, 28 };
    // ENGINE
    add (Kind::combo, id::quality, "QUALITY").cell = { 246, comboB, 118, comboH };
    add (Kind::combo, id::poly, "POLYPHONY").cell = { 372, comboB, 124, comboH };
    // TUNING
    add (Kind::combo, id::temper, "TEMPERAMENT").cell = { 516, comboB, 166, comboH };
    add (Kind::combo, id::temperKey, "KEY").cell = { 690, comboB, 68, comboH };
    temperArea = { 516, comboB + 34, 242, 34 };

    find (id::roomOn)->button.getProperties().set ("ledColour", "ffffb020");

    installBtn.setTooltip ("Scarica il pianoforte Maestro Concert Grand (circa 256 MB) dal repository ufficiale sfzinstruments su GitHub");
    installBtn.onClick = [this] { askInstall(); };
    libraryBtn.setTooltip ("Strumenti SFZ: recenti, libreria nella cartella Musica/Riky Piano Man, altro file...");
    libraryBtn.onClick = [this] { showLibraryMenu(); };
    panicBtn.setTooltip ("MIDI PANIC: spegne subito tutte le note e azzera i pedali");
    panicBtn.getProperties().set ("led", true);
    panicBtn.getProperties().set ("ledColour", "ffff3b30");
    panicBtn.onClick = [this]
    {
        proc.requestPanic();
        proc.keyboardState.allNotesOff (0);
        panicFlash = 15;
        touched = "PANIC"; touchedValue = "ALL NOTES OFF"; touchedMs = juce::Time::getMillisecondCounter();
    };
    for (auto* b : { &installBtn, &libraryBtn, &panicBtn }) addAndMakeVisible (*b);

    // SEQUENCER
    demoBtn.setTooltip ("Carica e suona la demo \"Riky's Theme\", brano originale scritto per questo pianoforte");
    demoBtn.onClick = [this] { proc.loadDemo(); proc.sequencer.play(); seqMessage = {}; };
    loadBtn.setTooltip ("Apri un file MIDI (.mid) da suonare");
    loadBtn.onClick = [this] { loadMidi(); };
    saveBtn.setTooltip ("Salva il brano o la registrazione come file MIDI (.mid)");
    saveBtn.onClick = [this] { saveMidi(); };
    recBtn.setTooltip ("Registra quello che suoni (tastiera MIDI, tastiera del pannello, SUSTAIN). STOP chiude la ripresa");
    recBtn.onClick = [this] { proc.sequencer.record(); seqMessage = {}; };
    playBtn.setTooltip ("Suona il brano dal principio");
    playBtn.onClick = [this] { proc.sequencer.play(); seqMessage = {}; };
    stopBtn.onClick = [this] { proc.sequencer.stop(); };
    exportBtn.setTooltip ("Esporta il brano in MP3 o WAV, fuori tempo reale e alla qualita' massima");
    exportBtn.onClick = [this] { showExportMenu(); };
    recBtn.getProperties().set ("led", true);   recBtn.getProperties().set ("ledColour", "ffff3b30");
    playBtn.getProperties().set ("led", true);  playBtn.getProperties().set ("ledColour", "ff38d26b");
    exportBtn.getProperties().set ("led", true); exportBtn.getProperties().set ("ledColour", "ffffb020");
    for (auto* b : { &demoBtn, &loadBtn, &saveBtn, &recBtn, &playBtn, &stopBtn, &exportBtn }) addAndMakeVisible (*b);
    addAndMakeVisible (meter);

    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffdcdcd8));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff0d0d0e));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff3a3c40));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, C::ledBlue.withAlpha (.55f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, C::ledBlue.withAlpha (.18f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (.5f));
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, C::key);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, C::print);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setWantsKeyboardFocus (false);
    addAndMakeVisible (keyboard);
    addAndMakeVisible (midiButtons);
    addAndMakeVisible (midiOverlay);

    juce::Component::SafePointer<RikyPanel> safeThis (this);
    proc.onStatusChanged = [safeThis]
    {
        if (safeThis == nullptr) return;
        if (safeThis->exportPending && ! safeThis->proc.exporter.busy())
        {
            safeThis->exportPending = false;
            const auto r = safeThis->proc.exporter.result();
            if (r.ok) { safeThis->lastExport = r.file; safeThis->seqMessage = {}; }
            else safeThis->seqMessage = "EXPORT: " + r.text;
        }
        safeThis->instrumentChanged();
        safeThis->updateButtons();
        safeThis->updateLcd();
        safeThis->repaint();
    };
    setSize (baseW, baseH);
    instrumentChanged();
    updateButtons();
    updateLcd();
    startTimerHz (30);
}

RikyPanel::~RikyPanel()
{
    stopTimer();
    proc.onStatusChanged = nullptr;
    for (auto& c : ctls) { c->sa.reset(); c->ca.reset(); c->ba.reset(); }
}

RikyPanel::Ctl& RikyPanel::add (Kind kind, const char* pid, const juce::String& label)
{
    auto c = std::make_unique<Ctl>();
    c->kind = kind; c->pid = pid; c->label = label;
    auto* cp = c.get();
    auto* param = proc.state.getParameter (pid);
    auto show = [this, cp, param]
    {
        touched = cp->label;
        touchedValue = param != nullptr ? param->getCurrentValueAsText().toUpperCase() : juce::String();
        touchedMs = juce::Time::getMillisecondCounter();
        repaint (displayArea);
        repaint (cp->cell.expanded (6, 4));
    };
    switch (kind)
    {
        case Kind::knob:
        case Kind::fader:
        {
            const juce::String p (pid);
            c->slider.setSliderStyle (kind == Kind::knob ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearVertical);
            c->slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            c->slider.setColour (juce::Slider::trackColourId, p.startsWith ("ROOM") ? C::ledAmber : p.startsWith ("SFZ") ? C::ledBlue : C::ledGreen);
            c->slider.setColour (juce::Slider::thumbColourId, C::print);
            c->slider.setMouseDragSensitivity (200);
            if (param != nullptr) c->slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
            if (p == id::tone || p == id::tune || p == id::transpose) c->slider.getProperties().set ("bipolar", true);
            c->sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.state, pid, c->slider);
            c->slider.onValueChange = show;
            break;
        }
        case Kind::combo:
            if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (param)) c->combo.addItemList (ch->choices, 1);
            c->ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.state, pid, c->combo);
            c->combo.onChange = show;
            break;
        case Kind::toggle:
            c->button.setButtonText (label);
            c->button.setClickingTogglesState (true);
            c->ba = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.state, pid, c->button);
            c->button.onClick = show;
            break;
    }
    if (auto* tip = dynamic_cast<juce::SettableTooltipClient*> (&c->comp()); tip != nullptr && param != nullptr)
        tip->setTooltip (param->getName (64));
    mbstudio::midi::tag (c->comp(), pid);
    addAndMakeVisible (c->comp());
    ctls.push_back (std::move (c));
    return *cp;
}

RikyPanel::Ctl* RikyPanel::find (const char* pid)
{
    for (auto& c : ctls) if (c->pid == pid) return c.get();
    return nullptr;
}

// ------------------------------------------------------------------------------------------------ campioni
void RikyPanel::askInstall()
{
    if (proc.library.state() == LibState::downloading || proc.library.state() == LibState::extracting)
    {
        proc.library.cancel();
        return;
    }
    const auto dest = SampleLibrary::installFolder();
    const auto opts = juce::MessageBoxOptions()
        .withIconType (juce::MessageBoxIconType::QuestionIcon)
        .withTitle ("Install Maestro Concert Grand")
        .withMessage ("Riky Piano Man will download the Maestro Concert Grand samples (about 256 MB) from the official GitHub repository:\n"
                      + juce::String (SampleLibrary::repoUrl) + "\n\nThey will be saved, unchanged and with the author's readme, in:\n"
                      + dest.getFullPathName() + "\n\nSamples (c) Mats Helgesson 2003. Free sound set: it may not be sold.")
        .withButton (juce::MessageBoxButtonOptions ("Download", 1))
        .withButton (juce::MessageBoxButtonOptions ("Cancel", 0).withRole (juce::MessageBoxButtonRole::cancel))
        .withAssociatedComponent (this);
    juce::Component::SafePointer<RikyPanel> safeThis (this);
    juce::AlertWindow::showAsync (opts, [safeThis] (int r)
    {
        if (safeThis != nullptr && r == 1) { safeThis->proc.installPiano(); safeThis->updateButtons(); safeThis->repaint(); }
    });
}

void RikyPanel::chooseFile()
{
    auto start = proc.engine.currentFile();
    chooser = std::make_unique<juce::FileChooser> ("Maestro Concert Grand.sfz", start.existsAsFile() ? start.getParentDirectory() : SampleLibrary::baseFolder(), "*.sfz;*.SFZ");
    juce::Component::SafePointer<RikyPanel> safeThis (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safeThis] (const juce::FileChooser& fc)
    {
        if (safeThis == nullptr) return;
        if (const auto f = fc.getResult(); f.existsAsFile()) safeThis->proc.loadPiano (f);
        safeThis->repaint();
    });
}

bool RikyPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (f.endsWithIgnoreCase (".sfz") || juce::File (f).isDirectory()) return true;
    return false;
}

void RikyPanel::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files)
    {
        juce::File file (f);
        if (file.isDirectory()) file = SampleLibrary::findSfz (file);
        if (file.existsAsFile() && file.hasFileExtension ("sfz")) { proc.loadPiano (file); break; }
    }
    repaint();
}

void RikyPanel::showDisplayMenu()
{
    const auto f = proc.engine.currentFile();
    juce::PopupMenu m;
    m.addSectionHeader ("MAESTRO CONCERT GRAND");
    m.addItem (1, "SHOW SAMPLES FOLDER", f.existsAsFile());
    m.addItem (2, "OPEN AUTHOR README (MCGREADME.TXT)", f.getSiblingFile ("mcgreadme.txt").existsAsFile());
    m.addItem (3, "RELOAD", f.existsAsFile());
    m.addSeparator();
    m.addItem (4, "CHOOSE ANOTHER SFZ FILE...");
    m.addItem (5, "DOWNLOAD AGAIN FROM GITHUB...");
    juce::Component::SafePointer<RikyPanel> safeThis (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (displayArea)), [safeThis, f] (int r)
    {
        if (safeThis == nullptr) return;
        if (r == 1) f.revealToUser();
        else if (r == 2) f.getSiblingFile ("mcgreadme.txt").startAsProcess();
        else if (r == 3) safeThis->proc.loadPiano (f);
        else if (r == 4) safeThis->chooseFile();
        else if (r == 5) safeThis->askInstall();
    });
}

void RikyPanel::mouseDown (const juce::MouseEvent& e)
{
    if (seqArea.contains (e.getPosition()) && lastExport.existsAsFile()) { lastExport.revealToUser(); return; }
    if (! displayArea.contains (e.getPosition()) || e.mods.isPopupMenu()) return;
    const auto st = proc.engine.status();
    if (st == Status::ready || st == Status::loading) showDisplayMenu();
    else askInstall();
}

// ------------------------------------------------------------------------------------------------ sequencer
void RikyPanel::loadMidi()
{
    chooser = std::make_unique<juce::FileChooser> ("MIDI", SampleLibrary::baseFolder(), "*.mid;*.midi;*.MID");
    juce::Component::SafePointer<RikyPanel> safeThis (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safeThis] (const juce::FileChooser& fc)
    {
        if (safeThis == nullptr) return;
        if (const auto f = fc.getResult(); f.existsAsFile())
            safeThis->seqMessage = safeThis->proc.sequencer.loadMidiFile (f) ? juce::String() : "NOT A READABLE MIDI FILE: " + f.getFileName().toUpperCase();
        safeThis->repaint();
    });
}

void RikyPanel::saveMidi()
{
    if (proc.sequencer.isEmpty()) return;
    const auto folder = SampleLibrary::baseFolder().getChildFile ("Songs");
    folder.createDirectory();
    const auto name = juce::File::createLegalFileName (proc.sequencer.name().isNotEmpty() ? proc.sequencer.name() : juce::String ("song"));
    chooser = std::make_unique<juce::FileChooser> ("MIDI", folder.getNonexistentChildFile (name, ".mid"), "*.mid");
    juce::Component::SafePointer<RikyPanel> safeThis (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safeThis] (const juce::FileChooser& fc)
    {
        if (safeThis == nullptr) return;
        auto f = fc.getResult();
        if (f == juce::File()) return;
        if (! f.hasFileExtension ("mid;midi")) f = f.withFileExtension ("mid");
        safeThis->seqMessage = safeThis->proc.sequencer.saveMidiFile (f) ? "SAVED " + f.getFileName().toUpperCase() : "CANNOT SAVE " + f.getFileName().toUpperCase();
        safeThis->repaint();
    });
}

void RikyPanel::showExportMenu()
{
    if (proc.exporter.busy()) { proc.exporter.cancel(); return; }
    using F = Exporter::Format;
    juce::PopupMenu m;
    m.addSectionHeader ("EXPORT THE SONG (OFFLINE, MAX QUALITY, 44.1 KHZ)");
    const bool can = ! proc.sequencer.isEmpty() && proc.engine.status() == Status::ready;
    m.addItem (1, "MP3  320 KBPS (CBR)", can);
    m.addItem (2, "MP3  V0 (VBR, ABOUT 245 KBPS)", can);
    m.addItem (3, "WAV  24 BIT", can);
    juce::Component::SafePointer<RikyPanel> safeThis (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportBtn), [safeThis] (int r)
    {
        if (safeThis == nullptr || r == 0) return;
        const auto fmt = r == 1 ? F::mp3_320 : r == 2 ? F::mp3_v0 : F::wav24;
        const auto folder = SampleLibrary::baseFolder().getChildFile ("Exports");
        folder.createDirectory();
        const auto name = juce::File::createLegalFileName (safeThis->proc.sequencer.name().isNotEmpty() ? safeThis->proc.sequencer.name() : juce::String ("Riky Piano Man"));
        safeThis->chooser = std::make_unique<juce::FileChooser> ("EXPORT", folder.getNonexistentChildFile (name, Exporter::extension (fmt)),
                                                                fmt == F::wav24 ? "*.wav" : "*.mp3");
        safeThis->chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                                        [safeThis, fmt] (const juce::FileChooser& fc)
        {
            if (safeThis == nullptr) return;
            auto f = fc.getResult();
            if (f == juce::File()) return;
            f = f.withFileExtension (Exporter::extension (fmt));
            safeThis->lastExport = juce::File();
            safeThis->exportPending = safeThis->proc.exportSong (f, fmt);
            safeThis->seqMessage = safeThis->exportPending ? juce::String() : juce::String ("EXPORT NOT STARTED");
            safeThis->repaint();
        });
    });
}

void RikyPanel::updateTransport()
{
    using S = Sequencer::State;
    const auto st = proc.sequencer.state();
    const bool exporting = proc.exporter.busy();
    recBtn.setToggleState (st == S::recording && (juce::Time::getMillisecondCounter() / 400) % 2 == 0, juce::dontSendNotification);
    playBtn.setToggleState (st == S::playing, juce::dontSendNotification);
    exportBtn.setToggleState (exporting, juce::dontSendNotification);
    exportBtn.setButtonText (exporting ? "CANCEL EXPORT" : "EXPORT MP3 / WAV");
    const bool empty = proc.sequencer.isEmpty();
    playBtn.setEnabled (! empty);
    saveBtn.setEnabled (! empty && st != S::recording);
    exportBtn.setEnabled (exporting || (! empty && st == S::stopped));
}

void RikyPanel::updateButtons()
{
    const auto ls = proc.library.state();
    const bool busy = ls == LibState::downloading || ls == LibState::extracting;
    const bool ready = proc.engine.status() == Status::ready || proc.engine.status() == Status::loading;
    installBtn.setButtonText (busy ? "CANCEL" : ready ? "RE-INSTALL" : "INSTALL");
    installBtn.getProperties().set ("led", ! ready);
    installBtn.getProperties().set ("ledColour", busy ? "ffffb020" : "ff3d8bff");
    installBtn.setToggleState (! ready, juce::dontSendNotification);
    installBtn.repaint();
}

// ------------------------------------------------------------------------------------------------ strumento
void RikyPanel::instrumentChanged()
{
    info = proc.engine.status() == Status::ready ? proc.engine.info() : nullptr;
    // SFZ CONTROLS: nome dal file, posti vuoti spenti
    for (auto& c : ctls)
        if (c->slot >= 0)
        {
            const bool on = info != nullptr && c->slot < (int) info->controls.size();
            c->label = on ? info->controls[(size_t) c->slot].label.toUpperCase() : juce::String ("-");
            c->slider.setEnabled (on);
            c->slider.setAlpha (on ? 1.f : .3f);
            c->slider.setTooltip (on ? info->controls[(size_t) c->slot].label + " (CC " + juce::String (info->controls[(size_t) c->slot].cc) + ")" : juce::String());
        }
    // tastiera: tutta l'estensione dello strumento, keyswitch compresi, mai meno di 88 tasti
    int lo = 21, hi = 108;
    if (info != nullptr)
        for (int k = 0; k < 128; ++k)
            if (info->keys[(size_t) k] || info->switches[(size_t) k]) { lo = juce::jmin (lo, k); hi = juce::jmax (hi, k); }
    // la tastiera parte e finisce su un tasto bianco
    auto black = [] (int n) { const int m = n % 12; return m == 1 || m == 3 || m == 6 || m == 8 || m == 10; };
    if (black (lo)) --lo;
    if (black (hi)) ++hi;
    keyboard.info = info;
    keyboard.setAvailableRange (lo, hi);
    keyboard.setLowestVisibleKey (lo);
    int whites = 0;
    for (int k = lo; k <= hi; ++k) if (! black (k)) ++whites;
    keyboard.setKeyWidth ((float) (baseW - 2 * margin) / (float) juce::jmax (1, whites));
    keyboard.repaint();
    layoutSwitches();
    repaint();
}

void RikyPanel::layoutSwitches()
{
    switchBtns.clear();
    if (info == nullptr) return;
    juce::Array<int> keys;
    for (int k = 0; k < 128; ++k) if (info->switches[(size_t) k]) keys.add (k);
    const int maxButtons = 6, cols = 3, bw = 144, bh = 26;
    const bool more = keys.size() > maxButtons;
    for (int i = 0; i < juce::jmin (keys.size(), maxButtons); ++i)
    {
        const bool isMore = more && i == maxButtons - 1;
        const int key = keys[i];
        auto it = info->switchLabels.find (key);
        juce::String text = isMore ? "MORE (" + juce::String (keys.size() - (maxButtons - 1)) + ")"
                                   : (it != info->switchLabels.end() ? it->second : juce::String (noteName (key)) + juce::String (key / 12 - 1)).toUpperCase();
        auto* b = switchBtns.add (new juce::TextButton (text));
        b->getProperties().set ("led", ! isMore);
        b->getProperties().set ("ledColour", "ffffb020");
        b->setTooltip (isMore ? juce::String ("Altre articolazioni") : "Keyswitch " + juce::String (noteName (key)) + juce::String (key / 12 - 1));
        b->setBounds (778 + (i % cols) * (bw + 8), cellB + 8 + (i / cols) * (bh + 12), bw, bh);
        b->getProperties().set ("switchKey", isMore ? -1 : key);
        b->onClick = [this, key, isMore, keys]
        {
            if (! isMore) { pressSwitch (key); return; }
            juce::PopupMenu m;
            for (int k : keys)
            {
                auto f = info->switchLabels.find (k);
                m.addItem (k + 1, (f != info->switchLabels.end() ? f->second : juce::String ("KEYSWITCH")) + "  (" + noteName (k) + juce::String (k / 12 - 1) + ")",
                           true, k == proc.engine.currentSwitch());
            }
            juce::Component::SafePointer<RikyPanel> safeThis (this);
            m.showMenuAsync (juce::PopupMenu::Options(), [safeThis] (int r) { if (safeThis != nullptr && r > 0) safeThis->pressSwitch (r - 1); });
        };
        addAndMakeVisible (b);
    }
    shownSwitch = -2;
}

void RikyPanel::pressSwitch (int key)
{
    // come premere il tasto sulla tastiera: il motore riconosce il keyswitch
    proc.keyboardState.noteOn (1, key, .8f);
    proc.keyboardState.noteOff (1, key, 0.f);
}

void RikyPanel::showLibraryMenu()
{
    juce::PopupMenu m;
    const auto current = proc.engine.currentFile();
    std::vector<juce::File> files;
    auto addFile = [&] (juce::PopupMenu& menu, const juce::File& f, const juce::String& text)
    {
        files.push_back (f);
        menu.addItem ((int) files.size(), text, true, f == current);
    };
    m.addSectionHeader ("RECENT");
    const auto rec = SampleLibrary::recent();
    if (rec.isEmpty()) m.addItem (-1, "(NONE)", false);
    for (auto& f : rec) addFile (m, f, f.getFileNameWithoutExtension().toUpperCase() + "   -   " + f.getParentDirectory().getFileName());
    m.addSectionHeader ("LIBRARY  (MUSIC / RIKY PIANO MAN)");
    const auto lib = SampleLibrary::findAllSfz (SampleLibrary::baseFolder());
    if (lib.isEmpty()) m.addItem (-1, "(EMPTY: PUT SFZ INSTRUMENTS IN THIS FOLDER)", false);
    juce::String lastFolder;
    juce::PopupMenu sub;
    for (auto& f : lib)
    {
        const auto rel = f.getRelativePathFrom (SampleLibrary::baseFolder());
        addFile (m, f, rel.replaceCharacter ('\\', '/').upToLastOccurrenceOf (".", false, false).toUpperCase());
    }
    m.addSeparator();
    m.addItem (10001, "OPEN ANOTHER SFZ FILE...");
    m.addItem (10002, "SHOW LIBRARY FOLDER");
    m.addItem (10003, "MAESTRO CONCERT GRAND: DOWNLOAD...");
    juce::Component::SafePointer<RikyPanel> safeThis (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&libraryBtn), [safeThis, files] (int r)
    {
        if (safeThis == nullptr || r == 0) return;
        if (r == 10001) safeThis->chooseFile();
        else if (r == 10002) { SampleLibrary::baseFolder().createDirectory(); SampleLibrary::baseFolder().startAsProcess(); }
        else if (r == 10003) safeThis->askInstall();
        else if (r > 0 && r <= (int) files.size()) safeThis->proc.loadPiano (files[(size_t) r - 1]);
    });
}

// ------------------------------------------------------------------------------------------------ tastiera
void RikyKeyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour line, juce::Colour text)
{
    juce::Colour c = findColour (whiteNoteColourId);
    if (isSwitch (note)) c = note == currentSwitch ? juce::Colour (0xffffb020) : juce::Colour (0xffb98a3a);
    else if (! used (note)) c = juce::Colour (0xff77787a);
    if (isDown) c = c.overlaidWith (findColour (keyDownOverlayColourId));
    if (isOver) c = c.overlaidWith (findColour (mouseOverKeyOverlayColourId));
    g.setColour (c);
    g.fillRect (area);
    g.setColour (line);
    g.fillRect (area.withWidth (1.f));
    if (note == getRangeEnd()) g.fillRect (area.withLeft (area.getRight() - 1.f));
    juce::ignoreUnused (text);
    if (note % 12 == 0)
    {
        g.setColour (juce::Colour (0xff3a3c40));
        g.setFont (mbstudio::ui::Fonts::label (10.f));
        g.drawText ("C" + juce::String (note / 12 - 1), area.withTrimmedTop (area.getHeight() - 16.f), juce::Justification::centred, false);
    }
}

void RikyKeyboard::drawBlackNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour fill)
{
    juce::Colour c = fill;
    if (isSwitch (note)) c = note == currentSwitch ? juce::Colour (0xffffb020) : juce::Colour (0xff7a5a20);
    else if (! used (note)) c = juce::Colour (0xff2a2b2e);
    if (isDown) c = c.overlaidWith (findColour (keyDownOverlayColourId));
    if (isOver) c = c.overlaidWith (findColour (mouseOverKeyOverlayColourId));
    g.setColour (c);
    g.fillRect (area);
    if (! isDown)
    {
        g.setColour (c.brighter (.35f));
        const auto sideIndent = area.getWidth() * .125f, topIndent = area.getHeight() * .875f;
        g.fillRect (area.reduced (sideIndent, 0).removeFromTop (topIndent));
        g.setColour (c);
        g.fillRect (area.reduced (sideIndent * 1.4f, 0).removeFromTop (topIndent * .95f));
    }
}

// ------------------------------------------------------------------------------------------------ layout
void RikyPanel::resized()
{
    displayArea = { 420, 14, 560, 90 };
    lcdArea = { margin, lcdY, lcdW, lcdH };
    installBtn.setBounds (994, 16, 112, 30);
    libraryBtn.setBounds (1112, 16, 114, 30);
    midiButtons.setBounds (994, 58, 166, 30);
    panicBtn.setBounds (1166, 58, 60, 30);

    for (auto& c : ctls)
    {
        switch (c->kind)
        {
            case Kind::knob:   c->slider.setBounds (c->cell.getX(), c->cell.getY() + 14, knobW, 54); break;
            case Kind::fader:  c->slider.setBounds (c->cell.getX(), c->cell.getY() - 2, c->cell.getWidth(), 112); break;
            case Kind::combo:  c->combo.setBounds (c->cell); break;
            case Kind::toggle: c->button.setBounds (c->cell); break;
        }
    }
    meter.setBounds (1070, 266, 144, 8);
    demoBtn.setBounds (margin, seqY, 84, 28);
    loadBtn.setBounds (margin + 92, seqY, 76, 28);
    saveBtn.setBounds (margin + 174, seqY, 76, 28);
    recBtn.setBounds (282, seqY, 84, 28);
    playBtn.setBounds (372, seqY, 84, 28);
    stopBtn.setBounds (462, seqY, 76, 28);
    seqArea = { 556, seqY - 2, 470, 32 };
    exportBtn.setBounds (1040, seqY, 186, 28);
    keyboard.setBounds (margin, keyboardY, baseW - 2 * margin, baseH - keyboardY - 12);
    midiOverlay.setBounds (getLocalBounds());
}

// ------------------------------------------------------------------------------------------------ disegno
void RikyPanel::paintDisplay (juce::Graphics& g)
{
    const auto d = displayArea.toFloat();
    ui::paintRecess (g, d);
    const auto in = d.reduced (14.f, 9.f);
    const auto st = proc.engine.status();
    const auto ls = proc.library.state();
    const bool busy = ls == LibState::downloading || ls == LibState::extracting;

    juce::String title;
    if (busy) title = ls == LibState::downloading ? "DOWNLOADING  " + juce::String (juce::roundToInt (proc.library.progress() * 100.f)) + "%" : "INSTALLING...";
    else if (st == Status::ready) title = info != nullptr ? info->name.toUpperCase() : juce::String ("READY");
    else if (st == Status::loading) title = "LOADING PIANO...";
    else if (st == Status::error) title = "PIANO NOT LOADED";
    else title = "PIANO NOT INSTALLED";
    g.setColour (C::print);
    g.setFont (ui::Fonts::display (28.f));
    g.drawText (title, in.withHeight (30.f).withTrimmedRight (110.f), juce::Justification::centredLeft, true);

    g.setFont (ui::Fonts::label (13.f));
    auto line = [&] (juce::Rectangle<float> r, std::initializer_list<std::pair<juce::String, juce::String>> parts)
    {
        float x = r.getX();
        for (auto& [label, value] : parts)
        {
            g.setColour (C::printDim);
            const float lw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), label);
            g.drawText (label, juce::Rectangle<float> (x, r.getY(), lw + 2.f, r.getHeight()), juce::Justification::centredLeft, false);
            x += lw + 6.f;
            g.setColour (C::print);
            const float vw = juce::jmin (r.getRight() - x, juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), value) + 2.f);
            g.drawText (value, juce::Rectangle<float> (x, r.getY(), vw, r.getHeight()), juce::Justification::centredLeft, true);
            x += vw + 18.f;
        }
    };
    const auto row2 = juce::Rectangle<float> (in.getX(), in.getY() + 34.f, in.getWidth(), 16.f);
    const auto row3 = juce::Rectangle<float> (in.getX(), in.getY() + 54.f, in.getWidth() - 130.f, 16.f);

    if (busy)
    {
        // barra di avanzamento a LED blu
        const auto bar = row2.withHeight (8.f).translated (0.f, 4.f);
        g.setColour (C::ledOff); g.fillRoundedRectangle (bar, 2.f);
        g.setColour (C::ledBlue); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * proc.library.progress()), 2.f);
        line (row3, { { "FROM", "GITHUB / SFZINSTRUMENTS" }, { "MB", juce::String (proc.library.bytesDone() / 1048576) + " / 256" } });
    }
    else if (ls == LibState::failed && st != Status::ready)
        line (row2, { { "ERROR", proc.library.errorText().toUpperCase() } });
    else if (st == Status::error)
        line (row2, { { "ERROR", proc.engine.errorText() }, { "FILE", proc.engine.currentFile().getFileName().toUpperCase() } });
    else if (st == Status::empty)
        line (row2, { { "", "CLICK INSTALL  -  256 MB FROM GITHUB, ONE TIME ONLY" } });
    else if (info != nullptr && info->name.containsIgnoreCase ("Maestro Concert Grand"))
        line (row2, { { "YAMAHA CF-3", "SAMPLED BY MATS HELGESSON 2003" }, { "SFZ", "KINWIE" } });
    else if (info != nullptr)
        line (row2, { { "SFZ", info->file.getFileName().toUpperCase() }, { "IN", info->file.getParentDirectory().getFileName().toUpperCase() } });

    if (! busy)
    {
        const bool showTouched = touched.isNotEmpty() && juce::Time::getMillisecondCounter() - touchedMs < 2500;
        if (showTouched)
            line (row3, { { touched, touchedValue } });
        else if (st == Status::ready || st == Status::loading)
        {
            const int last = proc.engine.lastNote();
            juce::String lastText = last >= 0 ? juce::String (noteName (last)) + juce::String (last / 12 - 1) : juce::String ("--");
            if (info != nullptr && last >= 0)
                if (auto k = info->keyLabels.find (last); k != info->keyLabels.end()) lastText << " " << k->second.toUpperCase();
            const int sw = proc.engine.currentSwitch();
            juce::String swText;
            if (info != nullptr && sw >= 0)
            {
                auto k = info->switchLabels.find (sw);
                swText = k != info->switchLabels.end() ? k->second.toUpperCase() : juce::String (noteName (sw)) + juce::String (sw / 12 - 1);
            }
            if (swText.isNotEmpty())
                line (row3, { { "REGIONS", juce::String (proc.engine.numRegions()) },
                              { "VOICES", juce::String (proc.engine.activeVoices()) },
                              { "ART", swText }, { "LAST", lastText } });
            else
                line (row3, { { "REGIONS", juce::String (proc.engine.numRegions()) },
                              { "SAMPLES", juce::String (info != nullptr ? info->samples : 0) },
                              { "VOICES", juce::String (proc.engine.activeVoices()) + " / " + juce::String (polyphony ((int) proc.state.getRawParameterValue (id::poly)->load())) },
                              { "LAST", lastText } });
        }
        else
            line (row3, { { "FOR", "RIKY MERCENATI" } });
    }

    auto ledLabel = [&] (float x, const juce::String& t, bool on)
    {
        ui::paintLed (g, { x, row3.getCentreY() }, 3.f, C::ledGreen, on ? 1.f : 0.f);
        g.setColour (C::printDim); g.setFont (ui::Fonts::label (11.f));
        g.drawText (t, juce::Rectangle<float> (x + 7.f, row3.getY(), 60.f, row3.getHeight()), juce::Justification::centredLeft, false);
    };
    ledLabel (d.getRight() - 128.f, "SUSTAIN", proc.engine.pedalDown());
    ledLabel (d.getRight() - 58.f, "SOFT", proc.engine.softDown());
    const juce::Colour stateLed = st == Status::ready ? C::ledGreen : st == Status::loading || busy ? C::ledAmber : C::ledRed;
    ui::paintLed (g, { d.getRight() - 14.f, d.getY() + 12.f }, 3.5f, stateLed,
                  st == Status::ready ? juce::jmax (.35f, juce::jmin (1.f, proc.outputLevel() * 3.f)) : 1.f);
    ui::paintPrinted (g, { d.getRight() - 120.f, d.getY() + 8.f, 98.f, 14.f }, st == Status::ready ? "CLICK = MENU" : "", 11.f,
                      juce::Justification::centredRight, C::printDim.withAlpha (.6f));
    if (dragOver)
    {
        g.setColour (C::ledAmber.withAlpha (.8f));
        g.drawRoundedRectangle (d.reduced (2.f), 3.f, 1.5f);
    }
}

void RikyPanel::paintTemperament (juce::Graphics& g)
{
    // scarto di ogni nota dal temperamento equabile, a partire dalla tonica scelta: barre a LED
    const int t = (int) proc.state.getRawParameterValue (id::temper)->load();
    const int key = (int) proc.state.getRawParameterValue (id::temperKey)->load();
    const auto* c = temperamentCents (t);
    const auto r = temperArea.toFloat();
    const float w = r.getWidth() / 12.f, mid = r.getY() + 12.f;
    g.setColour (C::printDim.withAlpha (.4f));
    g.fillRect (r.getX(), mid, r.getWidth(), 1.f);
    for (int n = 0; n < 12; ++n)
    {
        const int degree = (n - key + 12) % 12;
        const float dev = (float) (c[degree] - degree * 100.0);          // centesimi rispetto all'equabile
        const float h = juce::jlimit (-11.f, 11.f, dev * .9f);   // 1 pixel circa ogni centesimo
        const juce::Rectangle<float> bar (r.getX() + n * w + 3.f, h < 0 ? mid + 1.f : mid - h, w - 6.f, std::abs (h));
        g.setColour (t == 0 ? C::ledOff : (std::abs (dev) > 12.f ? C::ledAmber : C::ledGreen));
        g.fillRect (bar);
        g.setColour (n == key ? C::print : C::printDim);
        g.setFont (ui::Fonts::label (9.5f));
        g.drawText (noteName (n), juce::Rectangle<float> (r.getX() + n * w, r.getY() + 24.f, w, 10.f), juce::Justification::centred, false);
    }
}

void RikyPanel::paintSequencer (juce::Graphics& g)
{
    using S = Sequencer::State;
    const auto a = seqArea.toFloat();
    ui::paintRecess (g, a);
    const auto in = a.reduced (10.f, 4.f);
    auto mmss = [] (double t)
    {
        const int cs = juce::roundToInt (juce::jmax (0.0, t) * 10.0);
        return juce::String (cs / 600).paddedLeft ('0', 2) + ":" + juce::String ((cs / 10) % 60).paddedLeft ('0', 2) + "." + juce::String (cs % 10);
    };
    const auto st = proc.sequencer.state();
    const double len = proc.sequencer.length();
    const double posS = st == S::stopped ? 0.0 : proc.sequencer.position();

    juce::String left, right;
    float fraction = 0.f;
    juce::Colour bar = C::ledGreen;
    if (proc.exporter.busy())
    {
        left = "EXPORTING  " + proc.sequencer.name().toUpperCase();
        right = juce::String (juce::roundToInt (proc.exporter.progress() * 100.f)) + "%";
        fraction = proc.exporter.progress();
        bar = C::ledAmber;
    }
    else if (st == S::recording)
    {
        left = "RECORDING  -  PLAY, THEN STOP";
        right = mmss (posS);
        fraction = (float) std::fmod (posS, 10.0) / 10.f;
        bar = C::ledRed;
    }
    else if (seqMessage.isNotEmpty())
        left = seqMessage;
    else if (const auto r = proc.exporter.result(); r.text.isNotEmpty() && st == S::stopped && r.file == lastExport && lastExport != juce::File())
        left = r.text + "  -  CLICK TO SHOW";
    else if (proc.sequencer.isEmpty())
        left = "NO SONG  -  PRESS DEMO, LOAD A MIDI FILE OR REC";
    else
    {
        left = proc.sequencer.name().toUpperCase() + "   " + juce::String (proc.sequencer.numNotes()) + " NOTES";
        right = mmss (posS) + " / " + mmss (len);
        fraction = len > 0 ? (float) (posS / len) : 0.f;
    }
    g.setFont (ui::Fonts::label (12.f));
    g.setColour (C::print);
    g.drawText (left, in.withHeight (14.f).withTrimmedRight (130.f), juce::Justification::centredLeft, true);
    g.setFont (ui::Fonts::display (15.f));
    g.drawText (right, in.withHeight (16.f).withLeft (in.getRight() - 128.f), juce::Justification::centredRight, false);
    const auto track = juce::Rectangle<float> (in.getX(), in.getBottom() - 5.f, in.getWidth(), 4.f);
    g.setColour (C::ledOff);
    g.fillRoundedRectangle (track, 1.5f);
    g.setColour (bar);
    g.fillRoundedRectangle (track.withWidth (track.getWidth() * juce::jlimit (0.f, 1.f, fraction)), 1.5f);
}

void RikyPanel::paint (juce::Graphics& g)
{
    ui::paintChassis (g, getLocalBounds());

    paintRikyLogo (g, { 18.f, 14.f, 390.f, 52.f });
    ui::paintPrinted (g, { 18.f, 76.f, 400.f, 16.f }, "CUSTOM CONCERT GRAND FOR RIKY MERCENATI", 14.f, juce::Justification::centredLeft, C::printDim);

    paintDisplay (g);
    ui::paintEngravedLine (g, { (float) margin, (float) lineY }, { (float) baseW - margin, (float) lineY });
    ui::paintEngravedLine (g, { (float) margin, (float) lineB }, { (float) baseW - margin, (float) lineB });
    ui::paintEngravedLine (g, { (float) margin, (float) lineC }, { (float) baseW - margin, (float) lineC });
    paintSequencer (g);

    for (auto& s : sections)
    {
        const auto a = s.area.toFloat();
        const float cw = ui::chipWidth (s.title, 16.f);
        ui::paintChip (g, { a.getX(), a.getY(), cw, 16.f }, s.title, 14.f);
        float lineEnd = a.getRight();
        if (s.scope.isNotEmpty())
        {
            g.setFont (ui::Fonts::label (11.f));
            const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), s.scope) + 6.f;
            g.setColour (C::printDim);
            g.drawText (s.scope, juce::Rectangle<float> (a.getRight() - tw, a.getY() + 1.f, tw, 14.f), juce::Justification::centredRight, false);
            ui::paintLed (g, { a.getRight() - tw - 6.f, a.getY() + 8.f }, 2.5f, s.title == "ARTICULATION" ? C::ledAmber : C::ledBlue, 1.f);
            lineEnd = a.getRight() - tw - 14.f;
        }
        ui::paintEngravedLine (g, { a.getX() + cw + 4.f, a.getY() + 8.f }, { lineEnd, a.getY() + 8.f });
    }

    paintLcd (g);

    for (auto& c : ctls)
    {
        const auto cell = c->cell.toFloat();
        if (c->kind == Kind::knob)
        {
            const bool off = c->slot >= 0 && ! c->slider.isEnabled();
            ui::paintPrinted (g, cell.withHeight (14.f).expanded (2.f, 0.f), c->label, c->label.length() > 9 ? 10.f : 12.f, juce::Justification::centred,
                              off ? C::printDim.withAlpha (.5f) : C::print);
            if (! off)
                ui::paintPrinted (g, { cell.getX() - 8.f, cell.getY() + 70.f, cell.getWidth() + 16.f, 14.f },
                                  formatValue (c->pid, (float) c->slider.getValue()), 11.f, juce::Justification::centred, C::printDim);
            if (c->slot >= 0 && ! off && info != nullptr)
                ui::paintPrinted (g, { cell.getX(), (float) row2Y + 6.f, cell.getWidth(), 14.f },
                                  "CC " + juce::String (info->controls[(size_t) c->slot].cc), 10.f, juce::Justification::centred, C::printDim.withAlpha (.7f));
        }
        else if (c->kind == Kind::combo)
        {
            ui::paintPrinted (g, { cell.getX(), cell.getY() - 18.f, cell.getWidth() + 20.f, 14.f }, c->label, 12.f);
        }
        else if (c->kind == Kind::fader)
        {
            ui::paintPrinted (g, { cell.getX() - 10.f, cell.getY() + 112.f, cell.getWidth() + 20.f, 14.f }, c->label, 12.f, juce::Justification::centred);
            ui::paintPrinted (g, { cell.getX() - 10.f, cell.getY() + 126.f, cell.getWidth() + 20.f, 14.f },
                              formatValue (c->pid, (float) c->slider.getValue()), 11.f, juce::Justification::centred, C::printDim);
        }
    }
    ui::paintPrinted (g, { 1070.f, 248.f, 144.f, 14.f }, "OUT", 12.f);
    if (info == nullptr || info->controls.empty())
        ui::paintPrinted (g, { 246.f, (float) row2Y + 6.f, 512.f, 14.f }, info == nullptr ? "LOAD AN SFZ INSTRUMENT" : "THIS SFZ DECLARES NO CONTROLS (LABEL_CC)", 11.f,
                          juce::Justification::centredLeft, C::printDim);

    // ENGINE: stato del motore
    {
        const int poly = polyphony ((int) proc.state.getRawParameterValue (id::poly)->load());
        const int voices = proc.engine.activeVoices();
        ui::paintPrinted (g, { 246.f, (float) comboB + 36.f, 250.f, 14.f },
                          "BOUNCE: MAX QUALITY  -  DISK STREAMING", 10.f, juce::Justification::centredLeft, C::printDim);
        // LED di occupazione delle voci
        const float x0 = 246.f, y0 = (float) comboB + 58.f, w = 250.f / 16.f;
        for (int i = 0; i < 16; ++i)
        {
            const bool on = (float) voices / (float) poly > (float) i / 16.f && voices > 0;
            g.setColour (on ? (i >= 13 ? C::ledRed : i >= 10 ? C::ledAmber : C::ledGreen) : C::ledOff);
            g.fillRoundedRectangle (x0 + i * w + 1.f, y0, w - 3.f, 5.f, 1.f);
        }
    }
    paintTemperament (g);
    if (info == nullptr || info->switches.none())
        ui::paintPrinted (g, { 778.f, (float) cellB + 30.f, 448.f, 16.f }, info == nullptr ? "NO INSTRUMENT LOADED" : "THIS SFZ HAS NO KEYSWITCHES",
                          12.f, juce::Justification::centredLeft, C::printDim);

    ui::paintPrinted (g, { (float) margin, (float) footerY, 900.f, 16.f },
                      "DOUBLE CLICK A KNOB TO RESET   -   CC64 SUSTAIN   -   CC66 SOSTENUTO   -   CC67 SOFT   -   CC120 PANIC   -   DROP AN .SFZ ON THE PANEL", 11.f,
                      juce::Justification::centredLeft, C::printDim);
    ui::paintLogo (g, { (float) baseW - 260.f, (float) footerY, 246.f, 16.f }, false, ui::Imprint::debossed, juce::Justification::centredRight);
}

// ------------------------------------------------------------------------------------------------ display LCD
void RikyPanel::updateLcd()
{
    juce::String t;
    const auto ls = proc.library.state();
    if (ls == LibState::downloading) t = "DOWNLOAD " + juce::String (juce::roundToInt (proc.library.progress() * 100.f)) + "%";
    else if (ls == LibState::extracting) t = "INSTALLING";
    else if (proc.engine.status() == Status::loading) t = "LOADING";
    else if (proc.engine.status() == Status::error) t = "ERROR - " + proc.engine.errorText();
    else if (proc.engine.status() == Status::empty) t = "INSTALL PIANO";
    else if (proc.sequencer.state() == Sequencer::State::recording) t = "REC " + juce::String ((int) proc.sequencer.position() / 60) + ":" + juce::String ((int) proc.sequencer.position() % 60).paddedLeft ('0', 2);
    else if (proc.exporter.busy()) t = "EXPORT " + juce::String (juce::roundToInt (proc.exporter.progress() * 100.f)) + "%";
    else t = "RIKY PIANO MAN";
    if (t != lcdText) { lcdText = t; lcdOffset = 0; lcdTicks = 0; repaint (lcdArea); }
}

void RikyPanel::paintLcd (juce::Graphics& g)
{
    const auto outer = lcdArea.toFloat();
    ui::paintRecess (g, outer);
    const auto glass = outer.reduced (1.5f);
    g.setColour (lcdGlass);
    g.fillRoundedRectangle (glass, 2.f);

    const juce::String loop = lcdText.length() > lcdChars ? lcdText + "    " : lcdText;
    const float pitch = lcdPitch;
    const float dot = pitch * .8f;
    const float textW = pitch * (float) (lcdChars * 6 - 1), textH = pitch * 6.f + dot;
    const float x0 = glass.getCentreX() - textW * .5f, y0 = glass.getCentreY() - textH * .5f;
    // testo corto: centrato
    const int pad = lcdText.length() < lcdChars ? (lcdChars - lcdText.length()) / 2 : 0;
    const auto glow = C::ledBlue.withAlpha (.3f);
    for (int i = 0; i < lcdChars; ++i)
    {
        juce::juce_wchar ch = ' ';
        if (loop.length() > lcdChars) ch = loop[(lcdOffset + i) % loop.length()];
        else if (i - pad >= 0 && i - pad < loop.length()) ch = loop[i - pad];
        const auto* cols = campione::lcdfont::glyph ((char32_t) ch);
        for (int c = 0; c < 5; ++c)
            for (int r = 0; r < 7; ++r)
            {
                const juce::Rectangle<float> d (x0 + (float) (i * 6 + c) * pitch, y0 + (float) r * pitch, dot, dot);
                if ((cols[c] >> r) & 1)
                {
                    g.setColour (glow); g.fillRect (d.expanded (pitch * .22f));
                    g.setColour (C::ledBlue); g.fillRect (d);
                }
                else
                {
                    g.setColour (lcdDotOff); g.fillRect (d);
                }
            }
    }
}

void RikyPanel::timerCallback()
{
    updateLcd();
    if (lcdText.length() > lcdChars)
    {
        ++lcdTicks;
        const bool atStart = lcdOffset == 0;
        if ((atStart && lcdTicks >= 45) || (! atStart && lcdTicks >= 9))
        {
            lcdTicks = 0;
            lcdOffset = (lcdOffset + 1) % (lcdText.length() + 4);
            repaint (lcdArea);
        }
    }
    meter.setLevel (std::pow (proc.outputLevel(), .5f));
    if (const int sw = proc.engine.currentSwitch(); sw != shownSwitch)
    {
        shownSwitch = sw;
        keyboard.currentSwitch = sw;
        keyboard.repaint();
        for (auto* b : switchBtns)
            b->setToggleState ((int) b->getProperties()["switchKey"] == sw, juce::dontSendNotification);
    }
    repaint (246, comboB + 54, 252, 12);   // LED delle voci
    updateTransport();
    repaint (seqArea);
    repaint (temperArea);
    if (panicFlash > 0) --panicFlash;
    panicBtn.setToggleState (panicFlash > 0 && (panicFlash / 3) % 2 == 0, juce::dontSendNotification);
    repaint (displayArea);
}

// ================================================================================================ editor
RikyEditor::RikyEditor (RikyProcessor& p) : AudioProcessorEditor (p), proc (p), panel (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (panel);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) RikyPanel::baseW / RikyPanel::baseH);
    setResizeLimits (RikyPanel::baseW / 2, RikyPanel::baseH / 2, RikyPanel::baseW * 2, RikyPanel::baseH * 2);
    setSize (RikyPanel::baseW, RikyPanel::baseH);
}

RikyEditor::~RikyEditor() { setLookAndFeel (nullptr); }

void RikyEditor::paint (juce::Graphics& g) { ui::paintChassis (g, getLocalBounds()); }

void RikyEditor::resized()
{
    const float s = (float) getWidth() / (float) RikyPanel::baseW;
    panel.setBounds (0, 0, RikyPanel::baseW, RikyPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale (s));
}
