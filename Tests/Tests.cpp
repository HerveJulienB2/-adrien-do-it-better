// Tests hors DAW : paramètres, AUTO GAIN, réactions d'Adrien, et captures d'écran de l'interface.
// Lancement : ./AdrienTests [dossier_des_captures]
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Phrases.h"

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

static void setParam (AdrienAudioProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

// RMS de la sortie pour un sinus, mesuré après 1,5 s d'établissement
static float renderRms (AdrienAudioProcessor& p, float hz, float amp, double fs = 48000.0)
{
    p.prepareToPlay (fs, 512);
    AudioBuffer<float> buf (2, 512);
    MidiBuffer midi;
    double phase = 0.0, sumSq = 0.0;
    int counted = 0;
    const int blocks = (int) (2.5 * fs / 512);
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float v = amp * (float) std::sin (phase);
            phase += MathConstants<double>::twoPi * hz / fs;
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        p.processBlock (buf, midi);
        if (b * 512 > 1.5 * fs)
            for (int i = 0; i < 512; ++i) { sumSq += buf.getSample (0, i) * buf.getSample (0, i); ++counted; }
    }
    return (float) std::sqrt (sumSq / jmax (1, counted));
}

static void snapshot (EditorContent& c, const File& dir, const String& name)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    auto f = dir.getChildFile (name + ".png");
    f.deleteFile();
    FileOutputStream out (f);
    PNGImageFormat().writeImageToStream (img, out);
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File shots (argc > 1 ? String (argv[1]) : File::getCurrentWorkingDirectory().getFullPathName());
    shots.createDirectory();

    std::cout << "Paramètres" << std::endl;
    {
        AdrienAudioProcessor p;
        auto* f = dynamic_cast<AudioParameterChoice*> (p.apvts.getParameter (ParamIDs::freq));
        check (f != nullptr && f->choices.size() == 13, "FREQUENCY : 13 crans");
        check (f->choices[0] == "30 Hz" && f->choices[12] == "450 Hz", "FREQUENCY : de 30 à 450 Hz");
        check (f->getIndex() == 3 && f->getCurrentValueAsText() == "60 Hz", "FREQUENCY : 60 Hz par défaut");
        check (p.apvts.getParameter ("shape") == nullptr, "mode LOW/HIGH supprimé");
        check (p.apvts.getParameter (ParamIDs::autoGain) != nullptr
               && p.apvts.getRawParameterValue (ParamIDs::autoGain)->load() > 0.5f, "AUTO GAIN présent, activé par défaut");
    }

    std::cout << "Son" << std::endl;
    {
        AdrienAudioProcessor p;
        const float in = 0.125f / std::sqrt (2.0f);
        const float flat = renderRms (p, 100.0f, 0.125f);
        check (std::abs (Decibels::gainToDecibels (flat / in)) < 0.1f, "tout à zéro : transparent (" + String (Decibels::gainToDecibels (flat / in), 3) + " dB)");

        const auto names = AdrienAudioProcessor::satTypeNames();
        for (int t = 0; t < 5; ++t)
        {
            for (bool ag : { true, false })
            {
                AdrienAudioProcessor q;
                setParam (q, ParamIDs::satType, (float) t);
                setParam (q, ParamIDs::drive, 10.0f);
                setParam (q, ParamIDs::autoGain, ag ? 1.0f : 0.0f);
                const float r = renderRms (q, 100.0f, 0.125f);
                const float diff = Decibels::gainToDecibels (r / flat);
                if (ag)
                    check (std::abs (diff) < 1.0f, "AUTO GAIN on, " + names[t] + " DRIVE 10 : " + String (diff, 2) + " dB par rapport à DRIVE 0");
                else
                    std::cout << "  info  AUTO GAIN off, " << names[t] << " DRIVE 10 : " << String (diff, 2) << " dB" << std::endl;
            }
        }

        // les crans changent bien la fréquence : boost à 30 Hz vs 450 Hz mesuré à 30 Hz
        AdrienAudioProcessor lo, hi;
        for (auto* q : { &lo, &hi }) { setParam (*q, ParamIDs::boost, 10.0f); setParam (*q, ParamIDs::drive, 0.0f); }
        setParam (lo, ParamIDs::freq, 0.0f);
        setParam (hi, ParamIDs::freq, 12.0f);
        const float a = renderRms (lo, 30.0f, 0.05f), b = renderRms (hi, 30.0f, 0.05f);
        check (b > a, "BOOST : le cran 450 Hz pousse plus à 30 Hz que le cran 30 Hz (shelf)");
    }

    std::cout << "Interface et réactions d'Adrien" << std::endl;
    {
        AdrienAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        auto* editor = dynamic_cast<AdrienAudioProcessorEditor*> (ed.get());
        check (editor != nullptr && editor->getWidth() == 1120 && editor->getHeight() == 660, "éditeur 1120 x 660");
        check (editor->isResizable(), "fenêtre redimensionnable");
        auto& c = editor->getContent();
        c.tickForTest (3);
        snapshot (c, shots, "00_repos");

        struct Step { const char* id; float value; Reaction expect; const char* shot; };
        const Step steps[] = {
            { ParamIDs::freq,     0.0f,  Reaction::Freq,        "01_freq_30" },
            { ParamIDs::freq,     5.0f,  Reaction::Freq,        "02_freq_100" },
            { ParamIDs::boost,    8.0f,  Reaction::Boost,       "03_boost" },
            { ParamIDs::atten,    7.0f,  Reaction::Pultec,      "04_pultec" },
            { ParamIDs::boost,    0.0f,  Reaction::Boost,       nullptr },
            { ParamIDs::atten,    10.0f, Reaction::Atten,       "05_atten" },
            { ParamIDs::atten,    0.0f,  Reaction::Atten,       nullptr },
            { ParamIDs::eqMix,    40.0f, Reaction::EqMix,       "06_eqmix" },
            { ParamIDs::satType,  1.0f,  Reaction::SatType,     nullptr },
            { ParamIDs::drive,    8.0f,  Reaction::Drive,       "07_drive_warm" },
            { ParamIDs::satMix,   60.0f, Reaction::SatMix,      "08_satmix" },
            { ParamIDs::autoGain, 0.0f,  Reaction::AutoGainOff, "09_autogain_off" },
            { ParamIDs::autoGain, 1.0f,  Reaction::AutoGainOn,  "10_autogain_on" },
            { ParamIDs::output,   6.0f,  Reaction::OutputUp,    "11_output_up" },
            { ParamIDs::output,   -6.0f, Reaction::OutputDown,  "12_output_down" },
            { ParamIDs::satType,  2.0f,  Reaction::SatType,     "13_fuzz" },
            { ParamIDs::satType,  3.0f,  Reaction::SatType,     "14_crush" },
            { ParamIDs::satType,  4.0f,  Reaction::SatType,     "15_diode" },
        };
        for (auto& s : steps)
        {
            setParam (p, s.id, s.value);
            c.tickForTest (s.expect == Reaction::Freq ? 4 : 10);
            check (c.getVisualState().reaction == s.expect && c.getQuip().isNotEmpty(),
                   String (s.id) + " -> réaction " + String ((int) s.expect) + " : " + c.getQuip());
            if (s.shot != nullptr) snapshot (c, shots, s.shot);
        }

        // inactif 20 s : pause welsh ; silence : bâillement
        setParam (p, ParamIDs::drive, 0.0f);
        c.tickForTest (30 * 6);
        check (c.getVisualState().silent, "pas de son : il bâille (" + c.getQuip() + ")");
        snapshot (c, shots, "16_silence");
        c.tickForTest (30 * 16);
        check (c.getVisualState().idle && c.getQuip().contains ("welsh"), "20 s sans toucher : pause welsh (" + c.getQuip() + ")");
        snapshot (c, shots, "17_welsh");

        // redimensionnement
        editor->setSize (1680, 990);
        check (editor->getWidth() == 1680, "agrandie à 1680 x 990");
    }

    std::cout << "Phrases : " << Phrases::count() << std::endl;
    std::cout << (failures == 0 ? "TOUT EST OK" : String (failures) + " ECHEC(S)") << std::endl;
    return failures == 0 ? 0 : 1;
}
