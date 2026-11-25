// Prova del motore senza interfaccia: carica il Maestro Concert Grand con sfizz, suona e controlla il risultato.
// uso: riky_render "Maestro Concert Grand.sfz" uscita.wav
#include "PianoEngine.h"
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
        r.s.release = .1f;
        auto withNoise = r.play (2.0, { { .0, on (40, 100) }, { .6, off (40) } });
        r.s.keyNoise = 0.f;
        auto without = r.play (2.0, { { .0, on (40, 100) }, { .6, off (40) } });
        r.s.keyNoise = 1.f; r.s.release = .575f;
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
