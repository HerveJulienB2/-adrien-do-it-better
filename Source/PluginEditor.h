#pragma once

#include "PluginProcessor.h"
#include "AdrienView.h"
#include "EqScreen.h"

//==============================================================================
class AdrienLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AdrienLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
};

//==============================================================================
class AdrienAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit AdrienAudioProcessorEditor (AdrienAudioProcessor&);
    ~AdrienAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    // pour les tests : état visuel courant
    const VisualState& getVisualState() const { return vs; }
    void tickForTest (int frames) { for (int i = 0; i < frames; ++i) timerCallback(); }

private:
    void timerCallback() override;
    juce::String pickQuip() const;
    void setupKnob (juce::Slider&, const char* paramId, const juce::String& name);

    AdrienAudioProcessor& proc;
    AdrienLookAndFeel lnf;

    AdrienView adrien;
    EqScreen screen;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    juce::Slider freq, boost, atten, eqMix, drive, satMix, output;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::pair<juce::Slider*, juce::String>> knobNames;

    juce::TextButton typeButtons[5], shapeButtons[2];
    std::unique_ptr<juce::ParameterAttachment> typeAttachment, shapeAttachment;

    VisualState vs;
    float levelSmooth = 0.0f, bandSmooth = 0.0f;

    juce::String quip;
    float quipAge = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdrienAudioProcessorEditor)
};
