#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "ViewState.h"
#include "../model/ClipFades.h"

namespace tonkori::ui
{
    //==============================================================================
    /** Draws a fade curve in real time order (fade-outs fall left → right).
        Points mode: click to add, drag to move, drag outside to remove (Cubase behaviour).
        Other shapes: vertical drag adjusts the shape's parameter. */
    class FadeCurveView : public juce::Component
    {
    public:
        explicit FadeCurveView (bool fadeIn) : isIn (fadeIn) {}

        std::function<void (const FadeSpec&)> onChange;

        void setSpec (const FadeSpec& s)        { spec = s; repaint(); }
        const FadeSpec& getSpec() const         { return spec; }

        void paint (juce::Graphics& g) override
        {
            auto r = plotArea();
            g.setColour (colours::bg);
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

            // gain grid (linear axis, labelled in dB)
            g.setFont (font (9.0f));
            for (double db : { 0.0, -3.0, -6.0, -12.0, -24.0 })
            {
                const float y = gainToY (juce::Decibels::decibelsToGain (db), r);
                g.setColour (colours::line);
                g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
                g.setColour (colours::textFaint);
                g.drawText (juce::String ((int) db), (int) r.getRight() - 26, (int) y - 10, 24, 10, juce::Justification::right);
            }
            for (int i = 1; i < 4; ++i)
            {
                g.setColour (colours::line);
                g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 4.0f), r.getY(), r.getBottom());
            }

            // curve
            juce::Path p, fill;
            const int n = juce::jmax (2, (int) r.getWidth());
            for (int i = 0; i <= n; ++i)
            {
                const double u = (double) i / n;                      // real time fraction
                const double gain = spec.gainAt (isIn ? u : 1.0 - u);
                const float x = r.getX() + (float) u * r.getWidth(), y = gainToY (gain, r);
                if (i == 0) { p.startNewSubPath (x, y); fill.startNewSubPath (x, r.getBottom()); }
                else p.lineTo (x, y);
                fill.lineTo (x, y);
            }
            fill.lineTo (r.getRight(), r.getBottom());
            fill.closeSubPath();

            g.setColour (colours::accent.withAlpha (0.14f));
            g.fillPath (fill);
            g.setColour (colours::accent);
            g.strokePath (p, juce::PathStrokeType (2.0f));

            // equal-power reference
            g.setColour (colours::textFaint.withAlpha (0.6f));
            juce::Path ref;
            for (int i = 0; i <= n; i += 2)
            {
                const double u = (double) i / n;
                const float x = r.getX() + (float) u * r.getWidth();
                const float y = gainToY (std::sin ((isIn ? u : 1.0 - u) * juce::MathConstants<double>::halfPi), r);
                if (i == 0) ref.startNewSubPath (x, y); else ref.lineTo (x, y);
            }
            const float dashes[] = { 3.0f, 3.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (ref, ref, dashes, 2);
            g.fillPath (ref);

            if (spec.shape == FadeSpec::Shape::points)
                for (auto& pt : spec.points)
                {
                    auto c = pointToXY (pt, r);
                    g.setColour (colours::text);
                    g.fillEllipse (c.x - 4, c.y - 4, 8, 8);
                    g.setColour (colours::accent);
                    g.drawEllipse (c.x - 4, c.y - 4, 8, 8, 1.5f);
                }

            g.setColour (colours::textDim);
            g.setFont (font (10.0f));
            g.drawText (spec.shape == FadeSpec::Shape::points ? "click: add · drag: move · drag out: remove"
                                                              : "drag up/down to shape",
                        getLocalBounds().reduced (6, 3), juce::Justification::bottomLeft);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragIndex = -1;
            startSpec = spec;
            auto r = plotArea();

            if (spec.shape == FadeSpec::Shape::points)
            {
                for (int i = 0; i < (int) spec.points.size(); ++i)
                    if (pointToXY (spec.points[(size_t) i], r).getDistanceFrom (e.position) < 8.0f)
                        dragIndex = i;

                if (dragIndex < 0)
                {
                    spec.points.push_back (xyToPoint (e.position, r));
                    sortPoints();
                    for (int i = 0; i < (int) spec.points.size(); ++i)
                        if (pointToXY (spec.points[(size_t) i], r).getDistanceFrom (e.position) < 1.0f) dragIndex = i;
                    notify();
                }
            }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            auto r = plotArea();

            if (spec.shape == FadeSpec::Shape::points)
            {
                if (dragIndex < 0 || dragIndex >= (int) spec.points.size()) return;

                if (! getLocalBounds().toFloat().expanded (12.0f).contains (e.position))
                {
                    spec.points.erase (spec.points.begin() + dragIndex);
                    dragIndex = -1;
                }
                else
                {
                    spec.points[(size_t) dragIndex] = xyToPoint (e.position, r);
                    const auto moved = spec.points[(size_t) dragIndex];
                    sortPoints();
                    for (int i = 0; i < (int) spec.points.size(); ++i)
                        if (spec.points[(size_t) i].t == moved.t && spec.points[(size_t) i].gain == moved.gain) dragIndex = i;
                }
                notify();
                return;
            }

            const auto name = FadeSpec::getParamName (spec.shape);
            if (name.isEmpty())
            {
                // Convert a fixed shape into a shapeable one on first drag.
                spec.shape = FadeSpec::Shape::exponential;
                spec.param = 0.0;
                startSpec = spec;
            }

            const auto range = FadeSpec::getParamRange (spec.shape);
            const double dy = -(double) e.getDistanceFromDragStartY() / juce::jmax (40.0f, r.getHeight());
            double sign = (spec.shape == FadeSpec::Shape::exponential) ? (isIn ? -1.0 : -1.0) : 1.0;
            spec.param = range.clipValue (startSpec.param + sign * dy * range.getLength());
            notify();
        }

    private:
        bool isIn;
        FadeSpec spec, startSpec;
        int dragIndex = -1;

        juce::Rectangle<float> plotArea() const     { return getLocalBounds().toFloat().reduced (8.0f, 10.0f).withTrimmedBottom (10.0f); }
        static float gainToY (double gain, juce::Rectangle<float> r) { return r.getBottom() - (float) juce::jlimit (0.0, 1.0, gain) * r.getHeight(); }

        juce::Point<float> pointToXY (FadeSpec::Point p, juce::Rectangle<float> r) const
        {
            const double u = isIn ? p.t : 1.0 - p.t;
            return { r.getX() + (float) u * r.getWidth(), gainToY (p.gain, r) };
        }

        FadeSpec::Point xyToPoint (juce::Point<float> xy, juce::Rectangle<float> r) const
        {
            const double u = juce::jlimit (0.01, 0.99, (double) ((xy.x - r.getX()) / r.getWidth()));
            const double gain = juce::jlimit (0.0, 1.0, (double) ((r.getBottom() - xy.y) / r.getHeight()));
            return { isIn ? u : 1.0 - u, gain };
        }

        void sortPoints() { std::sort (spec.points.begin(), spec.points.end(), [] (auto& a, auto& b) { return a.t < b.t; }); }

        void notify()
        {
            repaint();
            if (onChange) onChange (spec);
        }
    };

    //==============================================================================
    /** Numeric fade editor for one side (in or out) of the selected clips. */
    class FadeEditorPanel : public juce::Component
    {
    public:
        using ClipList = std::function<juce::Array<te::AudioClipBase*>()>;

        FadeEditorPanel (ViewState& v, bool fadeIn, ClipList clips)
            : vs (v), isIn (fadeIn), getClips (std::move (clips)), curve (fadeIn)
        {
            title.setText (isIn ? "FADE IN" : "FADE OUT", juce::dontSendNotification);
            title.setFont (font (11.0f, true));
            title.setColour (juce::Label::textColourId, colours::textDim);

            for (auto* l : { &lengthLabel, &shapeLabel, &paramLabel, &pointsLabel, &specLabel })
            {
                l->setFont (font (11.0f));
                l->setColour (juce::Label::textColourId, colours::textDim);
                addAndMakeVisible (*l);
            }
            lengthLabel.setText ("Length", juce::dontSendNotification);
            shapeLabel.setText ("Shape", juce::dontSendNotification);
            pointsLabel.setText ("Points", juce::dontSendNotification);
            specLabel.setText ("Spec", juce::dontSendNotification);

            addAndMakeVisible (title);
            addAndMakeVisible (length);
            addAndMakeVisible (shape);
            addAndMakeVisible (param);
            addAndMakeVisible (interp);
            addAndMakeVisible (points);
            addAndMakeVisible (specText);
            addAndMakeVisible (curve);
            addAndMakeVisible (presets);

            length.setTooltip ("Fade length. Type 250, 250 ms, 0.25 s, 12000 smp or bars.beats.ticks");
            points.setTooltip ("Breakpoints as time% : gain dB, e.g. 25:-12 50:-4 75:-1 (end points are implied)");
            specText.setTooltip ("Full curve definition, e.g. 'exponential -0.5', 'sCurve 0.3', 'dBLinear -60', 'points damped 25:-12 75:-1'");

            shape.addItemList (FadeSpec::getShapeNames(), 1);
            interp.addItemList ({ "Spline", "Damped spline", "Linear" }, 1);
            param.setSliderStyle (juce::Slider::LinearHorizontal);
            param.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);

            for (auto* t : { &length, &points, &specText })
            {
                t->setFont (mono (12.0f));
                t->setSelectAllWhenFocused (true);
            }

            presets.setTextWhenNothingSelected ("Presets...");
            rebuildPresetMenu();

            length.onReturnKey = length.onFocusLost = [this] { commitLength(); };
            points.onReturnKey = points.onFocusLost = [this] { commitPoints(); };
            specText.onReturnKey = specText.onFocusLost = [this] { apply (FadeSpec::fromString (specText.getText())); };
            shape.onChange = [this]
            {
                if (updating) return;
                auto s = current;
                s.shape = (FadeSpec::Shape) (shape.getSelectedItemIndex());
                s.param = FadeSpec::getDefaultParam (s.shape);
                if (s.shape == FadeSpec::Shape::points && s.points.empty())
                    s.points = { { 0.5, juce::Decibels::decibelsToGain (-6.0) } };
                apply (s);
            };
            interp.onChange = [this] { if (! updating) { auto s = current; s.interp = (FadeSpec::Interp) interp.getSelectedItemIndex(); apply (s); } };
            param.onValueChange = [this] { if (! updating) { auto s = current; s.param = param.getValue(); apply (s); } };
            curve.onChange = [this] (const FadeSpec& s) { apply (s, false); };
            presets.onChange = [this] { choosePreset(); };

            refresh();
        }

        void refresh()
        {
            auto clips = getClips();
            const bool any = ! clips.isEmpty();
            for (auto* c : getChildren()) c->setEnabled (any);
            if (! any) return;

            auto* c = clips.getFirst();
            current = fades::getSpec (*c, isIn);
            updateControls (isIn ? c->getFadeIn() : c->getFadeOut());
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto top = r.removeFromTop (20);
            presets.setBounds (top.removeFromRight (110).reduced (0, 1));
            title.setBounds (top);
            r.removeFromTop (4);

            auto row = [&] (juce::Label& l, juce::Component& c)
            {
                auto line = r.removeFromTop (24);
                l.setBounds (line.removeFromLeft (52));
                c.setBounds (line.reduced (0, 1));
                r.removeFromTop (3);
            };

            row (lengthLabel, length);
            row (shapeLabel, shape);

            const bool hasParam = FadeSpec::getParamName (current.shape).isNotEmpty();
            const bool isPoints = current.shape == FadeSpec::Shape::points;
            paramLabel.setVisible (hasParam); param.setVisible (hasParam);
            pointsLabel.setVisible (isPoints); points.setVisible (isPoints); interp.setVisible (isPoints);

            if (hasParam) row (paramLabel, param);
            if (isPoints)
            {
                auto line = r.removeFromTop (24);
                pointsLabel.setBounds (line.removeFromLeft (52));
                interp.setBounds (line.reduced (0, 1));
                r.removeFromTop (3);
                auto line2 = r.removeFromTop (24);
                line2.removeFromLeft (52);
                points.setBounds (line2.reduced (0, 1));
                r.removeFromTop (3);
            }

            row (specLabel, specText);
            r.removeFromTop (2);
            curve.setBounds (r.removeFromTop (juce::jmax (90, juce::jmin (140, r.getHeight()))));
        }

        int getIdealHeight() const
        {
            const bool hasParam = FadeSpec::getParamName (current.shape).isNotEmpty();
            const bool isPoints = current.shape == FadeSpec::Shape::points;
            return 24 + 27 * 3 + (hasParam ? 27 : 0) + (isPoints ? 54 : 0) + 2 + 120;
        }

        std::function<void()> onLayoutChange;

    private:
        ViewState& vs;
        bool isIn;
        ClipList getClips;
        FadeSpec current;
        bool updating = false;

        juce::Label title, lengthLabel, shapeLabel, paramLabel, pointsLabel, specLabel;
        juce::TextEditor length, points, specText;
        juce::ComboBox shape, interp, presets;
        juce::Slider param;
        FadeCurveView curve;

        void updateControls (te::TimeDuration len)
        {
            juce::ScopedValueSetter<bool> svs (updating, true);
            const auto oldShape = shape.getSelectedItemIndex();

            length.setText (vs.formatFadeLength (len), false);
            shape.setSelectedItemIndex ((int) current.shape, juce::dontSendNotification);
            interp.setSelectedItemIndex ((int) current.interp, juce::dontSendNotification);

            const auto name = FadeSpec::getParamName (current.shape);
            paramLabel.setText (name, juce::dontSendNotification);
            const auto range = FadeSpec::getParamRange (current.shape);
            param.setRange (range.getStart(), range.getEnd(), current.shape == FadeSpec::Shape::dBLinear ? 0.5 : 0.01);
            param.setValue (current.param, juce::dontSendNotification);

            points.setText (pointsToText(), false);
            specText.setText (current.toString(), false);
            curve.setSpec (current);

            if (oldShape != (int) current.shape)
            {
                resized();
                if (onLayoutChange) onLayoutChange();
            }
        }

        juce::String pointsToText() const
        {
            juce::StringArray parts;
            auto pts = current.points;
            if (! isIn) std::reverse (pts.begin(), pts.end());
            for (auto& p : pts)
            {
                const double u = isIn ? p.t : 1.0 - p.t;
                const double db = juce::Decibels::gainToDecibels (p.gain, -144.0);
                parts.add (juce::String (u * 100.0, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + ":"
                           + (db <= -144.0 ? juce::String ("-inf") : juce::String (db, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".")));
            }
            return parts.joinIntoString (" ");
        }

        void commitPoints()
        {
            auto s = FadeSpec::fromString ("points " + juce::String (juce::StringArray { "spline", "damped", "linear" }[(int) current.interp]) + " " + points.getText());
            if (! isIn)
                for (auto& p : s.points) p.t = 1.0 - p.t;        // typed in real time order
            std::sort (s.points.begin(), s.points.end(), [] (auto& a, auto& b) { return a.t < b.t; });
            apply (s);
        }

        void commitLength()
        {
            if (auto d = vs.parseFadeLength (length.getText()))
            {
                auto clips = getClips();
                if (clips.isEmpty()) return;
                vs.edit.getUndoManager().beginNewTransaction (isIn ? "Fade in length" : "Fade out length");
                for (auto* c : clips) fades::setLength (*c, isIn, *d);
            }
            refresh();
        }

        void apply (const FadeSpec& s, bool updateCurve = true)
        {
            auto clips = getClips();
            if (clips.isEmpty()) return;

            vs.edit.getUndoManager().beginNewTransaction (isIn ? "Fade in shape" : "Fade out shape");
            current = s;
            for (auto* c : clips)
            {
                fades::setSpec (*c, isIn, s);
                // A shape with no length is inaudible: give it a sensible default.
                if ((isIn ? c->getFadeIn() : c->getFadeOut()) <= te::TimeDuration())
                    fades::setLength (*c, isIn, te::TimeDuration::fromSeconds (0.010));
            }

            if (updateCurve) updateControls (isIn ? clips.getFirst()->getFadeIn() : clips.getFirst()->getFadeOut());
            else
            {
                juce::ScopedValueSetter<bool> svs (updating, true);
                specText.setText (current.toString(), false);
                points.setText (pointsToText(), false);
                param.setValue (current.param, juce::dontSendNotification);
                shape.setSelectedItemIndex ((int) current.shape, juce::dontSendNotification);
                if (FadeSpec::getParamName (current.shape).isNotEmpty() != param.isVisible()) { resized(); if (onLayoutChange) onLayoutChange(); }
            }
            vs.changed();
        }

        //==============================================================================
        static juce::StringPairArray builtInPresets()
        {
            juce::StringPairArray p;
            p.set ("Equal power", "equalPower");
            p.set ("Equal gain (linear)", "linear");
            p.set ("Smooth S", "sCurve 0.25");
            p.set ("Steep S", "sCurve 0.6");
            p.set ("Fast start", "exponential -0.5");
            p.set ("Slow start", "exponential 0.5");
            p.set ("Long tail (dB-linear -60)", "dBLinear -60");
            p.set ("Natural decay (dB-linear -40)", "dBLinear -40");
            p.set ("Pyramix-style raised cosine", "sCurve 0.25");
            return p;
        }

        static juce::PropertiesFile* props()
        {
            static juce::ApplicationProperties ap;
            static bool init = false;
            if (! init)
            {
                juce::PropertiesFile::Options o;
                o.applicationName = "Tonkori DAW";
                o.filenameSuffix = ".settings";
                o.osxLibrarySubFolder = "Application Support";
                o.folderName = "Tonkori DAW";
                ap.setStorageParameters (o);
                init = true;
            }
            return ap.getUserSettings();
        }

        juce::StringPairArray userPresets() const
        {
            juce::StringPairArray p;
            if (auto xml = props()->getXmlValue ("fadePresets"))
                for (auto* e : xml->getChildIterator())
                    p.set (e->getStringAttribute ("name"), e->getStringAttribute ("spec"));
            return p;
        }

        void rebuildPresetMenu()
        {
            presets.clear (juce::dontSendNotification);
            int id = 1;
            auto b = builtInPresets();
            for (auto& k : b.getAllKeys()) presets.addItem (k, id++);
            auto u = userPresets();
            if (u.size() > 0)
            {
                presets.addSeparator();
                for (auto& k : u.getAllKeys()) presets.addItem (k, id++);
            }
            presets.addSeparator();
            presets.addItem ("Save current as preset...", 9000);
            presets.addItem ("Set as session auto-fade shape", 9001);
        }

        void choosePreset()
        {
            const int id = presets.getSelectedId();
            const auto name = presets.getText();
            presets.setSelectedId (0, juce::dontSendNotification);
            if (id == 0) return;

            if (id == 9001)
            {
                fades::setAutoFade (vs.edit, fades::isAutoFadeOn (vs.edit), fades::getAutoFadeMs (vs.edit), &current);
                return;
            }

            if (id == 9000)
            {
                auto* aw = new juce::AlertWindow ("Save fade preset", current.toString(), juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor ("name", "My fade");
                aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                const auto spec = current.toString();
                aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, spec] (int r)
                {
                    if (r == 0) return;
                    const auto n = aw->getTextEditorContents ("name").trim();
                    if (n.isEmpty()) return;
                    juce::XmlElement root ("presets");
                    auto u = userPresets();
                    u.set (n, spec);
                    for (auto& k : u.getAllKeys())
                    {
                        auto* e = root.createNewChildElement ("p");
                        e->setAttribute ("name", k);
                        e->setAttribute ("spec", u[k]);
                    }
                    props()->setValue ("fadePresets", &root);
                    props()->saveIfNeeded();
                    rebuildPresetMenu();
                }), true);
                return;
            }

            auto b = builtInPresets();
            auto u = userPresets();
            const auto spec = b.containsKey (name) ? b[name] : u[name];
            if (spec.isNotEmpty()) apply (FadeSpec::fromString (spec));
        }
    };
}
