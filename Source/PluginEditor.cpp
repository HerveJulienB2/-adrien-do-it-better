#include "PluginEditor.h"
#include "Phrases.h"

using namespace juce;

static String U8 (const char* s) { return String::fromUTF8 (s); }

//==============================================================================
//  KNOB
//==============================================================================
Knob::Knob (AudioProcessorValueTreeState& state, const String& paramId, const String& text, Colour cap, int ticks)
{
    title.setText (text, dontSendNotification);
    title.setJustificationType (Justification::centred);
    title.setFont (uiFont (10.5f, true));
    title.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (title);

    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 76, 16);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (ticks == 10 ? 220 : 160);
    slider.getProperties().set ("cap", (int) cap.getARGB());
    slider.getProperties().set ("ticks", ticks);
    addAndMakeVisible (slider);

    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);

    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

    slider.onDoubleClick = [this] { if (onDoubleClick) onDoubleClick(); };
}

void Knob::resized()
{
    auto r = getLocalBounds();
    title.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
}

//==============================================================================
//  CRANS DE FRÉQUENCE
//==============================================================================
FreqStrip::FreqStrip (RangedAudioParameter& p)
    : attachment (p, [this] (float v) { setIndex (roundToInt (v)); }, nullptr)
{
    attachment.sendInitialUpdate();
}

Rectangle<float> FreqStrip::cell (int i) const
{
    const auto r = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
    const float w = r.getWidth() / (float) AdrienAudioProcessor::numFreqSteps;
    return { r.getX() + w * (float) i, r.getY(), w, r.getHeight() };
}

Point<float> FreqStrip::getCellCentreInParent (int i) const
{
    return cell (i).getCentre() + getPosition().toFloat();
}

void FreqStrip::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff050605));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (Colour (0xff4a4d50));
    g.drawRoundedRectangle (b.reduced (0.75f), 6.0f, 1.5f);

    for (int i = 0; i < AdrienAudioProcessor::numFreqSteps; ++i)
    {
        const auto c = cell (i);
        const bool on = i == index;
        const auto label = String (roundToInt (AdrienAudioProcessor::freqForIndex (i)));

        if (on)
        {
            g.setColour (Theme::crtGreen.withAlpha (0.16f));
            g.fillRoundedRectangle (c.reduced (2.0f, 0.0f), 4.0f);
        }

        // témoin
        const auto led = Rectangle<float> (6.0f, 6.0f).withCentre ({ c.getCentreX(), c.getY() + 6.0f });
        if (on)
        {
            g.setColour (Theme::accentLit.withAlpha (0.35f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setColour (on ? Theme::accentLit : Colour (0xff2a3a2f));
        g.fillEllipse (led);

        g.setColour (on ? Theme::crtGreen : Theme::crtGreen.withAlpha (0.38f));
        g.setFont (monoFont (on ? 15.0f : 13.0f));
        g.drawText (label, c.withTrimmedTop (12.0f), Justification::centred);
    }

    g.setColour (Theme::crtGreen.withAlpha (0.38f));
    g.setFont (monoFont (10.0f));
    g.drawText ("Hz", b.reduced (8.0f, 2.0f), Justification::bottomRight);
}

void FreqStrip::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < AdrienAudioProcessor::numFreqSteps; ++i)
        if (cell (i).contains (e.position))
        {
            attachment.setValueAsCompleteGesture ((float) i);
            return;
        }
}

//==============================================================================
//  CONTENU DE L'ÉDITEUR
//==============================================================================
EditorContent::EditorContent (AdrienAudioProcessor& p)
    : proc (p),
      freq   (p.apvts, ParamIDs::freq,   "FREQUENCY",     Colour (0xffece5d3), AdrienAudioProcessor::numFreqSteps - 1),
      boost  (p.apvts, ParamIDs::boost,  "BOOST",         Colour (0xfff0c53f)),
      atten  (p.apvts, ParamIDs::atten,  "ATTEN",         Colour (0xff78b4dc)),
      eqMix  (p.apvts, ParamIDs::eqMix,  "EQ DRY / WET",  Colour (0xff9fa3a4)),
      drive  (p.apvts, ParamIDs::drive,  "DRIVE",         Colour (0xffe9643f)),
      satMix (p.apvts, ParamIDs::satMix, "SAT DRY / WET", Colour (0xff9fa3a4)),
      output (p.apvts, ParamIDs::output, "OUTPUT",        Colour (0xff1d1f20)),
      strip  (*p.apvts.getParameter (ParamIDs::freq))
{
    // grain fin de la peinture (poudre mate), comme sur Phosphor XY
    noise = Image (Image::ARGB, W, H, true, SoftwareImageType());
    {
        Image::BitmapData bd (noise, Image::BitmapData::writeOnly);
        Random rnd (1234);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                bd.setPixelColour (x, y, rnd.nextBool() ? Colour::fromRGBA (0, 0, 0, (uint8) rnd.nextInt (9))
                                                        : Colour::fromRGBA (255, 255, 255, (uint8) rnd.nextInt (11)));
    }

    for (auto* k : { &freq, &boost, &atten, &eqMix, &drive, &satMix, &output })
    {
        addAndMakeVisible (*k);
        k->onDoubleClick = [this] { resetPending = true; };
    }
    addAndMakeVisible (strip);

    // types de saturation : touches à témoin, une seule allumée
    typeAttachment = std::make_unique<ParameterAttachment> (*p.apvts.getParameter (ParamIDs::satType), [] (float) {}, nullptr);
    const auto names = AdrienAudioProcessor::satTypeNames();
    for (int i = 0; i < 5; ++i)
    {
        auto& k = typeKeys[i];
        k.setButtonText (names[i]);
        k.setClickingTogglesState (false);
        k.onClick = [this, i] { typeAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (k);
    }

    autoGainKey.setButtonText ("AUTO GAIN");
    autoGainAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::autoGain, autoGainKey);
    addAndMakeVisible (autoGainKey);

    addAndMakeVisible (adrien);

    setSize (W, H);
    timerCallback();
    startTimerHz (30);
}

EditorContent::~EditorContent()
{
    stopTimer();
}

void EditorContent::resized()
{
    // PULTEC TRICK
    {
        auto row = Rectangle<int> (30, 112, 660, 130);
        const int w = row.getWidth() / 4;
        for (auto* k : { &freq, &boost, &atten, &eqMix })
            k->setBounds (row.removeFromLeft (w).reduced (6, 0));
    }
    strip.setBounds (30, 256, 660, 50);

    // SATURATION
    {
        auto keys = Rectangle<int> (30, 354, 490, 36);
        const int kw = (keys.getWidth() - 4 * 8) / 5;
        for (auto& k : typeKeys)
        {
            k.setBounds (keys.removeFromLeft (kw));
            keys.removeFromLeft (8);
        }

        auto row = Rectangle<int> (30, 402, 490, 130);
        const int w = row.getWidth() / 3;
        drive.setBounds (row.removeFromLeft (w).reduced (6, 0));
        satMix.setBounds (row.removeFromLeft (w).reduced (6, 0));
        autoGainKey.setBounds (row.withSizeKeepingCentre (110, 40).translated (0, -6));
    }

    // OUTPUT
    output.setBounds (546, 402, 138, 130);

    // Adrien, en bas à droite
    adrien.setBounds (742, 276, 318, 346);
}

//==============================================================================
static void drawGrille (Graphics& g, Point<float> centre, float radius)
{
    const float pitch = 9.0f;
    for (float y = -radius; y <= radius; y += pitch)
        for (float x = -radius; x <= radius; x += pitch)
            if (x * x + y * y <= radius * radius)
            {
                const float px = centre.x + x, py = centre.y + y;
                g.setColour (Colours::white.withAlpha (0.55f));
                g.fillEllipse (px - 2.2f, py - 1.2f, 4.4f, 4.4f);
                g.setColour (Colour (0xff4b5052));
                g.fillEllipse (px - 2.2f, py - 2.2f, 4.4f, 4.4f);
            }
}

void EditorContent::paint (Graphics& g)
{
    // face avant gris clair mat
    g.setGradientFill (ColourGradient (Theme::panelTop, 0.0f, 0.0f, Theme::panelBottom, 0.0f, (float) H, false));
    g.fillAll();
    g.drawImageAt (noise, 0, 0);

    // titre : minuscules, sobre
    g.setColour (Theme::ink);
    g.setFont (uiFont (27.0f, true));
    g.drawText ("adrien do it better", 30, 18, 400, 34, Justification::centredLeft);
    g.setColour (Theme::inkSoft);
    g.setFont (uiFont (11.0f));
    g.drawText (U8 ("pultec trick   -   band saturator   -   hand-tuned by M\xc3\xa9" "chant Humain"),
                32, 48, 520, 14, Justification::centredLeft);

    // témoin de fonctionnement
    g.setColour (Theme::accentLit.withAlpha (0.30f));
    g.fillEllipse (1042.0f, 26.0f, 22.0f, 22.0f);
    g.setColour (Theme::accentLit);
    g.fillEllipse (1047.0f, 31.0f, 12.0f, 12.0f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (1047.0f, 31.0f, 12.0f, 12.0f, 1.0f);
    g.setColour (Theme::inkSoft);
    g.setFont (uiFont (9.5f, true));
    g.drawText ("V2.1  /  VST3", 920, 29, 116, 16, Justification::centredRight);

    // sections
    BraunLookAndFeel::drawHeader (g, { 30.0f, 84.0f, 660.0f, 22.0f }, "PULTEC TRICK");
    BraunLookAndFeel::drawHeader (g, { 30.0f, 326.0f, 490.0f, 22.0f }, "SATURATION",
                                  AdrienColours::forSatType (vs.satType));
    BraunLookAndFeel::drawHeader (g, { 540.0f, 326.0f, 150.0f, 22.0f }, "OUTPUT");

    // afficheur des réglages
    {
        const Rectangle<float> r (30.0f, 556.0f, 600.0f, 60.0f);
        g.setColour (Colour (0xff050605));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Colour (0xff4a4d50));
        g.drawRoundedRectangle (r.reduced (0.75f), 6.0f, 1.5f);

        auto text = [this] (const char* id) { return proc.apvts.getParameter (id)->getCurrentValueAsText(); };
        const String l1 = "FREQ " + text (ParamIDs::freq) + "   BOOST " + text (ParamIDs::boost)
                        + "   ATTEN " + text (ParamIDs::atten) + "   EQ " + text (ParamIDs::eqMix);
        String l2 = "SAT " + AdrienAudioProcessor::satTypeNames()[vs.satType] + " " + text (ParamIDs::drive)
                  + "   MIX " + text (ParamIDs::satMix) + "   OUT " + text (ParamIDs::output);
        if (vs.autoGain)
        {
            const float ag = proc.autoGainDb.load();
            l2 += "   AUTO " + String (ag > 0 ? "+" : "") + String (ag, 1) + " dB";
        }

        g.setColour (Theme::crtGreen);
        g.setFont (monoFont (14.0f));
        g.drawText (l1, r.reduced (12.0f, 8.0f).removeFromTop (20.0f), Justification::centredLeft);
        g.drawText (l2, r.reduced (12.0f, 8.0f).removeFromBottom (20.0f), Justification::centredLeft);
        if (vs.clipping)
        {
            g.setColour (Colour (0xffff5040));
            g.drawText ("CLIP", r.reduced (12.0f, 8.0f).removeFromTop (20.0f), Justification::centredRight);
        }
    }

    // grille perforée
    drawGrille (g, { 668.0f, 586.0f }, 28.0f);

    // la fenêtre d'Adrien : boîtier sombre comme l'écran de Phosphor XY
    const Rectangle<float> bezel (730.0f, 104.0f, 340.0f, 536.0f);
    g.setColour (Colours::black.withAlpha (0.18f));
    g.fillRoundedRectangle (bezel.translated (0.0f, 5.0f).expanded (1.0f), 20.0f);
    g.setGradientFill (ColourGradient (Colour (0xff35393b), bezel.getX(), bezel.getY(),
                                       Colour (0xff1a1c1d), bezel.getX(), bezel.getBottom(), false));
    g.fillRoundedRectangle (bezel, 18.0f);
    g.setColour (Colours::white.withAlpha (0.16f));
    g.drawRoundedRectangle (bezel.reduced (0.5f), 18.0f, 1.0f);

    const auto inset = bezel.reduced (12.0f);
    {
        const auto c = Point<float> (inset.getCentreX() + 20.0f, inset.getBottom() - 170.0f);
        ColourGradient grad (Colour (0xff4a524c).brighter (0.25f * levelSmooth), c, Colour (0xff101211), inset.getTopLeft(), true);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (inset, 14.0f);
    }
    // ombre au sol
    {
        const Point<float> c (adrien.getBounds().getCentreX() + 50.0f, (float) adrien.getBottom() - 4.0f);
        ColourGradient sh (Colours::black.withAlpha (0.5f), c, Colours::black.withAlpha (0.0f), c.translated (130.0f, 0), true);
        g.setGradientFill (sh);
        g.fillEllipse (Rectangle<float> (250.0f, 20.0f).withCentre (c));
    }
    g.setColour (Colour (0xff55595c));
    g.drawRoundedRectangle (inset, 14.0f, 2.0f);
}

void EditorContent::paintOverChildren (Graphics& g)
{
    // bulle d'Adrien, en haut de sa fenêtre
    if (quip.isNotEmpty())
    {
        const float pop = jmin (1.0f, quipAge / 0.18f);
        const auto font = uiFont (13.0f, true);
        const auto area = Rectangle<float> (744.0f, 118.0f, 312.0f, 120.0f);

        // hauteur selon le nombre de lignes
        GlyphArrangement ga;
        ga.addJustifiedText (font, quip, 0.0f, 0.0f, area.getWidth() - 28.0f, Justification::centred);
        const float textH = ga.getBoundingBox (0, -1, true).getHeight();
        const float singleW = GlyphArrangement::getStringWidth (font, quip) + 30.0f;
        const float bw = jmin (area.getWidth(), singleW);
        const auto head = adrien.getHeadTopInParent();
        const float bh = jmin (area.getHeight(), textH + 22.0f);
        const float bottom = jlimit (area.getY() + bh, area.getY() + 150.0f, head.y - 26.0f);
        auto bubble = Rectangle<float> (bw, bh).withCentre ({ area.getCentreX(), bottom - bh * 0.5f });
        const auto tailTip = Point<float> (jlimit (bubble.getX() + 20.0f, bubble.getRight() - 20.0f, head.x),
                                           jmax (bubble.getBottom() + 10.0f, head.y - 12.0f));

        Graphics::ScopedSaveState ss (g);
        const float sc = 0.85f + 0.15f * pop;
        g.addTransform (AffineTransform::scale (sc, sc, tailTip.x, tailTip.y));

        Path p;
        p.addRoundedRectangle (bubble, 12.0f);
        Path tail;
        const float tx = jlimit (bubble.getX() + 18.0f, bubble.getRight() - 30.0f, tailTip.x - 6.0f);
        tail.startNewSubPath (tx, bubble.getBottom() - 4.0f);
        tail.lineTo (tailTip);
        tail.lineTo (tx + 20.0f, bubble.getBottom() - 4.0f);
        tail.closeSubPath();
        p.addPath (tail);

        g.setColour (Colours::black.withAlpha (0.35f * pop));
        g.fillPath (p, AffineTransform::translation (2.0f, 3.0f));
        g.setColour (Theme::recess.withAlpha (pop));
        g.fillPath (p);
        g.setColour (Theme::line.withAlpha (pop));
        g.strokePath (p, PathStrokeType (1.0f));
        g.setColour (Theme::ink.withAlpha (pop));
        g.setFont (font);
        g.drawFittedText (quip, bubble.reduced (12.0f, 6.0f).toNearestInt(), Justification::centred, 5, 1.0f);
    }
}

//==============================================================================
void EditorContent::trigger (Reaction r)
{
    if (r != vs.reaction || vs.reactionAge > 4.0f)
    {
        reactionSeed = Random::getSystemRandom().nextInt64();
        vs.reaction = r;
        vs.reactionAge = 0.0f;
    }
    else if (r == Reaction::Freq)
    {
        vs.reactionAge = 0.0f;                       // chaque cran refait sauter Adrien
    }
    else
    {
        vs.reactionAge = jmin (vs.reactionAge, 0.15f);   // on tient la réaction pendant qu'on tourne le bouton
    }
    idleFor = 0.0f;
}

void EditorContent::timerCallback()
{
    constexpr float dt = 1.0f / 30.0f;
    auto& ap = proc.apvts;

    vs.freqIndex = jlimit (0, AdrienAudioProcessor::numFreqSteps - 1, roundToInt (ap.getRawParameterValue (ParamIDs::freq)->load()));
    vs.freq     = AdrienAudioProcessor::freqForIndex (vs.freqIndex);
    vs.boost    = ap.getRawParameterValue (ParamIDs::boost)->load();
    vs.atten    = ap.getRawParameterValue (ParamIDs::atten)->load();
    vs.eqMix    = ap.getRawParameterValue (ParamIDs::eqMix)->load() * 0.01f;
    vs.drive    = ap.getRawParameterValue (ParamIDs::drive)->load();
    vs.satMix   = ap.getRawParameterValue (ParamIDs::satMix)->load() * 0.01f;
    vs.outputDb = ap.getRawParameterValue (ParamIDs::output)->load();
    vs.satType  = jlimit (0, 4, roundToInt (ap.getRawParameterValue (ParamIDs::satType)->load()));
    vs.autoGain = ap.getRawParameterValue (ParamIDs::autoGain)->load() > 0.5f;

    vs.reactionAge += dt;
    idleFor += dt;

    // --- chaque réglage touché déclenche une réaction ---
    if (firstFrame)
    {
        firstFrame = false;
    }
    else if (resetPending)
    {
        resetPending = false;
        trigger (Reaction::Reset);
    }
    else
    {
        auto moved = [] (float a, float b, float eps) { return std::abs (a - b) > eps; };

        if (vs.freqIndex != prev.freqIndex)                 trigger (Reaction::Freq);
        if (vs.satType != prev.satType)                     trigger (Reaction::SatType);
        if (vs.autoGain != prev.autoGain)                   trigger (vs.autoGain ? Reaction::AutoGainOn : Reaction::AutoGainOff);
        if (moved (vs.eqMix, prev.eqMix, 0.002f))           trigger (Reaction::EqMix);
        if (moved (vs.satMix, prev.satMix, 0.002f))         trigger (Reaction::SatMix);
        if (moved (vs.drive, prev.drive, 0.005f))           trigger (Reaction::Drive);
        if (moved (vs.outputDb, prev.outputDb, 0.05f))      trigger (vs.outputDb > prev.outputDb ? Reaction::OutputUp : Reaction::OutputDown);
        if (moved (vs.boost, prev.boost, 0.005f) || moved (vs.atten, prev.atten, 0.005f))
        {
            if (vs.boost > 5.0f && vs.atten > 5.0f)         trigger (Reaction::Pultec);
            else if (moved (vs.boost, prev.boost, 0.005f))  trigger (Reaction::Boost);
            else                                            trigger (Reaction::Atten);
        }
    }
    prev = vs;

    // --- le son ---
    const float lvl = proc.outputLevel.load();
    const float band = proc.bandLevel.load();
    levelSmooth = lvl > levelSmooth ? lvl : levelSmooth * 0.88f;
    bandSmooth = band > bandSmooth ? band : bandSmooth * 0.88f;
    vs.level = levelSmooth;
    vs.bandLevel = bandSmooth;
    vs.time += dt;

    if (proc.peakLevel.load() >= 0.999f) clipHold = 1.5f;
    clipHold = jmax (0.0f, clipHold - dt);
    vs.clipping = clipHold > 0.0f;

    vs.loud = levelSmooth > 0.25f;                       // environ -12 dBFS RMS
    silentFor = levelSmooth < 0.0005f ? silentFor + dt : 0.0f;
    vs.silent = silentFor > 2.0f;
    vs.idle = idleFor > 20.0f;

    // --- l'interface suit les paramètres ---
    for (int i = 0; i < 5; ++i)
        typeKeys[i].setToggleState (i == vs.satType, dontSendNotification);

    // --- la bulle : CLIP d'abord, puis la réaction en cours, puis l'ambiance ---
    String q;
    if (vs.reaction != Reaction::None && vs.reactionAge < 4.0f)
    {
        Random r (reactionSeed);
        q = Phrases::forReaction (vs.reaction, vs, r);
        lastReactionQuip = q;
    }
    if (vs.clipping)                                  q = Phrases::clip();
    else if (q.isEmpty() && vs.idle)                  q = Phrases::idle ((int) ((idleFor - 20.0f) / 8.0f));
    else if (q.isEmpty() && vs.loud)                  q = Phrases::loud();
    else if (q.isEmpty() && vs.silent)                q = Phrases::silent();
    else if (q.isEmpty())                             q = lastReactionQuip;

    if (q != quip)
    {
        // la bulle ne « repoppe » que si elle était vide : sinon on change juste le texte,
        // pour qu'elle ne clignote pas quand on tourne un bouton
        quipAge = quip.isEmpty() ? 0.0f : jmax (quipAge, 0.18f);
        quip = q;
    }
    quipAge += dt;

    adrien.setPointTarget (strip.getCellCentreInParent (vs.freqIndex));
    adrien.setState (vs);
    repaint();
}

//==============================================================================
//  ÉDITEUR : redimensionnable, proportions fixes (comme Phosphor XY)
//==============================================================================
AdrienAudioProcessorEditor::AdrienAudioProcessorEditor (AdrienAudioProcessor& p)
    : AudioProcessorEditor (&p), content (p)
{
    setLookAndFeel (&laf);
    addAndMakeVisible (content);

    setResizeLimits (EditorContent::W / 2, EditorContent::H / 2, EditorContent::W * 2, EditorContent::H * 2);
    getConstrainer()->setFixedAspectRatio ((double) EditorContent::W / (double) EditorContent::H);
    setResizable (true, true);
    setSize (EditorContent::W, EditorContent::H);
}

AdrienAudioProcessorEditor::~AdrienAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void AdrienAudioProcessorEditor::resized()
{
    content.setBounds (0, 0, EditorContent::W, EditorContent::H);
    content.setTransform (AffineTransform::scale ((float) getWidth() / (float) EditorContent::W));
}
