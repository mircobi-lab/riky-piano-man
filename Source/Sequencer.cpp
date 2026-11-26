#include "Sequencer.h"

namespace riky
{
namespace
{
    bool keep (const juce::MidiMessage& m)
    {
        return m.isNoteOnOrOff() || m.isController() || m.isPitchWheel() || m.isChannelPressure() || m.isAftertouch();
    }
    void sortEvents (std::vector<SeqEvent>& ev)
    {
        // a parità di tempo: prima le note spente, poi i controlli, poi le note accese
        auto rank = [] (const juce::MidiMessage& m) { return m.isNoteOff() ? 0 : m.isNoteOn() ? 2 : 1; };
        std::stable_sort (ev.begin(), ev.end(), [&] (const SeqEvent& a, const SeqEvent& b)
        {
            return a.time != b.time ? a.time < b.time : rank (a.msg) < rank (b.msg);
        });
    }
}

// ------------------------------------------------------------------------------------------------ brano
void Sequencer::setEvents (std::vector<SeqEvent> ev, const juce::String& n)
{
    stop();
    takeBpm = 120.0;
    for (auto& e : ev) e.msg.setChannel (1);
    sortEvents (ev);
    take = std::move (ev);
    takeName = n;
    {
        const juce::SpinLock::ScopedLockType sl (playLock);
        playData = take;
    }
    if (onChange) onChange();
}

double Sequencer::length() const noexcept { return take.empty() ? 0.0 : take.back().time; }

int Sequencer::numNotes() const noexcept
{
    int n = 0;
    for (auto& e : take) if (e.msg.isNoteOn()) ++n;
    return n;
}

bool Sequencer::readMidi (juce::InputStream& in, std::vector<SeqEvent>& out)
{
    juce::MidiFile mf;
    if (! mf.readFrom (in)) return false;
    mf.convertTimestampTicksToSeconds();
    out.clear();
    for (int t = 0; t < mf.getNumTracks(); ++t)
        for (auto* h : *mf.getTrack (t))
            if (keep (h->message))
                out.push_back ({ h->message.getTimeStamp(), h->message });
    sortEvents (out);
    return true;
}

bool Sequencer::loadMidiFile (const juce::File& f)
{
    juce::FileInputStream in (f);
    std::vector<SeqEvent> ev;
    if (! in.openedOk() || ! readMidi (in, ev) || ev.empty()) return false;
    setEvents (std::move (ev), f.getFileNameWithoutExtension());
    return true;
}

juce::MidiFile Sequencer::toMidiFile (const std::vector<SeqEvent>& ev, const juce::String& name, double bpm)
{
    // 960 tick per semiminima al tempo indicato (120 BPM se il brano non ha un tempo suo)
    bpm = juce::jlimit (20.0, 300.0, bpm);
    const double ticksPerSecond = 960.0 * bpm / 60.0;
    juce::MidiMessageSequence seq;
    auto tn = juce::MidiMessage::textMetaEvent (3, name.isNotEmpty() ? name : juce::String ("Riky Piano Man"));
    tn.setTimeStamp (0);
    seq.addEvent (tn);
    auto tempo = juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / bpm));
    tempo.setTimeStamp (0);
    seq.addEvent (tempo);
    for (auto& e : ev)
    {
        auto m = e.msg;
        m.setTimeStamp (std::round (e.time * ticksPerSecond));
        seq.addEvent (m);
    }
    seq.updateMatchedPairs();
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (960);
    mf.addTrack (seq);
    return mf;
}

bool Sequencer::saveMidiFile (const juce::File& f) const
{
    f.deleteFile();
    juce::FileOutputStream out (f);
    return out.openedOk() && toMidiFile (take, takeName, takeBpm).writeTo (out, 1) && (out.flush(), out.getStatus().wasOk());
}

juce::MemoryBlock Sequencer::toMidiData() const
{
    juce::MemoryOutputStream out;
    if (! take.empty()) toMidiFile (take, takeName, takeBpm).writeTo (out, 1);
    return out.getMemoryBlock();
}

// ------------------------------------------------------------------------------------------------ comandi
void Sequencer::play()
{
    if (take.empty()) return;
    {
        const juce::SpinLock::ScopedLockType sl (playLock);
        playData = take;
    }
    request.store (reqPlay);
}

void Sequencer::stop() { request.store (reqStop); }

void Sequencer::record (double bpm, bool aligned)
{
    recBpm = bpm;
    recAligned = aligned;
    recording.clear();
    { const auto discard = fifo.read (fifo.getNumReady()); }   // svuota la coda
    request.store (reqRecord);
}

void Sequencer::update()
{
    drain();
    if (recordingFinished.exchange (false)) finishRecording();
}

void Sequencer::drain()
{
    const auto scope = fifo.read (fifo.getNumReady());
    auto take1 = [this] (int start, int size)
    {
        for (int i = 0; i < size; ++i)
        {
            const auto& r = fifoData[(size_t) (start + i)];
            recording.push_back ({ r.time, juce::MidiMessage (r.data, r.size, r.time) });
        }
    };
    take1 (scope.startIndex1, scope.blockSize1);
    take1 (scope.startIndex2, scope.blockSize2);
}

void Sequencer::finishRecording()
{
    drain();
    auto ev = std::move (recording);
    recording.clear();
    bool anyNote = false;
    for (auto& e : ev) anyNote |= e.msg.isNoteOn();
    if (! anyNote) { if (onChange) onChange(); return; }   // nessuna nota: resta il brano di prima

    // silenzio iniziale: senza metronomo la ripresa parte 0,2 s prima della prima nota (i pedali già premuti
    // restano all'inizio); con metronomo o conteggio resta allineata alle battute
    double first = 0;
    for (auto& e : ev) if (e.msg.isNoteOn()) { first = e.time; break; }
    const double shift = recAligned ? 0.0 : juce::jmax (0.0, first - .2);
    for (auto& e : ev) e.time = juce::jmax (0.0, e.time - shift);

    // note ancora tenute e pedale giù alla fine: si chiudono
    std::bitset<128> on;
    bool sus = false;
    double end = 0;
    for (auto& e : ev)
    {
        end = juce::jmax (end, e.time);
        if (e.msg.isNoteOn()) on.set ((size_t) e.msg.getNoteNumber());
        else if (e.msg.isNoteOff()) on.reset ((size_t) e.msg.getNoteNumber());
        else if (e.msg.isSustainPedalOn()) sus = true;
        else if (e.msg.isSustainPedalOff()) sus = false;
    }
    end += .05;
    for (int k = 0; k < 128; ++k) if (on[(size_t) k]) ev.push_back ({ end, juce::MidiMessage::noteOff (1, k) });
    if (sus) ev.push_back ({ end, juce::MidiMessage::controllerEvent (1, 64, 0) });
    setEvents (std::move (ev), "TAKE " + juce::String (++takeCounter));
    takeBpm = recAligned ? recBpm : 120.0;
}

// ------------------------------------------------------------------------------------------------ thread audio
void Sequencer::startState (State s) noexcept
{
    stateValue.store ((int) s);
    pos = 0;
    cursor = 0;
    positionSeconds.store (0.0);
}

void Sequencer::silence (juce::MidiBuffer& midi) noexcept
{
    for (int k = 0; k < 128; ++k)
        if (sounding[(size_t) k]) midi.addEvent (juce::MidiMessage::noteOff (1, k), 0);
    if (sustainDown) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    sounding.reset();
    sustainDown = false;
}

void Sequencer::processPlayback (juce::MidiBuffer& midi, int n, double sr, double countInSeconds) noexcept
{
    switch (request.exchange (none))
    {
        case reqPlay:
            if (state() == State::recording) recordingFinished.store (true);
            silence (midi);
            startState (State::playing);
            break;
        case reqRecord:
            silence (midi);
            startState (State::recording);
            pedalKnown = false;
            if (countInSeconds > 0.0)
            {
                countLeft = (juce::int64) std::llround (countInSeconds * sr);
                stateValue.store ((int) State::countIn);
                positionSeconds.store (-countInSeconds);
            }
            break;
        case reqStop:
            if (state() == State::recording) recordingFinished.store (true);
            if (state() == State::playing) silence (midi);
            stateValue.store ((int) State::stopped);
            break;
        default: break;
    }
    if (state() == State::countIn)
    {
        // conteggio: la registrazione parte esattamente alla fine dell'ultima battuta contata
        countLeft -= n;
        positionSeconds.store (-(double) countLeft / sr);
        if (countLeft <= 0)
        {
            // il blocco attraversa la fine del conteggio: la registrazione di questo blocco parte da quel punto
            pos = -(countLeft + n);
            stateValue.store ((int) State::recording);
        }
        return;
    }
    if (state() != State::playing) return;

    const juce::SpinLock::ScopedTryLockType tl (playLock);
    if (! tl.isLocked()) return;   // il brano viene sostituito proprio ora: si riprende al blocco dopo
    const auto end = pos + n;
    for (; cursor < playData.size(); ++cursor)
    {
        const auto& e = playData[cursor];
        const auto at = (juce::int64) std::llround (e.time * sr);
        if (at >= end) break;
        midi.addEvent (e.msg, (int) juce::jlimit<juce::int64> (0, n - 1, at - pos));
        if (e.msg.isNoteOn()) sounding.set ((size_t) e.msg.getNoteNumber());
        else if (e.msg.isNoteOff()) sounding.reset ((size_t) e.msg.getNoteNumber());
        else if (e.msg.isSustainPedalOn()) sustainDown = true;
        else if (e.msg.isSustainPedalOff()) sustainDown = false;
    }
    pos = end;
    positionSeconds.store ((double) pos / sr);
    if (cursor >= playData.size())   // fine del brano
    {
        silence (midi);
        stateValue.store ((int) State::stopped);
    }
}

void Sequencer::push (double time, const juce::MidiMessage& m) noexcept
{
    if (m.getRawDataSize() > 3) return;
    const auto scope = fifo.write (1);
    if (scope.blockSize1 + scope.blockSize2 == 0) return;   // coda piena: si perde l'evento
    auto& r = fifoData[(size_t) (scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2)];
    r.time = time;
    r.size = m.getRawDataSize();
    std::memcpy (r.data, m.getRawData(), (size_t) r.size);
}

void Sequencer::processRecording (const juce::MidiBuffer& midi, int n, double sr, bool pedalButton) noexcept
{
    if (state() != State::recording) return;
    // il pulsante SUSTAIN del pannello viene registrato come CC 64
    if (! pedalKnown || pedalButton != lastPedalButton)
    {
        if (pedalKnown || pedalButton)
            push (juce::jmax (0.0, (double) pos / sr), juce::MidiMessage::controllerEvent (1, 64, pedalButton ? 127 : 0));
        lastPedalButton = pedalButton;
        pedalKnown = true;
    }
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (keep (m)) push (juce::jmax (0.0, (double) (pos + meta.samplePosition) / sr), m);   // suonato un attimo prima del via: va sul via
    }
    pos += n;
    positionSeconds.store ((double) pos / sr);
}
}
