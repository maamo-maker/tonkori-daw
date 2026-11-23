#include "ArrangeView.h"
#include "Theme.h"
#include "../model/ClipFades.h"

namespace tonkori::ui
{
using namespace tracktion::literals;

//==============================================================================
namespace
{
    juce::Array<te::AudioTrack*> visibleTracks (te::Edit& e)  { return te::getAudioTracks (e); }

    juce::Colour colourFor (te::Track& t, int index)
    {
        auto c = t.getColour();
        return c.isTransparent() || c == juce::Colour() ? colours::trackColour (index) : c;
    }

    juce::Array<te::InputDeviceInstance*> inputsFor (te::AudioTrack& t)
    {
        return t.edit.getEditInputDevices().getDevicesForTargetTrack (t);
    }

    bool isArmed (te::AudioTrack& t)
    {
        for (auto* in : inputsFor (t))
            if (in->isRecordingEnabled (t.itemID)) return true;
        return false;
    }

    void toggleArm (te::AudioTrack& t)
    {
        const bool armed = isArmed (t);
        auto ins = inputsFor (t);

        if (! armed && ins.isEmpty())
            for (auto* in : t.edit.getAllInputDevices())
                if (in->getInputDevice().getDeviceType() == te::InputDevice::waveDevice && in->getTargets().isEmpty())
                {
                    [[maybe_unused]] auto r = in->setTarget (t.itemID, true, &t.edit.getUndoManager(), 0);
                    ins = inputsFor (t);
                    break;
                }

        for (auto* in : ins)
            in->setRecordingEnabled (t.itemID, ! armed);
    }

    juce::String dbText (float db)  { return db <= -95.0f ? juce::String ("-inf") : juce::String (db, 1); }
}

//==============================================================================
class TrackHeader : public juce::Component, private juce::Timer
{
public:
    TrackHeader (ViewState& v, te::AudioTrack& t, int idx) : vs (v), track (&t), index (idx)
    {
        name.setFont (font (13.0f, true));
        name.setColour (juce::Label::textColourId, colours::text);
        name.setEditable (false, true, false);
        name.onTextChange = [this] { if (track) { vs.edit.getUndoManager().beginNewTransaction ("Rename track"); track->setName (name.getText()); } };
        name.setInterceptsMouseClicks (false, true);
        addAndMakeVisible (name);

        auto setupToggle = [this] (juce::TextButton& b, const juce::String& tip, juce::Colour on)
        {
            b.setClickingTogglesState (false);
            b.setWantsKeyboardFocus (false);
            b.setTooltip (tip);
            b.setColour (juce::TextButton::buttonOnColourId, on);
            addAndMakeVisible (b);
        };
        setupToggle (arm, "Record arm (C)", colours::record);
        setupToggle (mon, "Input monitoring: auto / on / off", colours::play.darker (0.3f));
        setupToggle (mute, "Mute (0 / F1-F8)", colours::warn.darker (0.2f));
        setupToggle (solo, "Solo (S)", colours::accent);

        arm.onClick  = [this] { if (track) toggleArm (*track); refresh(); };
        mute.onClick = [this] { if (track) { vs.edit.getUndoManager().beginNewTransaction ("Mute"); track->setMute (! track->isMuted (false)); } refresh(); };
        solo.onClick = [this] { if (track) { vs.edit.getUndoManager().beginNewTransaction ("Solo"); track->setSolo (! track->isSolo (false)); } refresh(); };
        mon.onClick  = [this] { cycleMonitor(); };

        input.setWantsKeyboardFocus (false);
        input.setTooltip ("Input");
        input.onClick = [this] { showInputMenu(); };
        addAndMakeVisible (input);

        volume.setSliderStyle (juce::Slider::LinearHorizontal);
        volume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        volume.setRange (-96.0, 6.0, 0.1);
        volume.setSkewFactorFromMidPoint (-12.0);
        volume.setDoubleClickReturnValue (true, 0.0);
        volume.setTooltip ("Volume (double-click: 0 dB)");
        volume.onDragStart = [this] { vs.edit.getUndoManager().beginNewTransaction ("Volume"); };
        volume.onValueChange = [this] { if (auto* vp = track ? track->getVolumePlugin() : nullptr) vp->setVolumeDb ((float) volume.getValue()); refresh(); };
        addAndMakeVisible (volume);

        pan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        pan.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        pan.setRange (-1.0, 1.0, 0.01);
        pan.setDoubleClickReturnValue (true, 0.0);
        pan.setTooltip ("Pan (double-click: centre)");
        pan.onDragStart = [this] { vs.edit.getUndoManager().beginNewTransaction ("Pan"); };
        pan.onValueChange = [this] { if (auto* vp = track ? track->getVolumePlugin() : nullptr) vp->setPan ((float) pan.getValue()); };
        addAndMakeVisible (pan);

        dbLabel.setFont (mono (10.0f));
        dbLabel.setColour (juce::Label::textColourId, colours::textDim);
        dbLabel.setJustificationType (juce::Justification::centredRight);
        dbLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (dbLabel);

        refresh();
        startTimerHz (8);
    }

    te::AudioTrack* getTrack() const { return track.get(); }

    void paint (juce::Graphics& g) override
    {
        const bool selected = track && vs.selection.isSelected (track.get());
        g.fillAll (selected ? colours::panelHi : colours::panel);
        g.setColour (track ? colourFor (*track, index) : colours::accent);
        g.fillRect (0, 0, 4, getHeight());
        g.setColour (colours::line);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        g.setColour (colours::textFaint);
        g.setFont (mono (10.0f));
        g.drawText (juce::String (index + 1), getWidth() - 26, 4, 20, 14, juce::Justification::centredRight);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 4).withTrimmedLeft (2);
        auto top = r.removeFromTop (20);
        top.removeFromRight (18);
        name.setBounds (top);

        r.removeFromTop (3);
        auto row = r.removeFromTop (20);
        for (auto* b : { &arm, &mon, &mute, &solo })
        {
            b->setBounds (row.removeFromLeft (26));
            row.removeFromLeft (3);
        }
        row.removeFromLeft (4);
        input.setBounds (row);

        const bool roomy = getHeight() >= 76;
        volume.setVisible (roomy); pan.setVisible (roomy); dbLabel.setVisible (roomy);

        if (roomy)
        {
            r.removeFromTop (5);
            auto vrow = r.removeFromTop (20);
            pan.setBounds (vrow.removeFromRight (22));
            vrow.removeFromRight (4);
            dbLabel.setBounds (vrow.removeFromRight (38));
            volume.setBounds (vrow);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        resizing = e.y > getHeight() - 6;
        startHeight = getHeight();
        if (! resizing && track)
        {
            if (e.mods.isShiftDown() || e.mods.isCommandDown()) vs.selection.addToSelection (track.get());
            else vs.selection.selectOnly (track.get());
            if (e.mods.isPopupMenu()) showMenu();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (resizing && track)
            vs.setTrackHeight (*track, startHeight + e.getDistanceFromDragStartY());
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (e.y > getHeight() - 6 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseDoubleClick (const juce::MouseEvent&) override { name.showEditor(); }

private:
    ViewState& vs;
    te::AudioTrack::Ptr track;
    int index;
    juce::Label name, dbLabel;
    juce::TextButton arm { "R" }, mon { "I" }, mute { "M" }, solo { "S" }, input { "No input" };
    juce::Slider volume, pan;
    bool resizing = false;
    int startHeight = 0;

    void timerCallback() override { refresh(); }

    void refresh()
    {
        if (! track) return;
        if (! name.isBeingEdited()) name.setText (track->getName(), juce::dontSendNotification);
        arm.setToggleState (isArmed (*track), juce::dontSendNotification);
        mute.setToggleState (track->isMuted (false), juce::dontSendNotification);
        solo.setToggleState (track->isSolo (false), juce::dontSendNotification);

        auto ins = inputsFor (*track);
        juce::String inName = "No input";
        juce::String monText = "I";
        bool monOn = false;
        if (! ins.isEmpty())
        {
            auto& dev = ins.getFirst()->getInputDevice();
            inName = dev.getAliasOrName();
            const auto mode = dev.getMonitorMode();
            monOn = mode != te::InputDevice::MonitorMode::off;
            monText = mode == te::InputDevice::MonitorMode::automatic ? "A" : (mode == te::InputDevice::MonitorMode::on ? "I" : "-");
        }
        input.setButtonText (inName);
        mon.setButtonText (monText);
        mon.setToggleState (monOn, juce::dontSendNotification);

        if (auto* vp = track->getVolumePlugin())
        {
            if (! volume.isMouseButtonDown()) volume.setValue (vp->getVolumeDb(), juce::dontSendNotification);
            if (! pan.isMouseButtonDown()) pan.setValue (vp->getPan(), juce::dontSendNotification);
            dbLabel.setText (dbText (vp->getVolumeDb()), juce::dontSendNotification);
        }
        repaint();
    }

    void cycleMonitor()
    {
        auto ins = inputsFor (*track);
        if (ins.isEmpty()) return;
        auto& dev = ins.getFirst()->getInputDevice();
        using MM = te::InputDevice::MonitorMode;
        const auto m = dev.getMonitorMode();
        dev.setMonitorMode (m == MM::automatic ? MM::on : (m == MM::on ? MM::off : MM::automatic));
        refresh();
    }

    void showInputMenu()
    {
        if (! track) return;
        juce::PopupMenu m;
        m.addSectionHeader ("Input for " + track->getName());
        m.addItem (1, "No input", true, inputsFor (*track).isEmpty());
        auto all = vs.edit.getAllInputDevices();
        int id = 100;
        for (auto* in : all)
        {
            auto& d = in->getInputDevice();
            const auto label = (d.getDeviceType() == te::InputDevice::waveDevice ? "Audio: " : "MIDI: ")
                             + d.getAliasOrName();
            m.addItem (id++, label, true, in->getTargets().contains (track->itemID));
        }

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&input), [this, all] (int r)
        {
            if (r == 0 || ! track) return;
            auto* um = &vs.edit.getUndoManager();
            um->beginNewTransaction ("Track input");
            for (auto* in : inputsFor (*track))
                [[maybe_unused]] auto res = in->removeTarget (track->itemID, um);

            if (r >= 100)
                if (auto* in = all[r - 100])
                    [[maybe_unused]] auto res = in->setTarget (track->itemID, false, um, 0);
            refresh();
        });
    }

    void showMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Rename");
        juce::PopupMenu colourMenu;
        for (int i = 0; i < 10; ++i)
            colourMenu.addColouredItem (100 + i, "Colour " + juce::String (i + 1), colours::trackColour (i));
        m.addSubMenu ("Colour", colourMenu);
        m.addSeparator();
        m.addItem (2, "Delete track");
        m.showMenuAsync ({}, [this] (int r)
        {
            if (! track) return;
            if (r == 1) name.showEditor();
            if (r == 2) { vs.edit.getUndoManager().beginNewTransaction ("Delete track"); vs.edit.deleteTrack (track.get()); }
            if (r >= 100) { track->setColour (colours::trackColour (r - 100)); vs.changed(); }
        });
    }
};

//==============================================================================
class TrackHeaderList : public juce::Component
{
public:
    explicit TrackHeaderList (ViewState& v) : vs (v)
    {
        addTrack.setButtonText ("+  Audio track");
        addTrack.setWantsKeyboardFocus (false);
        addTrack.onClick = [this]
        {
            vs.edit.getUndoManager().beginNewTransaction ("Add track");
            const int n = te::getAudioTracks (vs.edit).size();
            vs.edit.ensureNumberOfAudioTracks (n + 1);
            if (auto* t = te::getAudioTracks (vs.edit).getLast()) { t->setName ("Audio " + juce::String (n + 1)); vs.selection.selectOnly (t); }
        };
        addAndMakeVisible (addTrack);
        rebuild();
    }

    void rebuild()
    {
        auto tracks = visibleTracks (vs.edit);
        bool same = tracks.size() == headers.size();
        for (int i = 0; same && i < tracks.size(); ++i) same = headers[i]->getTrack() == tracks[i];

        if (! same)
        {
            headers.clear();
            for (int i = 0; i < tracks.size(); ++i)
                addAndMakeVisible (headers.add (new TrackHeader (vs, *tracks[i], i)));
        }
        layout();
    }

    void layout()
    {
        int y = -vs.scrollY;
        for (auto* h : headers)
        {
            const int th = h->getTrack() ? vs.getTrackHeight (*h->getTrack()) : 80;
            h->setBounds (0, y, getWidth(), th);
            y += th;
        }
        addTrack.setBounds (10, y + 8, getWidth() - 20, 26);
    }

    void resized() override { layout(); }
    void paint (juce::Graphics& g) override { g.fillAll (colours::bg); }

private:
    ViewState& vs;
    juce::OwnedArray<TrackHeader> headers;
    juce::TextButton addTrack;
};

//==============================================================================
class Ruler : public juce::Component
{
public:
    explicit Ruler (ViewState& v) : vs (v) {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::panel);
        auto loopStrip = getLocalBounds().removeFromTop (12);
        g.setColour (colours::bg);
        g.fillRect (loopStrip);

        auto& t = vs.edit.getTransport();
        const auto loop = t.getLoopRange();
        if (loop.getLength() > 0_td)
        {
            const float x1 = vs.timeToX (loop.getStart()), x2 = vs.timeToX (loop.getEnd());
            const bool punch = vs.edit.recordingPunchInOut.get();
            const auto c = punch ? colours::record : colours::accent;
            g.setColour (c.withAlpha (t.looping.get() || punch ? 0.85f : 0.3f));
            g.fillRoundedRectangle (x1, 2.0f, x2 - x1, 8.0f, 2.0f);
        }

        drawTicks (g, getLocalBounds().withTrimmedTop (12));

        // insert marker + playhead
        const float im = vs.timeToX (vs.insertMarker);
        g.setColour (colours::textDim);
        juce::Path tri;
        tri.addTriangle (im - 5, (float) getHeight() - 7, im + 5, (float) getHeight() - 7, im, (float) getHeight());
        g.fillPath (tri);

        const float ph = vs.timeToX (t.isPlaying() ? t.getPosition() : vs.insertMarker);
        g.setColour (colours::playhead);
        g.fillRect (ph - 0.5f, 12.0f, 1.5f, (float) getHeight() - 12);

        g.setColour (colours::lineStrong);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto& t = vs.edit.getTransport();
        const auto time = vs.snap (vs.xToTime (e.x));
        downTime = time;
        inLoopStrip = e.y < 12;
        loopDrag = LoopDrag::none;

        if (inLoopStrip)
        {
            const auto loop = t.getLoopRange();
            const float x1 = vs.timeToX (loop.getStart()), x2 = vs.timeToX (loop.getEnd());
            vs.edit.getUndoManager().beginNewTransaction ("Loop range");
            originalLoop = loop;
            if (loop.getLength() > 0_td && std::abs (e.x - x1) < 6) loopDrag = LoopDrag::start;
            else if (loop.getLength() > 0_td && std::abs (e.x - x2) < 6) loopDrag = LoopDrag::end;
            else if (loop.getLength() > 0_td && e.x > x1 && e.x < x2) loopDrag = LoopDrag::move;
            else loopDrag = LoopDrag::create;
            return;
        }

        vs.insertMarker = time;
        if (! t.isPlaying() || e.mods.isAltDown()) t.setPosition (time);
        vs.changed();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto& t = vs.edit.getTransport();
        const auto time = vs.snap (vs.xToTime (e.x));

        if (inLoopStrip)
        {
            switch (loopDrag)
            {
                case LoopDrag::create: if (time != downTime) t.setLoopRange ({ juce::jmin (time, downTime), juce::jmax (time, downTime) }); break;
                case LoopDrag::start:  if (time < originalLoop.getEnd()) t.setLoopRange ({ time, originalLoop.getEnd() }); break;
                case LoopDrag::end:    if (time > originalLoop.getStart()) t.setLoopRange ({ originalLoop.getStart(), time }); break;
                case LoopDrag::move:
                {
                    auto delta = time - downTime;
                    auto start = originalLoop.getStart() + delta;
                    if (start < 0_tp) start = 0_tp;
                    t.setLoopRange ({ start, start + originalLoop.getLength() });
                    break;
                }
                default: break;
            }
            repaint();
            return;
        }

        // Dragging in the ruler selects time across all tracks.
        if (std::abs (e.getDistanceFromDragStartX()) > 3)
        {
            vs.timeSelection = te::TimeRange (juce::jmin (time, downTime), juce::jmax (time, downTime));
            vs.rangeTracks.clear();
            for (auto* tr : visibleTracks (vs.edit)) vs.rangeTracks.add (tr->itemID);
            vs.changed();
        }
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.y < 12)
        {
            auto& t = vs.edit.getTransport();
            t.looping = ! t.looping.get();
            vs.changed();
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (e.y < 12)
        {
            const auto loop = vs.edit.getTransport().getLoopRange();
            const bool edge = loop.getLength() > 0_td && (std::abs (e.x - vs.timeToX (loop.getStart())) < 6 || std::abs (e.x - vs.timeToX (loop.getEnd())) < 6);
            setMouseCursor (edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        }
        else setMouseCursor (juce::MouseCursor::NormalCursor);
    }

private:
    ViewState& vs;
    enum class LoopDrag { none, create, start, end, move } loopDrag = LoopDrag::none;
    te::TimeRange originalLoop;
    te::TimePosition downTime;
    bool inLoopStrip = false;

    void drawTicks (juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setFont (mono (10.5f));
        const float w = (float) getWidth();
        const auto t0 = vs.xToTime (0), t1 = vs.xToTime (w);

        if (vs.timeFormat == TimeFormat::barsBeats)
        {
            auto& ts = vs.edit.tempoSequence;
            const int num = ts.getTimeSigAt (t0).numerator;
            const float barWidth = vs.durationToWidth (ts.toTime (te::BeatPosition::fromBeats (num)) - te::TimePosition());
            const float beatWidth = barWidth / (float) num;
            int barStep = 1;
            while (barWidth * barStep < 56.0f) barStep *= 2;

            const auto startBar = juce::jmax (0, ts.toBarsAndBeats (t0).bars - 1);
            for (int bar = startBar; ; ++bar)
            {
                const auto bt = ts.toTime (te::BeatPosition::fromBeats ((double) bar * num));
                const float x = vs.timeToX (bt);
                if (bt > t1) break;

                if (bar % barStep == 0)
                {
                    g.setColour (colours::textDim);
                    g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getBottom());
                    g.setColour (colours::text);
                    g.drawText (juce::String (bar + 1), (int) x + 3, area.getY() + 2, 60, 14, juce::Justification::left);
                }
                if (beatWidth > 9.0f && barStep == 1)
                    for (int b = 1; b < num; ++b)
                    {
                        g.setColour (colours::lineStrong);
                        const float bx = x + beatWidth * b;
                        g.drawVerticalLine ((int) bx, (float) area.getBottom() - 7, (float) area.getBottom());
                    }
            }
            return;
        }

        // Seconds-based ruler labelled in the current format
        const double visible = (t1 - t0).inSeconds();
        const double target = visible / juce::jmax (1.0, w / 110.0);
        static const double steps[] = { 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600 };
        double step = 600;
        for (double s : steps) if (s >= target) { step = s; break; }

        for (double s = std::floor (t0.inSeconds() / step) * step; s <= t1.inSeconds(); s += step)
        {
            const float x = vs.timeToX (te::TimePosition::fromSeconds (s));
            g.setColour (colours::textDim);
            g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getBottom());
            g.setColour (colours::text);
            g.drawText (vs.format (te::TimePosition::fromSeconds (s)), (int) x + 3, area.getY() + 2, 110, 14, juce::Justification::left);
        }
    }
};

//==============================================================================
class ArrangeCanvas : public juce::Component
{
public:
    ArrangeCanvas (ViewState& v, ArrangeView& o) : vs (v), owner (o)
    {
        setWantsKeyboardFocus (false);
        setOpaque (true);
    }

    //==============================================================================
    struct Lane { te::AudioTrack* track; int y, h; };

    std::vector<Lane> lanes() const
    {
        std::vector<Lane> r;
        int y = -vs.scrollY;
        for (auto* t : visibleTracks (vs.edit))
        {
            const int h = vs.getTrackHeight (*t);
            r.push_back ({ t, y, h });
            y += h;
        }
        return r;
    }

    int laneIndexAt (int y) const
    {
        auto ls = lanes();
        for (int i = 0; i < (int) ls.size(); ++i)
            if (y >= ls[(size_t) i].y && y < ls[(size_t) i].y + ls[(size_t) i].h) return i;
        if (! ls.empty() && y >= ls.back().y + ls.back().h) return (int) ls.size() - 1;
        return ls.empty() ? -1 : 0;
    }

    juce::Rectangle<float> clipRect (te::Clip& c, const Lane& l) const
    {
        const auto pos = c.getPosition();
        const float x1 = vs.timeToX (pos.getStart()), x2 = vs.timeToX (pos.getEnd());
        return { x1, (float) l.y + 2.0f, juce::jmax (1.0f, x2 - x1), (float) l.h - 4.0f };
    }

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::bg);
        auto ls = lanes();

        for (size_t i = 0; i < ls.size(); ++i)
        {
            auto& l = ls[i];
            const bool sel = vs.selection.isSelected (l.track);
            g.setColour (sel ? colours::laneAlt.brighter (0.04f) : (i % 2 ? colours::laneAlt : colours::lane));
            g.fillRect (0, l.y, getWidth(), l.h);
        }

        drawGrid (g);

        // range selection
        if (vs.timeSelection)
        {
            const float x1 = vs.timeToX (vs.timeSelection->getStart()), x2 = vs.timeToX (vs.timeSelection->getEnd());
            for (auto& l : ls)
                if (vs.rangeTracks.contains (l.track->itemID))
                {
                    g.setColour (colours::selection);
                    g.fillRect (x1, (float) l.y, x2 - x1, (float) l.h);
                }
            g.setColour (colours::accent.withAlpha (0.8f));
            g.drawVerticalLine ((int) x1, 0.0f, (float) getHeight());
            g.drawVerticalLine ((int) x2, 0.0f, (float) getHeight());
        }

        for (auto& l : ls)
        {
            g.setColour (colours::line);
            g.drawHorizontalLine (l.y + l.h - 1, 0.0f, (float) getWidth());

            for (auto* c : l.track->getClips())
            {
                auto r = clipRect (*c, l);
                if (r.getRight() < 0 || r.getX() > getWidth()) continue;
                const bool dragged = drag.type == DragType::move && drag.hasMoved && isDragged (c);
                drawClip (g, *c, r, l, dragged ? 0.35f : 1.0f);
            }

            drawRecording (g, l);
        }

        if (drag.type == DragType::move && drag.hasMoved)
            drawMoveGhosts (g, ls);

        // insert marker + playhead
        auto& t = vs.edit.getTransport();
        g.setColour (colours::textDim.withAlpha (0.7f));
        const float im = vs.timeToX (vs.insertMarker);
        const float dashes[] = { 3.0f, 3.0f };
        g.drawDashedLine ({ im, 0.0f, im, (float) getHeight() }, dashes, 2, 1.0f);

        if (t.isPlaying())
        {
            const float ph = vs.timeToX (t.getPosition());
            g.setColour (t.isRecording() ? colours::record : colours::playhead);
            g.fillRect (ph - 0.5f, 0.0f, 1.5f, (float) getHeight());
        }
        else
        {
            g.setColour (colours::playhead.withAlpha (0.85f));
            g.fillRect (im - 0.5f, 0.0f, 1.5f, (float) getHeight());
        }
    }

    //==============================================================================
    // Mouse: Smart Tool (Pro Tools) — top corners fade, edges trim, upper half selects time,
    // lower half moves. Other tools force one behaviour. Everything works while playing/recording.
    void mouseMove (const juce::MouseEvent& e) override
    {
        auto hit = hitTest (e.position);
        using C = juce::MouseCursor;
        switch (hit.zone)
        {
            case Zone::trimStart: case Zone::trimEnd: setMouseCursor (C::LeftRightResizeCursor); break;
            case Zone::fadeIn: case Zone::fadeOut:    setMouseCursor (C::CrosshairCursor); break;
            case Zone::body:                          setMouseCursor (C::DraggingHandCursor); break;
            case Zone::split:                         setMouseCursor (C::CopyingCursor); break;
            default:                                  setMouseCursor (C::IBeamCursor); break;
        }
        if (hover.clip != hit.clip || hover.zone != hit.zone) { hover = hit; repaint(); }
    }

    void mouseExit (const juce::MouseEvent&) override { hover = {}; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = {};
        auto hit = hitTest (e.position);
        drag.downPos = e.position;
        drag.downTime = vs.xToTime (e.x);
        drag.downLane = laneIndexAt (e.y);

        if (e.mods.isPopupMenu())
        {
            if (hit.clip && ! vs.selection.isSelected (hit.clip)) vs.selection.selectOnly (hit.clip);
            showContextMenu (hit, e);
            return;
        }

        auto& um = vs.edit.getUndoManager();

        if (hit.zone == Zone::split && hit.clip)
        {
            um.beginNewTransaction ("Split");
            te::SelectableList l; l.add (hit.clip);
            vs.selection.select (te::splitClips (l, vs.snap (vs.xToTime (e.x))));
            return;
        }

        drag.clip = hit.clip;
        drag.audioClip = dynamic_cast<te::AudioClipBase*> (hit.clip);

        switch (hit.zone)
        {
            case Zone::fadeIn:
            case Zone::fadeOut:
            {
                if (! drag.audioClip) break;
                um.beginNewTransaction ("Fade");
                drag.type = e.mods.isAltDown() ? DragType::fadeShape : DragType::fade;
                drag.fadeIn = hit.zone == Zone::fadeIn;
                drag.startSpec = fades::getSpec (*drag.audioClip, drag.fadeIn);
                if (drag.type == DragType::fadeShape && FadeSpec::getParamName (drag.startSpec.shape).isEmpty())
                {
                    drag.startSpec.shape = FadeSpec::Shape::exponential;
                    drag.startSpec.param = 0.0;
                }
                vs.selection.selectOnly (hit.clip);
                break;
            }
            case Zone::trimStart:
            case Zone::trimEnd:
                um.beginNewTransaction ("Trim");
                drag.type = hit.zone == Zone::trimStart ? DragType::trimStart : DragType::trimEnd;
                drag.original = hit.clip->getPosition();
                captureTrackClips (hit.clip);
                vs.selection.selectOnly (hit.clip);
                break;

            case Zone::body:
                drag.type = DragType::move;
                if (e.mods.isCommandDown() || e.mods.isShiftDown())
                {
                    if (vs.selection.isSelected (hit.clip)) { vs.selection.deselect (hit.clip); drag.type = DragType::none; }
                    else vs.selection.addToSelection (hit.clip);
                }
                else if (! vs.selection.isSelected (hit.clip))
                {
                    vs.selection.selectOnly (hit.clip);
                }
                else drag.clickedSelected = true;

                for (auto* c : vs.selection.getItemsOfType<te::Clip>())
                    drag.moving.push_back ({ c->itemID, c->getPosition(), laneIndexOfClip (c) });
                drag.copy = e.mods.isAltDown();
                break;

            case Zone::range:
            case Zone::none:
            default:
                drag.type = DragType::range;
                drag.rangeAnchor = vs.snap (vs.xToTime (e.x));
                break;
        }
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        if (e.getDistanceFromDragStart() > 3) drag.hasMoved = true;
        if (! drag.hasMoved) return;

        const auto time = vs.xToTime (e.x);

        switch (drag.type)
        {
            case DragType::fade:
            {
                auto* c = drag.audioClip;
                const auto pos = c->getPosition();
                const auto len = drag.fadeIn ? time - pos.getStart() : pos.getEnd() - time;
                fades::setLength (*c, drag.fadeIn, te::TimeDuration::fromSeconds (juce::jmax (0.0, len.inSeconds())));
                if (! c->state.hasProperty (drag.fadeIn ? ids::fadeInSpec : ids::fadeOutSpec))
                    fades::setSpec (*c, drag.fadeIn, drag.startSpec);
                vs.changed();
                break;
            }
            case DragType::fadeShape:
            {
                auto s = drag.startSpec;
                const auto range = FadeSpec::getParamRange (s.shape);
                const double dy = -(double) e.getDistanceFromDragStartY() / 120.0;
                const double sign = s.shape == FadeSpec::Shape::exponential ? -1.0 : 1.0;
                s.param = range.clipValue (drag.startSpec.param + sign * dy * range.getLength());
                fades::setSpec (*drag.audioClip, drag.fadeIn, s);
                vs.changed();
                break;
            }
            case DragType::trimStart:
            {
                auto* c = drag.clip;
                auto newStart = vs.snap (time);
                const auto minStart = drag.original.getStart() - drag.original.getOffset();
                const auto maxStart = drag.original.getEnd() - te::TimeDuration::fromSeconds (0.002);
                newStart = juce::jlimit (juce::jmax (0_tp, minStart), maxStart, newStart);
                c->setStart (newStart, true, false);
                vs.changed();
                break;
            }
            case DragType::trimEnd:
            {
                auto* c = drag.clip;
                auto newEnd = vs.snap (time);
                newEnd = juce::jmax (drag.original.getStart() + te::TimeDuration::fromSeconds (0.002), newEnd);
                if (auto* ac = dynamic_cast<te::AudioClipBase*> (c))
                {
                    const auto maxEnd = drag.original.getStart() + (ac->getSourceLength() - drag.original.getOffset());
                    if (ac->getSourceLength() > 0_td && ! ac->isLooping()) newEnd = juce::jmin (newEnd, maxEnd);
                }
                c->setEnd (newEnd, true);
                if (vs.mode == EditMode::shuffle) rippleFollowing (c->getPosition().getEnd() - drag.original.getEnd());
                vs.changed();
                break;
            }
            case DragType::move:
            {
                if (drag.moving.empty()) break;
                auto& primary = drag.moving.front();
                for (auto& m : drag.moving) if (m.id == (drag.clip ? drag.clip->itemID : te::EditItemID())) primary = m;
                auto newStart = primary.pos.getStart() + (time - drag.downTime);
                newStart = vs.snap (newStart < 0_tp ? 0_tp : newStart);
                drag.moveDelta = newStart - primary.pos.getStart();
                drag.laneDelta = laneIndexAt (e.y) - drag.downLane;
                repaint();
                break;
            }
            case DragType::range:
            {
                const auto t2 = vs.snap (time);
                vs.timeSelection = te::TimeRange (juce::jmin (t2, drag.rangeAnchor), juce::jmax (t2, drag.rangeAnchor));
                vs.rangeTracks.clear();
                auto ls = lanes();
                const int a = juce::jlimit (0, (int) ls.size() - 1, drag.downLane), b = juce::jlimit (0, (int) ls.size() - 1, laneIndexAt (e.y));
                for (int i = juce::jmin (a, b); i <= juce::jmax (a, b) && i < (int) ls.size(); ++i)
                    vs.rangeTracks.add (ls[(size_t) i].track->itemID);
                vs.selection.deselectAll();
                vs.changed();
                break;
            }
            default: break;
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { drag = {}; return; }

        if (drag.type == DragType::move && drag.hasMoved)
            commitMove();
        else if (! drag.hasMoved)
        {
            if (drag.type == DragType::range)
            {
                // Plain click on empty space: place the insert marker, select the track.
                vs.timeSelection.reset();
                vs.rangeTracks.clear();
                const auto t = vs.snap (vs.xToTime (e.x));
                vs.insertMarker = t;
                if (! vs.edit.getTransport().isPlaying()) vs.edit.getTransport().setPosition (t);
                auto ls = lanes();
                const int li = laneIndexAt (e.y);
                if (li >= 0 && li < (int) ls.size() && e.y < ls.back().y + ls.back().h) vs.selection.selectOnly (ls[(size_t) li].track);
                else vs.selection.deselectAll();
            }
            else if (drag.type == DragType::move && drag.clip)
            {
                if (drag.clickedSelected && ! e.mods.isAnyModifierKeyDown()) vs.selection.selectOnly (drag.clip);
                vs.timeSelection.reset();
                vs.insertMarker = vs.snap (vs.xToTime (e.x));
                if (! vs.edit.getTransport().isPlaying()) vs.edit.getTransport().setPosition (vs.insertMarker);
            }
        }

        drag = {};
        vs.changed();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        auto hit = hitTest (e.position);
        if (! hit.clip) return;

        if ((hit.zone == Zone::fadeIn || hit.zone == Zone::fadeOut) && owner.onOpenFadeEditor)
            if (auto* ac = dynamic_cast<te::AudioClipBase*> (hit.clip))
            {
                owner.onOpenFadeEditor (*ac, hit.zone == Zone::fadeIn, localAreaToGlobal (clipRect (*hit.clip, lanes()[(size_t) laneIndexOfClip (hit.clip)]).toNearestInt()));
                return;
            }

        if (vs.mode == EditMode::spot && owner.onSpotClip) { owner.onSpotClip (*hit.clip); return; }

        // Pro Tools selector double-click: select the clip's time range.
        vs.timeSelection = hit.clip->getPosition().time;
        vs.rangeTracks = { hit.clip->getTrack()->itemID };
        vs.changed();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown() || e.mods.isAltDown() || e.mods.isCtrlDown())
        {
            vs.setZoom (vs.pixelsPerSecond * std::pow (1.0025, (w.deltaY != 0 ? w.deltaY : w.deltaX) * 600.0), e.x);
            return;
        }

        const float dx = e.mods.isShiftDown() ? w.deltaY : w.deltaX;
        const float dy = e.mods.isShiftDown() ? 0.0f : w.deltaY;

        if (dx != 0.0f)
        {
            vs.viewStart = juce::jmax (0.0, vs.viewStart - dx * 400.0 / vs.pixelsPerSecond);
            vs.changed();
        }
        if (dy != 0.0f)
        {
            vs.scrollY = juce::jlimit (0, juce::jmax (0, totalHeight() - getHeight() + 60), vs.scrollY - (int) (dy * 300.0f));
            vs.changed();
        }
    }

    void mouseMagnify (const juce::MouseEvent& e, float scale) override { vs.setZoom (vs.pixelsPerSecond * scale, e.x); }

    int totalHeight() const
    {
        int h = 0;
        for (auto* t : visibleTracks (vs.edit)) h += vs.getTrackHeight (*t);
        return h;
    }

    juce::Rectangle<int> screenBoundsOf (te::Clip& c) const
    {
        auto ls = lanes();
        const int li = laneIndexOfClip (&c);
        if (li < 0) return {};
        return localAreaToGlobal (clipRect (c, ls[(size_t) li]).toNearestInt());
    }

private:
    ViewState& vs;
    ArrangeView& owner;
    std::map<te::EditItemID, std::pair<juce::File, std::unique_ptr<te::SmartThumbnail>>> thumbs;

    enum class Zone { none, body, range, trimStart, trimEnd, fadeIn, fadeOut, split };
    struct Hit { te::Clip* clip = nullptr; Zone zone = Zone::none; };
    Hit hover;

    enum class DragType { none, move, trimStart, trimEnd, fade, fadeShape, range };
    struct Moving { te::EditItemID id; te::ClipPosition pos; int lane; };
    struct DragState
    {
        DragType type = DragType::none;
        te::Clip* clip = nullptr;
        te::AudioClipBase* audioClip = nullptr;
        juce::Point<float> downPos;
        te::TimePosition downTime, rangeAnchor;
        int downLane = 0, laneDelta = 0;
        bool hasMoved = false, copy = false, fadeIn = true, clickedSelected = false;
        te::ClipPosition original;
        FadeSpec startSpec;
        te::TimeDuration moveDelta;
        std::vector<Moving> moving;
        std::vector<std::pair<te::EditItemID, te::TimePosition>> following;
    } drag;

    bool isDragged (te::Clip* c) const
    {
        for (auto& m : drag.moving) if (m.id == c->itemID) return true;
        return false;
    }

    int laneIndexOfClip (te::Clip* c) const
    {
        auto ls = lanes();
        for (int i = 0; i < (int) ls.size(); ++i)
            if (c->getTrack() == ls[(size_t) i].track) return i;
        return -1;
    }

    //==============================================================================
    Hit hitTest (juce::Point<float> p) const
    {
        Hit h;
        auto ls = lanes();
        for (auto& l : ls)
        {
            if (p.y < l.y || p.y >= l.y + l.h) continue;

            for (auto* c : l.track->getClips())
            {
                auto r = clipRect (*c, l);
                if (! r.contains (p)) continue;
                h.clip = c;

                switch (vs.tool)
                {
                    case Tool::range: h.zone = Zone::range; return h;
                    case Tool::grab:  h.zone = Zone::body; return h;
                    case Tool::split: h.zone = Zone::split; return h;
                    case Tool::trim:  h.zone = p.x < r.getCentreX() ? Zone::trimStart : Zone::trimEnd; return h;
                    case Tool::fade:  h.zone = p.x < r.getCentreX() ? Zone::fadeIn : Zone::fadeOut; return h;
                    case Tool::smart: break;
                }

                // Fade corners: header strip plus the top third of the waveform, near either edge.
                const float header = r.getHeight() > 34 ? 15.0f : 0.0f;
                const float topZone = header + juce::jmax (8.0f, (r.getHeight() - header) * 0.35f);
                if (auto* ac = dynamic_cast<te::AudioClipBase*> (c); ac != nullptr && p.y < r.getY() + topZone)
                {
                    const float fi = r.getX() + vs.durationToWidth (ac->getFadeIn());
                    const float fo = r.getRight() - vs.durationToWidth (ac->getFadeOut());
                    if (p.x < juce::jmax (r.getX() + 18.0f, fi + 6.0f) && p.x < r.getCentreX()) { h.zone = Zone::fadeIn; return h; }
                    if (p.x > juce::jmin (r.getRight() - 18.0f, fo - 6.0f) && p.x >= r.getCentreX()) { h.zone = Zone::fadeOut; return h; }
                }

                const float edge = juce::jmin (7.0f, r.getWidth() * 0.2f);
                if (p.x < r.getX() + edge)     { h.zone = Zone::trimStart; return h; }
                if (p.x > r.getRight() - edge) { h.zone = Zone::trimEnd; return h; }

                h.zone = p.y < r.getCentreY() ? Zone::range : Zone::body;
                return h;
            }
        }
        return h;
    }

    //==============================================================================
    void captureTrackClips (te::Clip* c)
    {
        drag.following.clear();
        if (auto* ct = dynamic_cast<te::ClipTrack*> (c->getTrack()))
            for (auto* o : ct->getClips())
                if (o != c && o->getPosition().getStart() >= c->getPosition().getEnd() - te::TimeDuration::fromSeconds (1e-6))
                    drag.following.push_back ({ o->itemID, o->getPosition().getStart() });
    }

    void rippleFollowing (te::TimeDuration delta)
    {
        for (auto& [id, start] : drag.following)
            if (auto* o = te::findClipForID (vs.edit, id))
                o->setStart (start + delta, false, true);
    }

    void commitMove()
    {
        auto ls = lanes();
        if (ls.empty() || drag.moving.empty()) return;

        auto& um = vs.edit.getUndoManager();
        um.beginNewTransaction (drag.copy ? "Copy clips" : "Move clips");

        // Shuffle: single clip within its track reorders, closing and opening gaps (Pro Tools).
        if (vs.mode == EditMode::shuffle && drag.moving.size() == 1 && drag.laneDelta == 0 && ! drag.copy)
        {
            auto* c = te::findClipForID (vs.edit, drag.moving[0].id);
            auto* ct = c ? dynamic_cast<te::ClipTrack*> (c->getTrack()) : nullptr;
            if (c && ct)
            {
                const auto len = c->getPosition().getLength();
                const auto oldStart = c->getPosition().getStart();
                auto target = oldStart + drag.moveDelta;

                for (auto* o : ct->getClips())                                  // close the gap
                    if (o != c && o->getPosition().getStart() >= oldStart)
                        o->setStart (o->getPosition().getStart() - len, false, true);
                if (target > oldStart) target = target - len;
                if (target < 0_tp) target = 0_tp;
                for (auto* o : ct->getClips())                                  // open a gap at the target
                    if (o != c && o->getPosition().getStart() >= target)
                        o->setStart (o->getPosition().getStart() + len, false, true);
                c->setStart (target, false, true);
                if (vs.mode == EditMode::spot && owner.onSpotClip) owner.onSpotClip (*c);
                return;
            }
        }

        te::SelectableList moved;
        for (auto& m : drag.moving)
        {
            auto* c = te::findClipForID (vs.edit, m.id);
            if (! c) continue;

            te::Clip* target = c;
            if (drag.copy)
                if (auto n = te::duplicateClip (*c)) target = n.get();

            auto newStart = m.pos.getStart() + drag.moveDelta;
            if (newStart < 0_tp) newStart = 0_tp;
            target->setStart (newStart, false, true);

            const int newLane = juce::jlimit (0, (int) ls.size() - 1, m.lane + drag.laneDelta);
            if (newLane != m.lane)
                target->moveTo (*ls[(size_t) newLane].track);
            moved.add (target);
        }
        vs.selection.select (moved);

        if (vs.mode == EditMode::spot && owner.onSpotClip && moved.size() == 1)
            if (auto* c = dynamic_cast<te::Clip*> (moved[0])) owner.onSpotClip (*c);
    }

    //==============================================================================
    void drawGrid (juce::Graphics& g)
    {
        auto& ts = vs.edit.tempoSequence;
        const auto t0 = vs.xToTime (0), t1 = vs.xToTime (getWidth());
        const int num = ts.getTimeSigAt (t0).numerator;
        const double gridBeats = vs.gridBeats (t0).inBeats();
        const float gridPx = vs.durationToWidth (ts.toTime (te::BeatPosition::fromBeats (gridBeats)) - te::TimePosition());
        const double step = gridPx >= 6.0f ? gridBeats : (vs.durationToWidth (ts.toTime (te::BeatPosition::fromBeats (1.0)) - te::TimePosition()) >= 6.0f ? 1.0 : (double) num);

        double b = std::floor (ts.toBeats (t0).inBeats() / step) * step;
        const double endBeat = ts.toBeats (t1).inBeats();
        int guard = 0;
        for (; b <= endBeat && guard < 4000; b += step, ++guard)
        {
            const float x = vs.timeToX (ts.toTime (te::BeatPosition::fromBeats (b)));
            const bool isBar = std::fmod (b + 1e-6, (double) num) < 1e-3;
            const bool isBeat = std::fmod (b + 1e-6, 1.0) < 1e-3;
            g.setColour (isBar ? colours::lineStrong : (isBeat ? colours::line : colours::line.withAlpha (0.45f)));
            g.drawVerticalLine ((int) x, 0.0f, (float) getHeight());
        }
    }

    te::SmartThumbnail* thumbFor (te::AudioClipBase& c)
    {
        te::AudioFile af = c.getPlaybackFile();
        if (af.getFile() == juce::File()) af = te::AudioFile (vs.edit.engine, c.getOriginalFile());
        auto& entry = thumbs[c.itemID];
        if (entry.second == nullptr || entry.first != af.getFile())
        {
            entry.first = af.getFile();
            entry.second = std::make_unique<te::SmartThumbnail> (vs.edit.engine, af, *this, &vs.edit);
        }
        return entry.second.get();
    }

    void drawClip (juce::Graphics& g, te::Clip& c, juce::Rectangle<float> r, const Lane& lane, float alpha)
    {
        const bool selected = vs.selection.isSelected (&c);
        const int laneIdx = laneIndexOfClip (&c);
        auto base = colourFor (*lane.track, juce::jmax (0, laneIdx));
        auto* ac = dynamic_cast<te::AudioClipBase*> (&c);
        const bool muted = c.isMuted();
        if (muted) base = base.withSaturation (0.0f).darker (0.3f);

        auto clipArea = r.withRight (juce::jmin (r.getRight(), (float) getWidth() + 4.0f)).withLeft (juce::jmax (r.getX(), -4.0f));

        g.setColour (base.withAlpha (0.28f * alpha));
        g.fillRoundedRectangle (clipArea, 3.0f);

        const bool showHeader = r.getHeight() > 34;
        auto wave = r;
        if (showHeader)
        {
            auto header = clipArea.removeFromTop (15.0f);
            g.setColour (base.withAlpha ((selected ? 0.95f : 0.7f) * alpha));
            g.fillRoundedRectangle (header.withHeight (15.0f), 3.0f);
            g.setColour (juce::Colours::black.withAlpha (0.85f * alpha));
            g.setFont (font (11.0f, true));
            juce::String title = c.getName();
            if (muted) title = "(muted) " + title;
            if (ac && ac->getIsReversed()) title = "\xe2\x97\x80 " + title;       // ◀
            if (ac && ! fades::areFadesOn (*ac)) title += "   fades off";
            g.drawText (title, header.reduced (5, 0).toNearestInt(), juce::Justification::centredLeft, true);
            wave = r.withTrimmedTop (15.0f);
        }

        if (ac != nullptr && wave.getWidth() > 1 && wave.getHeight() > 4)
            drawWaveform (g, *ac, wave, base, alpha);

        if (ac != nullptr)
            drawFades (g, *ac, wave, alpha);

        g.setColour (selected ? colours::text : base.withAlpha (0.8f * alpha));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, selected ? 1.6f : 1.0f);

        // Hover affordances (Smart Tool)
        if (hover.clip == &c && ac != nullptr)
        {
            g.setColour (colours::text.withAlpha (0.9f));
            if (hover.zone == Zone::fadeIn)  g.fillRect (r.getX() + vs.durationToWidth (ac->getFadeIn()) - 3.0f, wave.getY(), 6.0f, 6.0f);
            if (hover.zone == Zone::fadeOut) g.fillRect (r.getRight() - vs.durationToWidth (ac->getFadeOut()) - 3.0f, wave.getY(), 6.0f, 6.0f);
            if (hover.zone == Zone::trimStart) g.fillRect (r.getX(), r.getY(), 3.0f, r.getHeight());
            if (hover.zone == Zone::trimEnd)   g.fillRect (r.getRight() - 3.0f, r.getY(), 3.0f, r.getHeight());
        }
    }

    void drawWaveform (juce::Graphics& g, te::AudioClipBase& c, juce::Rectangle<float> area, juce::Colour base, float alpha)
    {
        auto* thumb = thumbFor (c);
        if (thumb == nullptr) return;

        const int left = (int) juce::jmax (0.0f, area.getX()), right = (int) juce::jmin ((float) getWidth(), area.getRight());
        if (right <= left) return;

        const auto pos = c.getPosition();
        const auto t1 = vs.xToTime (left), t2 = vs.xToTime (right);
        te::TimeRange sourceRange;

        if (c.usesTimeStretchedProxy() || c.getIsReversed())
            sourceRange = { te::TimePosition() + (t1 - pos.getStart()), te::TimePosition() + (t2 - pos.getStart()) };
        else
        {
            const double speed = c.getSpeedRatio();
            sourceRange = { te::TimePosition::fromSeconds ((t1 - pos.getStart() + pos.getOffset()).inSeconds() * speed),
                            te::TimePosition::fromSeconds ((t2 - pos.getStart() + pos.getOffset()).inSeconds() * speed) };
        }

        const float gain = c.getGain();
        auto rect = juce::Rectangle<int> (left, (int) area.getY() + 1, right - left, (int) area.getHeight() - 2);
        g.setColour (base.brighter (0.35f).withAlpha (0.92f * alpha));

        if (thumb->getNumChannels() > 1 && rect.getHeight() > 30)
        {
            auto top = rect.removeFromTop (rect.getHeight() / 2);
            thumb->drawChannel (g, top, sourceRange, 0, gain);
            thumb->drawChannel (g, rect, sourceRange, 1, gain);
        }
        else
            thumb->drawChannel (g, rect, sourceRange, 0, gain);
    }

    void drawFades (juce::Graphics& g, te::AudioClipBase& c, juce::Rectangle<float> area, float alpha)
    {
        const bool on = fades::areFadesOn (c);
        const auto ovIn = fades::resolve (c, true), ovOut = fades::resolve (c, false);

        auto drawSide = [&] (bool isIn, te::TimeDuration len, const FadeSpec& spec, bool audible, bool isAuto)
        {
            const float w = vs.durationToWidth (len);
            if (w < 1.0f) return;

            juce::Path curve, shade;
            const float x0 = isIn ? area.getX() : area.getRight() - w;
            const int n = juce::jlimit (4, 400, (int) w);

            shade.startNewSubPath (x0, area.getY());
            for (int i = 0; i <= n; ++i)
            {
                const double u = (double) i / n;
                const double gain = spec.gainAt (isIn ? u : 1.0 - u);
                const float x = x0 + (float) u * w;
                const float y = area.getBottom() - (float) gain * area.getHeight();
                if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
                shade.lineTo (x, y);
            }
            shade.lineTo (x0 + w, area.getY());
            shade.closeSubPath();

            if (audible)
            {
                g.setColour (juce::Colours::black.withAlpha (0.38f * alpha));
                g.fillPath (shade);
                g.setColour ((isAuto ? colours::textDim : colours::text).withAlpha (0.9f * alpha));
                g.strokePath (curve, juce::PathStrokeType (1.4f));
            }
            else
            {
                const float dashes[] = { 3.0f, 3.0f };
                juce::Path d;
                juce::PathStrokeType (1.0f).createDashedStroke (d, curve, dashes, 2);
                g.setColour (colours::textDim.withAlpha (0.7f * alpha));
                g.fillPath (d);
            }
        };

        const bool hasIn = c.getFadeIn() > 0_td, hasOut = c.getFadeOut() > 0_td;
        const auto specIn = fades::getSpec (c, true), specOut = fades::getSpec (c, false);

        if (ovIn.valid && ovIn.length > 0_td)
        {
            const bool isAuto = ! on || ovIn.length > c.getFadeIn();
            drawSide (true, ovIn.length, isAuto ? fades::getAutoFadeSpec (vs.edit) : specIn, true, isAuto);
        }
        else if (! ovIn.valid && hasIn) drawSide (true, c.getFadeIn(), specIn, true, false);
        if (! on && hasIn) drawSide (true, c.getFadeIn(), specIn, false, false);

        if (ovOut.valid && ovOut.length > 0_td)
        {
            const bool isAuto = ! on || ovOut.length > c.getFadeOut();
            drawSide (false, ovOut.length, isAuto ? fades::getAutoFadeSpec (vs.edit) : specOut, true, isAuto);
        }
        else if (! ovOut.valid && hasOut) drawSide (false, c.getFadeOut(), specOut, true, false);
        if (! on && hasOut) drawSide (false, c.getFadeOut(), specOut, false, false);
    }

    void drawMoveGhosts (juce::Graphics& g, const std::vector<Lane>& ls)
    {
        for (auto& m : drag.moving)
        {
            auto* c = te::findClipForID (vs.edit, m.id);
            if (! c) continue;
            const int li = juce::jlimit (0, (int) ls.size() - 1, m.lane + drag.laneDelta);
            auto& l = ls[(size_t) li];
            const float x1 = vs.timeToX (m.pos.getStart() + drag.moveDelta);
            const float w = vs.durationToWidth (m.pos.getLength());
            juce::Rectangle<float> r (x1, (float) l.y + 2.0f, w, (float) l.h - 4.0f);
            g.setColour (colours::accent.withAlpha (0.22f));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (colours::accent);
            g.drawRoundedRectangle (r, 3.0f, 1.5f);
            g.setFont (mono (10.5f));
            g.setColour (colours::text);
            g.drawText (vs.format (m.pos.getStart() + drag.moveDelta) + (drag.copy ? "  copy" : ""),
                        r.reduced (4, 2).toNearestInt(), juce::Justification::topLeft, false);
        }
    }

    void drawRecording (juce::Graphics& g, const Lane& l)
    {
        auto& t = vs.edit.getTransport();
        if (! t.isRecording()) return;

        for (auto* in : inputsFor (*l.track))
        {
            if (! in->isRecording (l.track->itemID)) continue;

            const auto punchIn = in->getPunchInTime (l.track->itemID);
            const auto now = t.getPosition();
            if (now <= punchIn) continue;

            juce::Rectangle<float> r (vs.timeToX (punchIn), (float) l.y + 2.0f, vs.durationToWidth (now - punchIn), (float) l.h - 4.0f);
            g.setColour (colours::record.withAlpha (0.28f));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (colours::record);
            g.drawRoundedRectangle (r, 3.0f, 1.2f);

            const auto file = in->getRecordingFile (l.track->itemID);
            if (file.exists())
            {
                auto thumb = vs.edit.engine.getRecordingThumbnailManager().getThumbnailFor (file);
                if (thumb != nullptr && thumb->thumb != nullptr)
                {
                    g.setColour (colours::record.brighter (0.6f));
                    thumb->thumb->drawChannels (g, r.reduced (0, 3).toNearestInt(), 0.0, (now - punchIn).inSeconds(), 1.0f);
                }
            }
        }
    }

    //==============================================================================
    void showContextMenu (Hit hit, const juce::MouseEvent& e)
    {
        juce::PopupMenu m;
        auto* ac = dynamic_cast<te::AudioClipBase*> (hit.clip);
        const auto clickTime = vs.snap (vs.xToTime (e.x));

        if (hit.clip)
        {
            if (ac)
            {
                m.addItem (1, "Fade editor...");
                m.addItem (2, "Fades on", true, fades::areFadesOn (*ac));
                m.addItem (3, "Create 10 ms fades");
                m.addItem (4, "Remove fades");
                m.addSeparator();
                m.addItem (5, "Reverse", true, ac->getIsReversed());
            }
            m.addItem (6, "Mute", true, hit.clip->isMuted());
            m.addItem (7, "Split here");
            m.addItem (8, "Spot...");
            m.addSeparator();
            m.addItem (9, "Delete");
        }
        else
        {
            m.addItem (20, "Select whole track");
            m.addItem (21, "Place insert marker here");
        }

        juce::Component::SafePointer<ArrangeCanvas> safe (this);
        const auto clipID = hit.clip ? hit.clip->itemID : te::EditItemID();
        const auto screen = localAreaToGlobal (juce::Rectangle<int> ((int) e.x, (int) e.y, 1, 1));

        m.showMenuAsync ({}, [safe, clipID, clickTime, screen] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& vs = safe->vs;
            auto* clip = te::findClipForID (vs.edit, clipID);
            auto* ac = dynamic_cast<te::AudioClipBase*> (clip);
            auto& um = vs.edit.getUndoManager();
            um.beginNewTransaction ("Clip menu");

            switch (r)
            {
                case 1: if (ac && safe->owner.onOpenFadeEditor) safe->owner.onOpenFadeEditor (*ac, true, screen); break;
                case 2: if (ac) fades::setFadesOn (*ac, ! fades::areFadesOn (*ac)); break;
                case 3: if (ac) { fades::setLength (*ac, true, te::TimeDuration::fromSeconds (0.01)); fades::setLength (*ac, false, te::TimeDuration::fromSeconds (0.01)); } break;
                case 4: if (ac) { fades::setLength (*ac, true, {}); fades::setLength (*ac, false, {}); } break;
                case 5: if (ac) ac->setIsReversed (! ac->getIsReversed()); break;
                case 6: if (clip) clip->setMuted (! clip->isMuted()); break;
                case 7: if (clip) { te::SelectableList l; l.add (clip); vs.selection.select (te::splitClips (l, clickTime)); } break;
                case 8: if (clip && safe->owner.onSpotClip) safe->owner.onSpotClip (*clip); break;
                case 9: if (clip) clip->removeFromParent(); break;
                case 20:
                {
                    te::SelectableList l;
                    for (auto* t : te::getAudioTracks (vs.edit)) for (auto* c : t->getClips()) l.add (c);
                    vs.selection.select (l);
                    break;
                }
                case 21: vs.insertMarker = clickTime; if (! vs.edit.getTransport().isPlaying()) vs.edit.getTransport().setPosition (clickTime); break;
                default: break;
            }
            vs.changed();
        });
    }
};

//==============================================================================
ArrangeView::ArrangeView (ViewState& v) : vs (v)
{
    ruler = std::make_unique<Ruler> (vs);
    headers = std::make_unique<TrackHeaderList> (vs);
    canvas = std::make_unique<ArrangeCanvas> (vs, *this);

    addAndMakeVisible (*ruler);
    addAndMakeVisible (*headers);
    addAndMakeVisible (*canvas);
    addAndMakeVisible (hScroll);
    addAndMakeVisible (vScroll);
    hScroll.addListener (this);
    vScroll.addListener (this);
    hScroll.setAutoHide (false);
    vScroll.setAutoHide (true);

    cornerLabel.setFont (font (11.0f, true));
    cornerLabel.setColour (juce::Label::textColourId, colours::textDim);
    cornerLabel.setText ("TRACKS", juce::dontSendNotification);
    addAndMakeVisible (cornerLabel);

    vs.addChangeListener (this);
    vs.selection.addChangeListener (this);
    vs.edit.state.addListener (this);
    startTimerHz (30);
}

ArrangeView::~ArrangeView()
{
    vs.edit.state.removeListener (this);
    vs.selection.removeChangeListener (this);
    vs.removeChangeListener (this);
}

void ArrangeView::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
    g.setColour (colours::panel);
    g.fillRect (0, 0, headerWidth, rulerHeight);
    g.setColour (colours::lineStrong);
    g.drawVerticalLine (headerWidth - 1, 0.0f, (float) getHeight());
}

void ArrangeView::resized()
{
    auto r = getLocalBounds();
    auto bottom = r.removeFromBottom (12);
    auto left = r.removeFromLeft (headerWidth);
    cornerLabel.setBounds (left.removeFromTop (rulerHeight).reduced (12, 0));
    headers->setBounds (left.withTrimmedRight (1));
    vScroll.setBounds (r.removeFromRight (10).withTrimmedTop (rulerHeight));
    ruler->setBounds (r.removeFromTop (rulerHeight));
    canvas->setBounds (r);
    hScroll.setBounds (bottom.withTrimmedLeft (headerWidth).withTrimmedRight (10));
    updateScrollBars();
}

void ArrangeView::fitToSession()
{
    const double len = juce::jmax (8.0, vs.edit.getLength().inSeconds() * 1.05);
    vs.viewStart = 0.0;
    vs.pixelsPerSecond = juce::jmax (0.5, canvas->getWidth() / len);
    vs.changed();
}

void ArrangeView::filesDropped (const juce::StringArray& files, int x, int y)
{
    if (! onFilesDropped) return;
    const auto p = canvas->getLocalPoint (this, juce::Point<int> (x, y));
    const auto t = vs.snap (vs.xToTime (juce::jmax (0, p.x)));
    auto lanes = canvas->lanes();
    te::AudioTrack* track = nullptr;
    for (auto& l : lanes) if (p.y >= l.y && p.y < l.y + l.h) track = l.track;
    onFilesDropped (files, t, track);
}

juce::Rectangle<int> ArrangeView::getScreenBoundsOfClip (te::Clip& c) const
{
    return canvas->screenBoundsOf (c);
}

void ArrangeView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    headers->layout();
    headers->repaint();
    updateScrollBars();
    ruler->repaint();
    canvas->repaint();
}

void ArrangeView::handleAsyncUpdate()
{
    headers->rebuild();
    updateScrollBars();
    canvas->repaint();
    ruler->repaint();
}

void ArrangeView::timerCallback()
{
    auto& t = vs.edit.getTransport();
    if (t.isPlaying() || t.isRecording())
    {
        if (vs.follow && canvas->getWidth() > 0)
        {
            const float x = vs.timeToX (t.getPosition());
            if (x > canvas->getWidth() * 0.92f || x < 0)
            {
                vs.viewStart = juce::jmax (0.0, t.getPosition().inSeconds() - (canvas->getWidth() * 0.08) / vs.pixelsPerSecond);
                vs.changed();
            }
        }
        canvas->repaint();
        ruler->repaint();
    }
    else if (++idleTicks % 15 == 0)
    {
        ruler->repaint();
    }
}

void ArrangeView::updateScrollBars()
{
    const double len = juce::jmax (vs.edit.getLength().inSeconds() + 30.0, vs.viewStart + canvas->getWidth() / vs.pixelsPerSecond);
    hScroll.setRangeLimits (0.0, len, juce::dontSendNotification);
    hScroll.setCurrentRange (vs.viewStart, canvas->getWidth() / vs.pixelsPerSecond, juce::dontSendNotification);

    const int total = canvas->totalHeight() + 60;
    vScroll.setRangeLimits (0.0, (double) juce::jmax (total, canvas->getHeight()), juce::dontSendNotification);
    vScroll.setCurrentRange ((double) vs.scrollY, (double) canvas->getHeight(), juce::dontSendNotification);
}

void ArrangeView::scrollBarMoved (juce::ScrollBar* sb, double newStart)
{
    if (sb == &hScroll) vs.viewStart = newStart;
    else vs.scrollY = (int) newStart;
    vs.changed();
}
}
