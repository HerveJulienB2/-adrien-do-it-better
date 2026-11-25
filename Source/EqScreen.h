#pragma once

#include "VisualState.h"

//==============================================================================
//  L'écran qu'Adrien présente : courbe du Pultec trick, bande saturée, niveau.
//  Glisser horizontalement / molette = fréquence. Double-clic = 60 Hz.
//==============================================================================
class EqScreen : public juce::Component
{
public:
    explicit EqScreen (juce::RangedAudioParameter& freqParam);

    void setState (const VisualState& s);
    juce::Point<float> getMarkerInParent() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotArea() const;
    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    void setFreqFromX (float x);

    juce::RangedAudioParameter& freqParam;
    juce::ParameterAttachment freqAttachment;

    VisualState state;
    juce::Path curve;
    juce::Point<float> marker;
    bool hover = false, dragging = false;

    static constexpr float minF = 20.0f, maxF = 20000.0f, dbRange = 16.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqScreen)
};
