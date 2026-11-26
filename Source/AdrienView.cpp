#include "AdrienView.h"
#include "BinaryData.h"

using namespace juce;

namespace Asset
{
    // repères dans les PNG (photo d'origine x0.8)
    const Point<float> armOffset   { 615.2f, 279.2f };
    const Point<float> armPivot    { 612.0f, 376.0f };   // jonction manche / bras
    const Point<float> fingertip   { 952.0f, 302.0f };
    const Point<float> palm        { 885.0f, 314.0f };
    const Point<float> mouth       { 328.0f, 152.0f };
    const float mouthHalfWidth = 25.0f;
    const Point<float> scalp       { 334.0f, 100.0f };
    const float scalpRx = 76.0f, scalpRy = 90.0f;
    const Point<float> cheekL      { 300.0f, 134.0f };
    const Point<float> cheekR      { 364.0f, 134.0f };
    const Point<float> feet        { 330.0f, 1010.0f };
    const Point<float> torso       { 330.0f, 480.0f };
    const Point<float> eyeL        { 295.0f, 95.0f };
    const Point<float> eyeR        { 351.0f, 93.0f };
    const Point<float> earL        { 270.0f, 120.0f };
    const Point<float> earR        { 404.0f, 116.0f };
    const Point<float> hand        { 560.0f, 420.0f };   // main ouverte du bras plié
    const Point<float> headTop     { 334.0f, 8.0f };
    const float width = 990.0f;                          // largeur utile (bras tendu compris)
}

static float smoothstep (float e0, float e1, float x)
{
    const float t = jlimit (0.0f, 1.0f, (x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

//==============================================================================
AdrienView::AdrienView()
{
    setInterceptsMouseClicks (false, false);

    body = ImageCache::getFromMemory (BinaryData::adrien_body_png, BinaryData::adrien_body_pngSize);
    arm  = ImageCache::getFromMemory (BinaryData::adrien_arm_png,  BinaryData::adrien_arm_pngSize);

    mouthRoi = Rectangle<int> (294, 132, 70, 42);

    Random r (42);
    strands.reserve (190);
    for (int i = 0; i < 190; ++i)
        strands.push_back ({ r.nextFloat() * 2.0f - 1.0f,          // position le long du crâne
                             0.55f + r.nextFloat() * 0.9f,           // longueur relative
                             r.nextFloat() * 2.0f - 1.0f,            // boucle
                             r.nextFloat() * MathConstants<float>::twoPi,
                             r.nextFloat(),                          // teinte
                             1.1f + r.nextFloat() * 1.3f });         // épaisseur
}

void AdrienView::resized()
{
    // Adrien remplit la vue, pieds en bas
    baseScale = jmin ((float) getWidth() / Asset::width, (float) getHeight() / (float) body.getHeight());
    baseOffset = { (float) getWidth() - Asset::width * baseScale, (float) getHeight() - body.getHeight() * baseScale - 2.0f };
}

//==============================================================================
void AdrienView::setState (const VisualState& s)
{
    state = s;

    // bras : poursuite douce de la cible
    armAngle += (armTarget - armAngle) * 0.22f;

    // pompage audio : attaque rapide, relâche lente
    const float lvl = jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (s.level, -60.0f) + 42.0f) / 42.0f);
    bob = lvl > bob ? bob + (lvl - bob) * 0.6f : bob + (lvl - bob) * 0.12f;

    // tremblement quand ça chauffe
    const float heat = s.heat();
    const float amp = smoothstep (0.45f, 1.0f, heat) * 3.5f + s.bandLevel * heat * 6.0f;
    shake = { (rng.nextFloat() * 2.0f - 1.0f) * amp, (rng.nextFloat() * 2.0f - 1.0f) * amp * 0.6f };

    // sourire (BOOST) ou moue (ATTEN) ; grimace quand ça clippe
    float smile = (s.boost - s.atten) / 10.0f;
    if (s.clipping) smile = -1.0f;
    updateMouth (smile);
    repaint();
}

void AdrienView::setPointTarget (Point<float> targetInParent)
{
    auto local = targetInParent - getPosition().toFloat();
    auto assetPt = local.transformedBy (currentTransform().inverted());

    const auto toTarget = assetPt - Asset::armPivot;
    const auto toTip = Asset::fingertip - Asset::armPivot;
    float a = std::atan2 (toTarget.y, toTarget.x) - std::atan2 (toTip.y, toTip.x);
    armTarget = jlimit (degreesToRadians (-16.0f), degreesToRadians (16.0f), a);
}

AffineTransform AdrienView::currentTransform() const
{
    const auto& s = state;
    const float age = s.reactionAge;
    const float pi = MathConstants<float>::pi;
    const float pulse = 1.0f + bob * 0.018f;

    float wobble = 0.0f;
    if (s.satType == 0) // TAPE : léger pleurage
        wobble = std::sin ((float) s.time * 1.7f) * s.heat() * 3.0f;

    float dy = 0.0f, rot = 0.0f, sc = 1.0f;

    // FREQUENCY : petit sursaut à chaque cran
    if (s.reaction == Reaction::Freq && age < 0.3f)
        dy -= std::sin (pi * age / 0.3f) * 30.0f;

    // SAT DRY/WET : il hoche la tête, plus fort quand le mix monte
    if (s.reaction == Reaction::SatMix && age < 1.6f)
        dy += std::sin (age * 16.0f) * 12.0f * (1.0f - age / 1.6f) * (0.3f + 0.7f * s.satMix);

    // AUTO GAIN off : haussement d'épaules (deux fois)
    if (s.reaction == Reaction::AutoGainOff && age < 1.0f)
        dy -= std::sin (pi * std::fmod (age, 0.5f) / 0.5f) * 16.0f;

    // EQ DRY/WET : il penche la tête pour écouter
    rot += 0.07f * s.reactionAmount (Reaction::EqMix);

    // OUTPUT : il recule quand ça monte, se penche quand ça baisse
    sc *= 1.0f - 0.08f * s.reactionAmount (Reaction::OutputUp);
    sc *= 1.0f + 0.05f * s.reactionAmount (Reaction::OutputDown);
    rot -= 0.05f * s.reactionAmount (Reaction::OutputDown);

    // son fort : headbang
    if (s.loud)
        rot += 0.05f * std::sin ((float) s.time * 2.0f * pi * 2.2f);

    const auto anim = AffineTransform::scale (pulse, 1.0f + bob * 0.025f, Asset::feet.x, Asset::feet.y)
                          .followedBy (AffineTransform::rotation (rot, Asset::torso.x, Asset::feet.y))
                          .followedBy (AffineTransform::scale (sc, sc, Asset::feet.x, Asset::feet.y))
                          .translated (wobble, dy - bob * 3.0f);

    // photo retournée : Adrien regarde et pointe vers la gauche
    const auto mirror = AffineTransform::scale (-1.0f, 1.0f).translated (Asset::width, 0.0f);

    return anim.followedBy (mirror)
               .scaled (baseScale)
               .translated (baseOffset + shake);
}

AffineTransform AdrienView::armTransform() const
{
    return AffineTransform::rotation (armAngle, Asset::armPivot.x, Asset::armPivot.y);
}

Point<float> AdrienView::getPalmInParent() const
{
    return Asset::palm.transformedBy (armTransform().followedBy (currentTransform())) + getPosition().toFloat();
}

Point<float> AdrienView::getHeadTopInParent() const
{
    return Asset::headTop.transformedBy (currentTransform()) + getPosition().toFloat();
}

//==============================================================================
//  Sourire / moue : on déforme la zone de la bouche (les commissures montent ou descendent)
void AdrienView::updateMouth (float smile)
{
    smile = jlimit (-1.0f, 1.0f, smile);
    if (std::abs (smile - lastSmile) < 0.01f)
        return;
    lastSmile = smile;

    if (std::abs (smile) < 0.005f)
    {
        mouthPatch = {};
        return;
    }

    mouthPatch = Image (Image::ARGB, mouthRoi.getWidth(), mouthRoi.getHeight(), true);
    const Image::BitmapData src (body, Image::BitmapData::readOnly);
    Image::BitmapData dst (mouthPatch, Image::BitmapData::writeOnly);

    const float maxLift = 15.0f;   // pixels asset (plus marqué qu'en V1 : Adrien est plus petit)
    const float sigma = 8.0f;

    auto sample = [&] (float x, float y) -> Colour
    {
        x = jlimit (0.0f, (float) body.getWidth() - 2.0f, x);
        y = jlimit (0.0f, (float) body.getHeight() - 2.0f, y);
        const int x0 = (int) x, y0 = (int) y;
        const float fx = x - x0, fy = y - y0;
        auto c00 = src.getPixelColour (x0, y0),     c10 = src.getPixelColour (x0 + 1, y0);
        auto c01 = src.getPixelColour (x0, y0 + 1), c11 = src.getPixelColour (x0 + 1, y0 + 1);
        return c00.interpolatedWith (c10, fx).interpolatedWith (c01.interpolatedWith (c11, fx), fy);
    };

    for (int py = 0; py < mouthRoi.getHeight(); ++py)
    {
        for (int px = 0; px < mouthRoi.getWidth(); ++px)
        {
            const float x = (float) (mouthRoi.getX() + px);
            const float y = (float) (mouthRoi.getY() + py);
            const float u = (x - Asset::mouth.x) / Asset::mouthHalfWidth;
            const float gy = std::exp (-std::pow ((y - Asset::mouth.y) / sigma, 2.0f));
            // poids nul sur les bords du patch pour éviter toute couture
            const float edgeX = 1.0f - smoothstep (1.05f, 1.35f, std::abs (u));
            const float edgeY = 1.0f - smoothstep (0.75f, 1.0f, std::abs ((float) py - mouthRoi.getHeight() * 0.5f) / (mouthRoi.getHeight() * 0.5f));
            const float w = gy * edgeX * edgeY;

            const float lift = smile * maxLift * (u * u) * w;       // commissures
            const float centre = -smile * 1.5f * (1.0f - u * u) * w; // le centre bouge un peu à l'inverse
            const float stretch = smile * 2.0f * u * w;              // élargit le sourire
            dst.setPixelColour (px, py, sample (x - stretch, y + lift + (smile > 0 ? centre * 0.0f : centre)));
        }
    }
}

//==============================================================================
void AdrienView::paint (Graphics& g)
{
    const float heat = state.heat();
    const bool crush = state.satType == 3 && heat > 0.03f;

    if (crush)
    {
        // CRUSH : Adrien se pixelise
        const int px = 1 + roundToInt (heat * 10.0f);
        Image small (Image::ARGB, jmax (1, getWidth() / px), jmax (1, getHeight() / px), true);
        {
            Graphics gs (small);
            gs.addTransform (AffineTransform::scale (1.0f / (float) px));
            drawAdrien (gs, currentTransform());
            drawReactions (gs, currentTransform());
        }
        g.setImageResamplingQuality (Graphics::lowResamplingQuality);
        g.drawImage (small, getLocalBounds().toFloat().withSize ((float) small.getWidth() * px, (float) small.getHeight() * px));
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        drawFx (g, currentTransform());
    }
    else
    {
        drawAdrien (g, currentTransform());
        drawReactions (g, currentTransform());
        drawFx (g, currentTransform());
    }
}

void AdrienView::drawAdrien (Graphics& g, const AffineTransform& t)
{
    const float heat = state.heat();
    const float boost01 = state.boost / 10.0f;
    const float atten01 = state.atten / 10.0f;
    const auto typeCol = AdrienColours::forSatType (state.satType);

    g.setImageResamplingQuality (Graphics::highResamplingQuality);

    // --- halo derrière lui (WARM / DIODE) ---
    if ((state.satType == 1 || state.satType == 4) && heat > 0.02f)
    {
        const auto c = Asset::torso.transformedBy (t);
        const float r = 430.0f * baseScale * (1.0f + 0.08f * bob);
        ColourGradient grad (typeCol.withAlpha (0.42f * heat), c, typeCol.withAlpha (0.0f), c.translated (r, 0), true);
        g.setGradientFill (grad);
        g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c));
    }

    // --- bras tendu (sous le corps : la manche cache la jonction) ---
    const auto armT = AffineTransform::translation (Asset::armOffset).followedBy (armTransform()).followedBy (t);
    g.drawImageTransformed (arm, armT);

    // --- corps ---
    g.drawImageTransformed (body, t);

    // --- bouche déformée ---
    if (mouthPatch.isValid())
        g.drawImageTransformed (mouthPatch, AffineTransform::translation (mouthRoi.getPosition().toFloat()).followedBy (t));

    // --- joues : sourire ou surchauffe ---
    {
        const float blush = jmax (smoothstep (0.2f, 1.0f, boost01 - atten01) * 0.4f, heat * 0.5f);
        if (blush > 0.01f)
        {
            for (auto ch : { Asset::cheekL, Asset::cheekR })
            {
                const auto c = ch.transformedBy (t);
                const float r = 16.0f * baseScale;
                ColourGradient grad (Colour (0xffff4a3a).withAlpha (blush), c, Colour (0xffff4a3a).withAlpha (0.0f), c.translated (r, 0), true);
                g.setGradientFill (grad);
                g.fillEllipse (Rectangle<float> (r * 2.0f, r * 1.4f).withCentre (c));
            }
        }
    }

    // --- teintes de saturation sur la silhouette ---
    if (heat > 0.01f)
    {
        // une seule silhouette (corps + bras) pour éviter les doubles teintes à la jonction
        updateSilhouette();
        const auto silT = AffineTransform::scale (2.0f).followedBy (t);
        g.setColour (AdrienColours::hot.withAlpha (0.22f * heat));
        g.drawImageTransformed (silhouette, silT, true);

        float a = 0.0f;
        switch (state.satType)
        {
            case 0: a = 0.30f; break;   // tape : sépia
            case 1: a = 0.12f; break;
            case 2: a = 0.16f; break;
            case 4: a = 0.20f; break;
            default: break;
        }
        if (a > 0.0f)
        {
            g.setColour (typeCol.withAlpha (a * heat));
            g.drawImageTransformed (silhouette, silT, true);
        }
    }

    // --- cheveux (BOOST, et FUZZ qui fait friser) : par-dessus les teintes ---
    drawHair (g, t);
}

void AdrienView::updateSilhouette()
{
    if (silhouette.isValid() && std::abs (armAngle - silAngle) < 0.003f)
        return;
    silAngle = armAngle;
    silhouette = Image (Image::ARGB, body.getWidth() / 2, body.getHeight() / 2, true);
    Graphics gs (silhouette);
    gs.addTransform (AffineTransform::scale (0.5f));
    gs.drawImageTransformed (arm, AffineTransform::translation (Asset::armOffset).followedBy (armTransform()));
    gs.drawImageTransformed (body, {});
}

void AdrienView::drawHair (Graphics& g, const AffineTransform& t)
{
    const float heat = state.heat();
    const float fuzz = state.satType == 2 ? heat : 0.0f;
    // V2 : plus de banane avec le BOOST, seul le FUZZ fait pousser et friser les cheveux
    const float hair = fuzz * 0.5f;
    if (hair < 0.02f)
        return;

    // Une banane (pompadour) qui gonfle avec le BOOST.
    const float cx = Asset::scalp.x;
    const float grow = smoothstep (0.0f, 1.0f, hair);
    const float rx = 74.0f + 8.0f * grow;
    const float ry = 92.0f + 36.0f * grow;
    const float cy = 102.0f - 20.0f * grow;               // sommet = cy - ry : de 10 à -46
    const float hairlineY = 44.0f + 4.0f * (1.0f - grow); // ligne frontale
    const float templeY = 96.0f;

    auto hairline = [&] (float x)                         // y de la ligne de cheveux
    {
        const float u = (x - cx) / 72.0f;
        return hairlineY + (templeY - hairlineY) * u * u * u * u - 3.0f * std::exp (-u * u * 40.0f);
    };
    auto massPoint = [&] (float theta)                    // 0 = sommet
    {
        return Point<float> (cx + rx * std::sin (theta), cy - ry * std::cos (theta));
    };

    // masse : de la tempe gauche à la tempe droite en passant par le sommet
    Path mass;
    const float edge = 1.5f;
    mass.startNewSubPath (massPoint (-edge));
    for (int i = 0; i <= 24; ++i)
    {
        const float x = massPoint (-edge).x + (massPoint (edge).x - massPoint (-edge).x) * (float) i / 24.0f;
        mass.lineTo (x, hairline (x));
    }
    mass.addCentredArc (cx, cy, rx, ry, 0.0f, edge, -edge, false);
    mass.closeSubPath();

    const float alpha = smoothstep (0.05f, 0.45f, hair);
    const auto dark = Colour (0xff1e150f), mid = Colour (0xff3b2a1e), light = Colour (0xff6b4b31);
    {
        ColourGradient grad (mid.withAlpha (alpha), cx - rx * 0.3f, cy - ry, dark.withAlpha (alpha), cx, hairlineY + 20.0f, false);
        g.setGradientFill (grad);
        g.fillPath (mass, t);
    }

    // mèches peignées vers le haut, qui suivent le volume
    const int count = (int) (strands.size() * smoothstep (0.05f, 0.6f, hair));
    const float sway = 0.6f * bob + 0.3f * std::sin ((float) state.time * 1.4f);
    for (int i = 0; i < count; ++i)
    {
        const auto& s = strands[(size_t) i];
        const float u = s.t;                                       // -1..1
        const float x0 = cx + u * 66.0f;
        const Point<float> root (x0, hairline (x0) + 2.0f);
        const float theta = u * 1.15f + s.curl * 0.12f;
        const auto end = massPoint (theta) + Point<float> (0.0f, 4.0f + 6.0f * s.len);
        const auto c1 = Point<float> (root.x + (end.x - root.x) * 0.15f, root.y - (root.y - end.y) * 0.55f);
        const auto c2 = Point<float> (end.x - std::sin (theta) * 6.0f + s.curl * 10.0f * grow,
                                      end.y + 14.0f + 8.0f * s.len) + Point<float> (sway * std::sin (s.phase) * 3.0f, 0.0f);

        Path p;
        p.startNewSubPath (root);
        if (fuzz > 0.05f)
        {
            // FUZZ : les mèches frisent et crépitent
            const int segs = 10;
            for (int k = 1; k <= segs; ++k)
            {
                const float f = (float) k / segs;
                const float mf = 1.0f - f;
                const auto pt = root * (mf * mf * mf) + c1 * (3 * mf * mf * f) + c2 * (3 * mf * f * f) + end * (f * f * f);
                const float zz = ((k % 2) ? 1.0f : -1.0f) * fuzz * 4.0f * (0.6f + 0.4f * std::sin (s.phase + (float) state.time * 11.0f));
                p.lineTo (pt + Point<float> (zz, zz * 0.5f));
            }
        }
        else
        {
            p.cubicTo (c1, c2, end);
        }

        const auto col = dark.interpolatedWith (light, s.shade * 0.75f);
        g.setColour (col.withAlpha (alpha * (0.55f + 0.4f * s.shade)));
        g.strokePath (p, PathStrokeType (s.width * 1.25f, PathStrokeType::curved, PathStrokeType::rounded), t);
    }

    // reflet brillantine
    if (grow > 0.3f)
    {
        Path shine;
        shine.addCentredArc (cx - 8.0f, cy + 8.0f, rx * 0.72f, ry * 0.8f, 0.0f, -0.9f, 0.35f, true);
        g.setColour (Colour (0xffb08a5e).withAlpha (0.35f * smoothstep (0.3f, 1.0f, grow)));
        g.strokePath (shine, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded), t);
    }

    // FUZZ : quelques épis électriques autour
    if (fuzz > 0.2f)
    {
        Random r ((int64) (state.time * 12.0));
        g.setColour (dark.withAlpha (0.9f));
        for (int k = 0; k < (int) (fuzz * 26.0f); ++k)
        {
            const float th = (r.nextFloat() * 2.0f - 1.0f) * 1.5f;
            const auto a = massPoint (th);
            const auto dir = Point<float> (std::sin (th), -std::cos (th));
            Path spike;
            spike.startNewSubPath (a);
            spike.lineTo (a + dir * 6.0f + Point<float> (4.0f, 0.0f));
            spike.lineTo (a + dir * 12.0f * fuzz - Point<float> (3.0f, 0.0f));
            g.strokePath (spike, PathStrokeType (1.4f), t);
        }
    }
}

void AdrienView::drawFx (Graphics& g, const AffineTransform& t)
{
    const float heat = state.heat();
    const float time = (float) state.time;

    // gouttes de sueur
    if (heat > 0.3f)
    {
        const int drops = 1 + (int) ((heat - 0.3f) * 4.0f);
        const Point<float> spots[] = { { 272.0f, 70.0f }, { 396.0f, 64.0f }, { 262.0f, 112.0f }, { 404.0f, 108.0f } };
        for (int i = 0; i < jmin (drops, 4); ++i)
        {
            const float ph = std::fmod (time * 0.9f + i * 0.37f, 1.0f);
            const auto p = (spots[i] + Point<float> (0.0f, ph * 70.0f)).transformedBy (t);
            const float s = (5.0f + 2.0f * heat) * baseScale * 1.8f;
            Path drop;
            drop.startNewSubPath (p.x, p.y - s * 1.6f);
            drop.quadraticTo (p.x + s, p.y, p.x, p.y + s * 0.7f);
            drop.quadraticTo (p.x - s, p.y, p.x, p.y - s * 1.6f);
            g.setColour (Colour (0xffcfeeff).withAlpha (0.85f * (1.0f - ph)));
            g.fillPath (drop);
            g.setColour (Colours::white.withAlpha (0.9f * (1.0f - ph)));
            g.fillEllipse (p.x - s * 0.35f, p.y - s * 0.1f, s * 0.3f, s * 0.3f);
        }
    }

    // vapeur au-dessus de la tête
    if (heat > 0.55f)
    {
        const float k = smoothstep (0.55f, 1.0f, heat);
        for (int i = 0; i < 5; ++i)
        {
            const float ph = std::fmod (time * 0.55f + i / 5.0f, 1.0f);
            const auto p = Point<float> (334.0f + std::sin (ph * 6.0f + i) * 24.0f, 8.0f - ph * 130.0f).transformedBy (t);
            const float r = (10.0f + ph * 30.0f) * baseScale * 1.6f;
            ColourGradient puff (Colours::white.withAlpha (0.22f * k * (1.0f - ph)), p,
                                 Colours::white.withAlpha (0.0f), p.translated (r, 0.0f), true);
            g.setGradientFill (puff);
            g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (p));
        }
    }

    // DIODE : arcs électriques autour de la tête
    if (state.satType == 4 && heat > 0.2f)
    {
        Random r ((int64) (time * 15.0));
        const int bolts = 1 + (int) (heat * 4.0f);
        g.setColour (AdrienColours::forSatType (4).withAlpha (0.85f));
        for (int b = 0; b < bolts; ++b)
        {
            const float a = r.nextFloat() * MathConstants<float>::twoPi;
            Point<float> p (Asset::scalp.x + std::sin (a) * 95.0f, Asset::scalp.y - std::cos (a) * 110.0f);
            Path bolt;
            bolt.startNewSubPath (p.transformedBy (t));
            for (int k = 0; k < 5; ++k)
            {
                p += Point<float> (std::sin (a) * 14.0f + (r.nextFloat() - 0.5f) * 22.0f, -std::cos (a) * 14.0f + (r.nextFloat() - 0.5f) * 22.0f);
                bolt.lineTo (p.transformedBy (t));
            }
            g.strokePath (bolt, PathStrokeType (1.6f));
        }
    }

    // FUZZ : grain
    if (state.satType == 2 && heat > 0.05f)
    {
        Random r ((int64) (time * 30.0));
        const auto area = Rectangle<float> (0, 0, 990, 1034).transformedBy (t);
        const int n = (int) (400 * heat);
        for (int i = 0; i < n; ++i)
        {
            g.setColour ((r.nextBool() ? Colours::white : Colours::black).withAlpha (0.18f * heat));
            g.fillRect (area.getX() + r.nextFloat() * area.getWidth(), area.getY() + r.nextFloat() * area.getHeight(), 1.5f, 1.5f);
        }
    }

    // TAPE : lignes de bande
    if (state.satType == 0 && heat > 0.05f)
    {
        g.setColour (Colours::black.withAlpha (0.10f * heat));
        const float off = std::fmod (time * 20.0f, 4.0f);
        for (float y = off; y < (float) getHeight(); y += 4.0f)
            g.fillRect (0.0f, y, (float) getWidth(), 1.0f);
    }
}

//==============================================================================
//  Réactions dessinées par le plugin (pas de nouvelle photo) : pouce, casque, main à
//  l'oreille, mains sur les oreilles, cornes, bâillement, welsh, clin d'œil, CLIP.
//  Les formes sont en coordonnées asset (donc retournées avec Adrien) ; les textes sont
//  posés en coordonnées écran pour rester lisibles.
//==============================================================================
static void fillSkin (Graphics& g, const Path& p, const AffineTransform& t, float alpha)
{
    g.setColour (AdrienColours::skin.withAlpha (alpha));
    g.fillPath (p, t);
    g.setColour (Colour (0xff6b3f2a).withAlpha (alpha * 0.9f));
    g.strokePath (p, PathStrokeType (4.0f), t);
}

void AdrienView::drawReactions (Graphics& g, const AffineTransform& t)
{
    const auto& s = state;
    const float time = (float) s.time;
    const float pi = MathConstants<float>::pi;
    const auto green = Colour (0xff3fe07d);

    // --- son fort : cornes du diable ---
    if (s.loud)
    {
        for (int side = -1; side <= 1; side += 2)
        {
            const float x = Asset::headTop.x + (float) side * 38.0f;
            Path horn;
            horn.startNewSubPath (x - 14.0f, 30.0f);
            horn.quadraticTo (x + side * 10.0f, 4.0f, x + side * 26.0f, -40.0f);
            horn.quadraticTo (x + side * 2.0f, -2.0f, x + 14.0f, 28.0f);
            horn.closeSubPath();
            g.setColour (Colour (0xffc8231e));
            g.fillPath (horn, t);
            g.setColour (Colour (0xff6a0f0c));
            g.strokePath (horn, PathStrokeType (3.0f), t);
        }
    }

    // --- AUTO GAIN on : le casque ---
    if (const float a = s.reactionAmount (Reaction::AutoGainOn, 4.0f); a > 0.0f)
    {
        Path band;
        band.addCentredArc (Asset::scalp.x + 2.0f, Asset::scalp.y + 20.0f, 84.0f, 112.0f, 0.0f, -1.75f, 1.75f, true);
        g.setColour (Colour (0xff25292b).withAlpha (a));
        g.strokePath (band, PathStrokeType (13.0f, PathStrokeType::curved, PathStrokeType::rounded), t);
        g.setColour (Colours::white.withAlpha (0.25f * a));
        g.strokePath (band, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded), t);

        for (auto ear : { Asset::earL, Asset::earR })
        {
            Path cup;
            cup.addRoundedRectangle (Rectangle<float> (36.0f, 60.0f).withCentre (ear), 14.0f);
            g.setColour (Colour (0xff1a1c1d).withAlpha (a));
            g.fillPath (cup, t);
            g.setColour (green.withAlpha (a));
            g.strokePath (cup, PathStrokeType (4.0f), t);
        }
    }

    // --- pouce levé : Pultec trick, ou AUTO GAIN on ---
    {
        const float a = jmax (s.reactionAmount (Reaction::Pultec, 3.0f), s.reactionAmount (Reaction::AutoGainOn, 3.0f));
        if (a > 0.0f)
        {
            const auto c = Asset::hand.translated (0.0f, -110.0f);
            const float pop = 1.1f + 0.4f * a;
            const auto local = AffineTransform::scale (pop, pop, c.x, c.y).followedBy (t);

            Path glow;
            glow.addEllipse (Rectangle<float> (130.0f, 130.0f).withCentre (c));
            g.setColour (green.withAlpha (0.25f * a));
            g.fillPath (glow, local);

            Path fist;
            fist.addRoundedRectangle (c.x - 30.0f, c.y - 4.0f, 60.0f, 50.0f, 14.0f);
            Path thumb;
            thumb.addRoundedRectangle (c.x - 30.0f, c.y - 52.0f, 22.0f, 58.0f, 11.0f);
            fillSkin (g, thumb, local, a);
            fillSkin (g, fist, local, a);
            Path fingers;
            for (int k = 1; k <= 3; ++k)
            {
                fingers.startNewSubPath (c.x - 4.0f, c.y - 4.0f + (float) k * 12.0f);
                fingers.lineTo (c.x + 28.0f, c.y - 4.0f + (float) k * 12.0f);
            }
            g.setColour (Colour (0xff6b3f2a).withAlpha (0.6f * a));
            g.strokePath (fingers, PathStrokeType (3.0f), local);
        }
    }

    // --- EQ DRY/WET : main à l'oreille, il écoute ---
    if (const float a = s.reactionAmount (Reaction::EqMix); a > 0.0f)
    {
        Path cupHand;
        cupHand.addEllipse (Rectangle<float> (32.0f, 50.0f).withCentre (Asset::earR.translated (12.0f, 0.0f)));
        fillSkin (g, cupHand, t, a);
        for (int k = 0; k < 3; ++k)
        {
            const float ph = std::fmod (time * 1.5f + (float) k / 3.0f, 1.0f);
            Path arc;
            const float r = 30.0f + ph * 40.0f;
            arc.addCentredArc (Asset::earR.x + 20.0f, Asset::earR.y, r, r, 0.0f, pi * 0.25f, pi * 0.75f, true);
            g.setColour (green.withAlpha (a * (1.0f - ph)));
            g.strokePath (arc, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded), t);
        }
    }

    // --- OUTPUT au-delà de 0 dB : mains sur les oreilles ---
    if (const float a = s.reactionAmount (Reaction::OutputUp); a > 0.0f && s.outputDb > 0.0f)
    {
        for (auto ear : { Asset::earL, Asset::earR })
        {
            Path h;
            h.addEllipse (Rectangle<float> (38.0f, 56.0f).withCentre (ear));
            fillSkin (g, h, t, a);
        }
    }

    // AUTO GAIN off : le haussement d'épaules est dans currentTransform() (plus de « ? » flottants)

    // --- double-clic : clin d'œil ---
    if (s.reaction == Reaction::Reset && s.reactionAge < 0.9f)
    {
        Path lid;
        lid.addEllipse (Rectangle<float> (34.0f, 18.0f).withCentre (Asset::eyeR));
        g.setColour (AdrienColours::skin.darker (0.1f));
        g.fillPath (lid, t);
        Path lash;
        lash.startNewSubPath (Asset::eyeR.x - 17.0f, Asset::eyeR.y + 1.0f);
        lash.quadraticTo (Asset::eyeR.x, Asset::eyeR.y + 8.0f, Asset::eyeR.x + 17.0f, Asset::eyeR.y + 1.0f);
        g.setColour (Colour (0xff2a1a12));
        g.strokePath (lash, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded), t);
    }

    // --- pas de son : il bâille ---
    if (s.silent && ! s.idle && ! s.clipping && s.reactionAge > 4.0f)
    {
        const float ph = std::fmod (time / 3.5f, 1.0f);
        const float yawn = ph < 0.5f ? std::sin (pi * ph / 0.5f) : 0.0f;
        if (yawn > 0.02f)
        {
            Path m;
            m.addEllipse (Rectangle<float> (26.0f * yawn + 6.0f, 36.0f * yawn).withCentre (Asset::mouth.translated (0.0f, 4.0f)));
            g.setColour (Colour (0xff2b0e0a));
            g.fillPath (m, t);
        }
    }

    // --- rien touché depuis 20 s : pause welsh ---
    if (s.idle)
    {
        const auto c = Asset::hand.translated (10.0f, -14.0f);
        const auto big = AffineTransform::scale (1.5f, 1.5f, c.x, c.y + 22.0f).followedBy (t);
        Path plate;
        plate.addEllipse (Rectangle<float> (150.0f, 34.0f).withCentre (c.translated (0.0f, 22.0f)));
        g.setColour (Colours::white);
        g.fillPath (plate, big);
        g.setColour (Colour (0xff9a9f9b));
        g.strokePath (plate, PathStrokeType (3.0f), big);

        Path bread;
        bread.addRoundedRectangle (c.x - 52.0f, c.y - 6.0f, 104.0f, 26.0f, 6.0f);
        g.setColour (Colour (0xffa86b32));
        g.fillPath (bread, big);

        Path cheddar;   // le cheddar fondu qui coule
        cheddar.startNewSubPath (c.x - 56.0f, c.y - 4.0f);
        cheddar.quadraticTo (c.x, c.y - 26.0f, c.x + 56.0f, c.y - 4.0f);
        cheddar.lineTo (c.x + 50.0f, c.y + 10.0f);
        cheddar.quadraticTo (c.x + 40.0f, c.y + 2.0f, c.x + 30.0f, c.y + 12.0f);
        cheddar.quadraticTo (c.x + 10.0f, c.y + 2.0f, c.x - 10.0f, c.y + 16.0f);
        cheddar.quadraticTo (c.x - 30.0f, c.y + 2.0f, c.x - 56.0f, c.y + 8.0f);
        cheddar.closeSubPath();
        g.setColour (Colour (0xfff0921e));
        g.fillPath (cheddar, big);
        g.setColour (Colour (0xffb4580e));
        g.strokePath (cheddar, PathStrokeType (2.5f), big);

        for (int k = 0; k < 3; ++k)   // ça fume
        {
            const float ph = std::fmod (time * 0.6f + (float) k / 3.0f, 1.0f);
            Path steam;
            const float x = c.x - 30.0f + (float) k * 30.0f;
            steam.startNewSubPath (x, c.y - 20.0f - ph * 20.0f);
            for (int j = 1; j <= 6; ++j)
                steam.lineTo (x + std::sin ((float) j + time * 4.0f) * 6.0f, c.y - 20.0f - ph * 20.0f - (float) j * 10.0f);
            g.setColour (Colours::white.withAlpha (0.7f * (1.0f - ph)));
            g.strokePath (steam, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded), big);
        }

        // il mâche
        const float chew = std::abs (std::sin (time * 6.0f));
        Path m;
        m.addEllipse (Rectangle<float> (22.0f, 4.0f + 10.0f * chew).withCentre (Asset::mouth.translated (0.0f, 3.0f)));
        g.setColour (Colour (0xff2b0e0a).withAlpha (0.85f));
        g.fillPath (m, t);
    }

    // --- CLIP : panneau rouge qui clignote ---
    if (s.clipping && std::fmod (time * 6.0f, 2.0f) < 1.4f)
    {
        // sur son t-shirt : au-dessus de la tête, la place est pour la bulle
        const float textScale = baseScale / 0.36f;
        const auto p = Asset::mouth.translated (0.0f, 170.0f).transformedBy (t);
        const auto r = Rectangle<float> (70.0f * textScale, 26.0f * textScale).withCentre (p);
        g.setColour (Colour (0xffd8261c));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Colours::white);
        g.drawRoundedRectangle (r.reduced (2.0f), 4.0f, 1.5f);
        g.setFont (FontOptions (15.0f * textScale, Font::bold));
        g.drawText ("CLIP", r, Justification::centred);
    }
}
