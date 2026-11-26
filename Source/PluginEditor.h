#pragma once

#include "PluginProcessor.h"
#include "AdrienView.h"
#include "BraunLookAndFeel.h"

//==============================================================================
// Bouton rotatif façon Pittsburgh Modular (capuchon mat coloré) avec son titre et sa valeur.
// Le double-clic remet la valeur par défaut et fait cligner Adrien de l'œil.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text,
          juce::Colour cap = juce::Colour (0xff1d1f20), int ticks = 10);

    void resized() override;

    std::function<void()> onDoubleClick;

private:
    struct ResetSlider : public juce::Slider
    {
        std::function<void()> onDoubleClick;
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            juce::Slider::mouseDoubleClick (e);
            if (onDoubleClick) onDoubleClick();
        }
    };

    juce::Label title;
    ResetSlider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
// Les 13 crans de fréquence, lisibles d'un coup d'œil.
// Un clic sur un cran le sélectionne.
class FreqStrip : public juce::Component
{
public:
    explicit FreqStrip (juce::RangedAudioParameter& freqParam);

    void setIndex (int i) { if (i != index) { index = i; repaint(); } }
    juce::Point<float> getCellCentreInParent (int i) const;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> cell (int i) const;

    juce::ParameterAttachment attachment;
    int index = 3;
};

//==============================================================================
// Contenu de taille logique fixe : l'éditeur le met simplement à l'échelle.
class EditorContent : public juce::Component,
                      private juce::Timer
{
public:
    static constexpr int W = 1100, H = 660;

    explicit EditorContent (AdrienAudioProcessor&);
    ~EditorContent() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    // pour les tests
    const VisualState& getVisualState() const { return vs; }
    const juce::String& getQuip() const { return quip; }
    void tickForTest (int frames) { for (int i = 0; i < frames; ++i) timerCallback(); }

private:
    void timerCallback() override;
    void trigger (Reaction r);

    AdrienAudioProcessor& proc;
    juce::Image noise;

    Knob freq, boost, atten, eqMix, drive, satMix, output;
    FreqStrip strip;
    juce::ToggleButton typeKeys[5], autoGainKey;
    std::unique_ptr<juce::ParameterAttachment> typeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;
    AdrienView adrien;

    VisualState vs, prev;
    bool firstFrame = true, resetPending = false;
    float levelSmooth = 0.0f, bandSmooth = 0.0f;
    float clipHold = 0.0f, silentFor = 0.0f, idleFor = 0.0f;
    juce::int64 reactionSeed = 1;

    juce::String quip, lastReactionQuip;
    float quipAge = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorContent)
};

//==============================================================================
class AdrienAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AdrienAudioProcessorEditor (AdrienAudioProcessor&);
    ~AdrienAudioProcessorEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    EditorContent& getContent() { return content; }

private:
    BraunLookAndFeel laf;
    EditorContent content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdrienAudioProcessorEditor)
};
