#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

using namespace riky;

RikyProcessor::RikyProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "RIKYPIANOMAN", createLayout()),
      midiMap (*this, "Riky Piano Man")
{
    auto raw = [this] (const char* pid) { return state.getRawParameterValue (pid); };
    tone = raw (id::tone); tune = raw (id::tune); transpose = raw (id::transpose); a4ref = raw (id::a4ref); velCurve = raw (id::velCurve);
    quality = raw (id::quality); poly = raw (id::poly); temper = raw (id::temper); temperKey = raw (id::temperKey);
    for (int i = 0; i < numSlots; ++i) slots[(size_t) i] = raw (id::sfzSlot (i));
    pedal = raw (id::pedal); soft = raw (id::soft); softAmt = raw (id::softAmt);
    roomOn = raw (id::roomOn); roomSize = raw (id::roomSize); roomDamp = raw (id::roomDamp); roomMix = raw (id::roomMix);
    width = raw (id::width); volume = raw (id::volume);

    // banchi per i profili dei controller: A suono, B strumento, C pedali e stanza, D uscita
    // banchi per i profili dei controller: A gli 8 SFZ CONTROLS, B suono, C pedali e stanza, D motore e accordatura
    juce::StringArray bankA;
    for (int i = 0; i < numSlots; ++i) bankA.add (id::sfzSlot (i));
    midiMap.setBanks ({ bankA,
                        juce::StringArray { id::tone, id::tune, id::transpose, id::velCurve, id::roomMix, id::width, id::volume, id::a4ref },
                        juce::StringArray { id::pedal, id::soft, id::softAmt, id::roomOn, id::roomSize, id::roomDamp, id::roomMix, id::volume },
                        juce::StringArray { id::quality, id::poly, id::temper, id::temperKey, id::tune, id::a4ref, id::transpose, id::volume } });

    engine.setPolyphony (polyphony ((int) poly->load()));
    engine.setTemperament ((int) temper->load());
    engine.onStatusChanged = [this] { engineChanged(); };
    library.onProgress = [this] { notify(); };
    library.onInstalled = [this] (const juce::File& f) { loadPiano (f); };
    sequencer.onChange = [this] { notify(); };
    exporter.onFinished = [this] { notify(); };

    if (const auto f = SampleLibrary::locate(); f.existsAsFile()) engine.loadAsync (f);
    loadDemo();   // nel sequencer c'è già la demo, pronta per PLAY
    startTimerHz (20);
}

void RikyProcessor::engineChanged()
{
    // file nuovo (non quello del progetto salvato): gli SFZ CONTROLS partono dai valori scritti nel file
    if (engine.status() == PianoEngine::Status::ready)
        if (const auto inf = engine.info(); inf != nullptr && inf->file.getFullPathName() != slotsFile)
            resetSfzControls();
    notify();
}

void RikyProcessor::resetSfzControls()
{
    const auto inf = engine.info();
    if (inf == nullptr) return;
    for (int i = 0; i < (int) inf->controls.size(); ++i)
        if (auto* p = state.getParameter (id::sfzSlot (i)))
            p->setValueNotifyingHost (inf->controls[(size_t) i].defaultValue);
    slotsFile = inf->file.getFullPathName();
}

void RikyProcessor::timerCallback()
{
    // impostazioni che sfizz accetta solo dal thread di controllo
    engine.setPolyphony (polyphony ((int) poly->load()));
    engine.setTemperament ((int) temper->load());
    sequencer.update();
    // CC arrivati dal MIDI per gli SFZ CONTROLS: il pannello e l'automazione li seguono
    for (int i = 0; i < numSlots; ++i)
        if (const float v = engine.takeIncomingSlot (i); v >= 0.f)
            if (auto* p = state.getParameter (id::sfzSlot (i))) p->setValueNotifyingHost (v);
}

RikyProcessor::~RikyProcessor()
{
    stopTimer();
    engine.onStatusChanged = nullptr;
    sequencer.onChange = nullptr;
    exporter.onFinished = nullptr;
    library.onProgress = nullptr;
    library.onInstalled = nullptr;
}

void RikyProcessor::notify()
{
    if (onStatusChanged) onStatusChanged();
}

bool RikyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

riky::EngineSettings RikyProcessor::engineSettings() const
{
    EngineSettings s;
    s.a4 = a4Frequency ((int) a4ref->load());
    s.cents = tune->load();
    s.transpose = juce::roundToInt (transpose->load());
    s.velCurve = (int) velCurve->load();
    s.pedalButton = pedal->load() > .5f;
    s.softButton = soft->load() > .5f;
    s.softAmount = softAmt->load();
    for (int i = 0; i < numSlots; ++i) s.slots[(size_t) i] = slots[(size_t) i]->load();
    s.quality = sampleQuality ((int) quality->load());
    s.temperKey = (int) temperKey->load();
    return s;
}

riky::PostSettings RikyProcessor::postSettings() const
{
    PostSettings p;
    p.tone = tone->load();
    p.width = width->load();
    p.roomOn = roomOn->load() > .5f;
    p.roomSize = roomSize->load();
    p.roomDamp = roomDamp->load();
    p.roomMix = roomMix->load();
    p.volumeDb = volume->load();
    return p;
}

void RikyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int block = juce::jmax (samplesPerBlock, 32);
    engine.prepare (sampleRate, block);
    post.prepare (sampleRate, block, postSettings());
    keyboardState.reset();
}

void RikyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    // il sequencer suona prima della tastiera del pannello, così i tasti si muovono anche durante la demo
    sequencer.processPlayback (midi, n, sr);
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    midiMap.process (midi);

    auto s = engineSettings();
    sequencer.processRecording (midi, n, sr, s.pedalButton);
    s.offline = isNonRealtime();
    if (auto* ph = getPlayHead())
        if (const auto pos = ph->getPosition())
        {
            s.hasTransport = true;
            s.playing = pos->getIsPlaying();
            if (const auto b = pos->getBpm()) s.bpm = *b;
            if (const auto p = pos->getPpqPosition()) s.ppq = *p;
            if (const auto t = pos->getTimeSignature()) { s.sigNum = t->numerator; s.sigDen = t->denominator; }
        }
    engine.render (buffer, midi, s);
    post.process (buffer, postSettings());
    level.store (juce::jmax (post.peak(), level.load() * .8f));
}

// ------------------------------------------------------------------------------------------------ sequencer ed export
void RikyProcessor::loadDemo()
{
    juce::MemoryInputStream in (BinaryData::demorikytheme_mid, (size_t) BinaryData::demorikytheme_midSize, false);
    std::vector<SeqEvent> ev;
    if (Sequencer::readMidi (in, ev)) sequencer.setEvents (std::move (ev), "RIKY'S THEME (DEMO)");
}

bool RikyProcessor::exportSong (const juce::File& dest, Exporter::Format format)
{
    if (sequencer.isEmpty() || exporter.busy()) return false;
    const auto inf = engine.info();
    if (inf == nullptr) return false;
    Exporter::Job j;
    j.events = sequencer.events();
    j.sfz = inf->file;
    j.dest = dest;
    j.title = sequencer.name().isNotEmpty() ? sequencer.name() : juce::String ("Riky Piano Man");
    j.engine = engineSettings();
    j.engine.pedalButton = false;      // il pedale è già nel brano
    j.post = postSettings();
    j.temperament = (int) temper->load();
    j.polyphony = polyphony ((int) poly->load());
    j.format = format;
    return exporter.start (std::move (j));
}

// ------------------------------------------------------------------------------------------------ pianoforte
void RikyProcessor::loadPiano (const juce::File& sfz)
{
    if (! sfz.existsAsFile()) return;
    SampleLibrary::saveSfz (sfz);
    SampleLibrary::addRecent (sfz);
    engine.loadAsync (sfz);
}

// ------------------------------------------------------------------------------------------------ stato
void RikyProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = state.copyState();
    st.setProperty ("sfz", engine.currentFile().getFullPathName(), nullptr);
    st.setProperty ("slotsFile", slotsFile, nullptr);
    // il brano del sequencer resta nel progetto
    if (! sequencer.isEmpty())
    {
        st.setProperty ("song", sequencer.toMidiData().toBase64Encoding(), nullptr);
        st.setProperty ("songName", sequencer.name(), nullptr);
    }
    st.removeChild (st.getChildWithName ("MIDIMAP"), nullptr);
    st.appendChild (midiMap.toValueTree(), nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void RikyProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (! xml || ! xml->hasTagName (state.state.getType())) return;
    auto st = juce::ValueTree::fromXml (*xml);
    if (auto mm = st.getChildWithName ("MIDIMAP"); mm.isValid())
    {
        midiMap.fromValueTree (mm);
        st.removeChild (mm, nullptr);
    }
    const auto path = st.getProperty ("sfz", {}).toString();
    slotsFile = st.getProperty ("slotsFile", {}).toString();
    if (const auto song = st.getProperty ("song", {}).toString(); song.isNotEmpty())
    {
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (song))
        {
            juce::MemoryInputStream in (mb, false);
            std::vector<SeqEvent> ev;
            if (Sequencer::readMidi (in, ev)) sequencer.setEvents (std::move (ev), st.getProperty ("songName", "SONG").toString());
        }
    }
    st.removeProperty ("song", nullptr);
    state.replaceState (st);
    // il progetto può venire da un altro computer: se il percorso salvato non esiste si usa quello di questo computer
    if (path.isNotEmpty() && juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile() && juce::File (path) != engine.currentFile())
        engine.loadAsync (juce::File (path));
    notify();
}

juce::AudioProcessorEditor* RikyProcessor::createEditor() { return new RikyEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RikyProcessor(); }
