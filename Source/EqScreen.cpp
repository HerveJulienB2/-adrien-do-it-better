#include "EqScreen.h"
#include "Dsp.h"

using namespace juce;

EqScreen::EqScreen (RangedAudioParameter& p)
    : freqParam (p), freqAttachment (p, [] (float) {}, nullptr)
{
    setMouseCursor (MouseCursor::LeftRightResizeCursor);
}

void EqScreen::resized() {}

Rectangle<float> EqScreen::plotArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f).withTrimmedLeft (22.0f).withTrimmedBottom (16.0f).withTrimmedTop (16.0f);
}

float EqScreen::xForFreq (float f) const
{
    const auto r = plotArea();
    return r.getX() + r.getWidth() * std::log (f / minF) / std::log (maxF / minF);
}

float EqScreen::freqForX (float x) const
{
    const auto r = plotArea();
    return minF * std::pow (maxF / minF, jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth()));
}

float EqScreen::yForDb (float db) const
{
    const auto r = plotArea();
    return r.getCentreY() - (db / dbRange) * r.getHeight() * 0.5f;
}

Point<float> EqScreen::getMarkerInParent() const
{
    return marker + getPosition().toFloat();
}

//==============================================================================
void EqScreen::setState (const VisualState& s)
{
    state = s;

    const auto r = plotArea();
    curve.clear();
    const int n = (int) r.getWidth();
    for (int i = 0; i <= n; ++i)
    {
        const float x = r.getX() + (float) i;
        const float f = freqForX (x);
        const float db = (float) adrien::eqResponseDb (s.sampleRate, s.freq, s.boost, s.atten, s.highMode, s.eqMix, f);
        const float y = jlimit (r.getY(), r.getBottom(), yForDb (db));
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }

    const float mdb = (float) adrien::eqResponseDb (s.sampleRate, s.freq, s.boost, s.atten, s.highMode, s.eqMix, s.freq);
    marker = { xForFreq (s.freq), jlimit (r.getY(), r.getBottom(), yForDb (mdb)) };
    repaint();
}

//==============================================================================
void EqScreen::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto typeCol = AdrienColours::forSatType (state.satType);
    const float heat = state.heat();

    // cadre laiton + verre
    g.setColour (AdrienColours::brassDim);
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (AdrienColours::brass.withAlpha (0.6f));
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

    const auto glass = b.reduced (5.0f);
    ColourGradient bg (Colour (0xff1b130b), glass.getCentreX(), glass.getCentreY(),
                       Colour (0xff070504), glass.getX(), glass.getY(), true);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (glass, 7.0f);

    const auto r = plotArea();
    const auto amber = AdrienColours::amber;
    const auto gridCol = amber.withAlpha (0.10f);
    const auto labelCol = amber.withAlpha (0.45f);
    g.setFont (FontOptions (10.0f));

    // grille fréquences
    const float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (auto f : freqs)
    {
        const float x = xForFreq (f);
        g.setColour (gridCol);
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setColour (labelCol);
        const String txt = f >= 1000 ? String ((int) (f / 1000)) + "k" : String ((int) f);
        g.drawText (txt, Rectangle<float> (x - 16, r.getBottom() + 2, 32, 12), Justification::centred);
    }
    // grille dB
    for (int db = -12; db <= 12; db += 6)
    {
        const float y = yForDb ((float) db);
        g.setColour (db == 0 ? amber.withAlpha (0.22f) : gridCol);
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        g.setColour (labelCol);
        g.drawText ((db > 0 ? "+" : "") + String (db), Rectangle<float> (glass.getX() + 2, y - 6, 26, 12), Justification::centredRight);
    }

    // zone saturée autour de la fréquence
    if (heat > 0.01f)
    {
        const float lo = xForFreq (jmax (minF, state.freq / 1.9f));
        const float hi = xForFreq (jmin (maxF, state.freq * 1.9f));
        const float a = 0.12f + 0.25f * heat + 0.3f * jmin (1.0f, state.bandLevel * 4.0f);
        ColourGradient zone (typeCol.withAlpha (0.0f), lo, 0, typeCol.withAlpha (0.0f), hi, 0, false);
        zone.addColour (0.5, typeCol.withAlpha (jmin (0.55f, a)));
        g.setGradientFill (zone);
        g.fillRect (Rectangle<float>::leftTopRightBottom (lo, r.getY(), hi, r.getBottom()));

        g.setColour (typeCol.withAlpha (0.9f));
        g.setFont (FontOptions (10.0f, Font::bold));
        g.drawText ("SAT " + StringArray { "TAPE", "WARM", "FUZZ", "CRUSH", "DIODE" }[state.satType],
                    Rectangle<float> (lo, r.getY() + 2, hi - lo, 12), Justification::centred);
    }

    // remplissage sous la courbe
    {
        Path fill (curve);
        fill.lineTo (r.getRight(), yForDb (0));
        fill.lineTo (r.getX(), yForDb (0));
        fill.closeSubPath();
        g.setColour (amber.withAlpha (0.10f));
        g.fillPath (fill);
    }

    // courbe avec halo
    g.setColour (amber.withAlpha (0.18f));
    g.strokePath (curve, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (amber.withAlpha (0.35f));
    g.strokePath (curve, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colour (0xffffe2b0));
    g.strokePath (curve, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));

    // repère de fréquence
    {
        const float dash[] = { 3.0f, 4.0f };
        g.setColour (amber.withAlpha (0.35f));
        g.drawDashedLine (Line<float> (marker.x, r.getY(), marker.x, r.getBottom()), dash, 2, 1.0f);

        const float rad = dragging ? 7.5f : (hover ? 6.5f : 5.5f);
        g.setColour (amber.withAlpha (0.3f));
        g.fillEllipse (Rectangle<float> (rad * 3.2f, rad * 3.2f).withCentre (marker));
        g.setColour (Colour (0xffffe2b0));
        g.fillEllipse (Rectangle<float> (rad * 2, rad * 2).withCentre (marker));
        g.setColour (Colour (0xff1b130b));
        g.fillEllipse (Rectangle<float> (rad * 0.8f, rad * 0.8f).withCentre (marker));

        const auto txt = freqParam.getCurrentValueAsText();
        g.setFont (FontOptions (12.0f, Font::bold));
        const float tw = 64.0f;
        auto label = Rectangle<float> (marker.x - tw * 0.5f, marker.y - 28.0f, tw, 16.0f);
        if (label.getY() < r.getY()) label.setY (marker.y + 12.0f);
        label = label.constrainedWithin (r);
        g.setColour (Colour (0xcc1b130b));
        g.fillRoundedRectangle (label, 4.0f);
        g.setColour (Colour (0xffffe2b0));
        g.drawText (txt, label, Justification::centred);
    }

    // vumètre de sortie (en haut à droite)
    {
        const auto m = Rectangle<float> (r.getRight() - 96.0f, glass.getY() + 5.0f, 90.0f, 6.0f);
        const float lvl = jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (state.level, -60.0f) + 48.0f) / 48.0f);
        g.setColour (amber.withAlpha (0.12f));
        g.fillRoundedRectangle (m, 3.0f);
        ColourGradient vg (amber.withAlpha (0.8f), m.getX(), 0, AdrienColours::hot, m.getRight(), 0, false);
        g.setGradientFill (vg);
        g.fillRoundedRectangle (m.withWidth (m.getWidth() * lvl), 3.0f);
        g.setColour (labelCol);
        g.setFont (FontOptions (9.0f));
        g.drawText ("OUT", m.translated (-30.0f, -3.0f).withWidth (26.0f).withHeight (12.0f), Justification::centredRight);
    }

    g.setColour (labelCol);
    g.setFont (FontOptions (9.5f, Font::bold));
    g.drawText (state.highMode ? "ADRIEN-SCOPE  /  HIGH" : "ADRIEN-SCOPE  /  LOW",
                Rectangle<float> (r.getX(), glass.getY() + 1.0f, 200.0f, 14.0f), Justification::centredLeft);

    if (hover && ! dragging)
    {
        g.setColour (amber.withAlpha (0.55f));
        g.setFont (FontOptions (10.0f));
        g.drawText (String::fromUTF8 ("glisse pour choisir la fr\xc3\xa9quence"),
                    Rectangle<float> (r.getX(), r.getBottom() - 16.0f, r.getWidth(), 14.0f), Justification::centred);
    }

    // lignes de balayage + vignette
    g.setColour (Colours::black.withAlpha (0.16f));
    for (float y = glass.getY(); y < glass.getBottom(); y += 3.0f)
        g.fillRect (glass.getX(), y, glass.getWidth(), 1.0f);

    ColourGradient vig (Colours::transparentBlack, glass.getCentreX(), glass.getCentreY(),
                        Colours::black.withAlpha (0.45f), glass.getX(), glass.getY(), true);
    g.setGradientFill (vig);
    g.fillRoundedRectangle (glass, 7.0f);
}

//==============================================================================
void EqScreen::setFreqFromX (float x)
{
    freqAttachment.setValueAsPartOfGesture (jlimit (20.0f, 16000.0f, freqForX (x)));
}

void EqScreen::mouseDown (const MouseEvent& e)
{
    dragging = true;
    freqAttachment.beginGesture();
    setFreqFromX (e.position.x);
}

void EqScreen::mouseDrag (const MouseEvent& e)
{
    setFreqFromX (e.position.x);
}

void EqScreen::mouseUp (const MouseEvent&)
{
    dragging = false;
    freqAttachment.endGesture();
}

void EqScreen::mouseDoubleClick (const MouseEvent&)
{
    freqAttachment.setValueAsCompleteGesture (60.0f);
}

void EqScreen::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    const float f = state.freq * std::pow (2.0f, (w.deltaY + w.deltaX) * 0.6f);
    freqAttachment.setValueAsCompleteGesture (jlimit (20.0f, 16000.0f, f));
}

void EqScreen::mouseMove (const MouseEvent&) { hover = true; }
void EqScreen::mouseExit (const MouseEvent&) { hover = false; }
