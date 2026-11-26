# Correzione per sfizz 1.2.3 (src/sfizz/FilePool.cpp), applicata prima della compilazione.
# Nel render fuori tempo reale (freewheeling) waitForBackgroundLoading() aspettava solo i caricamenti già
# avviati, non i file ancora nella coda del thread di smistamento: a volte una nota appena partita
# restava senza dati e l'export aveva un buco di silenzio. Ora la coda viene svuotata e attesa.
# Uso: cmake -DSFIZZ_SOURCE=<cartella di sfizz> -P patch-sfizz.cmake   (si può ripetere: non cambia due volte)
set(_file "${SFIZZ_SOURCE}/src/sfizz/FilePool.cpp")
file(READ "${_file}" _src)
set(_old "void sfz::FilePool::waitForBackgroundLoading() noexcept
{
    std::lock_guard<std::mutex> guard { loadingJobsMutex };

    for (auto& job : loadingJobs)")
set(_new "void sfz::FilePool::waitForBackgroundLoading() noexcept
{
    std::lock_guard<std::mutex> guard { loadingJobsMutex };

    // Riky Piano Man: anche i file ancora in coda (il thread di smistamento prende la coda solo con questo mutex)
    QueuedFileData queuedData;
    while (filesToLoad->try_pop(queuedData)) {
        if (!queuedData.id.expired())
            loadingJobs.push_back(
                threadPool->enqueue([this](const QueuedFileData& data) { loadingJob(data); }, std::move(queuedData)));
    }

    for (auto& job : loadingJobs)")
string(FIND "${_src}" "Riky Piano Man: anche i file ancora in coda" _done)
if(_done GREATER -1)
    return()
endif()
string(FIND "${_src}" "${_old}" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR "patch-sfizz: testo di FilePool.cpp non trovato, versione di sfizz diversa?")
endif()
string(REPLACE "${_old}" "${_new}" _src "${_src}")
file(WRITE "${_file}" "${_src}")
message(STATUS "patch-sfizz: FilePool::waitForBackgroundLoading corretto")
