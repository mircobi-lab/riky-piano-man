#include "Exporter.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <lame.h>

namespace riky
{
namespace
{
    // codifica MP3 con LAME 3.100: tag ID3v2 in testa, fotogrammi, ID3v1 in coda e intestazione LAME/Xing riscritta alla fine
    class Mp3Writer
    {
    public:
        Mp3Writer (juce::FileOutputStream& o, double sr, Exporter::Format f, const juce::String& title) : out (o)
        {
            gf = lame_init();
            if (gf == nullptr) return;
            lame_set_in_samplerate (gf, (int) sr);
            lame_set_out_samplerate (gf, (int) sr);
            lame_set_num_channels (gf, 2);
            lame_set_mode (gf, JOINT_STEREO);
            lame_set_quality (gf, 2);                        // 2 = alta qualità (0 il massimo, molto più lento)
            if (f == Exporter::Format::mp3_320) { lame_set_VBR (gf, vbr_off); lame_set_brate (gf, 320); }
            else { lame_set_VBR (gf, vbr_default); lame_set_VBR_quality (gf, 0.f); }   // V0
            lame_set_bWriteVbrTag (gf, 1);
            lame_set_write_id3tag_automatic (gf, 0);
            id3tag_init (gf);
            id3tag_add_v2 (gf);
            id3tag_set_title (gf, title.toRawUTF8());
            id3tag_set_artist (gf, "Riky Piano Man");
            id3tag_set_album (gf, "Riky Piano Man - MBStudio91");
            id3tag_set_year (gf, juce::String (juce::Time::getCurrentTime().getYear()).toRawUTF8());
            id3tag_set_comment (gf, "Maestro Concert Grand (c) Mats Helgesson 2003");
            ok = lame_init_params (gf) >= 0;
            if (! ok) return;
            std::vector<unsigned char> tag (lame_get_id3v2_tag (gf, nullptr, 0));
            if (! tag.empty())
            {
                lame_get_id3v2_tag (gf, tag.data(), tag.size());
                out.write (tag.data(), tag.size());
            }
            firstFrame = out.getPosition();
        }
        ~Mp3Writer() { if (gf != nullptr) lame_close (gf); }
        bool isOk() const { return ok; }

        bool write (const float* l, const float* r, int n)
        {
            buf.resize ((size_t) (n + n / 4 + 7200));
            const int b = lame_encode_buffer_ieee_float (gf, l, r, n, buf.data(), (int) buf.size());
            return b >= 0 && out.write (buf.data(), (size_t) b);
        }

        bool finish()
        {
            buf.resize (7200);
            const int b = lame_encode_flush (gf, buf.data(), (int) buf.size());
            if (b < 0 || ! out.write (buf.data(), (size_t) b)) return false;
            unsigned char v1[128];
            if (const auto n = lame_get_id3v1_tag (gf, v1, sizeof (v1)); n > 0 && n <= sizeof (v1)) out.write (v1, n);
            std::vector<unsigned char> lt (lame_get_lametag_frame (gf, nullptr, 0));
            if (! lt.empty() && lame_get_lametag_frame (gf, lt.data(), lt.size()) == lt.size())
            {
                const auto end = out.getPosition();
                out.setPosition (firstFrame);
                out.write (lt.data(), lt.size());
                out.setPosition (end);
            }
            out.flush();
            return out.getStatus().wasOk();
        }

    private:
        juce::FileOutputStream& out;
        lame_t gf = nullptr;
        bool ok = false;
        juce::int64 firstFrame = 0;
        std::vector<unsigned char> buf;
    };
}

bool Exporter::start (Job j)
{
    if (isThreadRunning()) return false;
    job = std::move (j);
    progressValue.store (0.f);
    {
        const juce::ScopedLock sl (lock);
        res = {};
    }
    startThread();
    return true;
}

void Exporter::run()
{
    auto r = renderNow (job, [this] (float p) { progressValue.store (p); return ! threadShouldExit(); });
    {
        const juce::ScopedLock sl (lock);
        res = r;
    }
    juce::MessageManager::callAsync ([this, flag = alive] { if (flag->load() && onFinished) onFinished(); });
}

Exporter::Result Exporter::renderNow (Job job, std::function<bool (float)> progress)
{
    Result r;
    if (job.events.empty()) { r.text = "NOTHING TO EXPORT"; return r; }
    const double sr = job.sampleRate;
    constexpr int block = 512;

    // motore nuovo: stesso SFZ, stessi temperamento e polifonia; offline = qualità massima e attesa dei campioni dal disco
    PianoEngine engine;
    engine.setTemperament (job.temperament);
    engine.setPolyphony (job.polyphony);
    engine.prepare (sr, block);
    engine.loadAsync (job.sfz);
    for (int i = 0; i < 1200 && engine.status() == PianoEngine::Status::loading; ++i)
    {
        if (! progress (0.f)) { r.text = "CANCELLED"; return r; }
        juce::Thread::sleep (25);
    }
    if (engine.status() != PianoEngine::Status::ready) { r.text = "PIANO NOT LOADED: " + engine.errorText(); return r; }
    job.engine.offline = true;
    job.engine.hasTransport = false;

    PostChain post;
    post.prepare (sr, block, job.post);

    // file temporaneo accanto alla destinazione: se qualcosa va storto non resta un file a metà
    auto tmp = job.dest.getSiblingFile (job.dest.getFileNameWithoutExtension() + ".part" + job.dest.getFileExtension());
    tmp.deleteFile();
    bool writeOk = true;
    {
        auto fo = std::make_unique<juce::FileOutputStream> (tmp);
        if (! fo->openedOk()) { r.text = "CANNOT WRITE " + job.dest.getFullPathName(); return r; }
        std::unique_ptr<juce::AudioFormatWriter> wav;
        std::unique_ptr<Mp3Writer> mp3;
        if (job.format == Format::wav24)
        {
            juce::WavAudioFormat fmt;
            std::unordered_map<juce::String, juce::String> meta { { juce::WavAudioFormat::riffInfoTitle, job.title },
                                                                 { juce::WavAudioFormat::riffInfoArtist, "Riky Piano Man" } };
            std::unique_ptr<juce::OutputStream> stream (fo.release());   // il writer prende il file
            wav = fmt.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24)
                                                   .withMetadataValues (meta));
            if (wav == nullptr) { r.text = "WAV WRITER ERROR"; return r; }
        }
        else
        {
            mp3 = std::make_unique<Mp3Writer> (*fo, sr, job.format, job.title);
            if (! mp3->isOk()) { r.text = "MP3 ENCODER ERROR"; return r; }
        }

        const double lastTime = job.events.back().time;
        const auto lastSample = (juce::int64) std::llround (lastTime * sr);
        const auto maxTail = (juce::int64) (20.0 * sr);
        size_t cursor = 0;
        juce::int64 pos = 0, quietSince = -1;
        float peak = 0.f;
        juce::AudioBuffer<float> buf (2, block);
        for (;; pos += block)
        {
            juce::MidiBuffer midi;
            for (; cursor < job.events.size(); ++cursor)
            {
                const auto at = (juce::int64) std::llround (job.events[cursor].time * sr);
                if (at >= pos + block) break;
                midi.addEvent (job.events[cursor].msg, (int) juce::jlimit<juce::int64> (0, block - 1, at - pos));
            }
            buf.setSize (2, block, false, false, true);
            engine.render (buf, midi, job.engine);
            post.process (buf, job.post);
            peak = juce::jmax (peak, post.peak());

            // coda: dopo l'ultimo evento si va avanti finché il suono (riverbero compreso) scende sotto -90 dBFS per mezzo secondo
            if (pos > lastSample)
            {
                if (post.peak() < 3.2e-5f) { if (quietSince < 0) quietSince = pos; }
                else quietSince = -1;
            }
            const bool done = pos > lastSample && ((quietSince >= 0 && pos - quietSince > (juce::int64) (.5 * sr)) || pos - lastSample > maxTail);
            int n = block;
            if (done && quietSince >= 0) n = 0;   // l'ultimo blocco silenzioso non serve
            if (n > 0)
            {
                if (wav != nullptr) writeOk = wav->writeFromAudioSampleBuffer (buf, 0, n);
                else writeOk = mp3->write (buf.getReadPointer (0), buf.getReadPointer (1), n);
            }
            if (! writeOk) break;
            if (! progress ((float) juce::jmin (.99, (double) pos / (double) (lastSample + 3.0 * sr)))) { writeOk = false; r.text = "CANCELLED"; break; }
            if (done) break;
        }
        if (writeOk && mp3 != nullptr) writeOk = mp3->finish();
        mp3.reset();
        wav.reset();
        fo.reset();
        r.seconds = (double) pos / sr;
        r.peakDb = juce::Decibels::gainToDecibels (peak, -100.f);
    }
    if (! writeOk)
    {
        tmp.deleteFile();
        if (r.text.isEmpty()) r.text = "WRITE ERROR";
        return r;
    }
    job.dest.deleteFile();
    if (! tmp.moveFileTo (job.dest)) { r.text = "CANNOT WRITE " + job.dest.getFullPathName(); return r; }
    progress (1.f);
    r.ok = true;
    r.file = job.dest;
    r.text = "SAVED " + job.dest.getFileName().toUpperCase() + "   PEAK " + juce::String (r.peakDb, 1) + " DBFS"
           + (r.peakDb > -.1f ? "   CLIPPING: LOWER THE VOLUME" : "");
    return r;
}
}
