#pragma once

#include <JuceHeader.h>

// État partagé entre l'éditeur, l'écran EQ et Adrien (rafraîchi ~30 fois/s)
struct VisualState
{
    float freq = 60.0f;      // Hz
    float boost = 0.0f;      // 0..10
    float atten = 0.0f;      // 0..10
    float eqMix = 1.0f;      // 0..1
    float drive = 0.0f;      // 0..10
    float satMix = 1.0f;     // 0..1
    float outputDb = 0.0f;
    int satType = 0;         // 0 TAPE, 1 WARM, 2 FUZZ, 3 CRUSH, 4 DIODE
    bool highMode = false;
    float level = 0.0f;      // niveau de sortie lissé 0..1
    float bandLevel = 0.0f;  // activité de la bande saturée 0..1
    double sampleRate = 48000.0;
    double time = 0.0;       // secondes, pour les animations

    // 0..1 : intensité de la saturation telle qu'on la "voit"
    float heat() const { return juce::jlimit (0.0f, 1.0f, drive / 10.0f) * juce::jlimit (0.0f, 1.0f, satMix * 1.2f); }
};

namespace AdrienColours
{
    const juce::Colour bgDark   { 0xff15110d };
    const juce::Colour bgMid    { 0xff221b15 };
    const juce::Colour panel    { 0xff2b231c };
    const juce::Colour brass    { 0xffb8955c };
    const juce::Colour brassDim { 0xff6e5836 };
    const juce::Colour cream    { 0xffefe5cf };
    const juce::Colour creamDim { 0xffa89c86 };
    const juce::Colour amber    { 0xffffb347 };
    const juce::Colour hot      { 0xffe8553e };

    inline juce::Colour forSatType (int t)
    {
        switch (t)
        {
            case 0:  return juce::Colour (0xffd9a066); // tape : sépia
            case 1:  return juce::Colour (0xffff8a3d); // warm : orange
            case 2:  return juce::Colour (0xffe84a8a); // fuzz : magenta
            case 3:  return juce::Colour (0xff7fd46b); // crush : vert 8-bit
            default: return juce::Colour (0xff5ad1ff); // diode : électrique
        }
    }
}
