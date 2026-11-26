#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    if (const auto f = SampleLibrary::locate(); f.existsAsFile()) engine.loadAsync (f);
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
    // CC arrivati dal MIDI per gli SFZ CONTROLS: il pannello e l'automazione li seguono
    for (int i = 0; i < numSlots; ++i)
        if (const float v = engine.takeIncomingSlot (i); v >= 0.f)
            if (auto* p = state.getParameter (id::sfzSlot (i))) p->setValueNotifyingHost (v);
}

RikyProcessor::~RikyProcessor()
{
    stopTimer();
    engine.onStatusChanged = nullptr;
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

void RikyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int block = juce::jmax (samplesPerBlock, 32);
    engine.prepare (sampleRate, block);
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) block, 1 };
    for (int c = 0; c < 2; ++c)
    {
        lowShelf[c].coefficients = new juce::dsp::IIR::Coefficients<float> (1, 0, 0, 1, 0, 0);
        highShelf[c].coefficients = new juce::dsp::IIR::Coefficients<float> (1, 0, 0, 1, 0, 0);
        lowShelf[c].prepare (spec);
        highShelf[c].prepare (spec);
    }
    appliedTone = 1.0e9f;
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    gain.reset (sampleRate, .05);
    wet.reset (sampleRate, .05);
    widthSm.reset (sampleRate, .05);
    gain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volume->load() + makeupDb, -100.f));
    wet.setCurrentAndTargetValue (roomOn->load() > .5f ? roomMix->load() : 0.f);
    widthSm.setCurrentAndTargetValue (width->load());
    dry.setSize (2, block);
    keyboardState.reset();
}

void RikyProcessor::updateTone (float t)
{
    // inclinazione: scaffale basso a 250 Hz e alto a 2,5 kHz in senso opposto, fino a 9 dB (come Campione)
    const float dbAmount = 9.f * t;
    const auto lo = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf (sr, 250.f, .707f, juce::Decibels::decibelsToGain (-dbAmount));
    const auto hi = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (sr, 2500.f, .707f, juce::Decibels::decibelsToGain (dbAmount));
    for (int c = 0; c < 2; ++c)
    {
        *lowShelf[c].coefficients = lo;
        *highShelf[c].coefficients = hi;
    }
    appliedTone = t;
}

void RikyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    midiMap.process (midi);

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

    const int chans = juce::jmin (2, buffer.getNumChannels());

    // tono
    if (const float t = tone->load(); ! juce::exactlyEqual (t, appliedTone)) updateTone (t);
    if (std::abs (appliedTone) > .001f)
        for (int c = 0; c < chans; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i) d[i] = highShelf[c].processSample (lowShelf[c].processSample (d[i]));
        }

    // larghezza stereo: mid/side, 100% = come registrato (coppia X/Y)
    widthSm.setTargetValue (width->load());
    if (chans == 2 && (widthSm.isSmoothing() || std::abs (widthSm.getTargetValue() - 1.f) > .001f))
    {
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float w = widthSm.getNextValue();
            const float m = .5f * (l[i] + r[i]), sd = .5f * (l[i] - r[i]) * w;
            l[i] = m + sd; r[i] = m - sd;
        }
    }

    // stanza: riverbero solo bagnato, miscelato con un livello che scorre senza scatti
    juce::Reverb::Parameters rp;
    rp.roomSize = .3f + .68f * roomSize->load();
    rp.damping = roomDamp->load();
    rp.wetLevel = 1.f; rp.dryLevel = 0.f; rp.width = 1.f; rp.freezeMode = 0.f;
    reverb.setParameters (rp);
    wet.setTargetValue (roomOn->load() > .5f ? roomMix->load() : 0.f);
    if (wet.isSmoothing() || wet.getTargetValue() > 0.f)
    {
        reverbCleared = false;
        for (int start = 0; start < n;)
        {
            const int len = juce::jmin (n - start, dry.getNumSamples());
            for (int c = 0; c < chans; ++c) dry.copyFrom (c, 0, buffer, c, start, len);
            if (chans == 2) reverb.processStereo (buffer.getWritePointer (0, start), buffer.getWritePointer (1, start), len);
            else reverb.processMono (buffer.getWritePointer (0, start), len);
            for (int i = 0; i < len; ++i)
            {
                const float w = wet.getNextValue(), d = 1.f - .5f * w;
                for (int c = 0; c < chans; ++c)
                {
                    auto* o = buffer.getWritePointer (c, start);
                    o[i] = d * dry.getSample (c, i) + w * o[i];
                }
            }
            start += len;
        }
    }
    else if (! reverbCleared)
    {
        reverb.reset();
        reverbCleared = true;
    }

    // volume e livello per il LED meter
    gain.setTargetValue (volume->load() <= -59.9f ? 0.f : juce::Decibels::decibelsToGain (volume->load() + makeupDb));
    float peak = 0.f;
    for (int i = 0; i < n; ++i)
    {
        const float g = gain.getNextValue();
        for (int c = 0; c < chans; ++c)
        {
            auto* o = buffer.getWritePointer (c);
            o[i] *= g;
            peak = juce::jmax (peak, std::abs (o[i]));
        }
    }
    for (int c = chans; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    level.store (juce::jmax (peak, level.load() * .8f));
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
    state.replaceState (st);
    // il progetto può venire da un altro computer: se il percorso salvato non esiste si usa quello di questo computer
    if (path.isNotEmpty() && juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile() && juce::File (path) != engine.currentFile())
        engine.loadAsync (juce::File (path));
    notify();
}

juce::AudioProcessorEditor* RikyProcessor::createEditor() { return new RikyEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RikyProcessor(); }
