#pragma once

#include "VisualState.h"

//==============================================================================
//  Adrien en personne. Toutes les coordonnées "asset" sont celles des PNG embarqués
//  (photo d'origine x0.8). En V2, la photo est retournée : Adrien est en bas à droite
//  et tend le bras vers la gauche, vers les crans de fréquence.
//
//  Chaque réglage déclenche une réaction (voir Reaction dans VisualState.h) :
//  sourire, moue, sursaut, pouce levé, casque, main à l'oreille, haussement d'épaules,
//  mains sur les oreilles, headbang et cornes, panneau CLIP, bâillement, pause welsh,
//  clin d'œil, et les effets de saturation de la V1.
//==============================================================================
class AdrienView : public juce::Component
{
public:
    AdrienView();

    void setState (const VisualState& s);
    void setPointTarget (juce::Point<float> targetInParent);   // où le bras doit pointer
    juce::Point<float> getPalmInParent() const;                // pour le faisceau
    juce::Point<float> getHeadTopInParent() const;             // pour la bulle

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void drawAdrien (juce::Graphics& g, const juce::AffineTransform& t);
    void drawHair (juce::Graphics& g, const juce::AffineTransform& t);
    void drawFx (juce::Graphics& g, const juce::AffineTransform& t);
    void drawReactions (juce::Graphics& g, const juce::AffineTransform& t);
    void updateMouth (float smile);
    juce::AffineTransform currentTransform() const;
    juce::AffineTransform armTransform() const;

    void updateSilhouette();

    juce::Image body, arm, mouthPatch, silhouette;
    float silAngle = 1000.0f;
    juce::Rectangle<int> mouthRoi;          // en coordonnées asset
    float lastSmile = 1000.0f;

    VisualState state;
    float baseScale = 0.36f;
    juce::Point<float> baseOffset { 0.0f, 0.0f };

    float armAngle = 0.0f, armTarget = 0.0f;   // radians
    float bob = 0.0f;                          // pompage audio lissé
    juce::Point<float> shake;
    juce::Random rng { 1234 };

    struct Strand { float t, len, curl, phase, shade, width; };
    std::vector<Strand> strands;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdrienView)
};
