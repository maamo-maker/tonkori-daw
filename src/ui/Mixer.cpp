#include "Mixer.h"
#include "Theme.h"
#include "PluginMenu.h"

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
                for (auto* in : ins) in->setRecordingEnabled (track->itemID, ! armed);
            };
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

        if (currentPlugins().size() != inserts.size()) rebuildInserts();
        meter.update();
    }

    void paint (juce::Graphics& g) override
    {
        const bool sel = track && vs.selection.isSelected (track);
        g.setColour (sel ? colours::panelHi : colours::panel);
        g.fillRect (getLocalBounds().reduced (1, 0));
        g.setColour (track ? colours::trackColour (index) : colours::accent);
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
    juce::TextButton mute { "M" }, solo { "S" }, arm { "R" }, addInsert;
    juce::OwnedArray<juce::TextButton> inserts;
    Meter meter;

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
        for (auto* p : currentPlugins())
        {
            auto* b = inserts.add (new juce::TextButton (p->getName()));
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
        if (e.mods.isPopupMenu())
            for (int i = 0; i < inserts.size(); ++i)
                if (e.eventComponent == inserts[i])
                    if (auto* p = currentPlugins()[i]) showPluginSlotMenu (*p, inserts[i], [this] { rebuildInserts(); });
    }
};

//==============================================================================
Mixer::Mixer (ViewState& v) : vs (v)
{
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
    const int stripW = 86;
    if (master) master->setBounds (r.removeFromRight (stripW + 8).withTrimmedLeft (8));
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
