#include "Mixer.h"
#include "Theme.h"
#include "PluginMenu.h"
#include "LoudnessMeter.h"
#include "../model/Routing.h"

namespace tonkori::ui
{
//==============================================================================
/** Peak meter with hold and overload, fed by a Tracktion LevelMeasurer. */
class Meter : public juce::Component
{
public:
    ~Meter() override { detach(); }

    void attach (te::LevelMeasurer* m)
    {
        if (m == measurer) return;
        detach();
        measurer = m;
        if (measurer) measurer->addClient (client);
    }

    void detach()
    {
        if (measurer) measurer->removeClient (client);
        measurer = nullptr;
    }

    void update()
    {
        const auto now = juce::Time::getMillisecondCounter();
        for (int c = 0; c < 2; ++c)
        {
            const float db = measurer ? client.getAndClearAudioLevel (c).dB : -100.0f;
            level[c] = juce::jmax (db, level[c] - 1.2f);                 // ~36 dB/s fall at 30 Hz
            if (db >= hold[c] || now > holdUntil[c]) { hold[c] = db; holdUntil[c] = now + 1500; }
            if (db > -0.1f) clipped = true;
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (colours::bg);
        g.fillRoundedRectangle (r, 2.0f);

        auto clipLed = r.removeFromTop (5.0f);
        g.setColour (clipped ? colours::record : colours::line);
        g.fillRect (clipLed.reduced (1.0f, 0.0f));
        r.removeFromTop (2.0f);

        const float w = (r.getWidth() - 1.0f) / 2.0f;
        for (int c = 0; c < 2; ++c)
        {
            auto bar = juce::Rectangle<float> (r.getX() + c * (w + 1.0f), r.getY(), w, r.getHeight());
            const float y = dbToY (level[c], bar);
            juce::ColourGradient grad (colours::record, bar.getX(), dbToY (0.0f, bar),
                                       colours::play.darker (0.2f), bar.getX(), bar.getBottom(), false);
            grad.addColour (1.0 - (dbToY (-6.0f, bar) - bar.getY()) / bar.getHeight(), colours::warn);
            g.setGradientFill (grad);
            g.fillRect (bar.withTop (y));
            g.setColour (colours::text);
            g.fillRect (bar.getX(), dbToY (hold[c], bar), w, 1.5f);
        }
    }

    void mouseDown (const juce::MouseEvent&) override { clipped = false; }

private:
    te::LevelMeasurer* measurer = nullptr;
    te::LevelMeasurer::Client client;
    float level[2] = { -100.0f, -100.0f }, hold[2] = { -100.0f, -100.0f };
    juce::uint32 holdUntil[2] = { 0, 0 };
    bool clipped = false;

    static float dbToY (float db, juce::Rectangle<float> r)
    {
        // 0 dB at 6 % from the top, -60 dB at the bottom, gentle curve for resolution near 0.
        const float norm = juce::jlimit (0.0f, 1.0f, std::pow ((db + 60.0f) / 66.0f, 1.6f));
        return r.getBottom() - norm * r.getHeight();
    }
};

//==============================================================================
/** One send: bus name and level in a bar; drag sideways for level, double-click for 0 dB. */
class SendRow : public juce::Component, public juce::SettableTooltipClient
{
public:
    SendRow (te::AuxSendPlugin& s, bool pre, juce::UndoManager& u) : send (&s), preFader (pre), um (u)
    {
        setTooltip ("Send to " + s.getBusName() + (pre ? " (pre-fader)" : " (post-fader)") + ". Drag: level, double-click: 0 dB, right-click: options");
    }

    std::function<void (const juce::MouseEvent&)> onRightClick;
    bool isDragging() const { return dragging; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (colours::bg);
        g.fillRoundedRectangle (r, 2.0f);
        if (send == nullptr) return;
        const float db = send->getGainDb();
        const float frac = juce::jlimit (0.0f, 1.0f, (float) (std::pow (juce::Decibels::decibelsToGain ((double) db), 0.25) / std::pow (2.0, 0.25)));
        g.setColour ((send->isMute() ? colours::textFaint : colours::warn).withAlpha (0.45f));
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * frac), 2.0f);
        g.setFont (font (10.0f));
        g.setColour (colours::text);
        g.drawText (send->getBusName() + (preFader ? " pre" : ""), getLocalBounds().reduced (4, 0), juce::Justification::centredLeft, true);
        g.setFont (mono (9.5f));
        g.drawText (send->isMute() ? juce::String ("mute") : (db <= -95.0f ? juce::String ("-inf") : juce::String (db, 1)),
                    getLocalBounds().reduced (4, 0), juce::Justification::centredRight, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick (e); return; }
        um.beginNewTransaction ("Send level");
        startDb = send ? send->getGainDb() : 0.0f;
        dragging = true;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging || send == nullptr) return;
        const float scale = e.mods.isShiftDown() ? 0.05f : 0.25f;   // Shift: fine
        send->setGainDb (juce::jlimit (-96.0f, 6.0f, startDb + (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) * scale));
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override       { dragging = false; }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (send) { um.beginNewTransaction ("Send level"); send->setGainDb (0.0f); repaint(); } }

private:
    te::AuxSendPlugin* send;
    te::Plugin::Ptr keepAlive { send };      // declared after 'send' so it is initialised from it
    bool preFader, dragging = false;
    juce::UndoManager& um;
    float startDb = 0;
};

//==============================================================================
class ChannelStrip : public juce::Component
{
public:
    ChannelStrip (ViewState& v, te::AudioTrack* t, int idx) : vs (v), track (t), index (idx)
    {
        name.setFont (font (11.5f, true));
        name.setJustificationType (juce::Justification::centred);
        name.setColour (juce::Label::textColourId, colours::text);
        addAndMakeVisible (name);

        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setRange (-96.0, 6.0, 0.1);
        fader.setSkewFactorFromMidPoint (-12.0);
        fader.setDoubleClickReturnValue (true, 0.0);
        fader.onDragStart = [this] { vs.edit.getUndoManager().beginNewTransaction ("Fader"); };
        fader.onValueChange = [this] { if (auto* vp = volumePlugin()) vp->setVolumeDb ((float) fader.getValue()); };
        addAndMakeVisible (fader);

        pan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        pan.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        pan.setRange (-1.0, 1.0, 0.01);
        pan.setDoubleClickReturnValue (true, 0.0);
        pan.onDragStart = [this] { vs.edit.getUndoManager().beginNewTransaction ("Pan"); };
        pan.onValueChange = [this] { if (auto* vp = volumePlugin()) vp->setPan ((float) pan.getValue()); };
        addAndMakeVisible (pan);

        db.setFont (mono (10.5f));
        db.setJustificationType (juce::Justification::centred);
        db.setEditable (false, true, false);
        db.onTextChange = [this]
        {
            if (auto* vp = volumePlugin())
            {
                vs.edit.getUndoManager().beginNewTransaction ("Fader");
                vp->setVolumeDb (db.getText().startsWithIgnoreCase ("-inf") ? -100.0f : juce::jlimit (-100.0f, 6.0f, db.getText().getFloatValue()));
            }
        };
        db.setTooltip ("Double-click to type a level");
        addAndMakeVisible (db);
        addAndMakeVisible (meter);

        if (track)
        {
            for (auto* b : { &mute, &solo, &arm })
            {
                b->setWantsKeyboardFocus (false);
                addAndMakeVisible (*b);
            }
            mute.setColour (juce::TextButton::buttonOnColourId, colours::warn.darker (0.2f));
            solo.setColour (juce::TextButton::buttonOnColourId, colours::accent);
            arm.setColour (juce::TextButton::buttonOnColourId, colours::record);
            mute.onClick = [this] { vs.edit.getUndoManager().beginNewTransaction ("Mute"); track->setMute (! track->isMuted (false)); };
            solo.onClick = [this] { vs.edit.getUndoManager().beginNewTransaction ("Solo"); track->setSolo (! track->isSolo (false)); };
            arm.onClick = [this]
            {
                auto ins = vs.edit.getEditInputDevices().getDevicesForTargetTrack (*track);
                bool armed = false;
                for (auto* in : ins) armed = armed || in->isRecordingEnabled (track->itemID);
                if (! armed && ins.isEmpty())
                    for (auto* in : vs.edit.getAllInputDevices())
                        if (in->getInputDevice().getDeviceType() == te::InputDevice::waveDevice && in->getTargets().isEmpty())
                        {
                            [[maybe_unused]] auto r = in->setTarget (track->itemID, true, &vs.edit.getUndoManager(), 0);
                            ins = vs.edit.getEditInputDevices().getDevicesForTargetTrack (*track);
                            break;
                        }
                for (auto* in : ins) in->setRecordingEnabled (track->itemID, ! armed);
            };
            if (tonkori::routing::isBus (*track)) arm.setVisible (false);

            addSend.setButtonText ("+ send");
            addSend.setTooltip ("Send to a bus (pre- or post-fader)");
            addSend.setWantsKeyboardFocus (false);
            addSend.onClick = [this] { showAddSendMenu(); };
            addAndMakeVisible (addSend);

            output.setWantsKeyboardFocus (false);
            output.setTooltip ("Output: master, a bus (subgroup) or hardware outputs");
            output.onClick = [this] { showOutputMenu(); };
            addAndMakeVisible (output);
        }

        addInsert.setButtonText ("+");
        addInsert.setTooltip ("Add insert");
        addInsert.setWantsKeyboardFocus (false);
        addInsert.onClick = [this]
        {
            if (track) showAddPluginMenu (*track, &addInsert, [this] { rebuildInserts(); });
            else showAddPluginMenu (vs.edit, vs.edit.getMasterPluginList(), [this] { return vs.edit.getMasterPluginList().size(); }, &addInsert, [this] { rebuildInserts(); });
        };
        addAndMakeVisible (addInsert);

        if (track == nullptr && vs.loudness != nullptr)
        {
            loudness = std::make_unique<LoudnessMeter> (*vs.loudness, true);
            addAndMakeVisible (*loudness);
        }

        rebuildInserts();
        update();
    }

    te::AudioTrack* getTrack() const { return track; }

    void update()
    {
        if (track) name.setText (track->getName(), juce::dontSendNotification);
        else name.setText ("MASTER", juce::dontSendNotification);

        if (auto* vp = volumePlugin())
        {
            if (! fader.isMouseButtonDown()) fader.setValue (vp->getVolumeDb(), juce::dontSendNotification);
            if (! pan.isMouseButtonDown()) pan.setValue (vp->getPan(), juce::dontSendNotification);
            if (! db.isBeingEdited()) db.setText (vp->getVolumeDb() <= -95.0f ? juce::String ("-inf") : juce::String (vp->getVolumeDb(), 1), juce::dontSendNotification);
        }

        if (track)
        {
            mute.setToggleState (track->isMuted (false), juce::dontSendNotification);
            solo.setToggleState (track->isSolo (false), juce::dontSendNotification);
            bool armed = false;
            for (auto* in : vs.edit.getEditInputDevices().getDevicesForTargetTrack (*track)) armed = armed || in->isRecordingEnabled (track->itemID);
            arm.setToggleState (armed, juce::dontSendNotification);
            if (auto* lm = track->getLevelMeterPlugin()) meter.attach (&lm->measurer);
        }
        else if (auto* ctx = vs.edit.getCurrentPlaybackContext())
            meter.attach (&ctx->masterLevels);

        if (pluginSignature() != shownSignature) rebuildInserts();
        if (track)
        {
            if (sendSignature() != shownSends) rebuildSends();
            for (auto* sr : sendSliders) if (! sr->isDragging()) sr->repaint();
            output.setButtonText ("> " + tonkori::routing::currentDestination (*track));
        }
        meter.update();
    }

    void paint (juce::Graphics& g) override
    {
        const bool sel = track && vs.selection.isSelected (track);
        const bool bus = track && tonkori::routing::isBus (*track);
        g.setColour (sel ? colours::panelHi : (bus ? colours::panel.brighter (0.04f) : colours::panel));
        g.fillRect (getLocalBounds().reduced (1, 0));
        g.setColour (track ? (bus ? colours::warn : colours::trackColour (index)) : colours::accent);
        g.fillRect (1, 0, getWidth() - 2, 3);
        g.setColour (colours::line);
        g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
    }

    void mouseDown (const juce::MouseEvent&) override { if (track) vs.selection.selectOnly (track); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (5, 4);
        r.removeFromTop (2);
        name.setBounds (r.removeFromTop (18));
        r.removeFromTop (3);

        for (auto* b : inserts) { b->setBounds (r.removeFromTop (18)); r.removeFromTop (2); }
        addInsert.setBounds (r.removeFromTop (18));
        r.removeFromTop (4);

        if (track)
        {
            for (auto* sl : sendSliders) { sl->setBounds (r.removeFromTop (16)); r.removeFromTop (2); }
            addSend.setBounds (r.removeFromTop (16));
            r.removeFromTop (3);
            output.setBounds (r.removeFromTop (16));
            r.removeFromTop (3);
        }
        else if (loudness)
        {
            loudness->setBounds (r.removeFromTop (56));
            r.removeFromTop (4);
        }

        auto panRow = r.removeFromTop (30);
        pan.setBounds (panRow.withSizeKeepingCentre (30, 30));
        r.removeFromTop (2);

        if (track)
        {
            auto buttons = r.removeFromBottom (20);
            const int bw = (buttons.getWidth() - 4) / 3;
            mute.setBounds (buttons.removeFromLeft (bw)); buttons.removeFromLeft (2);
            solo.setBounds (buttons.removeFromLeft (bw)); buttons.removeFromLeft (2);
            arm.setBounds (buttons);
            r.removeFromBottom (3);
        }
        db.setBounds (r.removeFromBottom (16));
        meter.setBounds (r.removeFromRight (14).reduced (0, 4));
        r.removeFromRight (2);
        fader.setBounds (r);
    }

private:
    ViewState& vs;
    te::AudioTrack* track;
    int index;
    juce::Label name, db;
    juce::Slider fader, pan;
    juce::TextButton mute { "M" }, solo { "S" }, arm { "R" }, addInsert, addSend, output;
    juce::OwnedArray<juce::TextButton> inserts;
    juce::OwnedArray<SendRow> sendSliders;
    juce::String shownSignature, shownSends;
    std::unique_ptr<LoudnessMeter> loudness;
    Meter meter;

    juce::String pluginSignature() const
    {
        juce::String sig;
        for (auto* p : currentPlugins()) sig << p->itemID.toString() << (p->isEnabled() ? "+" : "-") << ";";
        return sig;
    }

    juce::String sendSignature() const
    {
        juce::String sig;
        if (track) for (auto& sd : tonkori::routing::sendsOf (*track)) sig << sd.plugin->itemID.toString() << sd.plugin->busNumber.get() << (sd.preFader ? "p" : "") << ";";
        return sig;
    }

    te::AuxSendPlugin* sendPlugin (int i) const
    {
        if (! track) return nullptr;
        auto all = tonkori::routing::sendsOf (*track);
        return i >= 0 && i < (int) all.size() ? all[(size_t) i].plugin : nullptr;
    }

    void rebuildSends()
    {
        sendSliders.clear();
        shownSends = sendSignature();
        auto all = tonkori::routing::sendsOf (*track);
        for (int i = 0; i < (int) all.size() && i < 4; ++i)
        {
            auto* sp = all[(size_t) i].plugin;
            auto* sl = sendSliders.add (new SendRow (*sp, all[(size_t) i].preFader, vs.edit.getUndoManager()));
            sl->onRightClick = [this, i] (const juce::MouseEvent&) { showSendMenu (i); };
            addAndMakeVisible (sl);
        }
        resized();
    }

void showSendMenu (int i)
    {
        auto* sp = sendPlugin (i);
        if (sp == nullptr || ! juce::isPositiveAndBelow (i, sendSliders.size())) return;
        const bool pre = tonkori::routing::sendsOf (*track)[(size_t) i].preFader;
        juce::PopupMenu m;
        m.addItem (1, "Pre-fader", true, pre);
        m.addItem (2, "Mute send", true, sp->isMute());
        m.addSeparator();
        m.addItem (3, "Remove send");
        te::Plugin::Ptr keep (sp);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (sendSliders[i]), [this, keep, pre] (int r)
        {
            auto* s2 = dynamic_cast<te::AuxSendPlugin*> (keep.get());
            if (s2 == nullptr || track == nullptr || r == 0) return;
            vs.edit.getUndoManager().beginNewTransaction ("Send");
            if (r == 1) tonkori::routing::setPreFader (*track, *s2, ! pre);
            if (r == 2) s2->setMute (! s2->isMute());
            if (r == 3) s2->deleteFromParent();
            rebuildSends();
        });
    }

        void showAddSendMenu()
    {
        juce::PopupMenu m;
        auto buses = tonkori::routing::buses (vs.edit);
        int id = 1;
        for (auto* b : buses)
            if (b != track) m.addItem (id + tonkori::routing::busNumberOf (*b), b->getName(), tonkori::routing::findSend (*track, tonkori::routing::busNumberOf (*b)) == nullptr);
        if (! buses.isEmpty()) m.addSeparator();
        m.addItem (1000, "New bus + post-fader send");
        m.addItem (1001, "New bus + pre-fader send");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addSend), [this] (int r)
        {
            if (r == 0 || track == nullptr) return;
            vs.edit.getUndoManager().beginNewTransaction ("Add send");
            if (r >= 1000)
            {
                if (auto* b = tonkori::routing::createBus (vs.edit))
                    tonkori::routing::addSend (*track, tonkori::routing::busNumberOf (*b), r == 1001);
            }
            else tonkori::routing::addSend (*track, r - 1, false);
            rebuildSends();
        });
    }

    void showOutputMenu()
    {
        auto dests = tonkori::routing::destinationsFor (*track);
        const auto current = tonkori::routing::currentDestination (*track);
        juce::PopupMenu m, hw;
        for (int i = 0; i < (int) dests.size(); ++i)
        {
            auto& d = dests[(size_t) i];
            if (d.device.isNotEmpty()) hw.addItem (i + 1, d.label.fromFirstOccurrenceOf ("Out: ", false, false), true, current == d.label);
            else m.addItem (i + 1, d.label, true, current == d.label);
        }
        if (hw.getNumItems() > 0) m.addSubMenu ("Hardware outputs", hw);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&output), [this, dests] (int r)
        {
            if (r <= 0 || track == nullptr) return;
            vs.edit.getUndoManager().beginNewTransaction ("Track output");
            tonkori::routing::setDestination (*track, dests[(size_t) (r - 1)]);
        });
    }

    te::VolumeAndPanPlugin* volumePlugin() const
    {
        return track ? track->getVolumePlugin() : vs.edit.getMasterVolumePlugin().get();
    }

    juce::Array<te::Plugin*> currentPlugins() const
    {
        if (track) return userPlugins (*track);
        juce::Array<te::Plugin*> r;
        for (auto* p : vs.edit.getMasterPluginList()) r.add (p);
        return r;
    }

    void rebuildInserts()
    {
        inserts.clear();
        shownSignature = pluginSignature();
        for (auto* p : currentPlugins())
        {
            auto* b = inserts.add (new juce::TextButton (slotName (*p)));
            b->setWantsKeyboardFocus (false);
            b->setColour (juce::TextButton::buttonColourId, p->isEnabled() ? colours::accent.withAlpha (0.25f) : colours::panelHi);
            te::Plugin::Ptr ptr (p);
            b->onClick = [ptr] { if (! juce::ModifierKeys::currentModifiers.isPopupMenu()) ptr->showWindowExplicitly(); };
            b->setTooltip ("Click: open editor - right-click: bypass / remove");
            b->addMouseListener (this, false);
            addAndMakeVisible (b);
            if (inserts.size() >= 6) break;
        }
        resized();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu()) return;
        for (int i = 0; i < inserts.size(); ++i)
            if (e.eventComponent == inserts[i])
                if (auto* p = currentPlugins()[i]) showPluginSlotMenu (*p, inserts[i], [this] { rebuildInserts(); });

    }
};

//==============================================================================
Mixer::Mixer (ViewState& v) : vs (v)
{
    addBus.setButtonText ("+ Bus");
    addBus.setTooltip ("New bus (aux) track: an Aux Return you can send to or route tracks into");
    addBus.setWantsKeyboardFocus (false);
    addBus.onClick = [this] { vs.edit.getUndoManager().beginNewTransaction ("New bus"); tonkori::routing::createBus (vs.edit); };
    addAndMakeVisible (addBus);
    viewport.setViewedComponent (&strips, false);
    viewport.setScrollBarsShown (false, true);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    rebuild();
    startTimerHz (30);
}

Mixer::~Mixer() = default;

void Mixer::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
    g.setColour (colours::lineStrong);
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
}

void Mixer::rebuild()
{
    channelStrips.clear();
    shown = te::getAudioTracks (vs.edit);
    for (int i = 0; i < shown.size(); ++i)
        strips.addAndMakeVisible (channelStrips.add (new ChannelStrip (vs, shown[i], i)));

    if (master == nullptr)
    {
        master = std::make_unique<ChannelStrip> (vs, nullptr, 0);
        addAndMakeVisible (*master);
    }
    resized();
}

void Mixer::resized()
{
    auto r = getLocalBounds().withTrimmedTop (1);
    const int stripW = 92;
    if (master) master->setBounds (r.removeFromRight (stripW + 8).withTrimmedLeft (8));
    addBus.setBounds (r.removeFromRight (54).withSizeKeepingCentre (48, 22).withY (6));
    viewport.setBounds (r);
    strips.setSize (juce::jmax (r.getWidth(), channelStrips.size() * stripW), r.getHeight() - (channelStrips.size() * stripW > r.getWidth() ? 8 : 0));
    for (int i = 0; i < channelStrips.size(); ++i)
        channelStrips[i]->setBounds (i * stripW, 0, stripW, strips.getHeight());
}

void Mixer::timerCallback()
{
    if (! isShowing()) return;
    if (te::getAudioTracks (vs.edit) != shown) rebuild();
    for (auto* s : channelStrips) s->update();
    if (master) master->update();
}
}
