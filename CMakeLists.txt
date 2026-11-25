cmake_minimum_required(VERSION 3.22)

# Riky Piano Man: pianoforte personalizzato per Riky Mercenati (VST3 / AU / Standalone), grafica MBStudio91 come Campione.
# Suono: Maestro Concert Grand di Mats Helgesson in formato SFZ (sfzinstruments), suonato con sfizz (BSD-2).
# I campioni non sono nel plugin: vengono scaricati dal repository ufficiale al primo uso.
# Progetto distribuito sotto AGPL v3 (vedi LICENSE.md), come MBStudio91 Campione da cui deriva.
project(RikyPianoMan VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# runtime statico su Windows: lo usa sfizz e lo usiamo anche noi (niente DLL di Visual C++ da installare)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(APPLE)
    # un'architettura per volta (sfizz non compila in universale): il workflow unisce x86_64 e arm64 con lipo
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}" CACHE STRING "" FORCE)
    endif()
    list(LENGTH CMAKE_OSX_ARCHITECTURES _archCount)
    if(_archCount GREATER 1)
        message(FATAL_ERROR "Compila un'architettura per volta (x86_64 oppure arm64)")
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
    endif()
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        # sfizz aggiungerebbe -mfpu=neon -mfloat-abi=hard, che clang per Apple Silicon rifiuta
        set(PROJECT_SYSTEM_PROCESSOR "aarch64" CACHE STRING "" FORCE)
        set(WAVPACK_ENABLE_ASM OFF CACHE BOOL "" FORCE)
    else()
        set(PROJECT_SYSTEM_PROCESSOR "x86_64" CACHE STRING "" FORCE)
    endif()
endif()

include(FetchContent)

# ---- JUCE
set(JUCE_DIR "" CACHE PATH "Percorso di una copia locale di JUCE")
if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.5
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---- sfizz (libreria statica, senza client)
set(SFIZZ_JACK OFF CACHE BOOL "" FORCE)
set(SFIZZ_RENDER OFF CACHE BOOL "" FORCE)
set(SFIZZ_SHARED OFF CACHE BOOL "" FORCE)
set(SFIZZ_TESTS OFF CACHE BOOL "" FORCE)
set(SFIZZ_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(SFIZZ_DEMOS OFF CACHE BOOL "" FORCE)
set(SFIZZ_DEVTOOLS OFF CACHE BOOL "" FORCE)
set(SFIZZ_USE_SNDFILE OFF CACHE BOOL "" FORCE)
set(ENABLE_LTO OFF CACHE BOOL "" FORCE)
set(SFIZZ_DIR "" CACHE PATH "Percorso di una copia locale di sfizz (con i sottomoduli)")
if(SFIZZ_DIR AND EXISTS "${SFIZZ_DIR}/CMakeLists.txt")
    add_subdirectory("${SFIZZ_DIR}" sfizz EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(sfizz
        GIT_REPOSITORY https://github.com/sfztools/sfizz.git
        GIT_TAG 1.2.3
        GIT_SHALLOW TRUE
        GIT_SUBMODULES_RECURSE TRUE
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(sfizz)
endif()

# ---- plugin
set(RIKY_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RIKY_FORMATS AU)
endif()

juce_add_plugin(RikyPianoMan
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/icons/riky-1024.png"
    COMPANY_NAME "MBStudio91"
    COMPANY_WEBSITE "https://mbstudio91.fun"
    BUNDLE_ID "fun.mbstudio91.rikypianoman"
    PRODUCT_NAME "Riky Piano Man"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Mbst
    PLUGIN_CODE Rkpm
    FORMATS ${RIKY_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Piano
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    HARDENED_RUNTIME_ENABLED TRUE
    HARDENED_RUNTIME_OPTIONS com.apple.security.network.client
    NEEDS_CURL TRUE)

include(MBStudioUI/MBStudioUI.cmake)
mbstudio_ui_add(RikyPianoMan)

set(RIKY_ENGINE_SOURCES Source/Params.cpp Source/PianoEngine.cpp)
target_sources(RikyPianoMan PRIVATE
    ${RIKY_ENGINE_SOURCES}
    Source/SampleLibrary.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

if(TARGET RikyPianoMan_Standalone)
    target_sources(RikyPianoMan_Standalone PRIVATE Source/StandaloneApp.cpp)
    target_compile_definitions(RikyPianoMan_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
endif()

target_compile_definitions(RikyPianoMan PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_JACK=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(RikyPianoMan
    PRIVATE
        sfizz::sfizz
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# prova offline: carica il Maestro, suona e controlla intonazione, pedali, rilasci, PANIC (non fa parte del plugin)
option(RIKY_BUILD_TOOLS "Compila lo strumento di render di prova" OFF)
if(RIKY_BUILD_TOOLS)
    juce_add_console_app(riky_render PRODUCT_NAME "riky_render")
    target_sources(riky_render PRIVATE tools/render.cpp ${RIKY_ENGINE_SOURCES})
    target_include_directories(riky_render PRIVATE Source)
    target_compile_definitions(riky_render PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(riky_render PRIVATE sfizz::sfizz juce::juce_audio_processors juce::juce_audio_basics juce::juce_audio_formats juce::juce_events juce::juce_gui_basics
                                      PUBLIC juce::juce_recommended_config_flags)
endif()
