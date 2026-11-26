#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
static String freqLabel (float hz) { return String (roundToInt (hz)) + " Hz"; }

const float* AdrienAudioProcessor::freqSteps()
{
    static const float steps[numFreqSteps] = { 30, 40, 50, 60, 80, 100, 150, 200, 250, 300, 350, 400, 450 };
    return steps;
}

float AdrienAudioProcessor::freqForIndex (int index)
{
    return freqSteps()[jlimit (0, numFreqSteps - 1, index)];
}

AudioProcessorValueTreeState::ParameterLayout AdrienAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    StringArray freqNames;
    for (int i = 0; i < numFreqSteps; ++i)
        freqNames.add (freqLabel (freqSteps()[i]));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::freq, 1 }, "Frequency", freqNames, defaultFreqIndex()));

    auto zeroToTen = NormalisableRange<float> (0.0f, 10.0f, 0.01f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::boost, 1 }, "Boost", zeroToTen, 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return "+" + String (adrien::PultecDesign::boostMaxDb * v / 10.0, 1) + " dB"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::atten, 1 }, "Atten", zeroToTen, 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return "-" + String (adrien::PultecDesign::attenMaxDb * v / 10.0, 1) + " dB"; })));

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::eqMix, 1 }, "EQ Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::drive, 1 }, "Drive", zeroToTen, 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::satType, 1 }, "Saturation", satTypeNames(), 0));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::satMix, 1 }, "Sat Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::output, 1 }, "Output", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; })));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::autoGain, 1 }, "Auto Gain", true));

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
    pEqMix   = apvts.getRawParameterValue (ParamIDs::eqMix);
    pDrive   = apvts.getRawParameterValue (ParamIDs::drive);
    pSatType = apvts.getRawParameterValue (ParamIDs::satType);
    pSatMix  = apvts.getRawParameterValue (ParamIDs::satMix);
    pOutput  = apvts.getRawParameterValue (ParamIDs::output);
    pAutoGain = apvts.getRawParameterValue (ParamIDs::autoGain);
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
    sFreq.reset (sampleRate, ramp);   sFreq.setCurrentAndTargetValue (freqForIndex (roundToInt (pFreq->load())));
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
    agPre = agPost = 0.0;
    agGain = 1.0f;
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

    sFreq.setTargetValue (freqForIndex (roundToInt (pFreq->load())));
    sBoost.setTargetValue (pBoost->load());
    sAtten.setTargetValue (pAtten->load());
    sEqMix.setTargetValue (pEqMix->load() * 0.01f);
    sDrive.setTargetValue (pDrive->load());
    sSatMix.setTargetValue (pSatMix->load() * 0.01f);
    sOutput.setTargetValue (Decibels::decibelsToGain (pOutput->load()));

    const bool autoGainOn = pAutoGain->load() > 0.5f;
    // AUTO GAIN : constante de temps ~300 ms pour suivre l'énergie sans pomper
    const double agCoef = std::exp (-(double) chunkSize / (0.3 * fs));
    const int typeIndex = jlimit (0, 4, (int) std::round (pSatType->load()));
    const auto type = (adrien::SatType) typeIndex;
    if (typeIndex != lastSatType)
    {
        for (auto& s : sat) s.reset();
        lastSatType = typeIndex;
    }

    double outSq = 0.0, bandSq = 0.0;
    float peak = 0.0f;

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
        adrien::PultecDesign::design (fs, freq, boost, atten, kB, kA);

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

        // énergie avant la saturation (pour l'AUTO GAIN)
        double preSq = 0.0;
        for (int c = 0; c < numCh; ++c)
        {
            const auto* d = buffer.getReadPointer (c, start);
            for (int i = 0; i < n; ++i)
                preSq += (double) d[i] * d[i];
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

        // --- 3) AUTO GAIN : on ramène la sortie de la saturation au niveau d'avant ---
        double postSq = 0.0;
        for (int c = 0; c < numCh; ++c)
        {
            const auto* d = buffer.getReadPointer (c, start);
            for (int i = 0; i < n; ++i)
                postSq += (double) d[i] * d[i];
        }
        const double k = std::pow (agCoef, (double) n / chunkSize);
        agPre  = k * agPre  + (1.0 - k) * preSq  / (double) (n * numCh);
        agPost = k * agPost + (1.0 - k) * postSq / (double) (n * numCh);

        float agTarget = 1.0f;
        if (autoGainOn)
        {
            if (agPre > 1.0e-9 && agPost > 1.0e-12)
                agTarget = (float) jlimit (0.125, 8.0, std::sqrt (agPre / agPost));   // ±18 dB max
            else
                agTarget = agGain;   // silence : on garde la dernière correction
        }
        const float agStart = agGain;
        const float agStep = (agTarget - agStart) * 0.25f;   // glisse vers la cible sur ~4 tranches
        agGain = agStart + agStep;

        // --- 4) Sortie ---
        for (int i = 0; i < n; ++i)
        {
            const float g = sOutput.getNextValue() * (agStart + agStep * (float) (i + 1) / (float) n);
            for (int c = 0; c < numCh; ++c)
            {
                auto* d = buffer.getWritePointer (c, start);
                d[i] *= g;
                outSq += (double) d[i] * d[i];
                peak = jmax (peak, std::abs (d[i]));
            }
        }
    }

    // mesure pour l'UI (enveloppe simple : attaque rapide, relâche lente côté UI)
    const float rmsOut = (float) std::sqrt (outSq / (numSamples * numCh));
    const float rmsBand = (float) std::sqrt (bandSq / (numSamples * numCh));
    outputLevel.store (jmax (rmsOut, outputLevel.load() * 0.9f));
    bandLevel.store (jmax (rmsBand, bandLevel.load() * 0.9f));
    peakLevel.store (jmax (peak, peakLevel.load() * 0.9f));
    autoGainDb.store (Decibels::gainToDecibels (agGain));
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
