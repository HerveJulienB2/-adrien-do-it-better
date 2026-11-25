#include "PluginEditor.h"

using namespace juce;

static String U8 (const char* s) { return String (CharPointer_UTF8 (s)); }

//==============================================================================
//  LOOK AND FEEL : potards façon Pultec (noir, jupe graduée, pointeur crème)
//==============================================================================
AdrienLookAndFeel::AdrienLookAndFeel()
{
    setColour (Slider::textBoxTextColourId, AdrienColours::cream);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, AdrienColours::amber.withAlpha (0.4f));
    setColour (Label::textColourId, AdrienColours::cream);
    setColour (TextEditor::textColourId, AdrienColours::cream);
    setColour (TextEditor::backgroundColourId, AdrienColours::bgDark);
    setColour (TextEditor::highlightColourId, AdrienColours::amber.withAlpha (0.4f));
    setColour (CaretComponent::caretColourId, AdrienColours::amber);
    setColour (TextEditor::focusedOutlineColourId, AdrienColours::brass);
}

void AdrienLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, Slider& s)
{
    const auto bounds = Rectangle<int> (x, y, w, h).toFloat();
    const auto c = bounds.getCentre();
    const float R = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 9.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool big = (bool) s.getProperties()["big"];
    const bool over = s.isMouseOverOrDragging();

    // graduations 0..10
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const float r1 = R + 3.0f, r2 = R + ((i % 5 == 0) ? 8.5f : 6.0f);
        const Point<float> p1 (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a));
        const Point<float> p2 (c.x + r2 * std::sin (a), c.y - r2 * std::cos (a));
        g.setColour (AdrienColours::creamDim.withAlpha (i % 5 == 0 ? 0.9f : 0.5f));
        g.drawLine ({ p1, p2 }, i % 5 == 0 ? 1.6f : 1.0f);
    }

    // arc de valeur
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, R + 1.5f, R + 1.5f, 0.0f, startAngle, angle, true);
        g.setColour (AdrienColours::amber.withAlpha (over ? 0.95f : 0.7f));
        g.strokePath (arc, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // jupe
    ColourGradient skirt (Colour (0xff3a3029), c.x, c.y - R, Colour (0xff0c0907), c.x, c.y + R, false);
    g.setGradientFill (skirt);
    g.fillEllipse (Rectangle<float> (R * 2, R * 2).withCentre (c));
    g.setColour (AdrienColours::brass.withAlpha (0.55f));
    g.drawEllipse (Rectangle<float> (R * 2, R * 2).withCentre (c), 1.0f);

    // corps du bouton (cannelé)
    const float rk = R * (big ? 0.78f : 0.74f);
    {
        Path flutes;
        const int n = 24;
        for (int i = 0; i < n; ++i)
        {
            const float a = angle + (float) i / n * MathConstants<float>::twoPi;
            const float rr = rk * (i % 2 ? 1.0f : 0.95f);
            const Point<float> p (c.x + rr * std::sin (a), c.y - rr * std::cos (a));
            if (i == 0) flutes.startNewSubPath (p); else flutes.lineTo (p);
        }
        flutes.closeSubPath();
        g.setColour (Colour (0xff050403));
        g.fillPath (flutes);
    }
    ColourGradient cap (Colour (0xff4a423b), c.x - rk * 0.5f, c.y - rk * 0.6f, Colour (0xff0a0807), c.x + rk * 0.4f, c.y + rk * 0.6f, true);
    g.setGradientFill (cap);
    g.fillEllipse (Rectangle<float> (rk * 1.7f, rk * 1.7f).withCentre (c));
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawEllipse (Rectangle<float> (rk * 1.7f, rk * 1.7f).withCentre (c), 1.0f);

    // pointeur
    const Point<float> p1 (c.x + rk * 0.25f * std::sin (angle), c.y - rk * 0.25f * std::cos (angle));
    const Point<float> p2 (c.x + rk * 0.95f * std::sin (angle), c.y - rk * 0.95f * std::cos (angle));
    g.setColour (AdrienColours::cream);
    g.drawLine ({ p1, p2 }, big ? 3.2f : 2.6f);
    g.setColour (AdrienColours::brass);
    g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (c));
}

Label* AdrienLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (FontOptions (12.5f, Font::bold));
    l->setJustificationType (Justification::centred);
    l->setColour (Label::textColourId, AdrienColours::cream);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    return l;
}

void AdrienLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto pill = Colour ((uint32) (int64) b.getProperties().getWithDefault ("pill", (int64) 0xffffb347));
    const float rad = r.getHeight() * 0.5f;

    if (b.getToggleState())
    {
        g.setColour (pill.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (1.5f), rad + 1.5f);
        g.setColour (pill);
        g.fillRoundedRectangle (r, rad);
    }
    else
    {
        g.setColour (Colour (0xff1a1410).brighter (over ? 0.15f : 0.0f).darker (down ? 0.2f : 0.0f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (AdrienColours::brass.withAlpha (over ? 0.8f : 0.4f));
        g.drawRoundedRectangle (r, rad, 1.0f);
    }
}

void AdrienLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setFont (FontOptions (11.0f, Font::bold));
    g.setColour (b.getToggleState() ? Colour (0xff15110d) : AdrienColours::creamDim);
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

//==============================================================================
//  ÉDITEUR
//==============================================================================
AdrienAudioProcessorEditor::AdrienAudioProcessorEditor (AdrienAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      screen (*p.apvts.getParameter (ParamIDs::freq))
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (adrien);
    addAndMakeVisible (screen);

    setupKnob (freq,   ParamIDs::freq,   "FREQUENCY");
    setupKnob (boost,  ParamIDs::boost,  "BOOST");
    setupKnob (atten,  ParamIDs::atten,  "ATTEN");
    setupKnob (eqMix,  ParamIDs::eqMix,  "EQ DRY / WET");
    setupKnob (drive,  ParamIDs::drive,  "DRIVE");
    setupKnob (satMix, ParamIDs::satMix, "SAT DRY / WET");
    setupKnob (output, ParamIDs::output, "OUTPUT");
    freq.getProperties().set ("big", true);
    freq.setDoubleClickReturnValue (true, 60.0);
    boost.setDoubleClickReturnValue (true, 0.0);
    atten.setDoubleClickReturnValue (true, 0.0);
    drive.setDoubleClickReturnValue (true, 0.0);
    output.setDoubleClickReturnValue (true, 0.0);

    // types de saturation
    auto names = AdrienAudioProcessor::satTypeNames();
    typeAttachment = std::make_unique<ParameterAttachment> (*p.apvts.getParameter (ParamIDs::satType), [] (float) {}, nullptr);
    for (int i = 0; i < 5; ++i)
    {
        auto& b = typeButtons[i];
        b.setButtonText (names[i]);
        b.getProperties().set ("pill", (int64) AdrienColours::forSatType (i).getARGB());
        b.onClick = [this, i] { typeAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    // LOW / HIGH
    shapeAttachment = std::make_unique<ParameterAttachment> (*p.apvts.getParameter (ParamIDs::shape), [] (float) {}, nullptr);
    const char* shapeNames[] = { "LOW", "HIGH" };
    for (int i = 0; i < 2; ++i)
    {
        auto& b = shapeButtons[i];
        b.setButtonText (shapeNames[i]);
        b.getProperties().set ("pill", (int64) AdrienColours::amber.getARGB());
        b.onClick = [this, i] { shapeAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    setSize (1060, 660);
    timerCallback();
    startTimerHz (30);
}

AdrienAudioProcessorEditor::~AdrienAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AdrienAudioProcessorEditor::setupKnob (Slider& s, const char* paramId, const String& name)
{
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::TextBoxBelow, false, 96, 18);
    s.setRotaryParameters (degreesToRadians (-135.0f), degreesToRadians (135.0f), true);
    s.setMouseDragSensitivity (220);
    s.setVelocityBasedMode (false);
    s.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    s.setPopupDisplayEnabled (false, false, this);
    addAndMakeVisible (s);
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (proc.apvts, paramId, s));
    knobNames.push_back ({ &s, name });
}

//==============================================================================
void AdrienAudioProcessorEditor::resized()
{
    adrien.setBounds (0, 0, 560, 660);
    screen.setBounds (560, 70, 484, 256);

    // section PULTEC
    const int x0 = 560, w = 484;
    const int row1 = 362, row2 = 528, knobH = 96;
    const int col = w / 4;
    freq .setBounds (x0 + 0 * col, row1 - 4, col, knobH + 26);
    boost.setBounds (x0 + 1 * col, row1, col, knobH + 18);
    atten.setBounds (x0 + 2 * col, row1, col, knobH + 18);
    eqMix.setBounds (x0 + 3 * col, row1, col, knobH + 18);

    shapeButtons[0].setBounds (x0 + w - 112, 337, 54, 20);
    shapeButtons[1].setBounds (x0 + w - 56, 337, 54, 20);

    // section SATURATION
    const int pillW = 58;
    for (int i = 0; i < 5; ++i)
        typeButtons[i].setBounds (x0 + w - (5 - i) * (pillW + 2), 503, pillW, 20);

    const int col3 = w / 3;
    drive .setBounds (x0 + 0 * col3, row2, col3, knobH + 18);
    satMix.setBounds (x0 + 1 * col3, row2, col3, knobH + 18);
    output.setBounds (x0 + 2 * col3, row2, col3, knobH + 18);
}

//==============================================================================
void AdrienAudioProcessorEditor::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // mur du studio
    ColourGradient wall (AdrienColours::bgMid, 0, 0, AdrienColours::bgDark, 0, b.getHeight(), false);
    g.setGradientFill (wall);
    g.fillAll();

    // projecteur sur Adrien
    {
        const Point<float> c (230.0f, 300.0f);
        ColourGradient spot (Colour (0xffffd9a0).withAlpha (0.16f + 0.10f * levelSmooth), c,
                             Colour (0xffffd9a0).withAlpha (0.0f), c.translated (330.0f, 0), true);
        g.setGradientFill (spot);
        g.fillEllipse (Rectangle<float> (660.0f, 660.0f).withCentre (c));
    }
    // ombre au sol
    {
        const Point<float> c (225.0f, 646.0f);
        ColourGradient sh (Colours::black.withAlpha (0.55f), c, Colours::black.withAlpha (0.0f), c.translated (200.0f, 0), true);
        g.setGradientFill (sh);
        g.fillEllipse (Rectangle<float> (420.0f, 34.0f).withCentre (c));
    }

    // titre
    g.setColour (AdrienColours::cream);
    g.setFont (FontOptions (36.0f, Font::bold).withHorizontalScale (0.86f));
    g.drawText ("ADRIEN", 22, 10, 170, 42, Justification::centredLeft);
    g.setColour (AdrienColours::amber);
    g.setFont (FontOptions (36.0f, Font::bold | Font::italic).withHorizontalScale (0.86f));
    g.drawText ("DO IT BETTER", 160, 10, 320, 42, Justification::centredLeft);

    g.setColour (AdrienColours::creamDim);
    g.setFont (FontOptions (11.0f, Font::bold));
    g.drawText (U8 ("PULTEC TRICK  \xc2\xb7  BAND SATURATOR"), getWidth() - 330, 14, 310, 16, Justification::centredRight);
    g.setColour (AdrienColours::brassDim);
    g.setFont (FontOptions (10.0f));
    g.drawText ("v1.0  /  hand-tuned by Adrien", getWidth() - 330, 31, 310, 14, Justification::centredRight);

    g.setColour (AdrienColours::brass.withAlpha (0.35f));
    g.drawHorizontalLine (58, 18.0f, (float) getWidth() - 18.0f);

    // panneau des réglages
    const auto panel = Rectangle<float> (556.0f, 332.0f, 492.0f, 318.0f);
    g.setColour (AdrienColours::panel.withAlpha (0.85f));
    g.fillRoundedRectangle (panel, 10.0f);
    g.setColour (AdrienColours::brass.withAlpha (0.35f));
    g.drawRoundedRectangle (panel, 10.0f, 1.0f);
    g.drawHorizontalLine (496, panel.getX() + 12.0f, panel.getRight() - 12.0f);

    g.setFont (FontOptions (12.0f, Font::bold));
    g.setColour (AdrienColours::brass);
    g.drawText ("PULTEC TRICK", 572, 338, 200, 18, Justification::centredLeft);
    g.drawText ("SATURATION", 572, 504, 200, 18, Justification::centredLeft);

    // noms des potards (au-dessus de chaque potard, côté jupe basse)
    g.setFont (FontOptions (10.0f, Font::bold));
    for (auto& [s, name] : knobNames)
    {
        g.setColour (AdrienColours::creamDim);
        auto r = s->getBounds().withY (s->getBottom() - 2).withHeight (14);
        g.drawText (name, r, Justification::centred);
    }
}

void AdrienAudioProcessorEditor::paintOverChildren (Graphics& g)
{
    // faisceau : de la paume d'Adrien jusqu'à la fréquence sur l'écran
    const auto from = adrien.getPalmInParent();
    const auto to = screen.getMarkerInParent();
    const auto amber = AdrienColours::amber;

    Path beam;
    beam.startNewSubPath (from);
    beam.quadraticTo ((from.x + to.x) * 0.5f, jmin (from.y, to.y) - 30.0f, to.x, to.y);

    const float alpha = 0.35f + 0.4f * levelSmooth;
    g.setColour (amber.withAlpha (alpha * 0.25f));
    g.strokePath (beam, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));

    Path dashed;
    const float dash[] = { 2.0f, 6.0f };
    PathStrokeType (1.6f).createDashedStroke (dashed, beam, dash, 2);
    g.setColour (Colour (0xffffe2b0).withAlpha (alpha + 0.2f));
    g.fillPath (dashed);

    g.setColour (amber.withAlpha (0.25f));
    g.fillEllipse (Rectangle<float> (18.0f, 18.0f).withCentre (from));
    g.setColour (Colour (0xffffe2b0).withAlpha (0.8f));
    g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (from));

    // bulle d'Adrien
    if (quip.isNotEmpty())
    {
        const float pop = jmin (1.0f, quipAge / 0.18f);
        const float sc = 0.85f + 0.15f * pop;
        g.setFont (FontOptions (13.0f, Font::bold));
        const float tw = jmin (250.0f, GlyphArrangement::getStringWidth (g.getCurrentFont(), quip) + 26.0f);
        auto bubble = Rectangle<float> (285.0f, 84.0f, tw, 34.0f);
        const auto anchor = Point<float> (bubble.getX(), bubble.getBottom());

        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::scale (sc, sc, anchor.x, anchor.y));
        g.setOpacity (pop);

        Path p;
        p.addRoundedRectangle (bubble, 14.0f);
        Path tail;
        tail.startNewSubPath (bubble.getX() + 16.0f, bubble.getBottom() - 6.0f);
        tail.lineTo (bubble.getX() - 26.0f, bubble.getBottom() + 22.0f);
        tail.lineTo (bubble.getX() + 34.0f, bubble.getBottom() - 2.0f);
        tail.closeSubPath();
        p.addPath (tail);

        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillPath (p, AffineTransform::translation (2.0f, 3.0f));
        g.setColour (AdrienColours::cream);
        g.fillPath (p);
        g.setColour (Colour (0xff15110d));
        g.drawText (quip, bubble.reduced (10.0f, 0.0f), Justification::centred, true);
    }
}

//==============================================================================
String AdrienAudioProcessorEditor::pickQuip() const
{
    const float heat = vs.heat();
    if (heat > 0.7f)
    {
        switch (vs.satType)
        {
            case 0:  return U8 ("\xc3\x87" "a sature comme en 74.");
            case 1:  return U8 ("\xc3\x87" "a chauffe, \xc3\xa7" "a chauffe !");
            case 2:  return U8 ("J'ai les cheveux qui fris" "ent.");
            case 3:  return U8 ("Je me pixelise l\xc3\xa0...");
            default: return U8 ("\xc3\x87" "a gr\xc3\xa9sille dans ma t\xc3\xaate.");
        }
    }
    if (vs.boost > 6.0f && vs.atten > 6.0f) return U8 ("Le Pultec trick. Classique.");
    if (vs.boost > 7.5f)                    return U8 ("Regarde-moi cette crini\xc3\xa8re.");
    if (vs.atten > 7.5f)                    return U8 ("Mes cheveux... MES CHEVEUX !");
    if (heat > 0.35f)                       return U8 ("Il fait chaud ici, non ?");
    if (vs.boost > 3.0f && vs.atten < 1.0f) return U8 ("\xc3\x87" "a pousse, \xc3\xa7" "a pousse.");
    if (vs.level < 0.0005f)                 return U8 ("Envoie du son, je m'ennuie.");
    return U8 ("Je te montre o\xc3\xb9 \xc3\xa7" "a se passe.");
}

void AdrienAudioProcessorEditor::timerCallback()
{
    auto& ap = proc.apvts;
    vs.freq     = ap.getRawParameterValue (ParamIDs::freq)->load();
    vs.boost    = ap.getRawParameterValue (ParamIDs::boost)->load();
    vs.atten    = ap.getRawParameterValue (ParamIDs::atten)->load();
    vs.eqMix    = ap.getRawParameterValue (ParamIDs::eqMix)->load() * 0.01f;
    vs.drive    = ap.getRawParameterValue (ParamIDs::drive)->load();
    vs.satMix   = ap.getRawParameterValue (ParamIDs::satMix)->load() * 0.01f;
    vs.outputDb = ap.getRawParameterValue (ParamIDs::output)->load();
    vs.satType  = jlimit (0, 4, roundToInt (ap.getRawParameterValue (ParamIDs::satType)->load()));
    vs.highMode = ap.getRawParameterValue (ParamIDs::shape)->load() > 0.5f;
    vs.sampleRate = proc.getCurrentSampleRate() > 0 ? proc.getCurrentSampleRate() : 48000.0;

    const float lvl = proc.outputLevel.load();
    const float band = proc.bandLevel.load();
    levelSmooth = lvl > levelSmooth ? lvl : levelSmooth * 0.88f;
    bandSmooth = band > bandSmooth ? band : bandSmooth * 0.88f;
    vs.level = levelSmooth;
    vs.bandLevel = bandSmooth;
    vs.time += 1.0 / 30.0;

    for (int i = 0; i < 5; ++i) typeButtons[i].setToggleState (i == vs.satType, dontSendNotification);
    for (int i = 0; i < 2; ++i) shapeButtons[i].setToggleState ((i == 1) == vs.highMode, dontSendNotification);

    // l'écran d'abord (position du repère), puis Adrien qui pointe dessus
    screen.setState (vs);
    adrien.setPointTarget (screen.getMarkerInParent());
    adrien.setState (vs);

    const auto q = pickQuip();
    if (q != quip) { quip = q; quipAge = 0.0f; }
    quipAge += 1.0f / 30.0f;

    repaint();
}
