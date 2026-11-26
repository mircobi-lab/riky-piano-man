// Prova del motore senza interfaccia: carica il Maestro Concert Grand con sfizz, suona e controlla il risultato.
// uso: riky_render "Maestro Concert Grand.sfz" uscita.wav [strumento-con-keyswitch.sfz] [cartella-export]
#include "PianoEngine.h"
#include "Sequencer.h"
#include "Exporter.h"
#include <BinaryData.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <cstdio>

using namespace riky;

namespace
{
constexpr double sr = 48000.0;
constexpr int block = 256;
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("%s  %s\n", ok ? "OK  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

struct Rig
{
    PianoEngine engine;
    EngineSettings s;
    juce::AudioBuffer<float> out { 2, 0 };

    // suona 'seconds' secondi con gli eventi indicati (tempo in secondi) e li aggiunge alla registrazione
    juce::AudioBuffer<float> play (double seconds, std::vector<std::pair<double, juce::MidiMessage>> events)
    {
        const int total = (int) (seconds * sr);
        juce::AudioBuffer<float> rec (2, total);
        juce::AudioBuffer<float> b (2, block);
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            b.setSize (2, n, false, false, true);
            juce::MidiBuffer m;
            for (auto& [t, msg] : events)
            {
                const int at = (int) (t * sr);
                if (at >= pos && at < pos + n) m.addEvent (msg, at - pos);
            }
            engine.render (b, m, s);
            for (int c = 0; c < 2; ++c) rec.copyFrom (c, pos, b, c, 0, n);
        }
        const int old = out.getNumSamples();
        out.setSize (2, old + total, true, true, false);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, old, rec, c, 0, total);
        return rec;
    }
};

float rms (const juce::AudioBuffer<float>& b, double from, double to)
{
    const int a = (int) (from * sr), z = juce::jmin (b.getNumSamples(), (int) (to * sr));
    double acc = 0;
    for (int c = 0; c < 2; ++c)
        for (int i = a; i < z; ++i) acc += (double) b.getSample (c, i) * b.getSample (c, i);
    return (float) std::sqrt (acc / juce::jmax (1, 2 * (z - a)));
}

bool finite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}

// frequenza del picco più forte tra lo e hi (DFT diretta, risoluzione 0,1 Hz)
double peakFreq (const juce::AudioBuffer<float>& b, double from, double len, double lo, double hi)
{
    const int a = (int) (from * sr), n = (int) (len * sr);
    double best = 0, bestF = 0;
    for (double f = lo; f <= hi; f += .1)
    {
        double re = 0, im = 0;
        for (int i = 0; i < n; ++i)
        {
            const double w = .5 - .5 * std::cos (2 * juce::MathConstants<double>::pi * i / (n - 1));
            const double x = w * (b.getSample (0, a + i) + b.getSample (1, a + i));
            const double ph = 2 * juce::MathConstants<double>::pi * f * i / sr;
            re += x * std::cos (ph); im -= x * std::sin (ph);
        }
        if (const double m = re * re + im * im; m > best) { best = m; bestF = f; }
    }
    return bestF;
}

double cents (double f, double ref) { return 1200.0 * std::log2 (f / ref); }
juce::MidiMessage on (int n, int v) { return juce::MidiMessage::noteOn (1, n, (juce::uint8) v); }
juce::MidiMessage off (int n) { return juce::MidiMessage::noteOff (1, n); }
juce::MidiMessage cc (int c, int v) { return juce::MidiMessage::controllerEvent (1, c, v); }
}

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("uso: riky_render file.sfz uscita.wav\n"); return 2; }
    juce::ScopedJuceInitialiser_GUI init;
    Rig r;
    r.s.offline = true;            // come un bounce: aspetta i campioni letti dal disco
    r.engine.prepare (sr, block);
    r.engine.loadAsync (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    for (int i = 0; i < 600 && r.engine.status() == PianoEngine::Status::loading; ++i) juce::Thread::sleep (50);
    check (r.engine.status() == PianoEngine::Status::ready, "caricamento SFZ (" + juce::String (r.engine.numRegions()) + " regioni) " + r.engine.errorText());
    if (r.engine.status() != PianoEngine::Status::ready) return 1;
    check (r.engine.numRegions() >= 800, "tutte le regioni del Maestro (attese 804)");

    // informazioni lette dal file: tasti, keyswitch, controlli con nome e valori iniziali
    const auto inf = r.engine.info();
    auto slotOf = [&] (int cc) { for (int i = 0; i < (int) inf->controls.size(); ++i) if (inf->controls[(size_t) i].cc == cc) return i; return -1; };
    {
        int lo = 128, hi = -1;
        for (int k = 0; k < 128; ++k) if (inf->keys[(size_t) k]) { lo = juce::jmin (lo, k); hi = juce::jmax (hi, k); }
        check (lo == 21 && hi == 108 && (int) inf->keys.count() == 88, "tasti che suonano: " + juce::String (lo) + ".." + juce::String (hi) + " (" + juce::String ((int) inf->keys.count()) + ")");
        check (inf->switches.none(), "nessun keyswitch nel Maestro");
        juce::String list;
        for (auto& c : inf->controls) list << c.cc << "=" << c.label << "(" << juce::String (c.defaultValue, 3) << ") ";
        std::printf ("      controlli: %s\n", list.toRawUTF8());
        const int rel = slotOf (72), vt = slotOf (99);
        check (rel >= 0 && std::abs (inf->controls[(size_t) rel].defaultValue - .575f) < .002f, "CC 72 Release con il valore del file (0,575)");
        check (vt >= 0 && inf->controls[(size_t) vt].label == "Veltrack", "CC 99 Veltrack");
        check (slotOf (64) < 0 && slotOf (67) < 0, "pedali fuori dagli SFZ CONTROLS");
        for (int i = 0; i < (int) inf->controls.size(); ++i) r.s.slots[(size_t) i] = inf->controls[(size_t) i].defaultValue;
    }

    // 1-2. intonazione: il motore deve riprodurre la registrazione com'è (il La4 del Maestro è registrato a circa 438 Hz),
    //      e diapason 432 e trasposizione devono spostarla della quantità giusta
    {
        auto b = r.play (2.0, { { .0, on (60, 90) }, { 1.5, off (60) } });
        const double f = peakFreq (b, .3, .5, 255, 268);
        check (std::abs (cents (f, 261.626)) < 6, "C4 a " + juce::String (f, 1) + " Hz (" + juce::String (cents (f, 261.626), 1) + " cent)");
        auto a = r.play (2.0, { { .0, on (69, 90) }, { 1.5, off (69) } });
        const double fa = peakFreq (a, .3, .5, 430, 450);
        check (std::abs (cents (fa, 440.0)) < 10, "A4 a " + juce::String (fa, 1) + " Hz (" + juce::String (cents (fa, 440.0), 1) + " cent, come il campione originale)");
        r.s.a4 = 432.f;
        auto b4 = r.play (2.0, { { .0, on (69, 90) }, { 1.5, off (69) } });
        const double f432 = peakFreq (b4, .3, .5, 422, 440);
        check (std::abs (cents (f432, fa) - cents (432.0, 440.0)) < 1, "diapason 432: " + juce::String (f432, 1) + " Hz (" + juce::String (cents (f432, fa), 1) + " cent, atteso -31,8)");
        r.s.a4 = 440.f;
        r.s.cents = 10.f;
        auto tu = r.play (2.0, { { .0, on (69, 90) }, { 1.5, off (69) } });
        r.s.cents = 0.f;
        const double ftu = peakFreq (tu, .3, .5, 430, 450);
        check (std::abs (cents (ftu, fa) - 10.0) < 1, "TUNE +10 cent: " + juce::String (cents (ftu, fa), 1) + " cent");
        r.s.transpose = 12;
        auto t = r.play (2.0, { { .0, on (57, 90) }, { 1.5, off (57) } });
        const double ft = peakFreq (t, .3, .5, 430, 450);
        check (std::abs (cents (ft, fa)) < 1, "A3 trasposto +12 suona come A4: " + juce::String (ft, 1) + " Hz");
        r.s.transpose = 0;
    }
    // 3. strati di velocity: piano e fortissimo
    {
        auto p = r.play (1.5, { { .0, on (60, 15) }, { 1.0, off (60) } });
        auto f = r.play (1.5, { { .0, on (60, 127) }, { 1.0, off (60) } });
        const float lp = rms (p, .0, .5), lf = rms (f, .0, .5);
        check (lf > lp * 3.f, "fortissimo piu' forte del piano: " + juce::String (juce::Decibels::gainToDecibels (lf / lp), 1) + " dB");
        check (juce::Decibels::gainToDecibels (f.getMagnitude (0, f.getNumSamples())) < -1.f, "nessuna saturazione a velocity 127");
    }
    // 4. pedale di risonanza: la nota resta dopo il rilascio del tasto
    {
        auto no = r.play (3.0, { { .0, on (48, 100) }, { .5, off (48) } });
        auto yes = r.play (4.0, { { .0, cc (64, 127) }, { .05, on (48, 100) }, { .55, off (48) }, { 3.0, cc (64, 0) } });
        const float lNo = rms (no, 1.5, 2.0), lYes = rms (yes, 1.55, 2.05);
        check (lYes > lNo * 4.f, "sustain CC64: dopo 1 s " + juce::String (juce::Decibels::gainToDecibels (lYes / juce::jmax (1e-9f, lNo)), 1) + " dB in piu'");
        // pedale dal pannello
        r.s.pedalButton = true;
        auto btn = r.play (2.5, { { .0, on (48, 100) }, { .5, off (48) } });
        r.s.pedalButton = false;
        r.play (2.0, {});
        check (rms (btn, 1.5, 2.0) > lNo * 4.f, "sustain dal pannello");
    }
    // 5. rumore di rilascio dei tasti (regioni trigger=release): con KEY NOISE a zero il rilascio è più pulito
    {
        const int rel = slotOf (72), key = slotOf (80);
        const float relDef = r.s.slots[(size_t) rel], keyDef = r.s.slots[(size_t) key];
        r.s.slots[(size_t) rel] = .1f;
        auto withNoise = r.play (2.0, { { .0, on (40, 100) }, { .6, off (40) } });
        r.s.slots[(size_t) key] = 0.f;
        auto without = r.play (2.0, { { .0, on (40, 100) }, { .6, off (40) } });
        r.s.slots[(size_t) key] = keyDef; r.s.slots[(size_t) rel] = relDef;
        const float a = rms (withNoise, .8, 1.4), b = rms (without, .8, 1.4);
        check (a > b * 2.f, "rumore di rilascio del tasto presente (" + juce::String (juce::Decibels::gainToDecibels (a / juce::jmax (1e-9f, b)), 1) + " dB)");
    }
    // 6. tutti gli 88 tasti suonano
    {
        int silent = 0;
        juce::String which;
        for (int n = 21; n <= 108; ++n)
        {
            auto b = r.play (.35, { { .0, on (n, 100) }, { .25, off (n) } });
            if (rms (b, .0, .25) < 1e-4f) { ++silent; which << n << " "; }
        }
        check (silent == 0, "88 tasti con suono" + (silent > 0 ? " - muti: " + which : juce::String()));
    }
    // 7. PANIC
    {
        r.s.pedalButton = false;
        std::vector<std::pair<double, juce::MidiMessage>> ev { { .0, cc (64, 127) } };
        for (int n : { 36, 48, 55, 60, 64, 67, 72 }) ev.push_back ({ .01, on (n, 110) });
        r.engine.requestPanic();   // viene eseguito all'inizio del blocco successivo
        r.play (.01, {});
        auto pre = r.play (1.0, ev);
        r.engine.requestPanic();
        auto post = r.play (.5, {});
        check (rms (pre, .5, 1.0) > 1e-3f && rms (post, .1, .5) < 1e-5f, "PANIC spegne tutto (prima " + juce::String (juce::Decibels::gainToDecibels (rms (pre, .5, 1.0)), 1) + " dB, dopo " + juce::String (juce::Decibels::gainToDecibels (rms (post, .1, .5)), 1) + " dB)");
        r.play (.2, { { .0, cc (64, 0) } });
    }
    // 8. trasposizione cambiata mentre il tasto è giù: nessuna nota bloccata
    {
        auto a = r.play (.5, { { .0, on (60, 100) } });
        r.s.transpose = 5;
        auto b = r.play (3.0, { { .0, off (60) } });
        r.s.transpose = 0;
        check (rms (b, 2.5, 3.0) < rms (a, .2, .5) * .05f, "nessuna nota bloccata cambiando trasposizione");
    }
    // 9. temperamenti: scarto di Mi rispetto a Do, confrontato con l'equabile (stesso campione, quindi misura relativa)
    {
        auto measure = [&] (int note, double lo, double hi)
        {
            auto b = r.play (1.6, { { .0, on (note, 90) }, { 1.2, off (note) } });
            return peakFreq (b, .3, .6, lo, hi);
        };
        const double cEq = measure (60, 250, 272), eEq = measure (64, 318, 342);
        r.engine.setTemperament (1);   // Werckmeister III, tonica Do
        const double cW = measure (60, 250, 272), eW = measure (64, 318, 342);
        const double dW = cents (eW, eEq) - cents (cW, cEq);
        check (std::abs (dW - (-9.775)) < 1.0, "Werckmeister III in Do: Mi rispetto a Do " + juce::String (dW, 2) + " cent (atteso -9,78)");
        std::printf ("      riferimento: Do %+.2f cent, Mi %+.2f cent rispetto all'equabile\n", cents (cW, cEq), cents (eW, eEq));
        r.s.temperKey = 2;            // tonica Re: Mi è il secondo grado (-7,82), Do il settimo minore (-3,91)
        r.play (.05, {});
        const double cD = measure (60, 250, 272), eD = measure (64, 318, 342);
        const double dD = cents (eD, eEq) - cents (cD, cEq);
        check (std::abs (dD - (-3.91)) < 1.0, "Werckmeister III in Re: Mi rispetto a Do " + juce::String (dD, 2) + " cent (atteso -3,91)");
        r.s.temperKey = 0;
        r.engine.setTemperament (0);
        const double cBack = measure (60, 250, 272);
        check (std::abs (cents (cBack, cEq)) < .5, "ritorno all'equabile");
    }
    // 10. SFZ CONTROL Vol (CC 7) a zero: silenzio
    if (const int vol = slotOf (7); vol >= 0)
    {
        const float def = r.s.slots[(size_t) vol];
        r.s.slots[(size_t) vol] = 0.f;
        auto b = r.play (1.0, { { .0, on (60, 110) }, { .8, off (60) } });
        r.s.slots[(size_t) vol] = def;
        r.play (.5, {});
        check (rms (b, .0, .5) < 1e-5f, "SFZ CONTROL Vol a zero spegne lo strumento");
    }
    // 11. qualità di ricampionamento: suono valido a ogni livello
    for (int q : { 2, 4, 10 })
    {
        r.s.quality = q;
        auto b = r.play (.6, { { .0, on (69, 100) }, { .5, off (69) } });
        check (rms (b, .05, .4) > 1e-3f && finite (b), "qualita' " + juce::String (q));
    }
    r.s.quality = 4;

    // 12. strumento con keyswitch (file di prova): articolazioni, nomi, trasposizione che non tocca i keyswitch
    if (argc > 3)
    {
        Rig k;
        k.s.offline = true;
        k.engine.prepare (sr, block);
        k.engine.loadAsync (juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]));
        for (int i = 0; i < 200 && k.engine.status() == PianoEngine::Status::loading; ++i) juce::Thread::sleep (50);
        const auto ki = k.engine.info();
        check (ki != nullptr && ki->switches[24] && ki->switches[26] && ki->switchLabels.count (24) && ki->switchLabels.at (24) == "Sine",
               "keyswitch letti dal file (C1 Sine, D1 Octave)");
        check (ki != nullptr && ki->defaultSwitch == 24 && k.engine.currentSwitch() == 24, "articolazione iniziale (sw_default)");
        int lvl = -1;
        if (ki != nullptr)
            for (int i = 0; i < (int) ki->controls.size(); ++i)
            {
                k.s.slots[(size_t) i] = ki->controls[(size_t) i].defaultValue;
                if (ki->controls[(size_t) i].cc == 20) lvl = i;
            }
        check (lvl >= 0 && ki->controls[(size_t) lvl].label == "Level" && ki->controls[(size_t) lvl].defaultValue > .99f, "controllo con nome: CC 20 Level, valore iniziale 127");
        k.s.transpose = 3;            // non deve spostare i keyswitch
        auto a = k.play (1.0, { { .0, on (24, 100) }, { .05, off (24) }, { .1, on (57, 100) }, { .9, off (57) } });
        check (k.engine.currentSwitch() == 24, "keyswitch C1 attivo");
        const double f1 = peakFreq (a, .3, .4, 200, 600);
        std::printf ("      livello articolazione 1: %.1f dBFS\n", juce::Decibels::gainToDecibels (rms (a, .3, .7)));
        auto b = k.play (1.0, { { .0, on (26, 100) }, { .05, off (26) }, { .1, on (57, 100) }, { .9, off (57) } });
        check (k.engine.currentSwitch() == 26, "keyswitch D1 attivo");
        const double f2 = peakFreq (b, .3, .4, 200, 600);
        check (std::abs (cents (f1, 261.626)) < 3 && std::abs (cents (f2, 523.251)) < 3,
               "articolazioni: A3+3 = C4 " + juce::String (f1, 1) + " Hz, poi un'ottava sopra " + juce::String (f2, 1) + " Hz");
        if (lvl >= 0) k.s.slots[(size_t) lvl] = 0.f;
        auto c = k.play (.6, { { .0, on (57, 100) }, { .5, off (57) } });
        check (rms (c, .1, .4) < 1e-5f, "CC 20 a zero spegne (amplitude_oncc20)");
    }

    // 13. sequencer: la demo incorporata suona nel motore, STOP spegne le note
    std::vector<SeqEvent> demo;
    {
        juce::MemoryInputStream in (BinaryData::demorikytheme_mid, (size_t) BinaryData::demorikytheme_midSize, false);
        const bool read = Sequencer::readMidi (in, demo);
        check (read && demo.size() > 600, "demo incorporata letta (" + juce::String ((int) demo.size()) + " eventi, "
                                          + juce::String (demo.empty() ? 0.0 : demo.back().time, 1) + " s)");
        Sequencer seq;
        seq.setEvents (demo, "DEMO");
        seq.play();
        juce::AudioBuffer<float> b (2, block);
        float during = 0.f;
        for (int i = 0; i < (int) (8.0 * sr / block); ++i)
        {
            juce::MidiBuffer m;
            seq.processPlayback (m, block, sr);
            b.setSize (2, block, false, false, true);
            r.engine.render (b, m, r.s);
            if (i * block > 4 * sr) during = juce::jmax (during, b.getMagnitude (0, block));
        }
        check (seq.state() == Sequencer::State::playing && std::abs (seq.position() - 8.0) < .02 && during > .01f, "PLAY della demo: suona e il tempo avanza");
        seq.stop();
        float after = 0.f;
        for (int i = 0; i < (int) (5.0 * sr / block); ++i)
        {
            juce::MidiBuffer m;
            seq.processPlayback (m, block, sr);
            b.setSize (2, block, false, false, true);
            r.engine.render (b, m, r.s);
            if (i * block > 4 * sr) after = juce::jmax (after, b.getMagnitude (0, block));
        }
        check (seq.state() == Sequencer::State::stopped && after < .002f, "STOP: note e pedale rilasciati (" + juce::String (juce::Decibels::gainToDecibels (after), 1) + " dBFS dopo 4 s)");
    }
    // 14. registrazione: note e pulsante SUSTAIN registrati, silenzio iniziale tolto, nota tenuta chiusa, file MIDI salvato e riletto
    {
        Sequencer seq;
        seq.record();
        juce::MidiBuffer m;
        seq.processPlayback (m, block, sr);   // il comando REC parte qui
        const int total = (int) (3.0 * sr / block);
        for (int i = 0; i < total; ++i)
        {
            juce::MidiBuffer in;
            const double t = (double) i * block / sr;
            if (i == (int) (1.0 * sr / block)) in.addEvent (on (60, 90), 17);
            if (i == (int) (1.5 * sr / block)) in.addEvent (off (60), 3);
            if (i == (int) (2.0 * sr / block)) in.addEvent (on (64, 70), 0);   // resta giù fino a STOP
            seq.processRecording (in, block, sr, t > 1.2 && t < 1.8);
            if (i % 20 == 0) seq.update();
        }
        seq.stop();
        juce::MidiBuffer m2;
        seq.processPlayback (m2, block, sr);
        seq.update();
        const auto& ev = seq.events();
        int ons = 0, offs = 0, peds = 0; double firstOn = -1;
        for (auto& e : ev)
        {
            if (e.msg.isNoteOn()) { ++ons; if (firstOn < 0) firstOn = e.time; }
            else if (e.msg.isNoteOff()) ++offs;
            else if (e.msg.isController() && e.msg.getControllerNumber() == 64) ++peds;
        }
        check (ons == 2 && offs == 2 && peds == 2, "ripresa: 2 note, 2 rilasci (uno aggiunto alla fine), pedale giu' e su");
        check (std::abs (firstOn - .2) < .002, "silenzio iniziale tolto: prima nota a " + juce::String (firstOn, 3) + " s");
        const auto mid = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riky-take.mid");
        Sequencer back;
        check (seq.saveMidiFile (mid) && back.loadMidiFile (mid) && back.events().size() == ev.size()
               && std::abs (back.events().front().time - ev.front().time) < .001, "salvataggio e rilettura del file MIDI");
        mid.deleteFile();
    }
    // 15. export: MP3 320 e WAV 24 bit della demo, fuori tempo reale
    if (argc > 4)
    {
        const juce::File outDir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[4]);
        outDir.createDirectory();
        for (auto fmt : { Exporter::Format::mp3_320, Exporter::Format::wav24 })
        {
            Exporter::Job j;
            j.events = demo;
            j.sfz = r.engine.currentFile();
            j.dest = outDir.getChildFile (juce::String ("riky-theme") + Exporter::extension (fmt));
            j.title = "Riky's Theme";
            j.engine = r.s;
            j.engine.pedalButton = false;
            j.format = fmt;
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            const auto res = Exporter::renderNow (j, [] (float) { return true; });
            const double secs = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
            const auto size = res.file.getSize();
            check (res.ok && res.seconds > demo.back().time && res.seconds < demo.back().time + 21,
                   "export " + j.dest.getFileName() + ": " + juce::String (res.seconds, 1) + " s di audio in " + juce::String (secs, 1) + " s, picco "
                   + juce::String (res.peakDb, 1) + " dBFS, " + juce::String (size / 1024) + " KB  " + res.text);
            check (res.peakDb < -.1f && res.peakDb > -30.f, "livello dell'export sensato, senza clip");
            if (fmt == Exporter::Format::mp3_320)
            {
                juce::MemoryBlock mb;
                res.file.loadFileAsData (mb);
                const auto* d = static_cast<const char*> (mb.getData());
                auto findBytes = [&] (const char* tag) { for (size_t i = 0; i + 4 < juce::jmin<size_t> (mb.getSize(), 8192); ++i) if (std::memcmp (d + i, tag, 4) == 0) return true; return false; };
                const double expected = 320000.0 / 8.0 * res.seconds;
                check (mb.getSize() > 3 && d[0] == 'I' && d[1] == 'D' && d[2] == '3', "MP3 con tag ID3v2");
                check (findBytes ("Info") || findBytes ("Xing"), "MP3 con intestazione LAME (durata esatta nei lettori)");
                check (std::abs ((double) mb.getSize() - expected) / expected < .03, "dimensione coerente con 320 kbps");
            }
        }
    }

    check (finite (r.out), "nessun NaN o infinito");

    juce::File wav = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);
    wav.deleteFile();
    juce::WavAudioFormat fmt;
    if (auto os = wav.createOutputStream())
    {
        std::unique_ptr<juce::OutputStream> stream (os.release());
        if (auto w = fmt.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24)))
            w->writeFromAudioSampleBuffer (r.out, 0, r.out.getNumSamples());
    }
    std::printf ("%s\n", failures == 0 ? "TUTTO OK" : "CI SONO ERRORI");
    return failures == 0 ? 0 : 1;
}
