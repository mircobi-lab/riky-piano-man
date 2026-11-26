#pragma once
#include "MBStudioMidi.h"
#include "Params.h"
#include "PianoEngine.h"
#include "SampleLibrary.h"
#include "Sequencer.h"
#include "Exporter.h"
#include "PostChain.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

class RikyProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    RikyProcessor();
    ~RikyProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- thread dei messaggi
    void loadPiano (const juce::File& sfz);          // un file .sfz: il Maestro o qualsiasi altro strumento SFZ
    void resetSfzControls();                         // SFZ CONTROLS ai valori scritti nel file
    void loadDemo();                                 // "Riky's Theme" nel sequencer
    bool exportSong (const juce::File& dest, riky::Exporter::Format);   // render offline del brano del sequencer
    void installPiano() { library.startInstall(); }

    float outputLevel() const noexcept { return level.load(); }
    void requestPanic() noexcept { engine.requestPanic(); }

    juce::AudioProcessorValueTreeState state;
    juce::MidiKeyboardState keyboardState;
    mbstudio::midi::MidiMapper midiMap;
    riky::PianoEngine engine;
    riky::SampleLibrary library;
    riky::Sequencer sequencer;
    riky::Exporter exporter;
    std::function<void()> onStatusChanged;           // per il pannello (thread dei messaggi)


private:
    std::atomic<float>* tone = nullptr, *tune = nullptr, *transpose = nullptr, *a4ref = nullptr, *velCurve = nullptr,
                       *quality = nullptr, *poly = nullptr, *temper = nullptr, *temperKey = nullptr,
                       *pedal = nullptr, *soft = nullptr, *softAmt = nullptr,
                       *roomOn = nullptr, *roomSize = nullptr, *roomDamp = nullptr, *roomMix = nullptr, *width = nullptr, *volume = nullptr;

    riky::PostChain post;
    double sr = 44100.0;
    std::atomic<float> level { 0.f };
    std::array<std::atomic<float>*, riky::numSlots> slots {};
    juce::String slotsFile;      // file a cui si riferiscono i valori degli SFZ CONTROLS (altrimenti si usano i valori del file)

    void timerCallback() override;
    void engineChanged();

    void notify();
    riky::EngineSettings engineSettings() const;
    riky::PostSettings postSettings() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RikyProcessor)
};
