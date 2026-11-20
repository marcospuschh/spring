#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SpringEngine.h"

class PTZRSpringProcessor : public juce::AudioProcessor
{
public:
    PTZRSpringProcessor();
    ~PTZRSpringProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    // Presets (de fábrica + do usuário)
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // usados pelo editor
    juce::String getPresetName() const;
    void loadPresetRelative (int delta);
    bool saveUserPreset (const juce::String& name);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> inPeak { 0.f }, outPeak { 0.f };

private:
    struct Preset
    {
        juce::String name;
        bool user = false;
        float v[8] {};            // mix, decay, tone, predelay, drive, type, damping, lowcut
        juce::File file;
    };

    void rebuildPresetList();
    void applyPreset (const Preset& p);
    void setParam (const juce::String& id, float actualValue);
    static juce::File userPresetDir();

    mutable juce::CriticalSection presetLock;
    std::vector<Preset> presets;
    int currentProgram = 0;

    ptzr::SpringReverbEngine engine;
    std::atomic<float>* pMix; std::atomic<float>* pDecay; std::atomic<float>* pTone; std::atomic<float>* pPre;
    std::atomic<float>* pDrive; std::atomic<float>* pType; std::atomic<float>* pDamp; std::atomic<float>* pLow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PTZRSpringProcessor)
};
