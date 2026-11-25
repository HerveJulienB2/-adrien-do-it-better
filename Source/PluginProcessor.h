#pragma once

#include <JuceHeader.h>
#include "Dsp.h"

namespace ParamIDs
{
    inline constexpr auto freq    = "freq";
    inline constexpr auto boost   = "boost";
    inline constexpr auto atten   = "atten";
    inline constexpr auto shape   = "shape";
    inline constexpr auto eqMix   = "eqmix";
    inline constexpr auto drive   = "drive";
    inline constexpr auto satType = "sattype";
    inline constexpr auto satMix  = "satmix";
    inline constexpr auto output  = "output";
}

class AdrienAudioProcessor : public juce::AudioProcessor
{
public:
    AdrienAudioProcessor();
    ~AdrienAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ADRIEN DO IT BETTER"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::StringArray satTypeNames() { return { "TAPE", "WARM", "FUZZ", "CRUSH", "DIODE" }; }

    juce::AudioProcessorValueTreeState apvts;

    // pour l'UI (lus par le timer de l'éditeur)
    std::atomic<float> outputLevel { 0.0f };   // RMS de sortie (linéaire)
    std::atomic<float> bandLevel   { 0.0f };   // RMS de la bande saturée
    double getCurrentSampleRate() const { return currentSampleRate; }

private:
    static constexpr int chunkSize = 32;
    static constexpr int maxChannels = 2;

    std::atomic<float>* pFreq = nullptr;
    std::atomic<float>* pBoost = nullptr;
    std::atomic<float>* pAtten = nullptr;
    std::atomic<float>* pShape = nullptr;
    std::atomic<float>* pEqMix = nullptr;
    std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pSatType = nullptr;
    std::atomic<float>* pSatMix = nullptr;
    std::atomic<float>* pOutput = nullptr;

    double currentSampleRate = 44100.0;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> sFreq;
    juce::SmoothedValue<float> sBoost, sAtten, sEqMix, sDrive, sSatMix, sOutput;

    adrien::Biquad boostFilt[maxChannels], attenFilt[maxChannels];
    adrien::Svf svf[maxChannels];
    adrien::Saturator sat[maxChannels];
    int lastSatType = 0;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdrienAudioProcessor)
};
