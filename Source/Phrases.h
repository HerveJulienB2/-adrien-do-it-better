#pragma once

#include "VisualState.h"

//==============================================================================
//  Les phrases d'Adrien, ingé son de la rue Doudeauville (validées par Julien, V2).
//  Une phrase est tirée au moment où le réglage change, puis reste affichée.
//==============================================================================
namespace Phrases
{
    inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

    inline juce::String pick (juce::Random& r, std::initializer_list<const char*> lines)
    {
        const int n = (int) lines.size();
        return u8 (*(lines.begin() + r.nextInt (n)));
    }

    // FREQUENCY : une phrase par cran quand il y en a une, sinon la bulle annonce la fréquence
    inline juce::String forFrequency (int hz)
    {
        switch (hz)
        {
            case 30:  return u8 ("30 Hz : tu le sens dans le ventre, pas dans les enceintes de ton laptop.");
            case 40:  return u8 ("40 Hz. La double pédale te dit merci.");
            case 60:  return u8 ("60 Hz, le kick classique. Comme le welsh du dimanche, on n'y touche pas.");
            case 100: return u8 ("100 Hz, là où le kick et la basse se battent. Choisis ton camp, biloute.");
            case 200: return u8 ("200 Hz, la zone du carton. Vas-y mollo.");
            case 300: return u8 ("300 Hz, ça sent la boue. Moi, je creuserais.");
            case 450: return u8 ("450 Hz ? T'es plus dans le grave, t'es dans le nasal, mon gars.");
            default:  return juce::String (hz) + " Hz.";
        }
    }

    inline juce::String forReaction (Reaction r, const VisualState& s, juce::Random& rnd)
    {
        switch (r)
        {
            case Reaction::Freq:
                return forFrequency (juce::roundToInt (s.freq));

            case Reaction::Boost:
                if (s.boost >= 9.95f) return u8 ("Boost à 10 ? T'as cru que c'était le Hellfest ?");
                if (s.boost < 0.05f)  return u8 ("Boost à zéro : le grave reste comme il est. Sage.");
                return pick (rnd, { "Ça gonfle comme un welsh qui gratine.",
                                    "Du grave, biloute ! Le kick va enfin exister." });

            case Reaction::Atten:
                if (s.atten >= 9.95f) return u8 ("Atten à fond… tu veux me rendre chauve ou quoi ?");
                if (s.atten < 0.05f)  return u8 ("Atten à zéro : je creuse plus rien, on garde tout.");
                return pick (rnd, { "Je creuse juste au-dessus. Le Pultec trick, à l'ancienne.",
                                    "On enlève la boue. Comme sur mes rangers après le Hellfest." });

            case Reaction::Pultec:
                return pick (rnd, { "Bosse et creux en même temps : c'est la recette. Cheddar ET bière.",
                                    "Le Pultec trick. Classique. Même un ingé de la rue Doudeauville le sait." });

            case Reaction::EqMix:
                if (s.eqMix < 0.005f) return u8 ("Wet à 0 ? Alors pourquoi t'as chargé le plugin ?");
                return u8 ("EQ en parallèle : on garde l'original, on ajoute la couleur.");

            case Reaction::SatType:
                switch (s.satType)
                {
                    case 0:  return u8 ("Bande magnétique : chaud, rond, un peu de pleurage. Comme mon oncle à la braderie de Lille.");
                    case 1:  return u8 ("Chaud comme une baraque à frites en décembre.");
                    case 2:  return u8 ("Fuzz ! Mes cheveux font du doom.");
                    case 3:  return u8 ("8 bits. Comme ma première démo de black metal enregistrée au dictaphone.");
                    default: return u8 ("Diode : ça grille. Comme le cheddar sous le gril.");
                }

            case Reaction::Drive:
                if (s.drive < 3.5f) return u8 ("Un peu de chaleur, comme une bistouille dans le café.");
                if (s.drive < 7.0f) return u8 ("Ça chauffe. Je transpire comme dans le métro à Marcadet en août.");
                return pick (rnd, { "Drive à 10 : ça sature plus qu'un ampli de grind dans une cave du 18e.",
                                    "Là, c'est plus du mix, c'est de la torture. J'adore." });

            case Reaction::SatMix:
                return u8 ("Saturation parallèle : la crasse sans perdre l'attaque.");

            case Reaction::AutoGainOn:
                return u8 ("Auto gain : on compare à volume égal. Plus fort, c'est pas mieux, c'est juste plus fort.");

            case Reaction::AutoGainOff:
                return u8 ("Sans auto gain, tu vas croire que c'est mieux juste parce que c'est plus fort. Les oreilles, ça se fait avoir.");

            case Reaction::OutputUp:
                return u8 ("Attention, biloute : 0 dBFS, c'est un mur, pas une suggestion.");

            case Reaction::OutputDown:
                return u8 ("On garde de la marge. Le mastering te dira merci.");

            case Reaction::Reset:
                return u8 ("Retour à zéro. Parfois, le meilleur réglage, c'est pas de réglage.");

            case Reaction::None:
            default:
                return {};
        }
    }

    inline juce::String clip()   { return u8 ("Ça clippe ! Même un growl ne passe pas à +3 dBFS."); }
    inline juce::String loud()   { return u8 ("Ça envoie ! Headbang autorisé."); }
    inline juce::String silent() { return u8 ("Rien ne passe. T'as oublié d'armer la piste ?"); }

    // Rien touché depuis 20 s : on fait tourner ces phrases
    inline juce::String idle (int n)
    {
        static const char* lines[] = {
            "Pause welsh. Tu règles, je mange.",
            "Un vrai ingé son écoute d'abord et tourne les boutons après.",
            "Passe ton mix en mono, tu vas découvrir ta phase.",
            "T'as mixé au casque Bluetooth, avoue.",
            "Rue Doudeauville, 3 h du mat, et toujours pas de kick qui tape."
        };
        return u8 (lines[((n % 5) + 5) % 5]);
    }

    // toutes les phrases (pour les tests)
    inline int count() { return 42; }
}
