#pragma once

#include <JuceHeader.h>

/*  Tonkori visual language: dark, quiet surfaces; colour only where it carries meaning
    (blue = selection/active, red = record, green = play, amber = warning). */
namespace tonkori::ui
{
    namespace colours
    {
        const juce::Colour bg         { 0xff15171b };
        const juce::Colour panel      { 0xff1c1f25 };
        const juce::Colour panelHi    { 0xff242830 };
        const juce::Colour lane       { 0xff1a1d22 };
        const juce::Colour laneAlt    { 0xff1d2026 };
        const juce::Colour line       { 0xff2c3139 };
        const juce::Colour lineStrong { 0xff3a404a };
        const juce::Colour text       { 0xffe4e7eb };
        const juce::Colour textDim    { 0xff8f97a3 };
        const juce::Colour textFaint  { 0xff5d6470 };
        const juce::Colour accent     { 0xff4c9aff };
        const juce::Colour record     { 0xffe5484d };
        const juce::Colour play       { 0xff3dd68c };
        const juce::Colour warn       { 0xffffb224 };
        const juce::Colour selection  { 0x334c9aff };
        const juce::Colour playhead   { 0xfff2f4f7 };

        inline juce::Colour trackColour (int index)
        {
            static const juce::uint32 palette[] = { 0xff5b8def, 0xff4fb6a5, 0xffd2a24c, 0xffc9677a, 0xff8f79d9,
                                                    0xff6fae5e, 0xffd17f4f, 0xff5aa6c8, 0xffb7739d, 0xff9a9f4f };
            return juce::Colour (palette[(size_t) (index % 10)]);
        }
    }

    inline juce::Font font (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline juce::Font mono (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, bold ? juce::Font::bold : juce::Font::plain));
    }

    //==============================================================================
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            using namespace colours;
            setColour (juce::ResizableWindow::backgroundColourId, bg);
            setColour (juce::DocumentWindow::backgroundColourId, bg);
            setColour (juce::TextButton::buttonColourId, panelHi);
            setColour (juce::TextButton::buttonOnColourId, accent);
            setColour (juce::TextButton::textColourOffId, text);
            setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            setColour (juce::ComboBox::backgroundColourId, panelHi);
            setColour (juce::ComboBox::outlineColourId, line);
            setColour (juce::ComboBox::textColourId, text);
            setColour (juce::ComboBox::arrowColourId, textDim);
            setColour (juce::Label::textColourId, text);
            setColour (juce::TextEditor::backgroundColourId, bg);
            setColour (juce::TextEditor::outlineColourId, line);
            setColour (juce::TextEditor::focusedOutlineColourId, accent);
            setColour (juce::TextEditor::textColourId, text);
            setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.4f));
            setColour (juce::CaretComponent::caretColourId, accent);
            setColour (juce::PopupMenu::backgroundColourId, panel);
            setColour (juce::PopupMenu::textColourId, text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
            setColour (juce::PopupMenu::headerTextColourId, textDim);
            setColour (juce::Slider::trackColourId, accent);
            setColour (juce::Slider::backgroundColourId, line);
            setColour (juce::Slider::thumbColourId, text);
            setColour (juce::Slider::rotarySliderFillColourId, accent);
            setColour (juce::Slider::rotarySliderOutlineColourId, line);
            setColour (juce::Slider::textBoxTextColourId, text);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            setColour (juce::ScrollBar::thumbColourId, lineStrong);
            setColour (juce::ToggleButton::textColourId, text);
            setColour (juce::ToggleButton::tickColourId, accent);
            setColour (juce::ToggleButton::tickDisabledColourId, textFaint);
            setColour (juce::TooltipWindow::backgroundColourId, panelHi);
            setColour (juce::TooltipWindow::textColourId, text);
            setColour (juce::TooltipWindow::outlineColourId, line);
            setColour (juce::AlertWindow::backgroundColourId, panel);
            setColour (juce::AlertWindow::textColourId, text);
            setColour (juce::AlertWindow::outlineColourId, line);
            setColour (juce::ListBox::backgroundColourId, bg);
            setColour (juce::TreeView::backgroundColourId, bg);
            setColour (juce::BubbleComponent::backgroundColourId, panelHi);
            setColour (juce::BubbleComponent::outlineColourId, line);
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgColour,
                                   bool highlighted, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            const bool on = b.getToggleState();
            auto base = on ? b.findColour (juce::TextButton::buttonOnColourId) : bgColour;
            if (! b.isEnabled()) base = base.withMultipliedAlpha (0.4f);
            if (down) base = base.brighter (0.15f);
            else if (highlighted) base = base.brighter (0.07f);

            g.setColour (base);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (on ? base.brighter (0.2f) : colours::line);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
        }

        juce::Font getTextButtonFont (juce::TextButton&, int h) override
        {
            return font (juce::jmin (13.0f, (float) h * 0.55f));
        }

        juce::Font getComboBoxFont (juce::ComboBox&) override             { return font (12.5f); }
        juce::Font getPopupMenuFont() override                           { return font (13.0f); }
        juce::Font getLabelFont (juce::Label& l) override                { return l.getFont(); }

        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (colours::panel);
            g.setColour (colours::line);
            g.drawRect (0, 0, w, h);
        }

        void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                               juce::Slider::SliderStyle style, juce::Slider& s) override
        {
            if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar)
            {
                auto r = juce::Rectangle<float> ((float) x, (float) y + (float) h * 0.5f - 2.0f, (float) w, 4.0f);
                g.setColour (colours::line);
                g.fillRoundedRectangle (r, 2.0f);
                g.setColour (s.findColour (juce::Slider::trackColourId).withMultipliedAlpha (s.isEnabled() ? 1.0f : 0.4f));
                g.fillRoundedRectangle (r.withRight (pos), 2.0f);
                g.setColour (colours::text);
                g.fillEllipse (pos - 5.0f, (float) y + (float) h * 0.5f - 5.0f, 10.0f, 10.0f);
                return;
            }

            if (style == juce::Slider::LinearVertical)
            {
                // Fader: groove + cap
                const float cx = (float) x + (float) w * 0.5f;
                g.setColour (colours::bg);
                g.fillRoundedRectangle (cx - 2.5f, (float) y, 5.0f, (float) h, 2.5f);
                g.setColour (colours::line);
                g.drawRoundedRectangle (cx - 2.5f, (float) y, 5.0f, (float) h, 2.5f, 1.0f);

                auto cap = juce::Rectangle<float> (cx - 13.0f, pos - 7.0f, 26.0f, 14.0f);
                g.setColour (colours::panelHi.brighter (0.25f));
                g.fillRoundedRectangle (cap, 3.0f);
                g.setColour (colours::text);
                g.fillRect (cap.withSizeKeepingCentre (18.0f, 1.5f));
                return;
            }

            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override
        {
            auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
            const float radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
            const auto c = r.getCentre();
            const float angle = startAngle + pos * (endAngle - startAngle);
            const float mid = startAngle + 0.5f * (endAngle - startAngle);

            juce::Path track;
            track.addCentredArc (c.x, c.y, radius - 2, radius - 2, 0, startAngle, endAngle, true);
            g.setColour (colours::line);
            g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            juce::Path val;
            val.addCentredArc (c.x, c.y, radius - 2, radius - 2, 0, juce::jmin (mid, angle), juce::jmax (mid, angle), true);
            g.setColour (colours::accent);
            g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            g.setColour (colours::text);
            g.drawLine (c.x, c.y, c.x + (radius - 5) * std::sin (angle), c.y - (radius - 5) * std::cos (angle), 1.5f);
        }
    };

    //==============================================================================
    /** Small flat button that draws a vector icon (transport). */
    class IconButton : public juce::Button
    {
    public:
        using Drawer = std::function<void (juce::Graphics&, juce::Rectangle<float>, juce::Colour)>;

        IconButton (const juce::String& name, Drawer d, juce::Colour onColour = colours::accent)
            : Button (name), drawer (std::move (d)), onCol (onColour)
        {
            setWantsKeyboardFocus (false);
        }

        void paintButton (juce::Graphics& g, bool hi, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            const bool on = getToggleState();
            g.setColour (on ? onCol.withAlpha (0.22f) : (hi ? colours::panelHi.brighter (0.06f) : colours::panelHi));
            if (down) g.setColour (colours::panelHi.brighter (0.15f));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (on ? onCol.withAlpha (0.7f) : colours::line);
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
            drawer (g, r.reduced (r.getHeight() * 0.26f), on ? onCol : colours::text);
        }

    private:
        Drawer drawer;
        juce::Colour onCol;
    };

    namespace icons
    {
        inline void play (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
        {
            juce::Path p;
            const float s = juce::jmin (r.getWidth(), r.getHeight());
            auto b = r.withSizeKeepingCentre (s * 0.9f, s);
            p.addTriangle (b.getX(), b.getY(), b.getX(), b.getBottom(), b.getRight(), b.getCentreY());
            g.setColour (c); g.fillPath (p);
        }
        inline void stop (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
        {
            const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.85f;
            g.setColour (c); g.fillRoundedRectangle (r.withSizeKeepingCentre (s, s), 1.5f);
        }
        inline void record (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour)
        {
            const float s = juce::jmin (r.getWidth(), r.getHeight());
            g.setColour (colours::record); g.fillEllipse (r.withSizeKeepingCentre (s, s));
        }
        inline void toStart (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
        {
            const float s = juce::jmin (r.getWidth(), r.getHeight());
            auto b = r.withSizeKeepingCentre (s, s);
            juce::Path p;
            p.addRectangle (b.getX(), b.getY(), s * 0.16f, s);
            p.addTriangle (b.getRight(), b.getY(), b.getRight(), b.getBottom(), b.getX() + s * 0.2f, b.getCentreY());
            g.setColour (c); g.fillPath (p);
        }
        inline void loop (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
        {
            auto b = r.withSizeKeepingCentre (r.getWidth() * 1.1f, r.getHeight() * 0.75f);
            juce::Path p;
            p.addRoundedRectangle (b, b.getHeight() * 0.45f);
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.8f));
            juce::Path a;
            a.addTriangle (b.getCentreX() + 1, b.getY() - 3.5f, b.getCentreX() + 1, b.getY() + 3.5f, b.getCentreX() + 5.5f, b.getY());
            g.fillPath (a);
        }
        inline void punch (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
        {
            g.setColour (c);
            const float w = r.getWidth(), h = r.getHeight();
            juce::Path p;
            p.startNewSubPath (r.getX() + w * 0.25f, r.getY()); p.lineTo (r.getX(), r.getY()); p.lineTo (r.getX(), r.getBottom()); p.lineTo (r.getX() + w * 0.25f, r.getBottom());
            p.startNewSubPath (r.getRight() - w * 0.25f, r.getY()); p.lineTo (r.getRight(), r.getY()); p.lineTo (r.getRight(), r.getBottom()); p.lineTo (r.getRight() - w * 0.25f, r.getBottom());
            g.strokePath (p, juce::PathStrokeType (1.8f));
            g.fillEllipse (r.withSizeKeepingCentre (h * 0.4f, h * 0.4f));
        }
        inline void metronome (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
        {
            juce::Path p;
            p.addTriangle (r.getCentreX(), r.getY(), r.getX() + r.getWidth() * 0.12f, r.getBottom(), r.getRight() - r.getWidth() * 0.12f, r.getBottom());
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.6f));
            g.drawLine (r.getCentreX(), r.getBottom() - 2, r.getRight(), r.getY() + 2, 1.6f);
        }
        inline void follow (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
        {
            g.setColour (c);
            g.drawLine (r.getX(), r.getCentreY(), r.getRight() - 2, r.getCentreY(), 1.8f);
            juce::Path p;
            p.addTriangle (r.getRight() - 6, r.getCentreY() - 4.5f, r.getRight() - 6, r.getCentreY() + 4.5f, r.getRight() + 1, r.getCentreY());
            g.fillPath (p);
            g.drawLine (r.getX(), r.getY(), r.getX(), r.getBottom(), 1.8f);
        }
    }
}
