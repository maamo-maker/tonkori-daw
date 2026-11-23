#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "ViewState.h"
#include "../model/Crossfade.h"

namespace tonkori::ui
{
    /*  Crossfade editor (Pyramix / Cubase): both curves over the crossfade, their summed
        gain (coherent = equal-gain view, uncorrelated = power view), and numeric control
        of length, splice point and alignment. Dragging in the graph moves the splice point;
        Opt-drag changes the length. Every change is applied live. */
    class CrossfadeEditor : public juce::Component
    {
    public:
        CrossfadeEditor (ViewState& v, tonkori::xfade::Pair pair) : vs (v), outId (pair.out->itemID), inId (pair.in->itemID)
        {
            params = tonkori::xfade::read (pair);
            setupLabel (title, "CROSSFADE  " + pair.out->getName() + "  >  " + pair.in->getName(), true);

            auto addField = [this] (juce::Label& caption, juce::Label& field, const juce::String& name, std::function<void (const juce::String&)> onEnter)
            {
                setupLabel (caption, name, false);
                field.setEditable (true, true, false);
                field.setFont (mono (12.0f));
                field.setColour (juce::Label::backgroundColourId, colours::bg);
                field.setColour (juce::Label::textColourId, colours::text);
                field.onTextChange = [&field, onEnter] { onEnter (field.getText()); };
                addAndMakeVisible (field);
            };

            addField (lengthCaption, lengthField, "Length", [this] (const juce::String& t)
            {
                if (auto d = vs.parseFadeLength (t)) { params.length = d->inSeconds(); apply(); }
            });
            addField (spliceCaption, spliceField, "Splice offset", [this] (const juce::String& t)
            {
                if (auto d = vs.parseFadeLength (t.trim().trimCharactersAtStart ("+")))
                {
                    const bool neg = t.trim().startsWithChar ('-');
                    params.splice = cutPoint + (neg ? -std::abs (d->inSeconds()) : std::abs (d->inSeconds()));
                    apply();
                }
            });

            for (auto* b : { &pre, &centre, &post })
            {
                b->setClickingTogglesState (false);
                b->setColour (juce::TextButton::buttonOnColourId, colours::accent.withAlpha (0.6f));
                addAndMakeVisible (*b);
            }
            pre.onClick    = [this] { params.align = 0.0; apply(); };
            centre.onClick = [this] { params.align = 0.5; apply(); };
            post.onClick   = [this] { params.align = 1.0; apply(); };

            align.setSliderStyle (juce::Slider::LinearHorizontal);
            align.setTextBoxStyle (juce::Slider::TextBoxRight, false, 120, 20);
            align.setRange (0.0, 100.0, 1.0);
            align.setTextValueSuffix (" % after splice");
            align.onValueChange = [this] { params.align = align.getValue() / 100.0; apply(); };
            addAndMakeVisible (align);

            auto setupShape = [this] (juce::ComboBox& box, bool isOut)
            {
                box.addItemList (FadeSpec::getShapeNames(), 1);
                box.onChange = [this, &box, isOut]
                {
                    auto& spec = isOut ? params.outSpec : params.inSpec;
                    const auto shape = (FadeSpec::Shape) (box.getSelectedItemIndex());
                    if (shape == spec.shape) return;
                    spec.shape = shape;
                    spec.param = FadeSpec::getDefaultParam (shape);
                    if (linked.getToggleState()) (isOut ? params.inSpec : params.outSpec) = spec;
                    apply();
                };
                addAndMakeVisible (box);
            };
            setupLabel (outCaption, "Out curve", false);
            setupLabel (inCaption, "In curve", false);
            setupShape (outShape, true);
            setupShape (inShape, false);

            linked.setButtonText ("Link curves");
            linked.setToggleState (params.inSpec == params.outSpec, juce::dontSendNotification);
            addAndMakeVisible (linked);

            coherent.setButtonText ("Sum: correlated");
            coherent.setClickingTogglesState (true);
            coherent.setToggleState (true, juce::dontSendNotification);
            coherent.onClick = [this] { coherent.setButtonText (coherent.getToggleState() ? "Sum: correlated" : "Sum: uncorrelated"); repaint(); };
            addAndMakeVisible (coherent);

            audition.setButtonText ("Audition");
            audition.setTooltip ("Plays 2 s before to 1 s after the crossfade");
            audition.onClick = [this]
            {
                auto& t = vs.edit.getTransport();
                const double start = juce::jmax (0.0, params.splice - params.length - 2.0);
                t.setPosition (te::TimePosition::fromSeconds (start));
                stopAt = params.splice + params.length + 1.0;
                t.play (false);
                startTimer();
            };
            addAndMakeVisible (audition);

            cutPoint = params.splice;
            refresh();
            setSize (460, 400);
        }

        ~CrossfadeEditor() override { timer.stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (colours::panel);
            auto r = graphArea().toFloat();
            g.setColour (colours::bg);
            g.fillRoundedRectangle (r, 4.0f);
            r = r.reduced (8.0f);

            // dB grid
            g.setFont (font (9.0f));
            for (double db : { 6.0, 3.0, 0.0, -6.0, -12.0, -24.0 })
            {
                const float y = dbToY (db, r);
                g.setColour (db == 0 ? colours::lineStrong : colours::line);
                g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
                g.setColour (colours::textFaint);
                g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), (int) r.getRight() - 24, (int) y - 10, 22, 10, juce::Justification::right);
            }

            // timeline: show [start - pad, end + pad]
            const double start = params.splice - params.length * (1.0 - params.align), end = start + params.length;
            const double pad = juce::jmax (0.002, params.length * 0.35);
            auto tx = [&] (double t) { return r.getX() + (float) ((t - (start - pad)) / (params.length + 2 * pad)) * r.getWidth(); };

            auto curve = [&] (bool isOut, juce::Colour c)
            {
                juce::Path p;
                const int n = (int) r.getWidth();
                for (int i = 0; i <= n; ++i)
                {
                    const double t = (start - pad) + (params.length + 2 * pad) * i / n;
                    const double x = params.length > 0 ? juce::jlimit (0.0, 1.0, (t - start) / params.length) : (t < start ? 0.0 : 1.0);
                    const double gIn = params.inSpec.gainAt (x), gOut = params.outSpec.gainAt (1.0 - x);
                    const double gdb = juce::Decibels::gainToDecibels (isOut ? gOut : gIn, -60.0);
                    const float px = r.getX() + (float) i, py = dbToY (gdb, r);
                    if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
                }
                g.setColour (c);
                g.strokePath (p, juce::PathStrokeType (2.0f));
            };
            curve (true, colours::warn);
            curve (false, colours::accent);

            // summed gain
            juce::Path sum;
            const int n = (int) r.getWidth();
            for (int i = 0; i <= n; ++i)
            {
                const double t = (start - pad) + (params.length + 2 * pad) * i / n;
                const double x = params.length > 0 ? juce::jlimit (0.0, 1.0, (t - start) / params.length) : 0.0;
                const float py = dbToY (tonkori::xfade::sumGainDb (params, x, coherent.getToggleState()), r);
                if (i == 0) sum.startNewSubPath (r.getX(), py); else sum.lineTo (r.getX() + (float) i, py);
            }
            const float dashes[] = { 4.0f, 3.0f };
            juce::Path dashed;
            juce::PathStrokeType (1.5f).createDashedStroke (dashed, sum, dashes, 2);
            g.setColour (colours::text.withAlpha (0.8f));
            g.fillPath (dashed);

            // splice point and edit point
            g.setColour (colours::playhead);
            g.drawVerticalLine ((int) tx (params.splice), r.getY(), r.getBottom());
            g.setColour (colours::textDim.withAlpha (0.6f));
            g.drawVerticalLine ((int) tx (cutPoint), r.getY(), r.getBottom());

            // peak of the sum
            double peak = -100;
            for (int i = 0; i <= 200; ++i) peak = juce::jmax (peak, tonkori::xfade::sumGainDb (params, i / 200.0, coherent.getToggleState()));
            g.setColour (colours::textDim);
            g.setFont (mono (10.5f));
            g.drawText ("out", (int) r.getX() + 4, (int) r.getY() + 2, 40, 12, juce::Justification::left);
            g.setColour (colours::accent);
            g.drawText ("in", (int) r.getRight() - 30, (int) r.getY() + 2, 26, 12, juce::Justification::right);
            g.setColour (colours::text);
            g.drawText ("sum peak " + juce::String (peak, 2) + " dB", (int) r.getCentreX() - 70, (int) r.getBottom() - 14, 140, 12, juce::Justification::centred);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (12, 10);
            title.setBounds (r.removeFromTop (20));
            r.removeFromTop (6);
            graph = r.removeFromTop (170);
            r.removeFromTop (8);

            auto row = [&] (juce::Label& c, juce::Component& f) { auto rr = r.removeFromTop (24); c.setBounds (rr.removeFromLeft (100)); f.setBounds (rr); r.removeFromTop (4); };
            row (lengthCaption, lengthField);
            row (spliceCaption, spliceField);
            auto ar = r.removeFromTop (24);
            const int bw = 56;
            pre.setBounds (ar.removeFromLeft (bw)); ar.removeFromLeft (3);
            centre.setBounds (ar.removeFromLeft (bw)); ar.removeFromLeft (3);
            post.setBounds (ar.removeFromLeft (bw)); ar.removeFromLeft (8);
            align.setBounds (ar);
            r.removeFromTop (4);
            row (outCaption, outShape);
            row (inCaption, inShape);
            auto br = r.removeFromTop (24);
            linked.setBounds (br.removeFromLeft (110));
            coherent.setBounds (br.removeFromLeft (140)); br.removeFromLeft (8);
            audition.setBounds (br);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! graphArea().contains (e.getPosition())) return;
            vs.edit.getUndoManager().beginNewTransaction ("Crossfade");
            dragStart = params;
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! graphArea().contains (e.getMouseDownPosition())) return;
            const auto r = graphArea().reduced (8);
            const double pad = juce::jmax (0.002, dragStart.length * 0.35);
            const double secsPerPx = (dragStart.length + 2 * pad) / juce::jmax (1, r.getWidth());
            if (e.mods.isAltDown())
                params.length = juce::jmax (0.0002, dragStart.length + e.getDistanceFromDragStartX() * secsPerPx * 2.0);
            else
                params.splice = dragStart.splice + e.getDistanceFromDragStartX() * secsPerPx;
            apply();
        }

    private:
        ViewState& vs;
        te::EditItemID outId, inId;
        tonkori::xfade::Params params, dragStart;
        double cutPoint = 0, stopAt = 0;
        juce::Rectangle<int> graph;

        juce::Label title, lengthCaption, lengthField, spliceCaption, spliceField, outCaption, inCaption;
        juce::TextButton pre { "Pre" }, centre { "Centre" }, post { "Post" }, coherent, audition;
        juce::ToggleButton linked;
        juce::Slider align;
        juce::ComboBox outShape, inShape;

        struct StopTimer : public juce::Timer
        {
            std::function<void()> fn;
            void timerCallback() override { fn(); }
        } timer;

        void startTimer()
        {
            timer.fn = [this]
            {
                auto& t = vs.edit.getTransport();
                if (! t.isPlaying() || t.getPosition().inSeconds() >= stopAt) { t.stop (false, false); timer.stopTimer(); }
            };
            timer.startTimerHz (60);
        }

        juce::Rectangle<int> graphArea() const { return graph; }

        static float dbToY (double db, juce::Rectangle<float> r)
        {
            const double lo = -30.0, hi = 7.0;
            return r.getBottom() - (float) ((juce::jlimit (lo, hi, db) - lo) / (hi - lo)) * r.getHeight();
        }

        void setupLabel (juce::Label& l, const juce::String& text, bool strong)
        {
            l.setText (text, juce::dontSendNotification);
            l.setFont (font (strong ? 12.0f : 11.5f, strong));
            l.setColour (juce::Label::textColourId, strong ? colours::text : colours::textDim);
            addAndMakeVisible (l);
        }

        tonkori::xfade::Pair pair() const
        {
            return { dynamic_cast<te::AudioClipBase*> (te::findClipForID (vs.edit, outId)),
                     dynamic_cast<te::AudioClipBase*> (te::findClipForID (vs.edit, inId)) };
        }

        void apply()
        {
            auto p = pair();
            if (p.out == nullptr || p.in == nullptr) return;
            if (linked.getToggleState()) params.inSpec = params.outSpec;
            params.length = tonkori::xfade::apply (p, params);
            params = tonkori::xfade::read (p);
            refresh();
            vs.changed();
        }

        void refresh()
        {
            lengthField.setText (vs.formatFadeLength (te::TimeDuration::fromSeconds (params.length)), juce::dontSendNotification);
            const double off = params.splice - cutPoint;
            spliceField.setText ((off >= 0 ? "+" : "-") + vs.formatFadeLength (te::TimeDuration::fromSeconds (std::abs (off))), juce::dontSendNotification);
            align.setValue (params.align * 100.0, juce::dontSendNotification);
            pre.setToggleState (params.align == 0.0, juce::dontSendNotification);
            centre.setToggleState (params.align == 0.5, juce::dontSendNotification);
            post.setToggleState (params.align == 1.0, juce::dontSendNotification);
            outShape.setSelectedItemIndex ((int) params.outSpec.shape, juce::dontSendNotification);
            inShape.setSelectedItemIndex ((int) params.inSpec.shape, juce::dontSendNotification);
            repaint();
        }
    };
}
