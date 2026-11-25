#pragma once

#include "VisualState.h"

//==============================================================================
//  Adrien en personne. Toutes les coordonnées "asset" sont celles des PNG embarqués
//  (photo d'origine x0.8). La vue applique ensuite son échelle + les animations.
//
//  - Boost  : il sourit et ses cheveux poussent
//  - Atten  : il fait la moue et perd ses cheveux (crâne qui brille)
//  - Freq   : son bras tendu pointe la fréquence sur l'écran
//  - Drive  : il chauffe, rougit, transpire, tremble ; chaque type de saturation a son filtre
//  - Audio  : il pulse avec le niveau de sortie
//==============================================================================
class AdrienView : public juce::Component
{
public:
    AdrienView();

    void setState (const VisualState& s);
    void setPointTarget (juce::Point<float> targetInParent);   // où le bras doit pointer
    juce::Point<float> getPalmInParent() const;                // pour le faisceau

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void drawAdrien (juce::Graphics& g, const juce::AffineTransform& t);
    void drawHair (juce::Graphics& g, const juce::AffineTransform& t);
    void drawFx (juce::Graphics& g, const juce::AffineTransform& t);
    void updateMouth (float smile);
    juce::AffineTransform currentTransform() const;
    juce::AffineTransform armTransform() const;

    void updateSilhouette();

    juce::Image body, arm, bald, mouthPatch, silhouette;
    float silAngle = 1000.0f;
    juce::Rectangle<int> mouthRoi;          // en coordonnées asset
    float lastSmile = 1000.0f;

    VisualState state;
    float baseScale = 0.56f;
    juce::Point<float> baseOffset { 6.0f, 16.0f };

    float armAngle = 0.0f, armTarget = 0.0f;   // radians
    float bob = 0.0f;                          // pompage audio lissé
    juce::Point<float> shake;
    juce::Random rng { 1234 };

    struct Strand { float t, len, curl, phase, shade, width; };
    std::vector<Strand> strands;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdrienView)
};
