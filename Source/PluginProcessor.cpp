#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>
#include "JobBuilder.h"

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
    tempo = raw (id::tempo); metro = raw (id::metro); countIn = raw (id::countIn); cuesOn = raw (id::cues);
    for (auto* t : { id::seqPlay, id::seqStop, id::seqRec }) state.addParameterListener (t, this);
    pedal = raw (id::pedal); soft = raw (id::soft); softAmt = raw (id::softAmt);
    roomOn = raw (id::roomOn); roomSize = raw (id::roomSize); roomDamp = raw (id::roomDamp); roomMix = raw (id::roomMix);
    width = raw (id::width); volume = raw (id::volume);
    for (auto* pid : soundParams()) { soundP[pid] = raw (pid); state.addParameterListener (pid, this); }

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
    layer.onStatusChanged = [this] { notify(); };
    library.onProgress = [this] { notify(); };
    library.onInstalled = [this] (const juce::File& f) { loadPiano (f); };
    sequencer.onChange = [this] { notify(); };
    exporter.onFinished = [this] { cues.post (exporter.result().ok ? Cues::done : Cues::error); notify(); };

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

void RikyProcessor::updateLayer()
{
    const bool on = pv (id::l2On) > .5f;
    if (! on) return;                                // resta caricato: riaccenderlo è immediato
    const int sound = (int) pv (id::l2Sound);
    const auto mainFile = engine.currentFile();
    if (sound == layerLoaded && (sound != 0 || mainFile.getFullPathName() == layerFor)) return;
    if (engine.status() == PianoEngine::Status::loading) return;   // il pianoforte con feltro aspetta il file principale
    layerLoaded = sound;
    if (sound == 0)
    {
        if (! mainFile.existsAsFile()) { layerLoaded = -2; return; }
        layerFor = mainFile.getFullPathName();
        layer.loadAsync (mainFile);
    }
    else
        layer.loadTextAsync (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Riky Layer " + layerSoundNames()[sound] + ".sfz"),
                             layerSfzText (sound));
}

bool RikyProcessor::selectPreset (int index)
{
    if (index < 0 || index >= presets::count()) return false;
    applyingPreset.store (true);
    const bool ok = presets::apply (state, index);
    applyingPreset.store (false);
    if (ok) { currentPreset.store (index); edited.store (false); updateLayer(); updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withProgramChanged (true)); notify(); }
    return ok;
}

juce::File RikyProcessor::saveUserPreset (const juce::String& name)
{
    const auto f = presets::saveUser (state, name);
    if (f.existsAsFile())
    {
        const auto files = presets::userFiles();
        currentPreset.store ((int) presets::factory().size() + files.indexOf (f));
        edited.store (false);
        updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withProgramChanged (true));
        notify();
    }
    return f;
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

void RikyProcessor::parameterChanged (const juce::String& pid, float v)
{
    if (soundParamIds().contains (pid))
    {
        if (! applyingPreset.load()) edited.store (true);
        return;
    }
    // tasti del controller assegnati con MIDI LEARN: il comando parte quando il tasto va giù
    if (v < .5f) return;
    if (pid == id::seqPlay) transportRequests.fetch_or (1);
    else if (pid == id::seqStop) transportRequests.fetch_or (2);
    else if (pid == id::seqRec) transportRequests.fetch_or (4);
}

double RikyProcessor::countInSeconds() const
{
    const int bars = (int) countIn->load();
    return bars * 4.0 * 60.0 / juce::jlimit (40.0, 240.0, (double) tempo->load());
}

void RikyProcessor::togglePlay()
{
    using S = Sequencer::State;
    if (sequencer.state() == S::stopped) sequencer.play();
    else sequencer.stop();
}

void RikyProcessor::stopSong() { sequencer.stop(); }

void RikyProcessor::toggleRecord()
{
    using S = Sequencer::State;
    if (sequencer.state() == S::recording || sequencer.state() == S::countIn) { sequencer.stop(); return; }
    const bool aligned = metro->load() > .5f || (int) countIn->load() > 0;
    sequencer.record (tempo->load(), aligned);
}

void RikyProcessor::timerCallback()
{
    // comandi arrivati dal MIDI
    if (const int r = transportRequests.exchange (0); r != 0)
    {
        if (r & 2) stopSong();
        else if (r & 4) toggleRecord();
        else if (r & 1) togglePlay();
        for (auto* t : { id::seqPlay, id::seqStop, id::seqRec })
            if (auto* p = state.getParameter (t); p != nullptr && p->getValue() > .5f) p->setValueNotifyingHost (0.f);
    }
    // program change dal controller: sceglie il preset (1 = Maestro, 2 = Nashville C3...)
    if (const int pc = programRequest.exchange (-1); pc >= 0 && pc < getNumPrograms()) selectPreset (pc);
    // impostazioni che sfizz accetta solo dal thread di controllo
    engine.setPolyphony (polyphony ((int) poly->load()));
    engine.setTemperament ((int) temper->load());
    layer.setPolyphony (polyphony ((int) poly->load()));
    layer.setTemperament ((int) temper->load());
    sequencer.update();
    updateLayer();
    // CC arrivati dal MIDI per gli SFZ CONTROLS: il pannello e l'automazione li seguono
    for (int i = 0; i < numSlots; ++i)
        if (const float v = engine.takeIncomingSlot (i); v >= 0.f)
            if (auto* p = state.getParameter (id::sfzSlot (i))) p->setValueNotifyingHost (v);
}

RikyProcessor::~RikyProcessor()
{
    stopTimer();
    for (auto* t : { id::seqPlay, id::seqStop, id::seqRec }) state.removeParameterListener (t, this);
    for (auto& pid : soundParamIds()) state.removeParameterListener (pid, this);
    layer.onStatusChanged = nullptr;
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
    if (pv (id::editOn) > .5f) { s.felt = pv (id::felt); s.attack = pv (id::attack); }
    return s;
}

riky::EngineSettings RikyProcessor::layerSettings (const riky::EngineSettings& main) const
{
    auto s = main;
    s.transpose += 12 * juce::roundToInt (pv (id::l2Oct));
    const bool feltPiano = layerLoaded == 0;
    s.felt = feltPiano ? layerFeltAmount : 0.f;
    s.attack = feltPiano ? layerFeltAttack : 0.f;
    s.slotsFromFile = true;   // il secondo strato usa i valori scritti nel suo file (volume, pan...)
    return s;
}

riky::VoiceSettings RikyProcessor::voiceSettings (bool hostPlaying, double bpm, double ppq) const
{
    VoiceSettings v;
    v.layerOn = pv (id::l2On) > .5f && layer.status() == PianoEngine::Status::ready;
    v.zone = (int) pv (id::l2Zone);
    v.split = juce::roundToInt (pv (id::split));
    v.arpOn = pv (id::arpOn) > .5f;
    v.arpMode = (int) pv (id::arpMode);
    v.arpRate = (int) pv (id::arpRate);
    v.arpOctaves = juce::roundToInt (pv (id::arpOct));
    v.arpGate = pv (id::arpGate);
    v.arpTarget = (int) pv (id::arpTarget);
    v.hostPlaying = hostPlaying;
    v.bpm = hostPlaying && bpm > 0 ? bpm : (double) tempo->load();
    v.ppq = ppq;
    return v;
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
    auto& e = p.edit;
    e.on = pv (id::editOn) > .5f;
    e.hpf = pv (id::hpf); e.lpf = pv (id::lpf);
    e.eqLow = pv (id::eqLow); e.eqLMid = pv (id::eqLMid); e.eqLMidF = pv (id::eqLMidF);
    e.eqHMid = pv (id::eqHMid); e.eqHMidF = pv (id::eqHMidF); e.eqHigh = pv (id::eqHigh);
    e.compOn = pv (id::compOn) > .5f;
    e.compThr = pv (id::compThr); e.compRatio = pv (id::compRatio); e.compAtt = pv (id::compAtt); e.compRel = pv (id::compRel);
    e.tape = pv (id::tape); e.noise = pv (id::noise); e.trimDb = pv (id::trim);
    return p;
}

void RikyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int block = juce::jmax (samplesPerBlock, 32);
    engine.prepare (sampleRate, block);
    layer.prepare (sampleRate, block);
    layerBuf.setSize (2, block);
    mainMidi.ensureSize (4096);
    layerMidi.ensureSize (4096);
    voicing.prepare (sampleRate);
    layerGain.reset (sampleRate, .05);
    post.prepare (sampleRate, block, postSettings());
    cues.prepare (sampleRate);
    keyboardState.reset();
}

void RikyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    // il sequencer suona prima della tastiera del pannello, così i tasti si muovono anche durante la demo
    // MIDI Machine Control dal controller: F0 7F <id> 06 <comando> F7 (01 stop, 02/03 play, 06 record)
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isProgramChange()) { programRequest.store (m.getProgramChangeNumber()); continue; }
        if (! m.isSysEx() || m.getSysExDataSize() < 4) continue;
        const auto* d = m.getSysExData();
        if (d[0] == 0x7F && d[2] == 0x06)
        {
            if (d[3] == 0x01) transportRequests.fetch_or (2);
            else if (d[3] == 0x02 || d[3] == 0x03) transportRequests.fetch_or (1);
            else if (d[3] == 0x06) transportRequests.fetch_or (4);
        }
    }

    using S = Sequencer::State;
    const auto state0 = sequencer.state();
    const double pos0 = sequencer.position();
    sequencer.processPlayback (midi, n, sr, countInSeconds());
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    midiMap.process (midi);

    auto s = engineSettings();
    sequencer.processRecording (midi, n, sr, s.pedalButton);

    // metronomo e conteggio: un click per battito, accentato sul primo della battuta (4/4)
    const auto state1 = sequencer.state();
    const bool metroOn = metro->load() > .5f;
    if (state1 == S::countIn || (state1 == S::recording && (metroOn || state0 == S::countIn)))
    {
        const double pos1 = sequencer.position();
        const double tStart = (state0 == S::countIn || state0 == S::recording) ? pos0 : pos1 - n / sr;
        cues.scheduleBeats (tStart, pos1, tempo->load(), metroOn, n);
    }
    // inizio e fine della registrazione (con il conteggio l'inizio lo segnano già i click)
    if (state1 == S::recording && state0 == S::stopped) cues.post (Cues::recStart);
    if ((state0 == S::recording || state0 == S::countIn) && state1 == S::stopped) cues.post (Cues::recStop);
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
    // smistamento tra i due strati e arpeggiatore
    mainMidi.clear();
    layerMidi.clear();
    const auto vs = voiceSettings (s.hasTransport && s.playing, s.bpm, s.ppq);
    voicing.process (midi, n, vs, mainMidi, layerMidi);
    engine.render (buffer, mainMidi, s);
    const float lg = vs.layerOn && pv (id::l2Vol) > -39.9f ? juce::Decibels::decibelsToGain (pv (id::l2Vol)) : 0.f;
    layerGain.setTargetValue (lg);
    if (vs.layerOn || layerGain.isSmoothing() || ! layerMidi.isEmpty())
    {
        if (layerBuf.getNumSamples() < n) layerBuf.setSize (2, n, false, false, true);
        juce::AudioBuffer<float> lb (layerBuf.getArrayOfWritePointers(), juce::jmin (2, buffer.getNumChannels()), n);
        layer.render (lb, layerMidi, layerSettings (s));
        for (int i = 0; i < n; ++i)
        {
            const float g = layerGain.getNextValue();
            for (int c = 0; c < lb.getNumChannels(); ++c) buffer.addSample (c, i, lb.getSample (c, i) * g);
        }
    }
    post.process (buffer, postSettings());
    level.store (juce::jmax (post.peak(), level.load() * .8f));
    cues.render (buffer, cuesOn->load() > .5f);   // dopo la catena: non finisce negli export
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
    auto j = buildJob (state, inf->file);
    j.events = sequencer.events();
    j.dest = dest;
    j.title = sequencer.name().isNotEmpty() ? sequencer.name() : juce::String ("Riky Piano Man");
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
    st.setProperty ("preset", currentPreset.load(), nullptr);
    st.setProperty ("presetEdited", edited.load(), nullptr);
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
    applyingPreset.store (true);
    state.replaceState (st);
    // replaceState salta un parametro se il valore salvato è uguale a quello attuale arrotondato: un interruttore
    // lasciato dall'host a 0,4 (che vale "spento") resterebbe a 0,4. Qui ogni parametro torna esattamente come salvato.
    for (auto* ap : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
        {
            const auto child = st.getChildWithProperty ("id", rp->getParameterID());
            if (! child.isValid() || ! child.hasProperty ("value")) continue;
            const float target = rp->convertTo0to1 ((float) child.getProperty ("value"));
            if (std::abs (rp->getValue() - target) > 1.0e-6f) rp->setValueNotifyingHost (target);
        }
    applyingPreset.store (false);
    currentPreset.store ((int) st.getProperty ("preset", 0));
    edited.store ((bool) st.getProperty ("presetEdited", false));
    // il progetto può venire da un altro computer: se il percorso salvato non esiste si usa quello di questo computer
    if (path.isNotEmpty() && juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile() && juce::File (path) != engine.currentFile())
        engine.loadAsync (juce::File (path));
    notify();
}

juce::AudioProcessorEditor* RikyProcessor::createEditor() { return new RikyEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RikyProcessor(); }
