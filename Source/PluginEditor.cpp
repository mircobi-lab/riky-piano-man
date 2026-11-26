#include "PluginEditor.h"
#include "LcdFont.h"
#include "ScreenReader.h"
#include <BinaryData.h>

namespace ui = mbstudio::ui;
namespace C = mbstudio::ui::colours;
using namespace riky;
using Status = PianoEngine::Status;
using LibState = SampleLibrary::State;

namespace
{
// disposizione: intestazione, poi il SEQUENCER (i comandi usati più spesso), poi il suono, poi pedali e motore
constexpr int margin = 14, lineY = 112, cellH = 88, knobW = 60, step = 64;
constexpr int chipC = 124, seqY = 144, seqY2 = 182, lineC = 220;                 // sequencer
constexpr int chipY = 232, cellY = 252, row2LabelY = 344, row2Y = 360, row2H = 28; // suono
constexpr int lineB = 400, chipB = 412, cellB = 432, comboB = 450, comboH = 26;    // pedali, motore, accordatura, articolazioni
constexpr int footerY = 532, keyboardY = 554;
constexpr int lcdY = chipY, lcdH = 16, lcdW = 136;
constexpr float lcdPitch = 1.32f;
const juce::Colour lcdGlass (0xff07142e), lcdDotOff (0xff10285a);

juce::String italianNote (int n)
{
    static const char* names[] = { "Do", "Do diesis", "Re", "Re diesis", "Mi", "Fa", "Fa diesis", "Sol", "Sol diesis", "La", "La diesis", "Si" };
    return juce::String (names[(n % 12 + 12) % 12]) + " " + juce::String (n / 12 - 1);
}

// nomi e spiegazioni per il lettore di schermo
struct A11yInfo { const char* pid; const char* title; const char* help; };
const A11yInfo a11yInfo[] = {
    { id::tone,      "Tono",                            "Inclinazione del suono: verso sinistra più scuro, verso destra più brillante." },
    { id::tune,      "Accordatura fine",                "Sposta tutta l'intonazione fino a 50 centesimi in su o in giù." },
    { id::transpose, "Trasposizione",                   "Sposta le note di semitoni, fino a un'ottava. Non sposta i keyswitch." },
    { id::a4ref,     "Diapason",                        "Frequenza del La: 440 o 432 hertz." },
    { id::velCurve,  "Curva di dinamica",               "Come la forza sul tasto diventa volume: normale, morbida, dura o fissa." },
    { id::roomSize,  "Dimensione della stanza",         "Grandezza dell'ambiente del riverbero." },
    { id::roomDamp,  "Smorzamento della stanza",        "Quanto la stanza assorbe gli acuti." },
    { id::roomMix,   "Quantità di stanza",              "Quanto riverbero si sente." },
    { id::roomOn,    "Stanza accesa",                   "Accende o spegne il riverbero." },
    { id::volume,    "Volume",                          "Volume generale in decibel." },
    { id::width,     "Larghezza stereo",                "Cento per cento come registrato; meno stringe verso il mono, più allarga." },
    { id::softAmt,   "Quantità del pedale una corda",   "Quanto il pedale una corda abbassa la dinamica." },
    { id::pedal,     "Pedale di risonanza",             "Tiene le note come il pedale destro. Si somma al pedale collegato alla tastiera." },
    { id::soft,      "Pedale una corda",                "Suono più morbido come il pedale sinistro." },
    { id::quality,   "Qualità del suono dal vivo",      "Standard consuma meno, alta è consigliata, massima è la stessa degli export." },
    { id::poly,      "Polifonia",                       "Quante note possono suonare insieme." },
    { id::temper,    "Temperamento",                    "Accordatura storica delle dodici note. Equabile è quella moderna." },
    { id::temperKey, "Tonica del temperamento",         "Nota da cui parte il temperamento." },
    { id::metro,     "Metronomo in registrazione",      "Click a tempo mentre si registra. Scorciatoia: lettera M." },
    { id::countIn,   "Conteggio prima di registrare",   "Battute di click prima che la registrazione parta. Scorciatoia: lettera C." },
    { id::tempo,     "Tempo",                           "Battiti al minuto per metronomo e conteggio. Scorciatoie: più e meno." },
    { id::cues,      "Segnali sonori",                  "Suoni brevi che confermano inizio e fine della registrazione, export finito ed errori." },
};
const A11yInfo* a11yFor (const juce::String& pid)
{
    for (auto& a : a11yInfo) if (pid == a.pid) return &a;
    return nullptr;
}

// testo delle voci dei menu a tendina, in italiano (stesso ordine delle scelte del parametro)
juce::StringArray italianChoices (const juce::String& pid)
{
    if (pid == id::a4ref) return { "440 Hz", "432 Hz" };
    if (pid == id::velCurve) return { "Normale", "Morbida", "Dura", "Fissa" };
    if (pid == id::quality) return { "Standard", "Alta", "Massima" };
    if (pid == id::poly) return { "64 voci", "128 voci", "256 voci" };
    if (pid == id::temper) return { "Equabile", "Werckmeister terzo", "Vallotti", "Kirnberger terzo", "Mesotonico un quarto di comma", "Pitagorico", "Naturale" };
    if (pid == id::temperKey) return { "Do", "Do diesis", "Re", "Re diesis", "Mi", "Fa", "Fa diesis", "Sol", "Sol diesis", "La", "La diesis", "Si" };
    if (pid == id::countIn) return { "Nessuno", "1 battuta", "2 battute" };
    return {};
}

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
    sections = { { "SEQUENCER", "MIDI PLAYER AND RECORDER", { margin, chipC, baseW - 2 * margin, 90 } },
                 { "SFZ CONTROLS", "FROM THE SFZ FILE", { 246, chipY, 512, 160 } },
                 { "ROOM", {}, { 778, chipY, 192, 160 } },
                 { "OUTPUT", {}, { 992, chipY, 234, 160 } },
                 { "PEDAL", {}, { margin, chipB, 212, 110 } },
                 { "ENGINE", {}, { 246, chipB, 250, 110 } },
                 { "TUNING", {}, { 516, chipB, 242, 110 } },
                 { "ARTICULATION", "KEYSWITCHES", { 778, chipB, 448, 110 } } };

    // gruppi per il lettore di schermo, nell'ordine in cui Tab li visita
    int order = 1;
    for (auto* g : { &gStatus, &gSeq, &gPiano, &gPedal, &gSfz, &gArt, &gRoom, &gOut, &gEngine, &gTuning, &gMidi })
    {
        g->setExplicitFocusOrder (order++);
        addAndMakeVisible (*g);
    }

    // PIANO
    add (Kind::knob, id::tone, "TONE", gPiano).cell = { margin, cellY, knobW, cellH };
    add (Kind::knob, id::tune, "TUNE", gPiano).cell = { margin + step, cellY, knobW, cellH };
    add (Kind::knob, id::transpose, "TRANSP", gPiano).cell = { margin + 2 * step, cellY, knobW, cellH };
    add (Kind::combo, id::a4ref, "A4 REF", gPiano).cell = { margin, row2Y, 92, row2H };
    add (Kind::combo, id::velCurve, "VEL CURVE", gPiano).cell = { margin + 100, row2Y, 104, row2H };
    // SFZ CONTROLS: i nomi arrivano dal file caricato
    for (int i = 0; i < numSlots; ++i)
    {
        auto& c = add (Kind::knob, id::sfzSlot (i), "-", gSfz);
        c.slot = i;
        c.cell = { 246 + i * step, cellY, knobW, cellH };
    }
    // ROOM
    add (Kind::knob, id::roomSize, "SIZE", gRoom).cell = { 778, cellY, knobW, cellH };
    add (Kind::knob, id::roomDamp, "DAMP", gRoom).cell = { 778 + step, cellY, knobW, cellH };
    add (Kind::knob, id::roomMix, "MIX", gRoom).cell = { 778 + 2 * step, cellY, knobW, cellH };
    add (Kind::toggle, id::roomOn, "ROOM", gRoom).cell = { 778, row2Y, 84, row2H };
    // OUTPUT
    add (Kind::fader, id::volume, "VOLUME", gOut).cell = { 998, cellY, 50, 136 };
    add (Kind::knob, id::width, "WIDTH", gOut).cell = { 1070, cellY, knobW, cellH };
    // PEDAL
    add (Kind::toggle, id::pedal, "SUSTAIN", gPedal).cell = { 94, cellB + 6, 132, 28 };
    add (Kind::toggle, id::soft, "SOFT", gPedal).cell = { 94, cellB + 46, 132, 28 };
    add (Kind::knob, id::softAmt, "SOFT AMT", gPedal).cell = { margin, cellB, knobW, cellH };
    // ENGINE
    add (Kind::combo, id::quality, "QUALITY", gEngine).cell = { 246, comboB, 118, comboH };
    add (Kind::combo, id::poly, "POLYPHONY", gEngine).cell = { 372, comboB, 124, comboH };
    // TUNING
    add (Kind::combo, id::temper, "TEMPERAMENT", gTuning).cell = { 516, comboB, 166, comboH };
    add (Kind::combo, id::temperKey, "KEY", gTuning).cell = { 690, comboB, 68, comboH };
    temperArea = { 516, comboB + 34, 242, 34 };
    // SEQUENCER, seconda riga: metronomo, conteggio, tempo, segnali sonori
    add (Kind::toggle, id::metro, "METRONOME", gSeq).cell = { margin, seqY2, 130, 26 };
    add (Kind::combo, id::countIn, "COUNT-IN", gSeq).cell = { 250, seqY2, 110, 26 };
    add (Kind::hslider, id::tempo, "TEMPO", gSeq).cell = { 420, seqY2, 240, 26 };
    add (Kind::toggle, id::cues, "AUDIO CUES", gSeq).cell = { 740, seqY2, 140, 26 };

    find (id::roomOn)->button.getProperties().set ("ledColour", "ffffb020");
    find (id::metro)->button.getProperties().set ("ledColour", "ffff3b30");
    find (id::cues)->button.getProperties().set ("ledColour", "ff3d8bff");

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
        say ("Panic: tutte le note spente");
    };
    gStatus.addAndMakeVisible (statusView);
    gStatus.addAndMakeVisible (installBtn);
    gStatus.addAndMakeVisible (libraryBtn);
    statusView.onPress = [this]
    {
        const auto st = proc.engine.status();
        if (st == Status::ready || st == Status::loading) showDisplayMenu();
        else askInstall();
    };

    // SEQUENCER
    demoBtn.onClick = [this] { proc.loadDemo(); proc.sequencer.play(); seqMessage = {}; };
    loadBtn.onClick = [this] { loadMidi(); };
    saveBtn.onClick = [this] { saveMidi(); };
    recBtn.onClick = [this] { proc.toggleRecord(); seqMessage = {}; };
    playBtn.onClick = [this] { proc.togglePlay(); seqMessage = {}; };
    stopBtn.onClick = [this] { proc.stopSong(); };
    exportBtn.onClick = [this] { showExportMenu(); };
    helpBtn.onClick = [this] { announceHelp(); openGuide(); };
    songView.onPress = [this] { if (lastExport.existsAsFile()) lastExport.revealToUser(); else say (songText()); };
    // i tasti del controller assegnati con LEARN a questi pulsanti comandano il sequencer
    mbstudio::midi::tag (playBtn, id::seqPlay);
    mbstudio::midi::tag (stopBtn, id::seqStop);
    mbstudio::midi::tag (recBtn, id::seqRec);
    recBtn.getProperties().set ("led", true);   recBtn.getProperties().set ("ledColour", "ffff3b30");
    playBtn.getProperties().set ("led", true);  playBtn.getProperties().set ("ledColour", "ff38d26b");
    exportBtn.getProperties().set ("led", true); exportBtn.getProperties().set ("ledColour", "ffffb020");
    for (auto* b : { &demoBtn, &loadBtn, &saveBtn, &recBtn, &playBtn, &stopBtn }) gSeq.addAndMakeVisible (*b);
    gSeq.addAndMakeVisible (songView);
    gSeq.addAndMakeVisible (exportBtn);
    gSeq.addAndMakeVisible (helpBtn);
    commandBox.setTitle ("Comando");
    commandBox.setHelpText (juce::CharPointer_UTF8 ("Scrivi una parola e premi Invio: suona, ferma, registra, demo, esporta, stato, tempo 90. Scrivi aiuto per l'elenco."));
    commandBox.setTooltip (commandBox.getHelpText());
    commandBox.setTextToShowWhenEmpty ("COMANDO: SCRIVI AIUTO", C::printDim);
    commandBox.setSelectAllWhenFocused (true);
    commandBox.setFont (ui::Fonts::label (14.f));
    commandBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff07090c));
    commandBox.setColour (juce::TextEditor::textColourId, C::print);
    commandBox.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3a3c40));
    commandBox.setColour (juce::TextEditor::focusedOutlineColourId, C::ledBlue);
    commandBox.setColour (juce::CaretComponent::caretColourId, C::ledBlue);
    commandBox.onReturnKey = [this] { const auto t = commandBox.getText(); commandBox.clear(); runCommand (t); };
    commandBox.onEscapeKey = [this] { commandBox.clear(); };
    gSeq.addAndMakeVisible (commandBox);
    // ordine di Tab dentro i gruppi: come si leggono i pulsanti, da sinistra a destra
    {
        int o = 1;
        for (juce::Component* c : { (juce::Component*) &commandBox, (juce::Component*) &demoBtn, (juce::Component*) &loadBtn, (juce::Component*) &saveBtn, (juce::Component*) &recBtn,
                                    (juce::Component*) &playBtn, (juce::Component*) &stopBtn, (juce::Component*) &songView, (juce::Component*) &exportBtn,
                                    &find (id::metro)->comp(), &find (id::countIn)->comp(), &find (id::tempo)->comp(), &find (id::cues)->comp(),
                                    (juce::Component*) &helpBtn })
            c->setExplicitFocusOrder (o++);
        statusView.setExplicitFocusOrder (1);
        installBtn.setExplicitFocusOrder (2);
        libraryBtn.setExplicitFocusOrder (3);
        find (id::pedal)->comp().setExplicitFocusOrder (1);
        find (id::soft)->comp().setExplicitFocusOrder (2);
        find (id::softAmt)->comp().setExplicitFocusOrder (3);
    }

    gMidi.addAndMakeVisible (midiButtons);
    gMidi.addAndMakeVisible (panicBtn);
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
    addAndMakeVisible (midiOverlay);
    setupAccessibility();

    juce::Component::SafePointer<RikyPanel> safeThis (this);
    proc.onStatusChanged = [safeThis]
    {
        if (safeThis == nullptr) return;
        if (safeThis->exportPending && ! safeThis->proc.exporter.busy())
        {
            safeThis->exportPending = false;
            const auto r = safeThis->proc.exporter.result();
            if (r.ok)
            {
                safeThis->lastExport = r.file;
                safeThis->seqMessage = {};
                safeThis->say ("Esportato: " + r.file.getFileName() + ". Picco " + juce::String (r.peakDb, 1).replaceCharacter ('.', ',') + " dB"
                               + (r.peakDb > -.1f ? ". Distorce: abbassa il volume" : ""));
            }
            else
            {
                safeThis->seqMessage = "EXPORT: " + r.text;
                safeThis->say ("Errore export: " + r.text.toLowerCase());
            }
        }
        safeThis->instrumentChanged();
        safeThis->updateButtons();
        safeThis->updateLcd();
        safeThis->announceChanges();
        safeThis->repaint();
    };
    setSize (baseW, baseH);
    instrumentChanged();
    updateButtons();
    updateLcd();
    // lo stato iniziale non si annuncia: solo i cambiamenti
    saidEngine = (int) proc.engine.status();
    saidLib = (int) proc.library.state();
    saidSeq = (int) proc.sequencer.state();
    saidSwitch = proc.engine.currentSwitch();
    lastSongName = proc.sequencer.name();
    startTimerHz (30);
}

RikyPanel::~RikyPanel()
{
    stopTimer();
    proc.onStatusChanged = nullptr;
    for (auto& c : ctls) { c->sa.reset(); c->ca.reset(); c->ba.reset(); }
}

RikyPanel::Ctl& RikyPanel::add (Kind kind, const char* pid, const juce::String& label, juce::Component& group)
{
    auto c = std::make_unique<Ctl>();
    c->kind = kind; c->pid = pid; c->label = label; c->group = &group;
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
        case Kind::hslider:
        {
            const juce::String p (pid);
            c->slider.setSliderStyle (kind == Kind::knob ? juce::Slider::RotaryHorizontalVerticalDrag
                                      : kind == Kind::fader ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal);
            c->slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            c->slider.setColour (juce::Slider::trackColourId, p.startsWith ("ROOM") ? C::ledAmber : p.startsWith ("SFZ") ? C::ledBlue : C::ledGreen);
            c->slider.setColour (juce::Slider::thumbColourId, C::print);
            c->slider.setMouseDragSensitivity (200);
            if (param != nullptr) c->slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
            if (p == id::tone || p == id::tune || p == id::transpose) c->slider.getProperties().set ("bipolar", true);
            c->sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.state, pid, c->slider);
            // valore letto dal lettore di schermo (dopo l'attachment, che imposta il suo)
            c->slider.textFromValueFunction = [p] (double v) { return accessibleValue (p, (float) v); };
            c->slider.onValueChange = show;
            break;
        }
        case Kind::combo:
            if (const auto it = italianChoices (pid); ! it.isEmpty()) c->combo.addItemList (it, 1);
            else if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (param)) c->combo.addItemList (ch->choices, 1);
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
    if (const auto* a = a11yFor (pid))
    {
        // il lettore di schermo legge il suggerimento come aiuto: è lo stesso testo, in italiano
        c->comp().setTitle (juce::CharPointer_UTF8 (a->title));
        c->comp().setHelpText (juce::CharPointer_UTF8 (a->help));
        if (auto* tip = dynamic_cast<juce::SettableTooltipClient*> (&c->comp())) tip->setTooltip (juce::CharPointer_UTF8 (a->help));
    }
    mbstudio::midi::tag (c->comp(), pid);
    group.addAndMakeVisible (c->comp());
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
    // finestra nativa del sistema: VoiceOver e NVDA la leggono come ogni altra finestra di dialogo
    const auto opts = juce::MessageBoxOptions()
        .withIconType (juce::MessageBoxIconType::QuestionIcon)
        .withTitle (juce::CharPointer_UTF8 ("Installa il pianoforte Maestro Concert Grand"))
        .withMessage (juce::String (juce::CharPointer_UTF8 ("Riky Piano Man scarica i campioni del Maestro Concert Grand, circa 256 megabyte, dal repository ufficiale su GitHub:\n"))
                      + juce::String (SampleLibrary::repoUrl) + juce::String (juce::CharPointer_UTF8 ("\n\nVengono salvati senza modifiche, con il file di note dell'autore, in:\n"))
                      + dest.getFullPathName() + juce::String (juce::CharPointer_UTF8 ("\n\nCampioni di Mats Helgesson, 2003. Set gratuito: non si può vendere.")))
        .withButton ("Scarica", 1)
        .withButton ("Annulla", 0)
        .withAssociatedComponent (this);
    juce::Component::SafePointer<RikyPanel> safeThis (this);
    juce::NativeMessageBox::showForOptionsAsync (opts, [safeThis] (int r)
    {
        if (safeThis != nullptr && r == 1)
        {
            safeThis->proc.installPiano();
            safeThis->say ("Download del pianoforte iniziato. Annuncio l'avanzamento ogni 10 per cento.");
            safeThis->updateButtons();
            safeThis->repaint();
        }
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
    focusStart();   // un clic sul fondo del pannello riporta il fuoco dentro: così le scorciatoie funzionano
    if (seqArea.contains (e.getPosition()) && lastExport.existsAsFile()) { lastExport.revealToUser(); return; }
    if (! displayArea.contains (e.getPosition()) || e.mods.isPopupMenu()) return;
    const auto st = proc.engine.status();
    if (st == Status::ready || st == Status::loading) showDisplayMenu();
    else askInstall();
}

// ------------------------------------------------------------------------------------------------ accessibilità
namespace
{
    // tempo breve per la barra braille: 1:34
    juce::String mmss (double t)
    {
        const int sec = juce::roundToInt (juce::jmax (0.0, t));
        return juce::String (sec / 60) + ":" + juce::String (sec % 60).paddedLeft ('0', 2);
    }
    juce::String songName (const juce::String& n) { return n.replace ("(DEMO)", "(demo)").replace ("TAKE", "Ripresa").toLowerCase().replace ("riky's theme", "Riky's Theme"); }
}

void RikyPanel::setupAccessibility()
{
    setTitle ("Riky Piano Man");
    setFocusContainerType (FocusContainerType::focusContainer);
    setDescription (juce::CharPointer_UTF8 ("Pianoforte per Riky Mercenati. Tab passa da un gruppo all'altro, F1 elenca i comandi da tastiera."));
    struct T { juce::Component* c; const char* title; const char* help; };
    const T items[] = {
        { &installBtn, "Installa il pianoforte", "Scarica i campioni del Maestro Concert Grand dal repository ufficiale, circa 256 megabyte, una volta sola." },
        { &libraryBtn, "Libreria degli strumenti", "Strumenti recenti, cartella Musica Riky Piano Man, altro file SFZ. Scorciatoia: Ctrl L." },
        { &panicBtn,   "Panic, spegne tutte le note", "Scorciatoia: Esc." },
        { &demoBtn,    "Demo, suona Riky's Theme", "Brano originale scritto per questo pianoforte. Scorciatoia: lettera D." },
        { &loadBtn,    "Apri un file MIDI", "Scorciatoia: Ctrl O." },
        { &saveBtn,    "Salva il brano come file MIDI", "Va nella cartella Musica, Riky Piano Man, Songs. Scorciatoia: Ctrl S." },
        { &recBtn,     "Registra", "Registra quello che suoni. Premi di nuovo, oppure S, per fermare. Scorciatoie: R oppure F6." },
        { &playBtn,    "Suona o ferma il brano", "Scorciatoie: barra spaziatrice oppure F5." },
        { &stopBtn,    "Ferma", "Scorciatoia: lettera S." },
        { &exportBtn,  "Esporta il brano in MP3 o WAV", "Ctrl E esporta subito in MP3 a 320 kilobit nella cartella Exports, Ctrl Maiuscole E in WAV." },
        { &helpBtn,    "Aiuto", "Elenca i comandi da tastiera e apre la guida. Scorciatoie: F1 oppure H." },
        { &statusView, "", "Stato del pianoforte. Invio apre il menu del pianoforte. Scorciatoia per sentirlo da ovunque: I oppure F2." },
        { &songView,   "", "Stato del brano. Dopo un'esportazione, Invio apre la cartella del file." },
    };
    for (auto& t : items)
    {
        if (*t.title != 0) t.c->setTitle (juce::CharPointer_UTF8 (t.title));
        t.c->setHelpText (keys (juce::CharPointer_UTF8 (t.help)));
        if (auto* tip = dynamic_cast<juce::SettableTooltipClient*> (t.c)) tip->setTooltip (keys (juce::CharPointer_UTF8 (t.help)));
    }
    // parti solo visive: la tastiera sullo schermo, il LED meter, il velo del MIDI LEARN
    keyboard.setAccessible (false);
    meter.setAccessible (false);
    midiOverlay.setAccessible (false);
    statusView.setText (statusText (false));
    songView.setText (songText());
    // all'apertura il fuoco va sul display dello stato
    juce::Component::SafePointer<RikyPanel> safeThis (this);
    juce::Timer::callAfterDelay (400, [safeThis] { if (safeThis != nullptr) safeThis->focusStart(); });
}

void RikyPanel::focusStart()
{
    // le scorciatoie arrivano al pannello solo se il fuoco è su qualcosa al suo interno
    auto* f = juce::Component::getCurrentlyFocusedComponent();
    if (isShowing() && (f == nullptr || ! isParentOf (f)) && (f == nullptr || f->getTopLevelComponent() == getTopLevelComponent()))
        statusView.grabKeyboardFocus();
}

void RikyPanel::say (const juce::String& rawText, bool important)
{
    if (rawText.isEmpty()) return;
    const auto text = keys (rawText);
    // Windows: notifica UI Automation, letta da NVDA, JAWS e Assistente vocale anche in braille.
    // Mac: annuncio di JUCE, letto da VoiceOver.
    if (! announceToScreenReader (*this, text, important))
        juce::AccessibilityHandler::postAnnouncement (text, important ? juce::AccessibilityHandler::AnnouncementPriority::high
                                                                      : juce::AccessibilityHandler::AnnouncementPriority::medium);
    // prove automatiche: gli annunci finiscono anche in un file
    if (const auto log = juce::SystemStats::getEnvironmentVariable ("RIKY_A11Y_LOG", {}); log.isNotEmpty())
        juce::File (log).appendText (text + "\n");
}

// Testi pensati per la barra braille di 40 celle: prima l'informazione, poi i dettagli
juce::String RikyPanel::statusText (bool full) const
{
    juce::String t;
    const auto st = proc.engine.status();
    const auto ls = proc.library.state();
    if (ls == LibState::downloading) t << "Download " << juce::roundToInt (proc.library.progress() * 100.f) << "%. ";
    else if (ls == LibState::extracting) t << "Installazione file. ";
    if (st == Status::ready && info != nullptr) t << "Pronto: " << info->name << ". ";
    else if (st == Status::loading) t << "Caricamento. ";
    else if (st == Status::error) t << "Errore: " << proc.engine.errorText().toLowerCase() << ". ";
    else if (ls != LibState::downloading && ls != LibState::extracting) t << "Non installato, Invio per scaricare. ";
    if (ls == LibState::failed && st != Status::ready) t << "Download fallito: " << proc.library.errorText().toLowerCase() << ". ";
    if (full && st == Status::ready && info != nullptr)
    {
        t << "Voci " << proc.engine.activeVoices() << " su " << polyphony ((int) proc.state.getRawParameterValue (id::poly)->load()) << ". ";
        if (proc.engine.pedalDown()) t << "Pedale giù. ";
        if (proc.engine.softDown()) t << "Una corda giù. ";
        if (const int sw = proc.engine.currentSwitch(); sw >= 0)
            if (auto k = info->switchLabels.find (sw); k != info->switchLabels.end()) t << "Articolazione " << k->second << ". ";
        if (const int last = proc.engine.lastNote(); last >= 0) t << "Ultima nota " << italianNote (last) << ". ";
        t << "Tempo " << juce::roundToInt (proc.state.getRawParameterValue (id::tempo)->load()) << ". ";
        const int temper = (int) proc.state.getRawParameterValue (id::temper)->load();
        if (temper > 0) t << "Temperamento " << italianChoices (id::temper)[temper] << ". ";
    }
    return t.trim();
}

juce::String RikyPanel::songText() const
{
    using S = Sequencer::State;
    if (commandReply.isNotEmpty() && juce::Time::getMillisecondCounter() - commandReplyMs < 15000) return commandReply;
    const auto st = proc.sequencer.state();
    if (proc.exporter.busy()) return "Esporta " + juce::String (juce::roundToInt (proc.exporter.progress() * 100.f)) + "%";
    if (st == S::countIn) return "Conteggio";
    if (st == S::recording) return "Registra " + mmss (proc.sequencer.position());
    if (proc.sequencer.isEmpty()) return "Nessun brano. D demo, R registra";
    const auto name = songName (proc.sequencer.name());
    if (st == S::playing) return "Suona " + mmss (proc.sequencer.position()) + " di " + mmss (proc.sequencer.length()) + ", " + name;
    juce::String t = "Fermo, " + name + ", " + mmss (proc.sequencer.length()) + ", " + juce::String (proc.sequencer.numNotes()) + " note";
    if (lastExport.existsAsFile()) t << ". Esportato: " << lastExport.getFileName();
    return t;
}

void RikyPanel::announceChanges()
{
    using S = Sequencer::State;
    // pianoforte
    if (const int st = (int) proc.engine.status(); st != saidEngine)
    {
        saidEngine = st;
        if ((Status) st == Status::ready && proc.engine.info() != nullptr)
            say ("Pronto: " + proc.engine.info()->name);
        else if ((Status) st == Status::error)
            say ("Errore pianoforte: " + proc.engine.errorText().toLowerCase());
    }
    // download
    if (const int ls = (int) proc.library.state(); ls != saidLib)
    {
        saidLib = ls;
        if ((LibState) ls == LibState::extracting) say ("Download finito, installazione");
        else if ((LibState) ls == LibState::failed) say ("Download fallito: " + proc.library.errorText().toLowerCase());
        saidProgress = -1;
    }
    if (proc.library.state() == LibState::downloading)
        if (const int step10 = juce::roundToInt (proc.library.progress() * 100.f) / 10; step10 != saidProgress && step10 > 0)
        {
            saidProgress = step10;
            say ("Download " + juce::String (step10 * 10) + "%", false);
        }
    // sequencer
    const auto seq = proc.sequencer.state();
    if ((int) seq != saidSeq)
    {
        const auto before = (S) saidSeq;
        saidSeq = (int) seq;
        if (seq == S::playing) say ("Suona: " + songName (proc.sequencer.name()));
        else if (seq == S::countIn)
        {
            const int bars = (int) proc.state.getRawParameterValue (id::countIn)->load();
            say ("Conteggio " + juce::String (bars) + (bars == 1 ? " battuta, " : " battute, ") + juce::String (juce::roundToInt (proc.state.getRawParameterValue (id::tempo)->load())) + " BPM");
        }
        else if (seq == S::recording && before != S::countIn) say ("Registra");
        else if (seq == S::stopped && before == S::playing)
            say (proc.sequencer.position() >= proc.sequencer.length() - .05 ? juce::String ("Fine del brano") : juce::String ("Fermo"));
        else if (seq == S::stopped && (before == S::recording || before == S::countIn) && proc.sequencer.name() == lastSongName)
            say ("Registrazione vuota: nessuna nota");
    }
    if (proc.sequencer.name() != lastSongName)
    {
        lastSongName = proc.sequencer.name();
        if (lastSongName.startsWith ("TAKE"))
            say ("Registrato: " + juce::String (proc.sequencer.numNotes()) + " note, " + mmss (proc.sequencer.length()));
        else if (lastSongName.isNotEmpty())
            say ("Brano: " + songName (lastSongName) + ", " + mmss (proc.sequencer.length()), false);
    }
    // articolazione
    if (const int sw = proc.engine.currentSwitch(); sw != saidSwitch)
    {
        saidSwitch = sw;
        if (sw >= 0 && info != nullptr)
            if (auto k = info->switchLabels.find (sw); k != info->switchLabels.end()) say ("Articolazione " + k->second, false);
    }
    // MIDI LEARN e profili del controller
    if (const auto ms = proc.midiMap.status(); ms.isNotEmpty() && ms != saidMidiStatus) { saidMidiStatus = ms; say (ms.toLowerCase(), false); }
    else if (ms.isEmpty()) saidMidiStatus = {};
    // esportazione
    if (proc.exporter.busy())
    {
        if (const int q = juce::roundToInt (proc.exporter.progress() * 100.f) / 25; q != saidProgress && q > 0 && q < 4)
        {
            saidProgress = q;
            say ("Esporta " + juce::String (q * 25) + "%", false);
        }
    }
}

void RikyPanel::announceHelp()
{
    // in breve: l'elenco completo è nella guida, che si legge meglio in braille
    say (juce::CharPointer_UTF8 ("Spazio suona, R registra, S ferma, D demo, Esc panic, I stato, M metronomo, C conteggio, "
         "più e meno tempo, Ctrl E esporta MP3. Nel campo Comando scrivi aiuto, oppure guida per la guida completa."));
}

// Campo Comando: si scrive una parola con la tastiera (anche braille) e si preme Invio
void RikyPanel::runCommand (const juce::String& typed)
{
    auto words = juce::StringArray::fromTokens (typed.toLowerCase().trim(), " ", "");
    words.removeEmptyStrings();
    if (words.isEmpty()) return;
    commandReply = {};   // "stato" deve leggere lo stato vero, non la risposta precedente
    const auto w = words[0];
    const auto arg = words[1];
    auto setParam = [this] (const char* pid, float realValue)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.state.getParameter (pid))) p->setValueNotifyingHost (p->convertTo0to1 (realValue));
    };
    auto is = [&] (std::initializer_list<const char*> names) { for (auto* n : names) if (w == n) return true; return false; };
    juce::String reply;
    using S = Sequencer::State;
    if (is ({ "suona", "play", "p", "via" }))
    {
        if (proc.sequencer.isEmpty()) reply = "Nessun brano: scrivi demo";
        else { if (proc.sequencer.state() != S::stopped) proc.stopSong(); proc.sequencer.play(); reply = "Suona: " + songName (proc.sequencer.name()); }
    }
    else if (is ({ "ferma", "stop", "s", "basta" })) { proc.stopSong(); reply = "Fermo"; }
    else if (is ({ "registra", "rec", "r" }))
    {
        if (proc.sequencer.state() == S::recording || proc.sequencer.state() == S::countIn) { proc.stopSong(); reply = "Fine registrazione"; }
        else { proc.toggleRecord(); reply = "Registra"; }
    }
    else if (is ({ "demo", "d" })) { proc.loadDemo(); proc.sequencer.play(); reply = "Suona: Riky's Theme (demo)"; }
    else if (is ({ "esporta", "export", "e" }))
    {
        quickExport (arg == "wav" ? Exporter::Format::wav24 : arg == "v0" ? Exporter::Format::mp3_v0 : Exporter::Format::mp3_320);
        reply = exportPending ? juce::String ("Esporta ") + (arg == "wav" ? "WAV" : "MP3") : juce::String ("Export non partito");
    }
    else if (is ({ "salva", "save" })) { saveMidi(); reply = "Salva: finestra del sistema"; }
    else if (is ({ "apri", "open", "carica" })) { loadMidi(); reply = "Apri: finestra del sistema"; }
    else if (is ({ "stato", "info", "i" })) reply = statusText (true) + " " + songText();
    else if (is ({ "aiuto", "help", "?", "h" }))
        reply = juce::CharPointer_UTF8 ("Comandi: suona, ferma, registra, demo, esporta, esporta wav, salva, apri, stato, tempo 90, metronomo, "
                                        "conteggio 0 1 2, volume -6, trasponi 2, accorda 10, diapason 432, panic, installa, audio, guida");
    else if (is ({ "guida" })) { openGuide(); reply = "Guida aperta nell'editor di testo"; }
    else if (is ({ "tempo", "bpm", "t" }))
    {
        if (arg.containsOnly ("0123456789")) { setParam (id::tempo, juce::jlimit (40.f, 240.f, (float) arg.getIntValue())); }
        reply = "Tempo " + juce::String (juce::roundToInt (proc.state.getRawParameterValue (id::tempo)->load()));
    }
    else if (is ({ "metronomo", "metro", "m" }))
    {
        const bool on = arg == "si" || arg == "sì" || arg == "on" ? true : arg == "no" || arg == "off" ? false
                        : proc.state.getRawParameterValue (id::metro)->load() < .5f;
        setParam (id::metro, on ? 1.f : 0.f);
        reply = on ? "Metronomo acceso" : "Metronomo spento";
    }
    else if (is ({ "conteggio", "count", "c" }))
    {
        const int n = arg.isNotEmpty() ? juce::jlimit (0, 2, arg.getIntValue()) : ((int) proc.state.getRawParameterValue (id::countIn)->load() + 1) % 3;
        setParam (id::countIn, (float) n);
        reply = "Conteggio: " + italianChoices (id::countIn)[n];
    }
    else if (is ({ "volume", "vol" }))
    {
        if (arg.isNotEmpty()) setParam (id::volume, juce::jlimit (-60.f, 12.f, arg.replaceCharacter (',', '.').getFloatValue()));
        reply = "Volume " + accessibleValue (id::volume, proc.state.getRawParameterValue (id::volume)->load());
    }
    else if (is ({ "trasponi", "trasposizione" }))
    {
        if (arg.isNotEmpty()) setParam (id::transpose, juce::jlimit (-12.f, 12.f, (float) arg.getIntValue()));
        reply = "Trasposizione " + accessibleValue (id::transpose, proc.state.getRawParameterValue (id::transpose)->load());
    }
    else if (is ({ "accorda", "intonazione" }))
    {
        if (arg.isNotEmpty()) setParam (id::tune, juce::jlimit (-50.f, 50.f, arg.replaceCharacter (',', '.').getFloatValue()));
        reply = "Accordatura " + accessibleValue (id::tune, proc.state.getRawParameterValue (id::tune)->load());
    }
    else if (is ({ "diapason", "la" }))
    {
        if (arg == "432" || arg == "440") setParam (id::a4ref, arg == "432" ? 1.f : 0.f);
        reply = "Diapason " + italianChoices (id::a4ref)[(int) proc.state.getRawParameterValue (id::a4ref)->load()];
    }
    else if (is ({ "panic", "panico" })) { panicBtn.triggerClick(); reply = "Panic: note spente"; }
    else if (is ({ "installa", "install" })) { askInstall(); reply = "Installa: finestra di conferma"; }
    else if (is ({ "libreria" })) { showLibraryMenu(); reply = "Libreria: menu aperto"; }
    else if (is ({ "audio", "midi", "scheda" }))
    {
        if (proc.showAudioSettings) { proc.showAudioSettings(); reply = "Audio e MIDI: finestra aperta"; }
        else reply = "Audio e MIDI: si impostano nel programma musicale";
    }
    else reply = "Sconosciuto: " + w + ". Scrivi aiuto";

    commandReply = reply;
    commandReplyMs = juce::Time::getMillisecondCounter();
    seqMessage = reply.toUpperCase();
    songView.setText (songText());
    say (reply);
}

void RikyPanel::openGuide()
{
    const auto f = SampleLibrary::baseFolder().getChildFile ("Guida Riky Piano Man.txt");
    SampleLibrary::baseFolder().createDirectory();
    f.replaceWithData (BinaryData::GUIDARIKY_txt, (size_t) BinaryData::GUIDARIKY_txtSize);
    f.startAsProcess();
}

void RikyPanel::quickExport (Exporter::Format fmt)
{
    if (proc.exporter.busy()) { say ("Esportazione già in corso."); return; }
    if (proc.sequencer.isEmpty()) { say ("Non c'è un brano da esportare."); return; }
    if (proc.engine.status() != Status::ready) { say ("Il pianoforte non è pronto."); return; }
    const auto folder = SampleLibrary::baseFolder().getChildFile ("Exports");
    folder.createDirectory();
    const auto name = juce::File::createLegalFileName (songName (proc.sequencer.name()) + " " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M"));
    const auto f = folder.getNonexistentChildFile (name, Exporter::extension (fmt));
    lastExport = juce::File();
    exportPending = proc.exportSong (f, fmt);
    saidProgress = 0;
    say (exportPending ? juce::String (fmt == Exporter::Format::wav24 ? "Esporta WAV" : "Esporta MP3") : juce::String ("Export non partito"));
}

void RikyPanel::learnFocused (bool forget)
{
    auto* focused = juce::Component::getCurrentlyFocusedComponent();
    const auto pid = mbstudio::midi::taggedParam (focused);
    if (pid.isEmpty()) { say ("Seleziona prima con Tab un controllo, poi premi Ctrl M."); return; }
    const auto title = focused->getTitle().isNotEmpty() ? focused->getTitle() : proc.midiMap.paramName (pid);
    if (forget) { proc.midiMap.forget (pid); say ("Assegnazione tolta: " + title + "."); return; }
    proc.midiMap.startLearn (pid);
    saidMidiStatus = proc.midiMap.status();   // il messaggio di attesa lo diciamo noi, in italiano completo
    say ("Muovi un controllo o premi un tasto del controller MIDI per assegnarlo a " + title + ". Esc annulla.");
}

bool RikyPanel::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    const bool cmd = mods.isCommandDown();
    const bool shift = mods.isShiftDown();
    const int code = k.getKeyCode();
    if (const auto log = juce::SystemStats::getEnvironmentVariable ("RIKY_A11Y_LOG", {}); log.isNotEmpty())
        juce::File (log).appendText ("[tasto " + k.getTextDescription() + "]\n");
    auto letter = [&] (char c) { return ! cmd && ! mods.isAltDown() && (code == c || code == c + 32); };
    auto param = [this] (const char* pid) { return proc.state.getParameter (pid); };

    if (code == juce::KeyPress::F1Key || letter ('H')) { announceHelp(); return true; }
    if (code == juce::KeyPress::F2Key || letter ('I')) { say (statusText (true) + " " + songText()); return true; }
    if (code == juce::KeyPress::F5Key || (code == juce::KeyPress::spaceKey && ! cmd)) { proc.togglePlay(); seqMessage = {}; return true; }
    if (code == juce::KeyPress::F6Key || letter ('R')) { proc.toggleRecord(); seqMessage = {}; return true; }
    if (letter ('S')) { proc.stopSong(); return true; }
    if (letter ('D')) { proc.loadDemo(); proc.sequencer.play(); seqMessage = {}; return true; }
    if (code == juce::KeyPress::escapeKey)
    {
        if (proc.midiMap.learningParam().isNotEmpty()) { proc.midiMap.cancelLearn(); say ("Assegnazione annullata."); return true; }
        panicBtn.triggerClick();
        return true;
    }
    if (letter ('M'))
    {
        auto* p = param (id::metro);
        p->setValueNotifyingHost (p->getValue() > .5f ? 0.f : 1.f);
        say (p->getValue() > .5f ? "Metronomo acceso." : "Metronomo spento.");
        return true;
    }
    if (letter ('C'))
    {
        auto* p = param (id::countIn);
        const int next = ((int) proc.state.getRawParameterValue (id::countIn)->load() + 1) % 3;
        p->setValueNotifyingHost (p->convertTo0to1 ((float) next));
        say ("Conteggio: " + italianChoices (id::countIn)[next] + ".");
        return true;
    }
    const auto ch = k.getTextCharacter();
    const bool tempoKey = ! cmd && (ch == '+' || ch == '=' || ch == '-' || code == juce::KeyPress::pageUpKey || code == juce::KeyPress::pageDownKey);
    if (tempoKey)
    {
        // + e - di un battito, Pagina su e Pagina giù di dieci (il + sulle tastiere italiane non ha Maiuscole)
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (param (id::tempo));
        const bool page = code == juce::KeyPress::pageUpKey || code == juce::KeyPress::pageDownKey;
        const float step = page ? 10.f : 1.f;
        const bool down = ch == '-' || code == juce::KeyPress::pageDownKey;
        const float now = proc.state.getRawParameterValue (id::tempo)->load();
        const float v = juce::jlimit (40.f, 240.f, now + (down ? -step : step));
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        say ("Tempo " + juce::String (juce::roundToInt (v)) + ".", false);
        return true;
    }
    if (cmd && (code == 'O' || code == 'o')) { loadMidi(); return true; }
    if (cmd && (code == 'S' || code == 's')) { saveMidi(); return true; }
    if (cmd && (code == 'E' || code == 'e')) { quickExport (shift ? Exporter::Format::wav24 : Exporter::Format::mp3_320); return true; }
    if (cmd && (code == 'L' || code == 'l')) { showLibraryMenu(); return true; }
    if (cmd && (code == 'M' || code == 'm')) { learnFocused (shift); return true; }
    if (cmd && ch == ',' && proc.showAudioSettings) { proc.showAudioSettings(); return true; }
    return false;
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
        {
            if (! safeThis->proc.sequencer.loadMidiFile (f))
            {
                safeThis->seqMessage = "NOT A READABLE MIDI FILE: " + f.getFileName().toUpperCase();
                safeThis->say ("File MIDI non leggibile: " + f.getFileName());
            }
            else safeThis->seqMessage = {};
        }
        safeThis->repaint();
    });
}

void RikyPanel::saveMidi()
{
    if (proc.sequencer.isEmpty()) { say ("Non c'è un brano da salvare."); return; }
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
        const bool ok = safeThis->proc.sequencer.saveMidiFile (f);
        safeThis->seqMessage = ok ? "SAVED " + f.getFileName().toUpperCase() : "CANNOT SAVE " + f.getFileName().toUpperCase();
        safeThis->say (ok ? "Salvato " + f.getFileName() + "." : "Salvataggio non riuscito.");
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
    // LED fissi (niente lampeggio) e pulsanti sempre attivi: un pulsante che si spegne perde il fuoco del lettore di schermo
    const bool rec = st == S::recording || st == S::countIn;
    if (recBtn.getToggleState() != rec) recBtn.setToggleState (rec, juce::dontSendNotification);
    if (playBtn.getToggleState() != (st == S::playing)) playBtn.setToggleState (st == S::playing, juce::dontSendNotification);
    if (exportBtn.getToggleState() != exporting)
    {
        exportBtn.setToggleState (exporting, juce::dontSendNotification);
        exportBtn.setButtonText (exporting ? "CANCEL EXPORT" : "EXPORT MP3 / WAV");
        exportBtn.setTitle (exporting ? "Annulla l'esportazione" : "Esporta il brano in MP3 o WAV");
    }
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
    auto now = proc.engine.status() == Status::ready ? proc.engine.info() : nullptr;
    if (now == info && info != nullptr) return;   // stesso strumento: niente da rifare (il fuoco del lettore di schermo resta dov'è)
    info = now;
    // SFZ CONTROLS: nome dal file, posti vuoti spenti
    for (auto& c : ctls)
        if (c->slot >= 0)
        {
            const bool on = info != nullptr && c->slot < (int) info->controls.size();
            c->label = on ? info->controls[(size_t) c->slot].label.toUpperCase() : juce::String ("-");
            c->slider.setEnabled (on);
            c->slider.setAlpha (on ? 1.f : .3f);
            c->slider.setTooltip (on ? info->controls[(size_t) c->slot].label + " (CC " + juce::String (info->controls[(size_t) c->slot].cc) + ")" : juce::String());
            // posti vuoti nascosti al lettore di schermo; gli altri con il nome scritto nel file
            c->slider.setAccessible (on);
            if (on)
            {
                const auto& ctl = info->controls[(size_t) c->slot];
                c->slider.setTitle (ctl.label);
                c->slider.setHelpText ("Controllo dello strumento, CC " + juce::String (ctl.cc) + ". Valore da 0 a 127.");
                c->slider.setTooltip (c->slider.getHelpText());
            }
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
        const juce::String name = it != info->switchLabels.end() ? it->second : juce::String (noteName (key)) + juce::String (key / 12 - 1);
        auto* b = switchBtns.add (new juce::TextButton (isMore ? "MORE (" + juce::String (keys.size() - (maxButtons - 1)) + ")" : name.toUpperCase()));
        b->getProperties().set ("led", ! isMore);
        b->getProperties().set ("ledColour", "ffffb020");
        b->setTitle (isMore ? "Altre articolazioni, " + juce::String (keys.size() - (maxButtons - 1)) : "Articolazione " + name);
        b->setHelpText (isMore ? juce::String ("Apre l'elenco di tutte le articolazioni.") : "Keyswitch sul tasto " + italianNote (key) + ".");
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
        gArt.addAndMakeVisible (b);
        place (*b, { 778 + (i % cols) * (bw + 8), cellB + 8 + (i / cols) * (bh + 12), bw, bh });
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
void RikyPanel::place (juce::Component& c, juce::Rectangle<int> r)
{
    if (auto* parent = c.getParentComponent(); parent != nullptr && parent != this) r = r.translated (-parent->getX(), -parent->getY());
    c.setBounds (r);
}

void RikyPanel::resized()
{
    displayArea = { 420, 14, 560, 90 };
    lcdArea = { margin, chipY, lcdW, lcdH };
    seqArea = { 556, seqY - 2, 470, 32 };

    // gruppi (coordinate del pannello): prima i gruppi, poi i controlli dentro
    gStatus.setBounds (420, 8, 812, 100);
    gMidi.setBounds (990, 54, 240, 38);
    gSeq.setBounds (margin, chipC, baseW - 2 * margin, lineC - chipC - 4);
    gPiano.setBounds (margin, chipY, 220, 160);
    gSfz.setBounds (246, chipY, 512, 160);
    gRoom.setBounds (778, chipY, 192, 160);
    gOut.setBounds (992, chipY, 234, 160);
    gPedal.setBounds (margin, chipB, 220, 112);
    gEngine.setBounds (246, chipB, 250, 112);
    gTuning.setBounds (516, chipB, 242, 112);
    gArt.setBounds (778, chipB, 448, 112);

    place (statusView, displayArea);
    place (installBtn, { 994, 16, 112, 30 });
    place (libraryBtn, { 1112, 16, 114, 30 });
    place (midiButtons, { 994, 58, 166, 30 });
    place (panicBtn, { 1166, 58, 60, 30 });

    for (auto& c : ctls)
    {
        switch (c->kind)
        {
            case Kind::knob:    place (c->slider, { c->cell.getX(), c->cell.getY() + 14, knobW, 54 }); break;
            case Kind::fader:   place (c->slider, { c->cell.getX(), c->cell.getY() - 2, c->cell.getWidth(), 112 }); break;
            case Kind::hslider: place (c->slider, c->cell.withTrimmedLeft (62).withTrimmedRight (74)); break;
            case Kind::combo:   place (c->combo, c->cell); break;
            case Kind::toggle:  place (c->button, c->cell); break;
        }
    }
    meter.setBounds (1070, row2Y + 14, 144, 8);
    place (demoBtn, { margin, seqY, 84, 28 });
    place (loadBtn, { margin + 92, seqY, 76, 28 });
    place (saveBtn, { margin + 174, seqY, 76, 28 });
    place (recBtn, { 282, seqY, 84, 28 });
    place (playBtn, { 372, seqY, 84, 28 });
    place (stopBtn, { 462, seqY, 76, 28 });
    place (songView, seqArea);
    place (exportBtn, { 1040, seqY, 186, 28 });
    place (commandBox, { 892, seqY2, 206, 26 });
    place (helpBtn, { 1106, seqY2, 120, 26 });
    layoutSwitches();
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
        else if (c->kind == Kind::combo && c->group == &gSeq)
        {
            ui::paintPrinted (g, { cell.getX() - 96.f, cell.getY(), 90.f, cell.getHeight() }, c->label, 12.f, juce::Justification::centredRight);
        }
        else if (c->kind == Kind::combo)
        {
            ui::paintPrinted (g, { cell.getX(), cell.getY() - 18.f, cell.getWidth() + 20.f, 14.f }, c->label, 12.f);
        }
        else if (c->kind == Kind::hslider)
        {
            ui::paintPrinted (g, cell.withWidth (58.f), c->label, 12.f, juce::Justification::centredLeft);
            ui::paintPrinted (g, cell.withLeft (cell.getRight() - 70.f), formatValue (c->pid, (float) c->slider.getValue()), 12.f, juce::Justification::centredRight);
        }
        else if (c->kind == Kind::fader)
        {
            ui::paintPrinted (g, { cell.getX() - 10.f, cell.getY() + 112.f, cell.getWidth() + 20.f, 14.f }, c->label, 12.f, juce::Justification::centred);
            ui::paintPrinted (g, { cell.getX() - 10.f, cell.getY() + 126.f, cell.getWidth() + 20.f, 14.f },
                              formatValue (c->pid, (float) c->slider.getValue()), 11.f, juce::Justification::centred, C::printDim);
        }
    }
    ui::paintPrinted (g, { 1070.f, (float) row2Y - 4.f, 144.f, 14.f }, "OUT", 12.f);
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
    // testi letti dal lettore di schermo e annunci dei cambiamenti (4 volte al secondo bastano)
    if (++a11yTicks % 8 == 0)
    {
        if (juce::Component::getCurrentlyFocusedComponent() == nullptr && hasKeyboardFocus (true) == false
            && getPeer() != nullptr && getPeer()->isFocused())
            focusStart();   // finestra attiva ma nessun controllo con il fuoco: si riparte dal display
        statusView.setText (statusText (false));
        songView.setText (songText());
        announceChanges();
    }
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
