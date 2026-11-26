#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Repris de Phosphor XY v2 (même auteur) : gris clair mat, encre sombre, un seul accent vert.
namespace Theme
{
    inline const juce::Colour panelTop    { 0xffdadcd9 };
    inline const juce::Colour panelBottom { 0xffc8cbc7 };
    inline const juce::Colour ink         { 0xff25292b };
    inline const juce::Colour inkSoft     { 0xff6b7174 };
    inline const juce::Colour line        { 0xff9a9f9b };
    inline const juce::Colour recess      { 0xffe8eae7 };
    inline const juce::Colour accent      { 0xff1f9d57 };
    inline const juce::Colour accentLit   { 0xff3fe07d };
    inline const juce::Colour crtGreen    { 0xff7dffa0 };
}

inline juce::Font uiFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::Font monoFont (float height)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
}

// Esprit appareils de laboratoire des annees 60 (Braun / Dieter Rams) :
// surfaces mates, boutons sobres (aluminium brosse ou noirs), touches plates, un seul accent de couleur.
class BraunLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BraunLookAndFeel()
    {
        using juce::Colour;
        setColour (juce::Slider::textBoxTextColourId, Theme::ink);
        setColour (juce::Slider::textBoxOutlineColourId, Theme::line);
        setColour (juce::Slider::textBoxBackgroundColourId, Theme::recess);
        setColour (juce::Slider::textBoxHighlightColourId, Theme::accent.withAlpha (0.4f));
        setColour (juce::Label::textColourId, Theme::ink);
        setColour (juce::ComboBox::backgroundColourId, Theme::recess);
        setColour (juce::ComboBox::textColourId, Theme::ink);
        setColour (juce::ComboBox::outlineColourId, Theme::line);
        setColour (juce::ComboBox::arrowColourId, Theme::ink);
        setColour (juce::PopupMenu::backgroundColourId, Theme::recess);
        setColour (juce::PopupMenu::textColourId, Theme::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::accent);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }

    juce::Font getLabelFont (juce::Label&) override { return uiFont (11.0f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return uiFont (11.0f, true); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        using juce::Colour;
        // Capuchon façon Pittsburgh Modular : cylindre mat d'une seule couleur, dessus plat,
        // trait repère fin. La couleur vient de la propriété "cap" du slider.
        const Colour cap ((juce::uint32) (int) slider.getProperties().getWithDefault ("cap", (int) 0xff1d1f20));
        const bool dark = cap.getPerceivedBrightness() < 0.35f;

        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto c = bounds.getCentre();
        const float outerR = size * 0.5f;

        // graduations imprimées (un point par cran pour un bouton cranté, comme sur la façade Pittsburgh)
        const int ticks = (int) slider.getProperties().getWithDefault ("ticks", 10);
        for (int i = 0; i <= ticks; ++i)
        {
            const float a = startAngle + (float) i / (float) ticks * (endAngle - startAngle);
            const bool major = ticks == 10 ? (i % 5) == 0 : true;
            g.setColour (Theme::inkSoft.withAlpha (major ? 1.0f : 0.65f));
            if (ticks == 10)
                g.drawLine (juce::Line<float> (c.getPointOnCircumference (outerR * (major ? 0.82f : 0.87f), a),
                                               c.getPointOnCircumference (outerR * 0.97f, a)),
                            major ? 1.4f : 1.0f);
            else
                g.fillEllipse (juce::Rectangle<float> (3.2f, 3.2f).withCentre (c.getPointOnCircumference (outerR * 0.91f, a)));
        }

        const float R = outerR * 0.68f;          // rayon du capuchon
        const float h = R * 0.16f;               // hauteur visible du cylindre
        const float a = startAngle + sliderPos * (endAngle - startAngle);
        const auto top = c.translated (0.0f, -h * 0.5f);

        // ombre portée douce
        g.setColour (juce::Colours::black.withAlpha (0.16f));
        g.fillEllipse (juce::Rectangle<float> (R * 2.0f + 4.0f, R * 2.0f + 4.0f).withCentre (c.translated (1.0f, h + 2.0f)));
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillEllipse (juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c.translated (0.0f, h * 0.9f)));

        // flanc du cylindre : même couleur, plus sombre, légèrement strié
        {
            juce::Path side;
            side.addEllipse (juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c.translated (0.0f, h * 0.5f)));
            g.setGradientFill (juce::ColourGradient (cap.darker (0.55f), c.x - R, c.y,
                                                     cap.darker (0.85f), c.x + R, c.y, false));
            g.fillPath (side);
            g.setColour (juce::Colours::black.withAlpha (0.10f));
            for (int i = 0; i < 24; ++i)
            {
                const float ang = juce::MathConstants<float>::twoPi * (float) i / 24.0f;
                const auto p1 = c.translated (0.0f, h * 0.5f).getPointOnCircumference (R * 0.97f, ang);
                const auto p2 = c.translated (0.0f, h * 0.5f).getPointOnCircumference (R, ang);
                g.drawLine (juce::Line<float> (p1, p2), 1.0f);
            }
        }

        // dessus plat, mat : un dégradé très léger, pas de reflet chromé
        const float T = R * 0.94f;
        g.setGradientFill (juce::ColourGradient (cap.brighter (0.10f), top.x - T * 0.6f, top.y - T * 0.8f,
                                                 cap.darker (0.12f), top.x + T * 0.6f, top.y + T * 0.9f, false));
        g.fillEllipse (juce::Rectangle<float> (T * 2.0f, T * 2.0f).withCentre (top));
        g.setColour (juce::Colours::white.withAlpha (dark ? 0.10f : 0.35f));
        g.drawEllipse (juce::Rectangle<float> (T * 2.0f - 1.0f, T * 2.0f - 1.0f).withCentre (top), 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawEllipse (juce::Rectangle<float> (T * 2.0f, T * 2.0f).withCentre (top), 0.8f);

        // trait repère : du centre jusqu'au bord
        g.setColour (dark ? Colour (0xfff2f0ea) : Colour (0xff1b1c1d));
        g.drawLine (juce::Line<float> (top.getPointOnCircumference (T * 0.18f, a),
                                       top.getPointOnCircumference (T * 0.94f, a)), 2.8f);
    }

    // Touche plate avec temoin lumineux : le temoin vert indique l'etat actif.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool down) override
    {
        using juce::Colour;
        const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();

        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 4.0f);

        g.setGradientFill (juce::ColourGradient (on ? Colour (0xffb9bdb8) : Colour (0xffe4e6e3), r.getX(), r.getY(),
                                                 on ? Colour (0xffc7cac6) : Colour (0xffc6c9c5), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.0f);
        if (highlighted || down)
        {
            g.setColour (juce::Colours::white.withAlpha (down ? 0.0f : 0.25f));
            g.fillRoundedRectangle (r, 4.0f);
        }
        g.setColour (Theme::line);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        if (on)
        {
            g.setColour (juce::Colours::black.withAlpha (0.12f));
            g.drawLine (r.getX() + 3.0f, r.getY() + 1.5f, r.getRight() - 3.0f, r.getY() + 1.5f, 2.0f);
        }

        const auto led = juce::Rectangle<float> (r.getCentreX() - 3.0f, r.getY() + 6.0f, 6.0f, 6.0f);
        if (on)
        {
            g.setColour (Theme::accentLit.withAlpha (0.35f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setColour (on ? Theme::accentLit : Colour (0xff8a8f8b));
        g.fillEllipse (led);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawEllipse (led, 1.0f);

        juce::String text = b.getButtonText();
        if (on && b.getProperties().contains ("onText"))
            text = b.getProperties()["onText"].toString();
        if (! on && b.getProperties().contains ("offText"))
            text = b.getProperties()["offText"].toString();

        g.setColour (Theme::ink);
        g.setFont (uiFont (10.5f, true));
        g.drawText (text, r.withTrimmedTop (14.0f).toNearestInt(), juce::Justification::centred);
    }

    // Titre de section : repere colore, texte, filet en relief.
    static void drawHeader (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title,
                            juce::Colour swatch = Theme::accent)
    {
        g.setColour (swatch);
        g.fillRect (area.getX(), area.getY() + 4.0f, 8.0f, 8.0f);

        g.setColour (Theme::ink);
        g.setFont (uiFont (11.0f, true));
        g.drawText (title, juce::Rectangle<float> (area.getX() + 14.0f, area.getY(), area.getWidth() - 14.0f, 16.0f),
                    juce::Justification::centredLeft);

        g.setColour (Theme::line);
        g.fillRect (area.getX(), area.getY() + 19.0f, area.getWidth(), 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillRect (area.getX(), area.getY() + 20.0f, area.getWidth(), 1.0f);
    }
};
