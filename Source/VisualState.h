#pragma once

#include <JuceHeader.h>

// Ce qu'Adrien vient de voir changer (une réaction par réglage touché)
enum class Reaction
{
    None,
    Freq,          // FREQUENCY : sursaut, le bras pointe le cran
    Boost,         // sourire
    Atten,         // moue
    Pultec,        // BOOST et ATTEN hauts : pouce levé
    EqMix,         // main à l'oreille, tête penchée
    SatType,       // effet du type de saturation
    Drive,         // il chauffe
    SatMix,        // il hoche la tête
    AutoGainOn,    // casque + pouce levé
    AutoGainOff,   // haussement d'épaules
    OutputUp,      // il recule, mains sur les oreilles au-delà de 0 dB
    OutputDown,    // il se penche pour mieux entendre
    Reset          // double-clic : clin d'œil
};

// État partagé entre l'éditeur et Adrien (rafraîchi ~30 fois/s)
struct VisualState
{
    int freqIndex = 3;       // cran 0..12
    float freq = 60.0f;      // Hz
    float boost = 0.0f;      // 0..10
    float atten = 0.0f;      // 0..10
    float eqMix = 1.0f;      // 0..1
    float drive = 0.0f;      // 0..10
    float satMix = 1.0f;     // 0..1
    float outputDb = 0.0f;
    int satType = 0;         // 0 TAPE, 1 WARM, 2 FUZZ, 3 CRUSH, 4 DIODE
    bool autoGain = true;
    float level = 0.0f;      // niveau de sortie lissé (RMS linéaire)
    float bandLevel = 0.0f;  // activité de la bande saturée
    double time = 0.0;       // secondes, pour les animations

    Reaction reaction = Reaction::None;
    float reactionAge = 99.0f;   // secondes depuis la dernière réaction

    bool clipping = false;   // la sortie a touché 0 dBFS récemment
    bool loud = false;       // gros niveau : headbang
    bool silent = false;     // aucun son depuis un moment : il bâille
    bool idle = false;       // rien touché depuis 20 s : pause welsh

    // 0..1 : intensité de la saturation telle qu'on la "voit"
    float heat() const { return juce::jlimit (0.0f, 1.0f, drive / 10.0f) * juce::jlimit (0.0f, 1.0f, satMix * 1.2f); }

    // 0..1 : force d'une réaction qui apparaît puis s'efface (fondu d'entrée + tenue + fondu de sortie)
    float reactionAmount (Reaction r, float hold = 2.5f) const
    {
        if (reaction != r) return 0.0f;
        const float in = juce::jlimit (0.0f, 1.0f, reactionAge / 0.15f);
        const float out = juce::jlimit (0.0f, 1.0f, (hold + 0.5f - reactionAge) / 0.5f);
        return in * out;
    }
};

namespace AdrienColours
{
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

    const juce::Colour hot  { 0xffe8553e };
    const juce::Colour skin { 0xffd8a283 };
}
