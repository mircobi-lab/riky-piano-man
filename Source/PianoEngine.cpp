#include "PianoEngine.h"
#include <sfizz.hpp>
#include <sfizz_message.h>

namespace riky
{
// ------------------------------------------------------------------------------------------------ lettura delle informazioni
namespace
{
    // risposte di sfizz alle domande sul file caricato (protocollo a messaggi tipo OSC)
    struct Reply
    {
        char type = 0;
        int i = 0, i2 = 0;
        float f = 0.f;
        std::string s;
        std::vector<uint8_t> blob;

        static void receive (void* data, int, const char*, const char* sig, const sfizz_arg_t* args)
        {
            auto& r = *static_cast<Reply*> (data);
            r.type = sig != nullptr && sig[0] != 0 ? sig[0] : 'N';
            if (r.type == 'i') { r.i = args[0].i; if (sig[1] == 'i') r.i2 = args[1].i; }
            else if (r.type == 'f') r.f = args[0].f;
            else if (r.type == 's') r.s = args[0].s != nullptr ? args[0].s : "";
            else if (r.type == 'b' && args[0].b != nullptr) r.blob.assign (args[0].b->data, args[0].b->data + args[0].b->size);
        }
    };

    std::bitset<128> toBits (const std::vector<uint8_t>& b)
    {
        std::bitset<128> bits;
        for (size_t i = 0; i < 128 && i / 8 < b.size(); ++i)
            if (b[i / 8] & (1u << (i % 8))) bits.set (i);
        return bits;
    }
}

std::shared_ptr<InstrumentInfo> PianoEngine::readInfo (sfz::Sfizz& s, const juce::File& f)
{
    auto info = std::make_shared<InstrumentInfo>();
    info->file = f;
    info->name = f.getFileNameWithoutExtension();
    info->regions = s.getNumRegions();
    info->groups = s.getNumGroups();
    info->masters = s.getNumMasters();
    info->curves = s.getNumCurves();

    Reply r;
    auto client = sfz::Sfizz::createClient (&r);
    sfz::Sfizz::setReceiveCallback (*client, &Reply::receive);
    // questa istanza non è ancora usata dal thread audio: qui si può interrogare
    auto ask = [&] (const std::string& path) -> Reply&
    {
        r = Reply();
        s.sendMessage (*client, 0, path.c_str(), "", nullptr);
        return r;
    };

    if (auto& a = ask ("/num_samples"); a.type == 'i') info->samples = a.i;
    // tasti che suonano: le regioni che partono con una nota (le regioni attivate da un CC, come i rumori
    // dei pedali, coprono di solito tutta la tastiera e non contano)
    for (int i = 0; i < info->regions; ++i)
        if (auto& t = ask ("/region" + std::to_string (i) + "/trigger_on_note"); t.type == 'T')
            if (auto& k = ask ("/region" + std::to_string (i) + "/key_range"); k.type == 'i')
                for (int n = juce::jlimit (0, 127, k.i); n <= juce::jlimit (0, 127, k.i2); ++n) info->keys.set ((size_t) n);
    if (info->keys.none())
        if (auto& a = ask ("/key/slots"); a.type == 'b') info->keys = toBits (a.blob);
    if (auto& a = ask ("/sw/last/slots"); a.type == 'b') info->switches = toBits (a.blob);
    if (auto& a = ask ("/sw/last/current"); a.type == 'i') info->defaultSwitch = a.i;
    if (auto& a = ask ("/image"); a.type == 's') info->image = juce::String::fromUTF8 (a.s.c_str());
    for (int k = 0; k < 128; ++k)
    {
        if (info->switches[(size_t) k])
            if (auto& a = ask ("/sw/last/" + std::to_string (k) + "/label"); a.type == 's' && ! a.s.empty())
                info->switchLabels[k] = juce::String::fromUTF8 (a.s.c_str());
        if (auto& a = ask ("/key" + std::to_string (k) + "/label"); a.type == 's' && ! a.s.empty())
            info->keyLabels[k] = juce::String::fromUTF8 (a.s.c_str());
    }

    // SFZ CONTROLS: i CC con un nome (label_cc), tranne i pedali e i messaggi di modo canale
    std::vector<std::pair<int, juce::String>> ccs;
    for (const auto& [cc, label] : s.getCCLabels())
        if (cc < 120 && cc != pedalCC::sustain && cc != pedalCC::sostenuto && cc != pedalCC::soft)
            ccs.push_back ({ (int) cc, juce::String::fromUTF8 (label.c_str()).trim() });
    // nessun nome: i CC usati dal file
    if (ccs.empty())
        if (auto& a = ask ("/cc/slots"); a.type == 'b')
            for (int cc = 0; cc < 120; ++cc)
                if ((size_t) cc / 8 < a.blob.size() && (a.blob[(size_t) cc / 8] & (1u << (cc % 8)))
                    && cc != pedalCC::sustain && cc != pedalCC::sostenuto && cc != pedalCC::soft)
                    ccs.push_back ({ cc, "CC " + juce::String (cc) });
    std::sort (ccs.begin(), ccs.end(), [] (auto& a, auto& b) { return a.first < b.first; });
    for (auto& [cc, label] : ccs)
    {
        if ((int) info->controls.size() >= numSlots) break;
        InstrumentInfo::Control c;
        c.cc = cc;
        c.label = label.isNotEmpty() ? label : "CC " + juce::String (cc);
        if (auto& a = ask ("/cc" + std::to_string (cc) + "/default"); a.type == 'f') c.defaultValue = juce::jlimit (0.f, 1.f, a.f);
        info->controls.push_back (c);
    }
    return info;
}

void PianoEngine::applyTemperament (sfz::Sfizz& s, int index)
{
    auto scala = temperamentScala (index);
    if (scala.empty()) scala = "! equal.scl\nEqual temperament\n 12\n!\n 100.\n 200.\n 300.\n 400.\n 500.\n 600.\n 700.\n 800.\n 900.\n 1000.\n 1100.\n 2/1\n";
    s.loadScalaString (scala);
}

// ------------------------------------------------------------------------------------------------ ciclo di vita
PianoEngine::PianoEngine()
{
    playedAs.fill (-1);
    for (auto& c : slotCC) c.store (-1);
    for (auto& v : incoming) v.store (-1.f);
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

void PianoEngine::setPolyphony (int n)
{
    if (polyVoices.exchange (n) == n) return;
    const juce::SpinLock::ScopedLockType sl (swapLock);   // il thread audio salta questo blocco
    if (synth != nullptr) synth->setNumVoices (n);
}

void PianoEngine::setTemperament (int index)
{
    if (temperament.exchange (index) == index) return;
    const juce::SpinLock::ScopedLockType sl (swapLock);
    if (synth != nullptr) applyTemperament (*synth, index);
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
std::shared_ptr<const InstrumentInfo> PianoEngine::info() const { const juce::ScopedLock sl (textLock); return infoPtr; }

void PianoEngine::loadAsync (const juce::File& sfz) { startLoad (sfz, {}); }
void PianoEngine::loadTextAsync (const juce::File& virtualFile, const juce::String& text) { startLoad (virtualFile, text.isEmpty() ? juce::String (" ") : text); }

namespace
{
    // testo del file e di quelli inclusi, solo per controllare quali opcode usa lo strumento
    void gatherText (const juce::File& root, const juce::File& f, juce::String& all, int depth)
    {
        if (depth > 8 || ! f.existsAsFile()) return;
        const auto t = f.loadFileAsString();
        all << t << "\n";
        int at = 0;
        while ((at = t.indexOf (at, "#include")) >= 0)
        {
            const int q1 = t.indexOfChar (at, '"'), q2 = q1 >= 0 ? t.indexOfChar (q1 + 1, '"') : -1;
            if (q1 < 0 || q2 < 0) break;
            gatherText (root, root.getParentDirectory().getChildFile (t.substring (q1 + 1, q2).replaceCharacter ('\\', '/')), all, depth + 1);
            at = q2 + 1;
        }
    }
}

juce::String PianoEngine::injectEdit (const juce::String& text, const juce::File& sfz, bool& feltAdded)
{
    // Il feltro e l'attacco morbido entrano nello strumento come opcode della sezione <global>, comandati dai CC
    // interni 300 e 301: un filtro passa-basso per ogni nota che segue la dinamica (come un feltro tra martelletto
    // e corde) e un attacco più lento. Il file sul disco non viene toccato: sfizz legge il testo modificato.
    // Il filtro si aggiunge solo se lo strumento non ne usa uno suo (altrimenti verrebbe sostituito).
    juce::String all;
    gatherText (sfz, sfz, all, 0);
    if (all.isEmpty()) all = text;
    const auto lower = all.toLowerCase();
    feltAdded = ! lower.contains ("cutoff") && ! lower.contains ("fil_type") && ! lower.contains ("fil1_");
    juce::String ops = " ampeg_attack_oncc" + juce::String (attackCC) + "=0.25";
    if (feltAdded)
        ops << " fil_type=lpf_2p cutoff=20000 fil_veltrack=2400 cutoff_oncc" << feltCC << "=-8400";
    ops << " ";
    juce::String out;
    int at = 0, found = 0;
    for (int g; (g = text.indexOfIgnoreCase (at, "<global>")) >= 0; at = g + 8, ++found)
        out << text.substring (at, g + 8) << ops;
    out << text.substring (at);
    if (found == 0) out = "<global>" + ops + "\n" + text;
    return out;
}

void PianoEngine::startLoad (const juce::File& sfz, const juce::String& text)
{
    joinLoader();
    const int gen = ++generation;
    {
        const juce::ScopedLock sl (textLock);
        file = sfz;
    }
    setStatus (Status::loading);
    loader = std::thread ([this, sfz, text, gen]
    {
        juce::Thread::setCurrentThreadName ("Riky Piano Man loader");
        auto s = std::make_unique<sfz::Sfizz>();
        const double rate = preparedRate.load();
        s->setSampleRate ((float) rate);
        s->setSamplesPerBlock (preparedBlock.load());
        s->setNumVoices (polyVoices.load());
        juce::String err;
        std::shared_ptr<InstrumentInfo> inf;
        const bool builtIn = text.isNotEmpty();
        bool feltAdded = false;
        if (! builtIn && ! sfz.existsAsFile())
            err = "FILE NOT FOUND";
        else
        {
            const auto source = builtIn ? text : sfz.loadFileAsString();
            const auto edited = injectEdit (source, sfz, feltAdded);
            if (! s->loadSfzString (sfz.getFullPathName().toStdString(), edited.toStdString()))
                err = "CANNOT READ THE SFZ FILE";
            else if (s->getNumRegions() == 0)
                err = "NO REGIONS IN THE SFZ FILE";
            else
            {
                inf = readInfo (*s, sfz);
                inf->feltAvailable = feltAdded;
                inf->builtIn = builtIn;
            }
        }
        finishLoad (err.isEmpty() ? std::move (s) : nullptr, inf, sfz, err, gen, rate);
    });
}

void PianoEngine::finishLoad (std::unique_ptr<sfz::Sfizz> fresh, std::shared_ptr<InstrumentInfo> inf, const juce::File& f,
                              const juce::String& err, int gen, double loadedRate)
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
        // frequenza, polifonia e temperamento possono essere cambiati durante il caricamento
        if (! juce::approximatelyEqual (preparedRate.load(), loadedRate))
            fresh->setSampleRate ((float) preparedRate.load());
        fresh->setSamplesPerBlock (preparedBlock.load());
        if (fresh->getNumVoices() != polyVoices.load()) fresh->setNumVoices (polyVoices.load());
        applyTemperament (*fresh, temperament.load());   // la nuova istanza non suona ancora: si può
        std::swap (synth, fresh);
        for (int i = 0; i < numSlots; ++i)
            slotCC[(size_t) i].store (i < (int) inf->controls.size() ? inf->controls[(size_t) i].cc : -1);
        switchKeys = inf->switches;
        freshDefaultSwitch = inf->defaultSwitch;
        switchValue.store (inf->defaultSwitch);
        freshSynth.store (true);
    }
    fresh.reset();   // la vecchia istanza si chiude qui, fuori dal thread audio
    regions.store (n);
    {
        const juce::ScopedLock sl (textLock);
        file = f;
        infoPtr = inf;
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
        for (auto& c : slotCC) c.store (-1);
        switchKeys.reset();
    }
    old.reset();
    regions.store (0);
    {
        const juce::ScopedLock sl (textLock);
        file = juce::File();
        infoPtr.reset();
    }
    setStatus (Status::empty);
}

// ------------------------------------------------------------------------------------------------ thread audio
void PianoEngine::resetAudioState() noexcept
{
    playedAs.fill (-1);
    seenParam.fill (-1.f);
    cc64 = cc67 = 0;
    sentSustain = sentLid = sentQuality = sentRootKey = -1;
    sentTuning = sentFelt = sentAttack = -1.f;
    sentBpm = -1.0; sentNum = sentDen = -1;
    freewheel = false;
    wasPlaying = false;
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
    if (freshSynth.exchange (false))
    {
        resetAudioState();
        switchValue.store (freshDefaultSwitch);
    }

    if (panicRequest.exchange (false))
    {
        // pedali su prima di spegnere: così anche il rumore di "pedale su" viene spento subito
        if (sentSustain != 0) sy.cc (0, pedalCC::sustain, 0);
        if (sentLid != 0) sy.cc (0, pedalCC::soft, 0);
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
    if (s.quality != sentQuality) { sy.setSampleQuality (sfz::Sfizz::ProcessLive, s.quality); sentQuality = s.quality; }
    if (s.temperKey != sentRootKey) { sy.setScalaRootKey (60 + s.temperKey); sentRootKey = s.temperKey; }

    // trasporto dell'host
    if (s.hasTransport)
    {
        if (! juce::approximatelyEqual (s.bpm, sentBpm) && s.bpm > 0) { sy.bpmTempo (0, (float) s.bpm); sentBpm = s.bpm; }
        if (s.sigNum != sentNum || s.sigDen != sentDen) { sy.timeSignature (0, s.sigNum, s.sigDen); sentNum = s.sigNum; sentDen = s.sigDen; }
        if (s.playing)
        {
            const double beatsPerBar = s.sigNum * 4.0 / juce::jmax (1, s.sigDen);
            const int bar = (int) std::floor (s.ppq / beatsPerBar);
            sy.timePosition (0, bar, s.ppq - bar * beatsPerBar);
        }
        if (s.playing != wasPlaying) { sy.playbackState (0, s.playing ? 1 : 0); wasPlaying = s.playing; }
    }

    // parametri del pannello verso lo strumento, solo quando cambiano
    if (const float t = s.a4 * std::pow (2.f, s.cents / 1200.f); ! juce::approximatelyEqual (t, sentTuning)) { sy.setTuningFrequency (t); sentTuning = t; }
    if (! juce::approximatelyEqual (s.felt, sentFelt)) { sy.hdcc (0, feltCC, s.felt); sentFelt = s.felt; }
    if (! juce::approximatelyEqual (s.attack, sentAttack)) { sy.hdcc (0, attackCC, s.attack); sentAttack = s.attack; }
    for (int i = 0; i < numSlots && ! s.slotsFromFile; ++i)
        if (const int cc = slotCC[(size_t) i].load(); cc >= 0 && ! juce::approximatelyEqual (s.slots[(size_t) i], seenParam[(size_t) i]))
        {
            sy.hdcc (0, cc, s.slots[(size_t) i]);
            seenParam[(size_t) i] = s.slots[(size_t) i];
        }

    auto updatePedals = [&] (int delay)
    {
        const int sus = juce::jmax (cc64, s.pedalButton ? 127 : 0);
        if (sus != sentSustain) { sy.cc (delay, pedalCC::sustain, sus); sentSustain = sus; }
        const int lid = juce::jmax (cc67, s.softButton ? 127 : 0);
        if (lid != sentLid) { sy.cc (delay, pedalCC::soft, lid); sentLid = lid; }
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
                if (switchKeys[(size_t) key])
                {
                    // keyswitch: niente trasposizione né curva, cambia articolazione
                    if (playedAs[(size_t) key] >= 0) sy.noteOff (delay, playedAs[(size_t) key], 0);
                    playedAs[(size_t) key] = key;
                    sy.noteOn (delay, key, m.getVelocity());
                    switchValue.store (key);
                    continue;
                }
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
                if (c == pedalCC::sustain) { cc64 = v; updatePedals (delay); }
                else if (c == pedalCC::soft) { cc67 = v; updatePedals (delay); }
                else if (c == 120) { sy.allSoundOff(); playedAs.fill (-1); }   // i pedali restano come sono
                else if (c == 123) { for (auto& p : playedAs) if (p >= 0) { sy.noteOff (delay, p, 0); p = -1; } }
                else if (c == 121) { cc64 = cc67 = 0; updatePedals (delay); sy.cc (delay, c, v); }
                else
                {
                    sy.cc (delay, c, v);
                    for (int i = 0; i < numSlots; ++i)      // il controllo sul pannello segue il MIDI
                        if (slotCC[(size_t) i].load() == c) incoming[(size_t) i].store ((float) v / 127.f);
                }
            }
            else if (m.isPitchWheel())
                sy.pitchWheel (delay, m.getPitchWheelValue() - 8192);
            else if (m.isChannelPressure())
                sy.channelAftertouch (delay, m.getChannelPressureValue());
            else if (m.isAftertouch())
                sy.polyAftertouch (delay, m.getNoteNumber() + s.transpose, m.getAfterTouchValue());
            else if (m.isProgramChange())
                sy.programChange (delay, m.getProgramChangeNumber());
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
