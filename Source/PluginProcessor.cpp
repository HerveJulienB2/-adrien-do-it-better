#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
static String freqToText (float v, int)
{
    return v < 1000.0f ? String (v, v < 100.0f ? 1 : 0) + " Hz"
                       : String (v / 1000.0f, v < 10000.0f ? 2 : 1) + " kHz";
}

static float textToFreq (const String& t)
{
    auto s = t.trim().toLowerCase();
    float v = s.getFloatValue();
    if (s.contains ("k")) v *= 1000.0f;
    return v;
}

AudioProcessorValueTreeState::ParameterLayout AdrienAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    NormalisableRange<float> freqRange (20.0f, 16000.0f,
        [] (float s, float e, float n) { return s * std::pow (e / s, n); },
        [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); });

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::freq, 1 }, "Frequency", freqRange, 60.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (freqToText)
                                       .withValueFromStringFunction (textToFreq)));

    auto zeroToTen = NormalisableRange<float> (0.0f, 10.0f, 0.01f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::boost, 1 }, "Boost", zeroToTen, 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return "+" + String (adrien::PultecDesign::boostMaxDb * v / 10.0, 1) + " dB"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::atten, 1 }, "Atten", zeroToTen, 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return "-" + String (adrien::PultecDesign::attenMaxDb * v / 10.0, 1) + " dB"; })));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::shape, 1 }, "Shape", StringArray { "LOW", "HIGH" }, 0));

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::eqMix, 1 }, "EQ Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::drive, 1 }, "Drive", zeroToTen, 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::satType, 1 }, "Saturation", satTypeNames(), 0));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::satMix, 1 }, "Sat Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::output, 1 }, "Output", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; })));

    return { p.begin(), p.end() };
}

//==============================================================================
AdrienAudioProcessor::AdrienAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ADRIEN", createLayout())
{
    pFreq    = apvts.getRawParameterValue (ParamIDs::freq);
    pBoost   = apvts.getRawParameterValue (ParamIDs::boost);
    pAtten   = apvts.getRawParameterValue (ParamIDs::atten);
    pShape   = apvts.getRawParameterValue (ParamIDs::shape);
    pEqMix   = apvts.getRawParameterValue (ParamIDs::eqMix);
    pDrive   = apvts.getRawParameterValue (ParamIDs::drive);
    pSatType = apvts.getRawParameterValue (ParamIDs::satType);
    pSatMix  = apvts.getRawParameterValue (ParamIDs::satMix);
    pOutput  = apvts.getRawParameterValue (ParamIDs::output);
}

bool AdrienAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void AdrienAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;

    // x4 (2 étages), filtres IIR polyphasés : peu de latence, pas d'aliasing sur la saturation
    oversampler = std::make_unique<dsp::Oversampling<float>> ((size_t) maxChannels, 2,
                      dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampler->initProcessing ((size_t) chunkSize);
    setLatencySamples (roundToInt (oversampler->getLatencyInSamples()));

    const double ramp = 0.05;
    sFreq.reset (sampleRate, ramp);   sFreq.setCurrentAndTargetValue (pFreq->load());
    for (auto* s : { &sBoost, &sAtten, &sEqMix, &sDrive, &sSatMix, &sOutput })
        s->reset (sampleRate, ramp);
    sBoost.setCurrentAndTargetValue (pBoost->load());
    sAtten.setCurrentAndTargetValue (pAtten->load());
    sEqMix.setCurrentAndTargetValue (pEqMix->load() * 0.01f);
    sDrive.setCurrentAndTargetValue (pDrive->load());
    sSatMix.setCurrentAndTargetValue (pSatMix->load() * 0.01f);
    sOutput.setCurrentAndTargetValue (Decibels::decibelsToGain (pOutput->load()));

    for (int c = 0; c < maxChannels; ++c)
    {
        boostFilt[c].reset(); attenFilt[c].reset();
        svf[c].reset(); sat[c].reset();
    }
}

void AdrienAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int numCh = jmin (buffer.getNumChannels(), maxChannels);
    const int numSamples = buffer.getNumSamples();
    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);
    if (numCh == 0 || numSamples == 0 || oversampler == nullptr)
        return;

    const double fs = currentSampleRate;
    const double osFactor = (double) oversampler->getOversamplingFactor();
    const double fsOs = fs * osFactor;

    sFreq.setTargetValue (pFreq->load());
    sBoost.setTargetValue (pBoost->load());
    sAtten.setTargetValue (pAtten->load());
    sEqMix.setTargetValue (pEqMix->load() * 0.01f);
    sDrive.setTargetValue (pDrive->load());
    sSatMix.setTargetValue (pSatMix->load() * 0.01f);
    sOutput.setTargetValue (Decibels::decibelsToGain (pOutput->load()));

    const bool highMode = pShape->load() > 0.5f;
    const int typeIndex = jlimit (0, 4, (int) std::round (pSatType->load()));
    const auto type = (adrien::SatType) typeIndex;
    if (typeIndex != lastSatType)
    {
        for (auto& s : sat) s.reset();
        lastSatType = typeIndex;
    }

    double outSq = 0.0, bandSq = 0.0;

    for (int start = 0; start < numSamples; start += chunkSize)
    {
        const int n = jmin (chunkSize, numSamples - start);

        // --- paramètres lissés, mis à jour par tranche de 32 échantillons ---
        const double freq  = sFreq.skip (n);
        const double boost = sBoost.skip (n);
        const double atten = sAtten.skip (n);
        const float  eqMix = sEqMix.skip (n);
        const double drive = sDrive.skip (n);
        const double satMix = sSatMix.skip (n);

        adrien::BiquadCoeffs kB, kA;
        adrien::PultecDesign::design (fs, freq, boost, atten, highMode, kB, kA);

        // --- 1) Pultec trick + EQ dry/wet ---
        for (int c = 0; c < numCh; ++c)
        {
            auto* d = buffer.getWritePointer (c, start);
            for (int i = 0; i < n; ++i)
            {
                const float x = d[i];
                const float y = attenFilt[c].process (boostFilt[c].process (x, kB), kA);
                d[i] = x + eqMix * (y - x);
            }
        }

        // --- 2) Saturation de la bande autour de la fréquence (suréchantillonnée) ---
        float* chans[maxChannels] = { buffer.getWritePointer (0, start),
                                      numCh > 1 ? buffer.getWritePointer (1, start) : nullptr };
        dsp::AudioBlock<float> block (chans, (size_t) numCh, (size_t) n);
        auto up = oversampler->processSamplesUp (block);

        const double drive01 = drive / 10.0;
        const double gain = Decibels::decibelsToGain (drive * 3.0);      // 0 .. +30 dB dans la bande
        const double makeup = std::pow (gain, -0.65);
        const double effMix = satMix * jlimit (0.0, 1.0, drive / 0.6);    // drive 0 = transparent
        const double svfQ = 0.8;                                         // ~1.7 octave autour de la fréquence

        for (int c = 0; c < numCh; ++c)
        {
            svf[c].setup (fsOs, freq, svfQ);
            auto* d = up.getChannelPointer ((size_t) c);
            const int nUp = (int) up.getNumSamples();
            for (int i = 0; i < nUp; ++i)
            {
                const double x = d[i];
                const double band = svf[c].band (x);
                const double wet = sat[c].process (band * gain, type, drive01, fsOs) * makeup;
                bandSq += wet * wet * effMix / osFactor;
                d[i] = (float) (x + effMix * (wet - band));
            }
        }

        oversampler->processSamplesDown (block);

        // --- 3) Sortie ---
        for (int i = 0; i < n; ++i)
        {
            const float g = sOutput.getNextValue();
            for (int c = 0; c < numCh; ++c)
            {
                auto* d = buffer.getWritePointer (c, start);
                d[i] *= g;
                outSq += (double) d[i] * d[i];
            }
        }
    }

    // mesure pour l'UI (enveloppe simple : attaque rapide, relâche lente côté UI)
    const float rmsOut = (float) std::sqrt (outSq / (numSamples * numCh));
    const float rmsBand = (float) std::sqrt (bandSq / (numSamples * numCh));
    outputLevel.store (jmax (rmsOut, outputLevel.load() * 0.9f));
    bandLevel.store (jmax (rmsBand, bandLevel.load() * 0.9f));
}

//==============================================================================
AudioProcessorEditor* AdrienAudioProcessor::createEditor()
{
    return new AdrienAudioProcessorEditor (*this);
}

void AdrienAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AdrienAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AdrienAudioProcessor();
}
