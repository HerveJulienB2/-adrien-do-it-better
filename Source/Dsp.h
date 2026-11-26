#pragma once

#include <cmath>
#include <complex>
#include <algorithm>

// =====================================================================================
//  ADRIEN DO IT BETTER — briques DSP
//  - Biquads RBJ (low/high shelf, bell) pour le "Pultec trick"
//  - SVF TPT (Zavalishin) pour isoler la bande saturée autour de la fréquence
//  - 5 saturations : TAPE, WARM, FUZZ, CRUSH, DIODE
// =====================================================================================
namespace adrien
{
constexpr double pi = 3.14159265358979323846;

//==============================================================================
struct BiquadCoeffs
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    static BiquadCoeffs identity() { return {}; }

    static BiquadCoeffs lowShelf (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * pi * f / fs, c = std::cos (w), s = std::sin (w);
        const double alpha = s / (2.0 * q), sa = 2.0 * std::sqrt (A) * alpha;
        const double a0 = (A + 1) + (A - 1) * c + sa;
        BiquadCoeffs k;
        k.b0 = A * ((A + 1) - (A - 1) * c + sa) / a0;
        k.b1 = 2 * A * ((A - 1) - (A + 1) * c) / a0;
        k.b2 = A * ((A + 1) - (A - 1) * c - sa) / a0;
        k.a1 = -2 * ((A - 1) + (A + 1) * c) / a0;
        k.a2 = ((A + 1) + (A - 1) * c - sa) / a0;
        return k;
    }

    static BiquadCoeffs highShelf (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * pi * f / fs, c = std::cos (w), s = std::sin (w);
        const double alpha = s / (2.0 * q), sa = 2.0 * std::sqrt (A) * alpha;
        const double a0 = (A + 1) - (A - 1) * c + sa;
        BiquadCoeffs k;
        k.b0 = A * ((A + 1) + (A - 1) * c + sa) / a0;
        k.b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
        k.b2 = A * ((A + 1) + (A - 1) * c - sa) / a0;
        k.a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
        k.a2 = ((A + 1) - (A - 1) * c - sa) / a0;
        return k;
    }

    static BiquadCoeffs bell (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * pi * f / fs, c = std::cos (w);
        const double alpha = std::sin (w) / (2.0 * q);
        const double a0 = 1 + alpha / A;
        BiquadCoeffs k;
        k.b0 = (1 + alpha * A) / a0;
        k.b1 = -2 * c / a0;
        k.b2 = (1 - alpha * A) / a0;
        k.a1 = -2 * c / a0;
        k.a2 = (1 - alpha / A) / a0;
        return k;
    }

    std::complex<double> response (double fs, double f) const
    {
        const auto z1 = std::polar (1.0, -2.0 * pi * f / fs); // z^-1
        const auto z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }
};

struct Biquad
{
    double z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }

    inline float process (float xin, const BiquadCoeffs& k)
    {
        const double x = xin;
        const double y = k.b0 * x + z1;
        z1 = k.b1 * x - k.a1 * y + z2;
        z2 = k.b2 * x - k.a2 * y;
        return (float) y;
    }
};

//==============================================================================
//  Le "Pultec trick" : boost en shelf + atténuation en cloche décalée d'une octave.
//  Les deux réglés sur LA MÊME fréquence => bosse sous la fréquence, creux au-dessus.
//  V2 : seulement le grave (30 à 450 Hz), le mode HIGH a été retiré.
//==============================================================================
struct PultecDesign
{
    static constexpr double boostMaxDb = 13.5;   // potard 10 => +13.5 dB
    static constexpr double attenMaxDb = 10.0;   // potard 10 => -10 dB
    static constexpr double boostQ = 0.8;
    static constexpr double attenQ = 0.7;
    static constexpr double attenRatio = 2.0;    // le creux est une octave au-dessus

    static void design (double fs, double freq, double boost0to10, double atten0to10,
                        BiquadCoeffs& boostK, BiquadCoeffs& attenK)
    {
        const double nyq = fs * 0.5;
        const double f = std::clamp (freq, 10.0, nyq * 0.9);
        const double gB = boostMaxDb * std::clamp (boost0to10, 0.0, 10.0) / 10.0;
        const double gA = -attenMaxDb * std::clamp (atten0to10, 0.0, 10.0) / 10.0;

        boostK = BiquadCoeffs::lowShelf (fs, f, boostQ, gB);
        attenK = BiquadCoeffs::bell (fs, std::min (f * attenRatio, nyq * 0.9), attenQ, gA);
    }
};

//==============================================================================
//  SVF TPT : x = lp + k*bp + hp  => bande = k*bp, reste = x - bande (reconstruction parfaite)
//==============================================================================
struct Svf
{
    double ic1 = 0, ic2 = 0;
    double g = 0, k = 1.25, a1 = 0, a2 = 0, a3 = 0;

    void reset() { ic1 = ic2 = 0; }

    void setup (double fs, double f, double q)
    {
        f = std::clamp (f, 10.0, fs * 0.45);
        g = std::tan (pi * f / fs);
        k = 1.0 / q;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    // renvoie la bande (gain unitaire au centre)
    inline double band (double x)
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1;
        ic2 = 2 * v2 - ic2;
        return k * v1;
    }
};

//==============================================================================
enum class SatType { Tape = 0, Warm, Fuzz, Crush, Diode };

struct Saturator
{
    // état par canal
    double dcX = 0, dcY = 0;      // DC blocker
    double lp = 0;                // HF loss (tape)
    double hold = 0;              // crush : échantillon tenu
    double holdPhase = 0;

    void reset() { dcX = dcY = lp = hold = holdPhase = 0; }

    // x : signal déjà multiplié par le drive ; drive01 : 0..1 ; fs : fréquence (suréchantillonnée)
    inline double process (double x, SatType type, double drive01, double fs)
    {
        double y = 0;
        switch (type)
        {
            case SatType::Tape:
            {
                // tanh légèrement asymétrique + perte d'aigus qui augmente avec le drive
                const double bias = 0.12;
                y = std::tanh (x + bias) - std::tanh (bias);
                const double fc = 16000.0 - 9000.0 * drive01;
                const double a = std::exp (-2.0 * pi * fc / fs);
                lp = (1.0 - a) * y + a * lp;
                y = lp;
                break;
            }
            case SatType::Warm:
            {
                // triode : alternance positive douce, négative plus serrée => harmoniques paires
                y = x >= 0 ? 1.0 - std::exp (-x)
                           : -(1.0 - std::exp (1.6 * x)) / 1.6;
                break;
            }
            case SatType::Fuzz:
            {
                // gros gain, écrêtage dur arrondi + un peu de redressement (octave up)
                const double hot = x * 3.0;
                const double clipped = hot / std::pow (1.0 + std::pow (std::abs (hot), 6.0), 1.0 / 6.0);
                y = 0.8 * clipped + 0.35 * drive01 * (std::abs (clipped) - 0.5);
                break;
            }
            case SatType::Crush:
            {
                // réduction de bits + de fréquence d'échantillonnage
                const double bits = 12.0 - 9.0 * drive01;          // 12 -> 3 bits
                const double steps = std::pow (2.0, bits);
                const double targetRate = 24000.0 * std::pow (0.08, drive01); // 24 kHz -> ~2 kHz
                holdPhase += targetRate / fs;
                if (holdPhase >= 1.0)
                {
                    holdPhase -= std::floor (holdPhase);
                    const double soft = std::tanh (x);
                    hold = std::round (soft * steps) / steps;
                }
                y = hold;
                break;
            }
            case SatType::Diode:
            {
                // diode asymétrique : positif dur, négatif plus mou
                y = x >= 0 ? std::tanh (1.8 * x) / 1.8 * 1.4
                           : 0.55 * std::tanh (x / 0.55);
                break;
            }
        }

        // DC blocker (~10 Hz)
        const double R = 1.0 - (2.0 * pi * 10.0 / fs);
        const double out = y - dcX + R * dcY;
        dcX = y;
        dcY = out;
        return out;
    }
};

} // namespace adrien
