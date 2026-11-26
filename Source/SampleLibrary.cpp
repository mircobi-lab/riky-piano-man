#include "SampleLibrary.h"

namespace riky
{
namespace
{
    juce::PropertiesFile::Options settingsOptions()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Riky Piano Man";
        o.folderName = "Riky Piano Man";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }
}

SampleLibrary::SampleLibrary() : juce::Thread ("Riky Piano Man installer") {}

SampleLibrary::~SampleLibrary()
{
    alive->store (false);
    stopThread (15000);
}

juce::File SampleLibrary::baseFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Riky Piano Man");
}

juce::File SampleLibrary::installFolder() { return baseFolder().getChildFile ("Maestro Concert Grand"); }

juce::File SampleLibrary::findSfz (const juce::File& folder)
{
    if (! folder.isDirectory()) return {};
    if (auto f = folder.getChildFile (sfzName); f.existsAsFile()) return f;
    for (const auto& e : juce::RangedDirectoryIterator (folder, false, "*", juce::File::findDirectories))
        if (auto f = e.getFile().getChildFile (sfzName); f.existsAsFile()) return f;
    return {};
}

bool SampleLibrary::looksComplete (const juce::File& sfz)
{
    if (! sfz.existsAsFile()) return false;
    const auto dir = sfz.getParentDirectory();
    return dir.getChildFile ("Data").isDirectory()
        && dir.getChildFile ("Samples").getNumberOfChildFiles (juce::File::findFiles, "*.flac") >= 800;
}

juce::File SampleLibrary::savedSfz()
{
    juce::PropertiesFile p (settingsOptions());
    const auto path = p.getValue ("sfz");
    return path.isNotEmpty() && juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
}

void SampleLibrary::saveSfz (const juce::File& f)
{
    juce::PropertiesFile p (settingsOptions());
    p.setValue ("sfz", f.getFullPathName());
    p.saveIfNeeded();
}

juce::File SampleLibrary::locate()
{
    if (auto f = savedSfz(); f.existsAsFile()) return f;
    return findSfz (installFolder());
}

void SampleLibrary::addRecent (const juce::File& f)
{
    juce::PropertiesFile p (settingsOptions());
    juce::StringArray list;
    list.addTokens (p.getValue ("recent"), "\n", {});
    list.removeString (f.getFullPathName());
    list.insert (0, f.getFullPathName());
    list.removeEmptyStrings();
    while (list.size() > 10) list.remove (list.size() - 1);
    p.setValue ("recent", list.joinIntoString ("\n"));
    p.saveIfNeeded();
}

juce::Array<juce::File> SampleLibrary::recent()
{
    juce::PropertiesFile p (settingsOptions());
    juce::StringArray list;
    list.addTokens (p.getValue ("recent"), "\n", {});
    juce::Array<juce::File> out;
    for (auto& s : list)
        if (juce::File::isAbsolutePath (s) && juce::File (s).existsAsFile()) out.add (juce::File (s));
    return out;
}

juce::Array<juce::File> SampleLibrary::findAllSfz (const juce::File& folder, int maxFiles)
{
    juce::Array<juce::File> out;
    if (! folder.isDirectory()) return out;
    for (const auto& e : juce::RangedDirectoryIterator (folder, true, "*.sfz", juce::File::findFiles))
    {
        // i file in Data/ e simili sono pezzi inclusi da altri .sfz: si tengono quelli in cima alle cartelle degli strumenti
        if (e.getFile().getParentDirectory().getFileName().equalsIgnoreCase ("Data")) continue;
        out.add (e.getFile());
        if (out.size() >= maxFiles) break;
    }
    struct ByPath { static int compareElements (const juce::File& a, const juce::File& b) { return a.getFullPathName().compareNatural (b.getFullPathName()); } } cmp;
    out.sort (cmp);
    return out;
}

juce::String SampleLibrary::errorText() const { const juce::ScopedLock sl (lock); return error; }

void SampleLibrary::post (std::function<void()> fn)
{
    juce::MessageManager::callAsync ([flag = alive, fn = std::move (fn)] { if (flag->load()) fn(); });
}

void SampleLibrary::fail (const juce::String& e)
{
    {
        const juce::ScopedLock sl (lock);
        error = e;
    }
    stateValue.store ((int) State::failed);
    post ([this] { if (onProgress) onProgress(); });
}

void SampleLibrary::startInstall()
{
    if (isThreadRunning()) return;
    {
        const juce::ScopedLock sl (lock);
        error = {};
    }
    bytes.store (0);
    progressValue.store (0.f);
    stateValue.store ((int) State::downloading);
    startThread();
}

void SampleLibrary::cancel() { signalThreadShouldExit(); }

void SampleLibrary::run()
{
    const auto base = baseFolder();
    if (! base.createDirectory()) { fail ("CANNOT CREATE " + base.getFullPathName()); return; }
    const auto zipFile = base.getChildFile ("download.zip");
    zipFile.deleteFile();

    // 1. download dell'archivio del repository ufficiale, alla versione fissata
    juce::URL url (juce::String (repoUrl) + "/archive/" + repoCommit + ".zip");
    // solo per le prove automatiche: un archivio locale al posto di GitHub
    if (const auto testZip = juce::SystemStats::getEnvironmentVariable ("RIKY_TEST_ARCHIVE", {}); testZip.isNotEmpty())
        url = juce::URL (juce::File (testZip));
    int status = 0;
    auto in = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                         .withConnectionTimeoutMs (20000)
                                         .withNumRedirectsToFollow (8)
                                         .withStatusCode (&status));
    if (in == nullptr) { fail ("NO CONNECTION TO GITHUB"); return; }
    if (status != 0 && status != 200) { fail ("GITHUB ANSWERED " + juce::String (status)); return; }
    const auto total = in->getTotalLength();
    {
        juce::FileOutputStream out (zipFile);
        if (! out.openedOk()) { fail ("CANNOT WRITE " + zipFile.getFullPathName()); return; }
        juce::HeapBlock<char> buf (1 << 16);
        float lastPosted = -1.f;
        for (;;)
        {
            if (threadShouldExit()) { out.flush(); zipFile.deleteFile(); fail ("CANCELLED"); return; }
            const int n = in->read (buf.get(), 1 << 16);
            if (n < 0) { zipFile.deleteFile(); fail ("DOWNLOAD INTERRUPTED"); return; }
            if (n == 0) { if (in->isExhausted()) break; wait (5); continue; }
            if (! out.write (buf.get(), (size_t) n)) { zipFile.deleteFile(); fail ("DISK FULL?"); return; }
            bytes += n;
            const float p = (float) juce::jlimit (0.0, .99, (double) bytes.load() / (double) (total > 0 ? total : expectedBytes));
            progressValue.store (p);
            if (p - lastPosted > .005f) { lastPosted = p; post ([this] { if (onProgress) onProgress(); }); }
        }
        out.flush();
        if (out.getStatus().failed()) { zipFile.deleteFile(); fail ("CANNOT WRITE THE ARCHIVE"); return; }
    }
    in.reset();
    if (total > 0 && bytes.load() != total) { zipFile.deleteFile(); fail ("DOWNLOAD INCOMPLETE"); return; }

    // 2. estrazione: file invariati, compreso mcgreadme.txt con le note legali dell'autore
    stateValue.store ((int) State::extracting);
    post ([this] { if (onProgress) onProgress(); });
    const auto tmp = base.getChildFile ("extract-tmp");
    tmp.deleteRecursively();
    {
        juce::ZipFile zip (zipFile);
        if (zip.getNumEntries() == 0) { zipFile.deleteFile(); fail ("ARCHIVE NOT VALID"); return; }
        if (const auto r = zip.uncompressTo (tmp, true); r.failed()) { zipFile.deleteFile(); tmp.deleteRecursively(); fail ("EXTRACT FAILED: " + r.getErrorMessage()); return; }
    }
    zipFile.deleteFile();
    const auto sfz = findSfz (tmp);
    if (! looksComplete (sfz)) { tmp.deleteRecursively(); fail ("SAMPLES INCOMPLETE"); return; }
    const auto dest = installFolder();
    dest.deleteRecursively();
    if (! sfz.getParentDirectory().moveFileTo (dest)) { fail ("CANNOT MOVE TO " + dest.getFullPathName()); return; }
    tmp.deleteRecursively();

    const auto finalSfz = findSfz (dest);
    if (! looksComplete (finalSfz)) { fail ("SAMPLES INCOMPLETE"); return; }
    saveSfz (finalSfz);
    progressValue.store (1.f);
    stateValue.store ((int) State::done);
    post ([this, finalSfz] { if (onInstalled) onInstalled (finalSfz); if (onProgress) onProgress(); });
}
}
