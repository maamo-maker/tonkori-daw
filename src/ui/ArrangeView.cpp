#include "ArrangeView.h"
#include "Theme.h"
#include "../model/ClipFades.h"
#include "../model/GainLine.h"
#include "../model/Takes.h"
#include "../model/Stretch.h"
#include "../model/Crossfade.h"
#include "../model/Routing.h"

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

    //==============================================================================
    // Row layout shared by the headers and the canvas: each track's main row, then its
    // take lanes when shown. The picture track sits above, fixed (not scrolled).
    constexpr int videoRowHeight = 56;
    int videoHeight (const ViewState& vs)  { return vs.video.present ? videoRowHeight : 0; }

    struct Row { te::AudioTrack* track = nullptr; int lane = 0; int y = 0, h = 0; };

    int laneRowHeight (const ViewState& vs, te::AudioTrack& t) { return juce::jmax (34, (int) (vs.getTrackHeight (t) * 0.62)); }

    std::vector<Row> layoutRows (const ViewState& vs)
    {
        std::vector<Row> r;
        int y = videoHeight (vs) - vs.scrollY;
        for (auto* t : visibleTracks (vs.edit))
        {
            const int h = vs.getTrackHeight (*t);
            r.push_back ({ t, 0, y, h });
            y += h;
            if (tonkori::takes::lanesShown (*t))
                for (int lane = 1, n = tonkori::takes::numLanes (*t); lane <= n; ++lane)
                {
                    const int lh = laneRowHeight (vs, *t);
                    r.push_back ({ t, lane, y, lh });
                    y += lh;
                }
        }
        return r;
    }

    // Gain line display range: +12 dB at the top of the waveform, -36 dB at the bottom.
    constexpr double gainTop = 12.0, gainBottom = -36.0;
    float gainToY (double db, juce::Rectangle<float> wave)  { return wave.getY() + (float) ((gainTop - juce::jlimit (gainBottom, gainTop, db)) / (gainTop - gainBottom)) * wave.getHeight(); }
    double yToGain (float y, juce::Rectangle<float> wave)   { return gainTop - (double) ((y - wave.getY()) / juce::jmax (1.0f, wave.getHeight())) * (gainTop - gainBottom); }
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

        setupToggle (lanes, "Show / hide take lanes", colours::accent.withAlpha (0.6f));
        lanes.onClick = [this]
        {
            if (! track) return;
            tonkori::takes::showLanes (*track, ! tonkori::takes::lanesShown (*track));
            vs.changed();
        };

        isBus = tonkori::routing::isBus (t);
        if (isBus) { arm.setVisible (false); mon.setVisible (false); input.setVisible (false); }

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
        g.drawText (isBus ? juce::String ("BUS") : juce::String (index + 1), getWidth() - 34, 4, 28, 14, juce::Justification::centredRight);
        if (isBus)
        {
            g.setColour (colours::textDim);
            g.setFont (font (10.5f));
            g.drawText ("Bus " + juce::String (tonkori::routing::busNumberOf (*track) + 1) + " return", 12, 27, getWidth() - 60, 16, juce::Justification::left);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 4).withTrimmedLeft (2);
        auto top = r.removeFromTop (20);
        top.removeFromRight (26);
        name.setBounds (top);

        r.removeFromTop (3);
        auto row = r.removeFromTop (20);
        lanes.setBounds (row.removeFromRight (24));
        row.removeFromRight (3);
        if (isBus)
        {
            row.removeFromLeft (row.getWidth() - 55);
            for (auto* b : { &mute, &solo }) { b->setBounds (row.removeFromLeft (26)); row.removeFromLeft (3); }
        }
        else
        {
            for (auto* b : { &arm, &mon, &mute, &solo })
            {
                b->setBounds (row.removeFromLeft (26));
                row.removeFromLeft (3);
            }
            row.removeFromLeft (4);
            input.setBounds (row);
        }

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
    juce::TextButton arm { "R" }, mon { "I" }, mute { "M" }, solo { "S" }, input { "No input" }, lanes { "L" };
    juce::Slider volume, pan;
    bool resizing = false, isBus = false;
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

        const int nl = tonkori::takes::numLanes (*track);
        lanes.setEnabled (nl > 0);
        lanes.setButtonText (nl > 0 ? juce::String (nl) : "L");
        lanes.setToggleState (tonkori::takes::lanesShown (*track) && nl > 0, juce::dontSendNotification);
        lanes.setTooltip (nl > 0 ? "Show / hide " + juce::String (nl) + " take lane" + (nl > 1 ? "s" : "") : "No takes yet: record over this track to create take lanes");

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
/** Header of one take lane: name, take count, promote-whole-take button. */
class LaneHeader : public juce::Component
{
public:
    LaneHeader (ViewState& v, te::AudioTrack& t, int laneIndex) : vs (v), track (&t), lane (laneIndex)
    {
        promote.setButtonText ("Use take");
        promote.setTooltip ("Comp the whole take onto the main lane (or drag across part of a take)");
        promote.setWantsKeyboardFocus (false);
        promote.onClick = [this]
        {
            if (! track) return;
            vs.edit.getUndoManager().beginNewTransaction ("Comp take");
            for (auto* c : tonkori::takes::clipsOnLane (*track, lane))
                tonkori::takes::promote (*c, c->getPosition().time);
            vs.changed();
        };
        addAndMakeVisible (promote);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::bg.brighter (0.03f));
        g.setColour (colours::accent.withAlpha (0.4f));
        g.fillRect (0, 0, 4, getHeight());
        g.setColour (colours::line);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        g.setColour (colours::textDim);
        g.setFont (font (11.5f, true));
        juce::String name = "Take " + juce::String (lane);
        if (track)
            for (auto* c : tonkori::takes::clipsOnLane (*track, lane)) { name << "  " << c->getName(); break; }
        g.drawText (name, 16, 4, getWidth() - 100, 16, juce::Justification::left, true);
    }

    void resized() override { promote.setBounds (getWidth() - 78, 4, 70, 18); }

    te::AudioTrack* getTrack() const { return track.get(); }
    int getLane() const { return lane; }

private:
    ViewState& vs;
    te::AudioTrack::Ptr track;
    int lane;
    juce::TextButton promote;
};

/** Header of the picture track. */
class VideoHeader : public juce::Component
{
public:
    explicit VideoHeader (ViewState& v) : vs (v)
    {
        window.setButtonText ("Window");
        window.setWantsKeyboardFocus (false);
        window.onClick = [this] { if (vs.video.openWindow) vs.video.openWindow(); };
        addAndMakeVisible (window);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::panel);
        g.setColour (colours::playhead.withAlpha (0.8f));
        g.fillRect (0, 0, 4, getHeight());
        g.setColour (colours::lineStrong);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        g.setColour (colours::text);
        g.setFont (font (13.0f, true));
        g.drawText ("Video", 12, 6, 80, 18, juce::Justification::left);
        g.setColour (colours::textDim);
        g.setFont (font (11.0f));
        g.drawText (vs.video.name, 12, 26, getWidth() - 20, 16, juce::Justification::left, true);
    }

    void resized() override { window.setBounds (getWidth() - 74, 6, 66, 18); }

private:
    ViewState& vs;
    juce::TextButton window;
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
        addChildComponent (videoHeader);
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
        // lane headers follow the rows
        auto rows = layoutRows (vs);
        juce::String key;
        for (auto& r : rows) if (r.lane > 0) key << r.track->itemID.toString() << ":" << r.lane << ";";
        if (key != laneKey)
        {
            laneKey = key;
            laneHeaders.clear();
            for (auto& r : rows)
                if (r.lane > 0) addAndMakeVisible (laneHeaders.add (new LaneHeader (vs, *r.track, r.lane)));
        }

        int li = 0, y = videoHeight (vs) - vs.scrollY;
        for (auto& r : rows)
        {
            if (r.lane == 0)
            {
                for (auto* h : headers) if (h->getTrack() == r.track) h->setBounds (0, r.y, getWidth(), r.h);
            }
            else if (auto* lh = laneHeaders[li++]) lh->setBounds (0, r.y, getWidth(), r.h);
            y = r.y + r.h;
        }
        addTrack.setBounds (10, y + 8, getWidth() - 20, 26);

        videoHeader.setVisible (vs.video.present);
        videoHeader.setBounds (0, 0, getWidth(), videoHeight (vs));
        videoHeader.toFront (false);
        videoHeader.repaint();
    }

    void resized() override { layout(); }
    void paint (juce::Graphics& g) override { g.fillAll (colours::bg); }

private:
    ViewState& vs;
    juce::OwnedArray<TrackHeader> headers;
    juce::OwnedArray<LaneHeader> laneHeaders;
    juce::String laneKey;
    VideoHeader videoHeader { vs };
    juce::TextButton addTrack;
};

//==============================================================================
class Ruler : public juce::Component, public juce::SettableTooltipClient
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

        drawMarkers (g, getLocalBounds().withTrimmedTop (12).removeFromTop (markerStrip));
        drawTicks (g, getLocalBounds().withTrimmedTop (12 + markerStrip));

        // insert marker + playhead
        const float im = vs.timeToX (vs.insertMarker);
        g.setColour (colours::textDim);
        juce::Path tri;
        tri.addTriangle (im - 5, (float) getHeight() - 7, im + 5, (float) getHeight() - 7, im, (float) getHeight());
        g.fillPath (tri);

        const float ph = vs.timeToX (t.isPlaying() ? t.getPosition() : vs.insertMarker);
        g.setColour (colours::playhead);
        g.fillRect (ph - 0.5f, 12.0f + markerStrip, 1.5f, (float) getHeight() - 12 - markerStrip);

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

        if (e.y >= 12 && e.y < 12 + markerStrip)
        {
            markerDrag = {};
            if (auto* m = markerAt (e.x))
            {
                if (e.mods.isAltDown())
                {
                    vs.edit.getUndoManager().beginNewTransaction ("Delete marker");
                    m->removeFromParent();
                    vs.changed();
                    return;
                }
                if (e.mods.isPopupMenu()) { markerMenu (*m); return; }
                vs.edit.getUndoManager().beginNewTransaction ("Move marker");
                markerDrag = m->itemID;
                markerStart = m->getPosition().getStart();
                vs.insertMarker = markerStart;
                if (! t.isPlaying()) t.setPosition (markerStart); else t.setPosition (markerStart);
                vs.changed();
            }
            inMarkerStrip = true;
            return;
        }
        inMarkerStrip = false;

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

        if (inMarkerStrip)
        {
            if (markerDrag.isValid() && std::abs (e.getDistanceFromDragStartX()) > 2)
                if (auto* m = te::findClipForID (vs.edit, markerDrag))
                {
                    auto nt = vs.snap (markerStart + (vs.xToTime (e.x) - vs.xToTime (e.getMouseDownX())));
                    m->setStart (nt < 0_tp ? 0_tp : nt, false, true);
                    repaint();
                }
            return;
        }

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
        else if (e.y < 12 + markerStrip)
        {
            if (auto* m = markerAt (e.x)) renameMarker (*m);
            else
            {
                vs.edit.getUndoManager().beginNewTransaction ("Add marker");
                addMarkerAt (vs.edit, vs.snap (vs.xToTime (e.x)));
                vs.changed();
            }
        }
    }

    static te::MarkerClip* addMarkerAt (te::Edit& edit, te::TimePosition t)
    {
        edit.ensureMarkerTrack();
        auto& mm = edit.getMarkerManager();
        const int n = mm.getNextUniqueID();
        auto m = mm.createMarker (n, t, te::TimeDuration(), nullptr);
        if (m != nullptr) m->setName ("Marker " + juce::String (n));
        return dynamic_cast<te::MarkerClip*> (m.get());
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (e.y >= 12 && e.y < 12 + markerStrip)
        {
            setMouseCursor (markerAt (e.x) ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
            setTooltip (markerAt (e.x) ? "Click: locate - drag: move - double-click: rename - Opt-click: delete"
                                       : "Double-click to add a marker");
            return;
        }
        setTooltip ({});
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
    te::TimePosition downTime, markerStart;
    bool inLoopStrip = false, inMarkerStrip = false;
    te::EditItemID markerDrag;
    static constexpr int markerStrip = 15;

    te::MarkerClip* markerAt (int x) const
    {
        for (auto* m : vs.edit.getMarkerManager().getMarkers())
        {
            const float mx = vs.timeToX (m->getPosition().getStart());
            const float w = juce::jmax (14.0f, juce::GlyphArrangement::getStringWidth (font (10.5f, true), m->getName()) + 12.0f);
            if (x >= mx - 3 && x <= mx + w) return m;
        }
        return nullptr;
    }

    void drawMarkers (juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setColour (colours::bg.brighter (0.02f));
        g.fillRect (area);
        g.setFont (font (10.5f, true));
        for (auto* m : vs.edit.getMarkerManager().getMarkers())
        {
            const float x = vs.timeToX (m->getPosition().getStart());
            if (x < -200 || x > getWidth()) continue;
            const auto col = m->getColour().isTransparent() ? colours::warn : m->getColour();
            const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), m->getName()) + 10.0f;
            g.setColour (col.withAlpha (0.85f));
            g.fillRoundedRectangle (x, (float) area.getY() + 1, w, (float) area.getHeight() - 2, 2.0f);
            g.fillRect (x, (float) area.getY(), 1.5f, (float) getHeight() - area.getY());
            g.setColour (juce::Colours::black.withAlpha (0.85f));
            g.drawText (m->getName(), juce::Rectangle<float> (x + 5, (float) area.getY(), w, (float) area.getHeight()).toNearestInt(), juce::Justification::centredLeft, false);
        }
    }

    void renameMarker (te::MarkerClip& m)
    {
        auto* aw = new juce::AlertWindow ("Marker", "Name and position", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", m.getName());
        aw->addTextEditor ("time", vs.format (m.getPosition().getStart()));
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        const auto id = m.itemID;
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, id] (int r)
        {
            if (r == 0) return;
            if (auto* mk = te::findClipForID (vs.edit, id))
            {
                vs.edit.getUndoManager().beginNewTransaction ("Edit marker");
                mk->setName (aw->getTextEditorContents ("name"));
                if (auto t = vs.parse (aw->getTextEditorContents ("time"))) mk->setStart (*t, false, true);
                vs.changed();
            }
        }), true);
    }

    void markerMenu (te::MarkerClip& m)
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Rename / move...");
        menu.addItem (2, "Select to next marker");
        menu.addItem (3, "Delete");
        const auto id = m.itemID;
        menu.showMenuAsync ({}, [this, id] (int r)
        {
            auto* mk = dynamic_cast<te::MarkerClip*> (te::findClipForID (vs.edit, id));
            if (mk == nullptr) return;
            if (r == 1) renameMarker (*mk);
            if (r == 2)
            {
                const auto s = mk->getPosition().getStart();
                auto* next = vs.edit.getMarkerManager().getNextMarker (s + te::TimeDuration::fromSeconds (1e-6));
                const auto e = next ? next->getPosition().getStart() : te::TimePosition::fromSeconds (vs.edit.getLength().inSeconds());
                if (e > s)
                {
                    vs.timeSelection = te::TimeRange (s, e);
                    vs.rangeTracks.clear();
                    for (auto* t : visibleTracks (vs.edit)) vs.rangeTracks.add (t->itemID);
                }
            }
            if (r == 3) { vs.edit.getUndoManager().beginNewTransaction ("Delete marker"); mk->removeFromParent(); }
            vs.changed();
        });
    }

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
    using Lane = Row;

    std::vector<Row> lanes() const     { return layoutRows (vs); }

    juce::Array<te::Clip*> clipsIn (const Row& r) const
    {
        juce::Array<te::Clip*> out;
        for (auto* c : r.track->getClips())
            if (tonkori::takes::laneOf (*c) == r.lane) out.add (c);
        return out;
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

    static juce::Rectangle<float> waveArea (juce::Rectangle<float> r) { return r.getHeight() > 34 ? r.withTrimmedTop (15.0f) : r; }

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::bg);
        auto ls = lanes();

        int trackIdx = 0;
        for (size_t i = 0; i < ls.size(); ++i)
        {
            auto& l = ls[i];
            if (l.lane == 0) ++trackIdx;
            const bool sel = vs.selection.isSelected (l.track);
            auto c = l.lane > 0 ? colours::bg.brighter (0.025f) : (trackIdx % 2 ? colours::lane : colours::laneAlt);
            g.setColour (sel && l.lane == 0 ? c.brighter (0.04f) : c);
            g.fillRect (0, l.y, getWidth(), l.h);
        }

        drawGrid (g);

        // range selection
        if (vs.timeSelection)
        {
            const float x1 = vs.timeToX (vs.timeSelection->getStart()), x2 = vs.timeToX (vs.timeSelection->getEnd());
            for (auto& l : ls)
                if (l.lane == 0 && vs.rangeTracks.contains (l.track->itemID))
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

            for (auto* c : clipsIn (l))
            {
                auto r = clipRect (*c, l);
                if (r.getRight() < 0 || r.getX() > getWidth()) continue;
                const bool dragged = drag.type == DragType::move && drag.hasMoved && isDragged (c);
                drawClip (g, *c, r, l, dragged ? 0.35f : 1.0f);
            }

            if (l.lane == 0) drawRecording (g, l);
        }

        // swipe comp in progress
        if (drag.type == DragType::swipe && drag.hasMoved && drag.swipeRow >= 0 && drag.swipeRow < (int) ls.size())
        {
            auto& l = ls[(size_t) drag.swipeRow];
            const float x1 = vs.timeToX (drag.swipeRange.getStart()), x2 = vs.timeToX (drag.swipeRange.getEnd());
            g.setColour (colours::accent.withAlpha (0.35f));
            g.fillRect (x1, (float) l.y, x2 - x1, (float) l.h);
            g.setColour (colours::accent);
            g.drawRect (x1, (float) l.y, x2 - x1, (float) l.h, 1.5f);
        }

        if (drag.type == DragType::move && drag.hasMoved)
            drawMoveGhosts (g, ls);

        drawVideoRow (g);

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
            case Zone::gainLine:                      setMouseCursor (C::UpDownResizeCursor); break;
            case Zone::gainPoint:                     setMouseCursor (C::PointingHandCursor); break;
            case Zone::xfade:                         setMouseCursor (C::PointingHandCursor); break;
            case Zone::video:                         setMouseCursor (C::DraggingHandCursor); break;
            default:                                  setMouseCursor (C::IBeamCursor); break;
        }
        if (hover.clip != hit.clip || hover.zone != hit.zone) { hover = hit; repaint(); }
    }

    void mouseExit (const juce::MouseEvent&) override { hover = {}; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = {};
        auto hit = hitTest (e.position);
        if (e.y < videoHeight (vs) && hit.zone != Zone::video)
        {
            if (e.mods.isPopupMenu()) return;
            vs.insertMarker = vs.snap (vs.xToTime (e.x));
            if (! vs.edit.getTransport().isPlaying()) vs.edit.getTransport().setPosition (vs.insertMarker);
            vs.changed();
            return;
        }
        if (hit.zone == Zone::video && e.mods.isPopupMenu()) { showVideoMenu(); return; }
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

        if (hit.zone == Zone::video)
        {
            um.beginNewTransaction ("Move video");
            drag.type = DragType::video;
            drag.videoStart = vs.video.start;
            return;
        }

        if (hit.zone == Zone::gainPoint && drag.audioClip)
        {
            um.beginNewTransaction ("Clip gain");
            auto pts = tonkori::gainline::getPoints (*hit.clip);
            if (e.mods.isAltDown())
            {
                pts.erase (pts.begin() + hit.point);
                tonkori::gainline::setPoints (*hit.clip, pts);
                vs.changed();
                return;
            }
            drag.type = DragType::gainPoint;
            drag.gainIndex = hit.point;
            drag.gainStart = pts;
            vs.selection.selectOnly (hit.clip);
            return;
        }

        if (hit.zone == Zone::gainLine && drag.audioClip)
        {
            um.beginNewTransaction ("Clip gain");
            auto pts = tonkori::gainline::getPoints (*hit.clip);
            vs.selection.selectOnly (hit.clip);
            if (e.mods.isCommandDown())
            {
                drag.type = DragType::gainAll;                      // drag the whole line
                if (pts.empty()) pts = { { tonkori::gainline::contentTime (*hit.clip, hit.clip->getPosition().getStart()), 0.0 } };
                drag.gainStart = pts;
                return;
            }
            const auto r = clipRect (*hit.clip, lanes()[(size_t) hit.row]);
            const double t = tonkori::gainline::contentTime (*hit.clip, vs.xToTime (e.x));
            pts.push_back ({ t, juce::jlimit (gainBottom, gainTop, yToGain (e.position.y, waveArea (r))) });
            tonkori::gainline::setPoints (*hit.clip, pts);
            auto sorted = tonkori::gainline::getPoints (*hit.clip);
            drag.type = DragType::gainPoint;
            for (int i = 0; i < (int) sorted.size(); ++i) if (sorted[(size_t) i].t == t) drag.gainIndex = i;
            drag.gainStart = sorted;
            vs.changed();
            return;
        }

        if ((hit.zone == Zone::trimStart || hit.zone == Zone::trimEnd) && e.mods.isAltDown() && drag.audioClip)
        {
            um.beginNewTransaction ("Time-stretch (TCE)");
            drag.type = hit.zone == Zone::trimStart ? DragType::tceStart : DragType::tceEnd;
            drag.original = hit.clip->getPosition();
            drag.originalRatio = drag.audioClip->getSpeedRatio();
            vs.selection.selectOnly (hit.clip);
            return;
        }

        if (hit.row >= 0 && lanes()[(size_t) hit.row].lane > 0 && hit.zone != Zone::body)
        {
            drag.type = DragType::swipe;                            // comp by dragging across a take
            drag.swipeRow = hit.row;
            drag.rangeAnchor = vs.snap (vs.xToTime (e.x));
            drag.swipeRange = { drag.rangeAnchor, drag.rangeAnchor };
            return;
        }

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
            case Zone::xfade:
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
            case DragType::video:
            {
                const double ns = juce::jmax (0.0, vs.snap (te::TimePosition::fromSeconds (drag.videoStart) + (time - drag.downTime)).inSeconds());
                if (vs.video.setStart) vs.video.setStart (ns);
                vs.changed();
                break;
            }
            case DragType::tceStart:
            case DragType::tceEnd:
            {
                auto edge = vs.snap (time);
                tonkori::stretch::tceTrim (*drag.audioClip, drag.original, drag.originalRatio, edge, drag.type == DragType::tceEnd);
                vs.changed();
                break;
            }
            case DragType::gainPoint:
            case DragType::gainAll:
            {
                auto pts = drag.gainStart;
                auto ls = lanes();
                const int li = laneIndexOfClip (drag.clip);
                if (li < 0) break;
                const auto wave = waveArea (clipRect (*drag.clip, ls[(size_t) li]));
                const double dDb = -(double) e.getDistanceFromDragStartY() / juce::jmax (1.0f, wave.getHeight()) * (gainTop - gainBottom);
                if (drag.type == DragType::gainAll)
                    for (auto& pt : pts) pt.db = juce::jlimit (-144.0, gainTop, pt.db + dDb);
                else if (drag.gainIndex >= 0 && drag.gainIndex < (int) pts.size())
                {
                    auto& pt = pts[(size_t) drag.gainIndex];
                    pt.db = juce::jlimit (gainBottom, gainTop, pt.db + dDb);
                    if (! e.mods.isShiftDown())       // Shift: vertical only
                    {
                        double nt = pt.t + (time - drag.downTime).inSeconds();
                        const double lo = drag.gainIndex > 0 ? pts[(size_t) drag.gainIndex - 1].t + 1e-4 : -1e9;
                        const double hi = drag.gainIndex + 1 < (int) pts.size() ? pts[(size_t) drag.gainIndex + 1].t - 1e-4 : 1e9;
                        pt.t = juce::jlimit (lo, hi, nt);
                    }
                }
                tonkori::gainline::setPoints (*drag.clip, pts);
                vs.changed();
                break;
            }
            case DragType::swipe:
            {
                const auto t2 = vs.snap (time);
                drag.swipeRange = { juce::jmin (t2, drag.rangeAnchor), juce::jmax (t2, drag.rangeAnchor) };
                repaint();
                break;
            }
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
                    vs.rangeTracks.addIfNotAlreadyThere (ls[(size_t) i].track->itemID);
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

        if (drag.type == DragType::swipe)
        {
            auto ls = lanes();
            if (drag.hasMoved && drag.swipeRow >= 0 && drag.swipeRow < (int) ls.size() && drag.swipeRange.getLength() > te::TimeDuration())
            {
                vs.edit.getUndoManager().beginNewTransaction ("Comp");
                for (auto* c : clipsIn (ls[(size_t) drag.swipeRow]))
                    if (c->getPosition().time.overlaps (drag.swipeRange))
                        tonkori::takes::promote (*c, drag.swipeRange.getIntersectionWith (c->getPosition().time));
            }
            else
            {
                vs.insertMarker = vs.snap (vs.xToTime (e.x));
                if (! vs.edit.getTransport().isPlaying()) vs.edit.getTransport().setPosition (vs.insertMarker);
            }
            drag = {};
            vs.changed();
            return;
        }

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
        if (hit.zone == Zone::video) { if (vs.video.spot) vs.video.spot(); return; }
        if (! hit.clip) return;

        if (hit.zone == Zone::xfade && vs.onOpenCrossfade)
        {
            auto* a = dynamic_cast<te::AudioClipBase*> (hit.clip);
            auto* b = dynamic_cast<te::AudioClipBase*> (hit.other);
            if (a && b)
            {
                if (a->getPosition().getStart() > b->getPosition().getStart()) std::swap (a, b);
                vs.onOpenCrossfade (*a, *b);
            }
            return;
        }

        if (tonkori::takes::isTake (*hit.clip))
        {
            vs.edit.getUndoManager().beginNewTransaction ("Comp take");
            tonkori::takes::promote (*hit.clip, hit.clip->getPosition().time);
            vs.changed();
            return;
        }

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
        int h = videoHeight (vs);
        for (auto& r : layoutRows (vs)) h += r.h;
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

    enum class Zone { none, body, range, trimStart, trimEnd, fadeIn, fadeOut, split, gainLine, gainPoint, xfade, video };
    struct Hit { te::Clip* clip = nullptr; Zone zone = Zone::none; int point = -1; int row = -1; te::Clip* other = nullptr; };
    Hit hover;

    enum class DragType { none, move, trimStart, trimEnd, tceStart, tceEnd, fade, fadeShape, range, swipe, gainPoint, gainAll, video };
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
        double originalRatio = 1.0;
        int swipeRow = -1, gainIndex = -1;
        te::TimeRange swipeRange;
        std::vector<tonkori::gainline::Point> gainStart;
        double videoStart = 0;
    } drag;

    bool isDragged (te::Clip* c) const
    {
        for (auto& m : drag.moving) if (m.id == c->itemID) return true;
        return false;
    }

    int laneIndexOfClip (te::Clip* c) const
    {
        auto ls = lanes();
        const int lane = tonkori::takes::laneOf (*c);
        for (int i = 0; i < (int) ls.size(); ++i)
            if (c->getTrack() == ls[(size_t) i].track && ls[(size_t) i].lane == lane) return i;
        return -1;
    }

    //==============================================================================
    Hit hitTest (juce::Point<float> p) const
    {
        Hit h;
        if (vs.video.present && p.y < videoHeight (vs))
        {
            const float x1 = vs.timeToX (te::TimePosition::fromSeconds (vs.video.start));
            const float x2 = vs.timeToX (te::TimePosition::fromSeconds (vs.video.start + vs.video.length));
            if (p.x >= x1 && p.x <= x2) h.zone = Zone::video;
            return h;
        }

        auto ls = lanes();
        for (int ri = 0; ri < (int) ls.size(); ++ri)
        {
            auto& l = ls[(size_t) ri];
            if (p.y < l.y || p.y >= l.y + l.h) continue;
            h.row = ri;

            auto inRow = clipsIn (l);
            // later clips draw on top: test from the top down
            for (int ci = inRow.size(); --ci >= 0;)
            {
                auto* c = inRow[ci];
                auto r = clipRect (*c, l);
                if (! r.contains (p)) continue;
                h.clip = c;

                if (l.lane > 0) { h.zone = vs.tool == Tool::grab ? Zone::body : Zone::range; return h; }   // take lanes: swipe to comp

                switch (vs.tool)
                {
                    case Tool::range: h.zone = Zone::range; return h;
                    case Tool::grab:  h.zone = Zone::body; return h;
                    case Tool::split: h.zone = Zone::split; return h;
                    case Tool::trim:  h.zone = p.x < r.getCentreX() ? Zone::trimStart : Zone::trimEnd; return h;
                    case Tool::fade:  h.zone = p.x < r.getCentreX() ? Zone::fadeIn : Zone::fadeOut; return h;
                    case Tool::smart: break;
                }

                // Overlap with a neighbour: crossfade zone (double-click opens the crossfade editor).
                for (auto* o : inRow)
                    if (o != c && clipRect (*o, l).contains (p) && dynamic_cast<te::AudioClipBase*> (o) && dynamic_cast<te::AudioClipBase*> (c)
                        && p.y > r.getY() + r.getHeight() * 0.55f)
                    { h.zone = Zone::xfade; h.other = o; return h; }

                // Fade corners: header strip plus the top third of the waveform, near either edge.
                const float header = r.getHeight() > 34 ? 15.0f : 0.0f;
                const float topZone = header + juce::jmax (8.0f, (r.getHeight() - header) * 0.35f);
                auto* ac = dynamic_cast<te::AudioClipBase*> (c);
                if (ac != nullptr && p.y < r.getY() + topZone)
                {
                    const float fi = r.getX() + vs.durationToWidth (ac->getFadeIn());
                    const float fo = r.getRight() - vs.durationToWidth (ac->getFadeOut());
                    if (p.x < juce::jmax (r.getX() + 18.0f, fi + 6.0f) && p.x < r.getCentreX()) { h.zone = Zone::fadeIn; return h; }
                    if (p.x > juce::jmin (r.getRight() - 18.0f, fo - 6.0f) && p.x >= r.getCentreX()) { h.zone = Zone::fadeOut; return h; }
                }

                const float edge = juce::jmin (7.0f, r.getWidth() * 0.2f);
                if (p.x < r.getX() + edge)     { h.zone = Zone::trimStart; return h; }
                if (p.x > r.getRight() - edge) { h.zone = Zone::trimEnd; return h; }

                // Gain line (when shown): points first, then the line itself.
                if (ac != nullptr && tonkori::gainline::isShown (vs.edit))
                {
                    const auto wave = waveArea (r);
                    const auto pts = tonkori::gainline::getPoints (*c);
                    for (int i = 0; i < (int) pts.size(); ++i)
                    {
                        const float px = vs.timeToX (tonkori::gainline::timelineTime (*c, pts[(size_t) i].t));
                        const float py = gainToY (pts[(size_t) i].db, wave);
                        if (std::abs (p.x - px) <= 5.0f && std::abs (p.y - py) <= 5.0f) { h.zone = Zone::gainPoint; h.point = i; return h; }
                    }
                    const double db = tonkori::gainline::dbAt (pts, tonkori::gainline::contentTime (*c, vs.xToTime (p.x)));
                    if (std::abs (p.y - gainToY (db, wave)) <= 4.0f) { h.zone = Zone::gainLine; return h; }
                }

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
            if (newLane != m.lane && m.lane >= 0)
            {
                auto& row = ls[(size_t) newLane];
                if (row.track != target->getTrack()) target->moveTo (*row.track);
                if (row.lane != tonkori::takes::laneOf (*target)) tonkori::takes::setLane (*target, row.lane);
            }
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
        const bool isTake = lane.lane > 0;
        const bool muted = c.isMuted() && ! isTake;
        if (muted) base = base.withSaturation (0.0f).darker (0.3f);
        if (isTake) base = base.withMultipliedSaturation (0.55f);

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
            if (ac && tonkori::stretch::isStretched (*ac))
            {
                title += "   \xe2\x86\x94 " + juce::String (tonkori::stretch::getPercent (*ac), 1) + " %";      // ↔
                if (std::abs (ac->getPitchChange()) > 1e-4f)
                    title += "  " + juce::String (ac->getPitchChange() > 0 ? "+" : "") + juce::String (ac->getPitchChange(), 2) + " st";
            }
            if (c.state.hasProperty (tonkori::takes::fromLaneProp) && ! isTake)
                title += "   take " + c.state.getProperty (tonkori::takes::fromLaneProp).toString();
            g.drawText (title, header.reduced (5, 0).toNearestInt(), juce::Justification::centredLeft, true);
            wave = r.withTrimmedTop (15.0f);
        }

        if (ac != nullptr && wave.getWidth() > 1 && wave.getHeight() > 4)
            drawWaveform (g, *ac, wave, base, alpha);

        if (ac != nullptr)
            drawFades (g, *ac, wave, alpha);

        if (ac != nullptr && ! isTake && tonkori::gainline::isShown (vs.edit))
            drawGainLine (g, c, wave, alpha);

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

    void drawGainLine (juce::Graphics& g, te::Clip& c, juce::Rectangle<float> wave, float alpha)
    {
        const auto pts = tonkori::gainline::getPoints (c);
        const bool hovered = hover.clip == &c && (hover.zone == Zone::gainLine || hover.zone == Zone::gainPoint);
        if (pts.empty() && ! hovered && ! vs.selection.isSelected (&c)) return;

        juce::Path p;
        const float x0 = juce::jmax (0.0f, wave.getX()), x1 = juce::jmin ((float) getWidth(), wave.getRight());
        for (float x = x0; x <= x1; x += 2.0f)
        {
            const double db = tonkori::gainline::dbAt (pts, tonkori::gainline::contentTime (c, vs.xToTime (x)));
            const float y = gainToY (db, wave);
            if (x == x0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (colours::gain.withAlpha ((pts.empty() ? 0.45f : 0.95f) * alpha));
        g.strokePath (p, juce::PathStrokeType (hovered ? 2.0f : 1.4f));

        g.setFont (mono (9.5f));
        for (int i = 0; i < (int) pts.size(); ++i)
        {
            const float px = vs.timeToX (tonkori::gainline::timelineTime (c, pts[(size_t) i].t));
            if (px < wave.getX() - 4 || px > wave.getRight() + 4) continue;
            const float py = gainToY (pts[(size_t) i].db, wave);
            const bool hot = hover.clip == &c && hover.zone == Zone::gainPoint && hover.point == i;
            g.setColour (colours::gain);
            g.fillEllipse (px - 3.5f, py - 3.5f, 7.0f, 7.0f);
            if (hot || (drag.type == DragType::gainPoint && drag.clip == &c && drag.gainIndex == i))
            {
                g.setColour (colours::text);
                g.drawText (juce::String (pts[(size_t) i].db, 1) + " dB", (int) px + 6, (int) py - 14, 60, 12, juce::Justification::left);
            }
        }
    }

    void drawVideoRow (juce::Graphics& g)
    {
        if (! vs.video.present) return;
        const int h = videoHeight (vs);
        g.setColour (colours::panel.darker (0.15f));
        g.fillRect (0, 0, getWidth(), h);

        const float x1 = vs.timeToX (te::TimePosition::fromSeconds (vs.video.start));
        const float x2 = vs.timeToX (te::TimePosition::fromSeconds (vs.video.start + vs.video.length));
        juce::Rectangle<float> r (x1, 3.0f, juce::jmax (2.0f, x2 - x1), (float) h - 7.0f);
        g.setColour (juce::Colours::black);
        g.fillRect (r);

        // thumbnails at frame-aligned slots
        const float thumbH = r.getHeight() - 2.0f, thumbW = juce::jmax (24.0f, thumbH * (float) vs.video.aspect);
        const double secsPerThumb = thumbW / vs.pixelsPerSecond;
        if (vs.video.thumbAt && vs.video.fps > 0)
        {
            const double frameDur = 1.0 / vs.video.fps;
            const double step = juce::jmax (frameDur, std::ceil (secsPerThumb / frameDur) * frameDur);
            const double visStart = juce::jmax (0.0, vs.xToTime (0).inSeconds() - vs.video.start);
            for (double s0 = std::floor (visStart / step) * step; s0 < vs.video.length; s0 += step)
            {
                const float tx = vs.timeToX (te::TimePosition::fromSeconds (vs.video.start + s0));
                if (tx > getWidth()) break;
                auto img = vs.video.thumbAt (s0);
                juce::Rectangle<float> cell (tx, r.getY() + 1, thumbW, thumbH);
                if (img.isValid()) g.drawImage (img, cell.getIntersection (r), juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
                else { g.setColour (colours::bg); g.fillRect (cell.reduced (1).getIntersection (r)); }
            }
        }
        g.setColour (colours::playhead.withAlpha (0.8f));
        g.drawRect (r, 1.2f);
        g.setColour (colours::lineStrong);
        g.drawHorizontalLine (h - 1, 0.0f, (float) getWidth());
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

    void showVideoMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Video window");
        m.addItem (2, "Spot video...");
        m.addSeparator();
        m.addItem (3, "Remove video from session");
        juce::Component::SafePointer<ArrangeCanvas> safe (this);
        m.showMenuAsync ({}, [safe] (int r)
        {
            if (safe == nullptr) return;
            auto& v = safe->vs.video;
            if (r == 1 && v.openWindow) v.openWindow();
            if (r == 2 && v.spot) v.spot();
            if (r == 3 && v.remove) v.remove();
        });
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
