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
constexpr int row2LabelY = 236, row2Y = 252, row2H = 28, footerY = 300, keyboardY = 322;
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
    : proc (p), keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      midiButtons (p.midiMap), midiOverlay (p.midiMap)
{
    sections = { { "MAESTRO", "SFZ", { 246, chipY, 280, 160 } },
                 { "PEDAL", {}, { 548, chipY, 190, 160 } },
                 { "ROOM", {}, { 760, chipY, 210, 160 } },
                 { "OUTPUT", {}, { 992, chipY, 234, 160 } } };

    // PIANO
    add (Kind::knob, id::tone, "TONE").cell = { margin, cellY, knobW, cellH };
    add (Kind::knob, id::tune, "TUNE").cell = { margin + step, cellY, knobW, cellH };
    add (Kind::knob, id::transpose, "TRANSP").cell = { margin + 2 * step, cellY, knobW, cellH };
    add (Kind::combo, id::a4ref, "A4 REF").cell = { margin, row2Y, 92, row2H };
    add (Kind::combo, id::velCurve, "VEL CURVE").cell = { margin + 100, row2Y, 104, row2H };
    // MAESTRO: i quattro controlli del file SFZ
    add (Kind::knob, id::dynamics, "DYNAMICS").cell = { 246, cellY, knobW, cellH };
    add (Kind::knob, id::release, "RELEASE").cell = { 246 + step, cellY, knobW, cellH };
    add (Kind::knob, id::keyNoise, "KEY NOISE").cell = { 246 + 2 * step, cellY, knobW, cellH };
    add (Kind::knob, id::pedNoise, "PED NOISE").cell = { 246 + 3 * step, cellY, knobW, cellH };
    // PEDAL
    add (Kind::knob, id::softAmt, "SOFT AMT").cell = { 548, cellY, knobW, cellH };
    add (Kind::toggle, id::pedal, "SUSTAIN").cell = { 548, row2Y, 96, row2H };
    add (Kind::toggle, id::soft, "SOFT").cell = { 650, row2Y, 76, row2H };
    // ROOM
    add (Kind::knob, id::roomSize, "SIZE").cell = { 760, cellY, knobW, cellH };
    add (Kind::knob, id::roomDamp, "DAMP").cell = { 760 + step, cellY, knobW, cellH };
    add (Kind::knob, id::roomMix, "MIX").cell = { 760 + 2 * step, cellY, knobW, cellH };
    add (Kind::toggle, id::roomOn, "ROOM").cell = { 760, row2Y, 84, row2H };
    // OUTPUT
    add (Kind::fader, id::volume, "VOLUME").cell = { 998, cellY, 50, 136 };
    add (Kind::knob, id::width, "WIDTH").cell = { 1070, cellY, knobW, cellH };

    find (id::roomOn)->button.getProperties().set ("ledColour", "ffffb020");

    installBtn.setTooltip ("Scarica il pianoforte Maestro Concert Grand (circa 256 MB) dal repository ufficiale sfzinstruments su GitHub");
    installBtn.onClick = [this] { askInstall(); };
    folderBtn.setTooltip ("Scegli il file \"Maestro Concert Grand.sfz\" se i campioni sono in un'altra cartella");
    folderBtn.onClick = [this] { chooseFile(); };
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
    for (auto* b : { &installBtn, &folderBtn, &panicBtn }) addAndMakeVisible (*b);
    addAndMakeVisible (meter);

    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffdcdcd8));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff0d0d0e));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff3a3c40));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, C::ledBlue.withAlpha (.55f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, C::ledBlue.withAlpha (.18f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (.5f));
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, C::key);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, C::print);
    keyboard.setAvailableRange (21, 108);
    keyboard.setLowestVisibleKey (21);
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
        safeThis->updateButtons();
        safeThis->updateLcd();
        safeThis->repaint();
    };
    updateButtons();
    updateLcd();
    setSize (baseW, baseH);
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
            const bool maestro = p == id::dynamics || p == id::release || p == id::keyNoise || p == id::pedNoise;
            c->slider.setColour (juce::Slider::trackColourId, p.startsWith ("ROOM") ? C::ledAmber : maestro ? C::ledBlue : C::ledGreen);
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
    if (! displayArea.contains (e.getPosition()) || e.mods.isPopupMenu()) return;
    const auto st = proc.engine.status();
    if (st == Status::ready || st == Status::loading) showDisplayMenu();
    else askInstall();
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

// ------------------------------------------------------------------------------------------------ layout
void RikyPanel::resized()
{
    displayArea = { 420, 14, 560, 90 };
    lcdArea = { margin, lcdY, lcdW, lcdH };
    installBtn.setBounds (994, 16, 112, 30);
    folderBtn.setBounds (1112, 16, 114, 30);
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
    keyboard.setBounds (margin, keyboardY, baseW - 2 * margin, baseH - keyboardY - 12);
    keyboard.setKeyWidth ((float) (baseW - 2 * margin) / 52.f);
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
    else if (st == Status::ready) title = "MAESTRO CONCERT GRAND";
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
    else
        line (row2, { { "YAMAHA CF-3", "SAMPLED BY MATS HELGESSON 2003" }, { "SFZ", "KINWIE" } });

    if (! busy)
    {
        const bool showTouched = touched.isNotEmpty() && juce::Time::getMillisecondCounter() - touchedMs < 2500;
        if (showTouched)
            line (row3, { { touched, touchedValue } });
        else if (st == Status::ready || st == Status::loading)
        {
            const int last = proc.engine.lastNote();
            line (row3, { { "REGIONS", juce::String (proc.engine.numRegions()) },
                          { "VOICES", juce::String (proc.engine.activeVoices()) },
                          { "LAST", last >= 0 ? juce::String (noteName (last)) + juce::String (last / 12 - 1) : juce::String ("--") } });
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

void RikyPanel::paint (juce::Graphics& g)
{
    ui::paintChassis (g, getLocalBounds());

    paintRikyLogo (g, { 18.f, 14.f, 390.f, 52.f });
    ui::paintPrinted (g, { 18.f, 76.f, 400.f, 16.f }, "CUSTOM CONCERT GRAND FOR RIKY MERCENATI", 14.f, juce::Justification::centredLeft, C::printDim);

    paintDisplay (g);
    ui::paintEngravedLine (g, { (float) margin, (float) lineY }, { (float) baseW - margin, (float) lineY });

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
            ui::paintLed (g, { a.getRight() - tw - 6.f, a.getY() + 8.f }, 2.5f, C::ledBlue, 1.f);
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
            ui::paintPrinted (g, cell.withHeight (14.f).expanded (8.f, 0.f), c->label, 12.f, juce::Justification::centred);
            ui::paintPrinted (g, { cell.getX() - 8.f, cell.getY() + 70.f, cell.getWidth() + 16.f, 14.f },
                              formatValue (c->pid, (float) c->slider.getValue()), 11.f, juce::Justification::centred, C::printDim);
        }
        else if (c->kind == Kind::combo)
        {
            ui::paintPrinted (g, { cell.getX(), cell.getY() - (float) (row2Y - row2LabelY) - 2.f, cell.getWidth() + 20.f, 14.f }, c->label, 12.f);
        }
        else if (c->kind == Kind::fader)
        {
            ui::paintPrinted (g, { cell.getX() - 10.f, cell.getY() + 112.f, cell.getWidth() + 20.f, 14.f }, c->label, 12.f, juce::Justification::centred);
            ui::paintPrinted (g, { cell.getX() - 10.f, cell.getY() + 126.f, cell.getWidth() + 20.f, 14.f },
                              formatValue (c->pid, (float) c->slider.getValue()), 11.f, juce::Justification::centred, C::printDim);
        }
    }
    ui::paintPrinted (g, { 1070.f, 248.f, 144.f, 14.f }, "OUT", 12.f);
    ui::paintPrinted (g, { 246.f, (float) row2Y, 270.f, (float) row2H }, "CONTROLS OF THE SFZ  -  CC 99  72  80  81", 11.f,
                      juce::Justification::centredLeft, C::printDim);

    ui::paintPrinted (g, { (float) margin, (float) footerY, 800.f, 16.f },
                      "DOUBLE CLICK A KNOB TO RESET   -   CC64 SUSTAIN   -   CC66 SOSTENUTO   -   CC67 SOFT   -   CC120 PANIC", 11.f,
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
