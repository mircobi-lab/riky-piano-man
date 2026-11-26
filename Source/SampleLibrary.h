#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>

// Dove stanno i campioni del Maestro Concert Grand e come si scaricano.
// I campioni NON sono dentro il plugin: la licenza di Mats Helgesson non consente di modificarli e ridistribuirli.
// Il plugin li scarica, invariati e con il loro mcgreadme.txt, dal repository ufficiale sfzinstruments su GitHub.
namespace riky
{
class SampleLibrary : private juce::Thread
{
public:
    // versione fissata del repository (commit del 23 agosto 2020, l'ultima)
    static constexpr const char* repoCommit = "717078b450eccc4a7733755f4b5973426ebc23e9";
    static constexpr const char* repoUrl = "https://github.com/sfzinstruments/MatsHelgesson.MaestroConcertGrandPiano";
    static constexpr const char* sfzName = "Maestro Concert Grand.sfz";
    static constexpr juce::int64 expectedBytes = 268000000;   // circa 256 MB di FLAC

    enum class State { idle, downloading, extracting, done, failed };

    SampleLibrary();
    ~SampleLibrary() override;

    static juce::File baseFolder();                           // Musica/Riky Piano Man
    static juce::File installFolder();                        // Musica/Riky Piano Man/Maestro Concert Grand
    static juce::File findSfz (const juce::File& folder);     // cerca il file .sfz nella cartella (fino a 2 livelli)
    static bool looksComplete (const juce::File& sfz);        // ci sono il file e i campioni?

    // percorso scelto dall'utente, condiviso fra tutte le istanze e l'app
    static juce::File savedSfz();
    static void saveSfz (const juce::File&);
    static juce::File locate();                               // scelto dall'utente, altrimenti cartella predefinita
    static void addRecent (const juce::File&);                // ultimi file aperti (fino a 10)
    static juce::Array<juce::File> recent();
    static juce::Array<juce::File> findAllSfz (const juce::File& folder, int maxFiles = 300);   // libreria: tutti i .sfz della cartella

    void startInstall();                                      // download + estrazione in un thread
    void cancel();
    State state() const noexcept { return (State) stateValue.load(); }
    float progress() const noexcept { return progressValue.load(); }      // 0..1 (stima se il server non dà la lunghezza)
    juce::int64 bytesDone() const noexcept { return bytes.load(); }
    juce::String errorText() const;
    std::function<void (const juce::File& sfz)> onInstalled;  // thread dei messaggi
    std::function<void()> onProgress;                         // thread dei messaggi

private:
    std::atomic<int> stateValue { (int) State::idle };
    std::atomic<float> progressValue { 0.f };
    std::atomic<juce::int64> bytes { 0 };
    mutable juce::CriticalSection lock;
    juce::String error;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    void run() override;
    void fail (const juce::String&);
    void post (std::function<void()>);
};
}
