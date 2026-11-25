#include "PianoEngine.h"
#include "Params.h"
#include <sfizz.hpp>
#include <juce_events/juce_events.h>

namespace riky
{
namespace
{
    constexpr int maxVoices = 128;
}

PianoEngine::PianoEngine()
{
    playedAs.fill (-1);
}

PianoEngine::~PianoEngine()
{
    joinLoader();
    alive->store (false);
}

void PianoEngine::joinLoader()
{
    if (loader.joinable()) loader.join();
}

float PianoEngine::shapeVelocity (float v, int curve, bool soft, float softAmount) noexcept
{
    switch (curve)
    {
        case 1: v = std::pow (v, .6f); break;     // SOFT: più forte con poca forza
        case 2: v = std::pow (v, 1.6f); break;    // HARD: serve più forza
        case 3: v = .8f; break;                   // FIXED
        default: break;
    }
    if (soft) v *= 1.f - .5f * softAmount;        // una corda: dinamica più bassa
    return juce::jlimit (1.f / 127.f, 1.f, v);
}

// ------------------------------------------------------------------------------------------------ thread dei messaggi
void PianoEngine::prepare (double sampleRate, int maxBlock)
{
    maxBlock = juce::jmax (32, maxBlock);
    preparedRate.store (sampleRate);
    preparedBlock.store (maxBlock);
    scratchL.assign ((size_t) maxBlock, 0.f);
    scratchR.assign ((size_t) maxBlock, 0.f);
    const juce::SpinLock::ScopedLockType sl (swapLock);
    if (synth != nullptr)
    {
        synth->setSampleRate ((float) sampleRate);
        synth->setSamplesPerBlock (maxBlock);
    }
    resetAudioState();
}

void PianoEngine::setStatus (Status s, const juce::String& err)
{
    {
        const juce::ScopedLock sl (textLock);
        error = err;
    }
    statusValue.store ((int) s);
    juce::MessageManager::callAsync ([this, flag = alive]
    {
        if (flag->load() && onStatusChanged) onStatusChanged();   // il motore può essere già stato distrutto
    });
}

juce::String PianoEngine::errorText() const { const juce::ScopedLock sl (textLock); return error; }
juce::File PianoEngine::currentFile() const { const juce::ScopedLock sl (textLock); return file; }

void PianoEngine::loadAsync (const juce::File& sfz)
{
    joinLoader();
    const int gen = ++generation;
    {
        const juce::ScopedLock sl (textLock);
        file = sfz;
    }
    setStatus (Status::loading);
    loader = std::thread ([this, sfz, gen]
    {
        juce::Thread::setCurrentThreadName ("Riky Piano Man loader");
        auto s = std::make_unique<sfz::Sfizz>();
        const double rate = preparedRate.load();
        s->setSampleRate ((float) rate);
        s->setSamplesPerBlock (preparedBlock.load());
        s->setNumVoices (maxVoices);
        juce::String err;
        if (! sfz.existsAsFile())
            err = "FILE NOT FOUND";
        else if (! s->loadSfzFile (sfz.getFullPathName().toStdString()))
            err = "CANNOT READ THE SFZ FILE";
        else if (s->getNumRegions() == 0)
            err = "NO REGIONS IN THE SFZ FILE";
        finishLoad (err.isEmpty() ? std::move (s) : nullptr, sfz, err, gen, rate);
    });
}

void PianoEngine::finishLoad (std::unique_ptr<sfz::Sfizz> fresh, const juce::File& f, const juce::String& err, int gen, double loadedRate)
{
    if (gen != generation.load()) return;   // nel frattempo è stato chiesto un altro file
    if (fresh == nullptr)
    {
        setStatus (Status::error, err);
        return;
    }
    const int n = fresh->getNumRegions();
    {
        const juce::SpinLock::ScopedLockType sl (swapLock);
        // la frequenza può essere cambiata durante il caricamento
        if (! juce::approximatelyEqual (preparedRate.load(), loadedRate))
            fresh->setSampleRate ((float) preparedRate.load());
        fresh->setSamplesPerBlock (preparedBlock.load());
        std::swap (synth, fresh);
        freshSynth.store (true);
    }
    fresh.reset();   // la vecchia istanza si chiude qui, fuori dal thread audio
    regions.store (n);
    {
        const juce::ScopedLock sl (textLock);
        file = f;
    }
    setStatus (Status::ready);
}

void PianoEngine::unload()
{
    joinLoader();
    ++generation;
    std::unique_ptr<sfz::Sfizz> old;
    {
        const juce::SpinLock::ScopedLockType sl (swapLock);
        std::swap (synth, old);
    }
    old.reset();
    regions.store (0);
    {
        const juce::ScopedLock sl (textLock);
        file = juce::File();
    }
    setStatus (Status::empty);
}

// ------------------------------------------------------------------------------------------------ thread audio
void PianoEngine::resetAudioState() noexcept
{
    playedAs.fill (-1);
    cc64 = cc67 = 0;
    sentSustain = sentLid = -1;
    sentDyn = sentRel = sentKey = sentPed = sentTuning = -1.f;
    freewheel = false;
}

void PianoEngine::render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const EngineSettings& s) noexcept
{
    buffer.clear();
    const juce::SpinLock::ScopedTryLockType tl (swapLock);
    if (! tl.isLocked() || synth == nullptr)
    {
        voices.store (0);
        return;
    }
    auto& sy = *synth;
    if (freshSynth.exchange (false)) resetAudioState();

    if (panicRequest.exchange (false))
    {
        // pedali su prima di spegnere: così anche il rumore di "pedale su" viene spento subito
        if (sentSustain != 0) sy.cc (0, maestroCC::sustain, 0);
        if (sentLid != 0) sy.cc (0, maestroCC::lid, 0);
        sy.allSoundOff();
        playedAs.fill (-1);
        cc64 = cc67 = 0;
        sentSustain = sentLid = 0;
    }
    if (s.offline != freewheel)
    {
        freewheel = s.offline;
        if (freewheel) sy.enableFreeWheeling(); else sy.disableFreeWheeling();
    }

    // parametri del pannello verso lo strumento, solo quando cambiano
    if (const float t = s.a4 * std::pow (2.f, s.cents / 1200.f); ! juce::approximatelyEqual (t, sentTuning)) { sy.setTuningFrequency (t); sentTuning = t; }
    if (! juce::approximatelyEqual (s.dynamics, sentDyn)) { sy.hdcc (0, maestroCC::veltrack, s.dynamics); sentDyn = s.dynamics; }
    if (! juce::approximatelyEqual (s.release, sentRel))  { sy.hdcc (0, maestroCC::release, s.release);   sentRel = s.release; }
    if (! juce::approximatelyEqual (s.keyNoise, sentKey)) { sy.hdcc (0, maestroCC::keyNoise, s.keyNoise); sentKey = s.keyNoise; }
    if (! juce::approximatelyEqual (s.pedNoise, sentPed)) { sy.hdcc (0, maestroCC::pedNoise, s.pedNoise); sentPed = s.pedNoise; }

    auto updatePedals = [&] (int delay)
    {
        const int sus = juce::jmax (cc64, s.pedalButton ? 127 : 0);
        if (sus != sentSustain) { sy.cc (delay, maestroCC::sustain, sus); sentSustain = sus; }
        const int lid = juce::jmax (cc67, s.softButton ? 127 : 0);
        if (lid != sentLid) { sy.cc (delay, maestroCC::lid, lid); sentLid = lid; }
    };
    updatePedals (0);

    const int total = buffer.getNumSamples();
    const int chans = buffer.getNumChannels();
    const int maxChunk = juce::jmax (1, juce::jmin (preparedBlock.load(), (int) scratchL.size()));
    auto it = midi.cbegin();
    const auto end = midi.cend();

    for (int start = 0; start < total;)
    {
        const int len = juce::jmin (maxChunk, total - start);
        for (; it != end && (*it).samplePosition < start + len; ++it)
        {
            const auto m = (*it).getMessage();
            const int delay = juce::jlimit (0, len - 1, (*it).samplePosition - start);
            if (m.isNoteOn())
            {
                const int key = m.getNoteNumber();
                const int n = key + s.transpose;
                if (n < 0 || n > 127) continue;
                if (playedAs[(size_t) key] >= 0) sy.noteOff (delay, playedAs[(size_t) key], 0);
                const bool soft = s.softButton || cc67 >= 64;
                const float v = shapeVelocity (m.getFloatVelocity(), s.velCurve, soft, s.softAmount);
                playedAs[(size_t) key] = n;
                sy.noteOn (delay, n, juce::jlimit (1, 127, juce::roundToInt (v * 127.f)));
                lastNoteValue.store (n);
            }
            else if (m.isNoteOff())
            {
                const int key = m.getNoteNumber();
                const int n = playedAs[(size_t) key];
                if (n >= 0) sy.noteOff (delay, n, m.getVelocity());
                playedAs[(size_t) key] = -1;
            }
            else if (m.isController())
            {
                const int c = m.getControllerNumber(), v = m.getControllerValue();
                if (c == maestroCC::sustain) { cc64 = v; updatePedals (delay); }
                else if (c == maestroCC::lid) { cc67 = v; updatePedals (delay); }
                else if (c == maestroCC::release || c == maestroCC::keyNoise || c == maestroCC::pedNoise || c == maestroCC::veltrack) {}   // decide il pannello
                else if (c == 120) { sy.allSoundOff(); playedAs.fill (-1); }   // i pedali restano come sono
                else if (c == 123) { for (auto& p : playedAs) if (p >= 0) { sy.noteOff (delay, p, 0); p = -1; } }
                else if (c == 121) { cc64 = cc67 = 0; updatePedals (delay); sy.cc (delay, c, v); }
                else sy.cc (delay, c, v);
            }
            else if (m.isPitchWheel())
                sy.pitchWheel (delay, m.getPitchWheelValue() - 8192);
        }

        float* outs[2];
        outs[0] = chans > 0 ? buffer.getWritePointer (0, start) : scratchL.data();
        outs[1] = chans > 1 ? buffer.getWritePointer (1, start) : scratchR.data();
        sy.renderBlock (outs, (size_t) len, 1);
        if (chans == 1)
            for (int i = 0; i < len; ++i) outs[0][i] = .5f * (outs[0][i] + outs[1][i]);
        start += len;
    }

    voices.store (sy.getNumActiveVoices());
    pedalState.store (sentSustain >= 64);
    softState.store (s.softButton || cc67 >= 64);
}
}
