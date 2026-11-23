#include "Inspector.h"
#include "Theme.h"
#include "FadeEditor.h"
#include "PluginMenu.h"
#include "../model/Stretch.h"
#include "../model/GainLine.h"
#include "../model/Routing.h"

namespace tonkori::ui
{
using namespace tracktion::literals;

namespace
{
    juce::Label* makeLabel (juce::OwnedArray<juce::Component>& owner, juce::Component& parent, const juce::String& text, bool header = false)
    {
        auto* l = new juce::Label ({}, text);
        l->setFont (header ? font (11.0f, true) : font (11.5f));
        l->setColour (juce::Label::textColourId, header ? colours::textDim : colours::textDim);
        owner.add (l);
        parent.addAndMakeVisible (l);
        return l;
    }

    struct Field : public juce::TextEditor
    {
        Field()
        {
            setFont (mono (12.0f));
            setSelectAllWhenFocused (true);
            setJustification (juce::Justification::centredLeft);
        }
        std::function<void (const juce::String&)> onCommit;
        void commit() { if (onCommit) onCommit (getText()); }
    };

    Field* makeField (juce::OwnedArray<juce::Component>& owner, juce::Component& parent)
    {
        auto* f = new Field();
        f->onReturnKey = [f] { f->commit(); f->giveAwayKeyboardFocus(); };
        f->onFocusLost = [f] { f->commit(); };
        f->onEscapeKey = [f] { f->giveAwayKeyboardFocus(); };
        owner.add (f);
        parent.addAndMakeVisible (f);
        return f;
    }

    juce::TextButton* makeToggle (juce::OwnedArray<juce::Component>& owner, juce::Component& parent, const juce::String& text)
    {
        auto* b = new juce::TextButton (text);
        b->setWantsKeyboardFocus (false);
        owner.add (b);
        parent.addAndMakeVisible (b);
        return b;
    }

    /** Simple vertical form layout. */
    struct Form
    {
        juce::Rectangle<int> area;
        int y = 0;
        static constexpr int labelW = 64, rowH = 24, gap = 4;

        juce::Rectangle<int> row (int h = rowH) { auto r = juce::Rectangle<int> (area.getX(), area.getY() + y, area.getWidth(), h); y += h + gap; return r; }
        void space (int h) { y += h; }
        void labelled (juce::Component& label, juce::Component& c, int h = rowH)
        {
            auto r = row (h);
            label.setBounds (r.removeFromLeft (labelW));
            c.setBounds (r);
        }
    };

    //==============================================================================
    class ClipPanel : public juce::Component
    {
    public:
        ClipPanel (ViewState& v) : vs (v),
            fadeIn (v, true,  [this] { return clips(); }),
            fadeOut (v, false, [this] { return clips(); })
        {
            title = makeLabel (owned, *this, "CLIP", true);
            nameL = makeLabel (owned, *this, "Name");    name = makeField (owned, *this);
            startL = makeLabel (owned, *this, "Start");  start = makeField (owned, *this);
            endL = makeLabel (owned, *this, "End");      end = makeField (owned, *this);
            lenL = makeLabel (owned, *this, "Length");   length = makeField (owned, *this);
            offL = makeLabel (owned, *this, "Offset");   offset = makeField (owned, *this);
            gainL = makeLabel (owned, *this, "Gain");    gain = makeField (owned, *this);

            mute = makeToggle (owned, *this, "Mute");
            reverse = makeToggle (owned, *this, "Reverse");
            fadesOn = makeToggle (owned, *this, "Fades on");
            fadesOn->setColour (juce::TextButton::buttonOnColourId, colours::accent);
            mute->setColour (juce::TextButton::buttonOnColourId, colours::warn.darker (0.2f));

            start->setTooltip ("Clip start (spot). Any format: 5.1.000, 00:00:12:05, 1:02.5, 96000 smp");
            gain->setTooltip ("Clip gain in dB (pre-fader)");

            name->onCommit = [this] (const juce::String& s) { if (auto* c = first()) if (s.isNotEmpty() && s != c->getName()) { tx ("Rename clip"); c->setName (s); } };
            start->onCommit = [this] (const juce::String& s)
            {
                if (auto* c = first()) if (auto t = vs.parse (s)) if (*t != c->getPosition().getStart()) { tx ("Spot clip"); c->setStart (*t, false, true); }
                refresh();
            };
            end->onCommit = [this] (const juce::String& s)
            {
                if (auto* c = first()) if (auto t = vs.parse (s)) if (*t > c->getPosition().getStart() && *t != c->getPosition().getEnd()) { tx ("Clip end"); c->setEnd (*t, true); }
                refresh();
            };
            length->onCommit = [this] (const juce::String& s)
            {
                if (auto* c = first()) if (auto d = vs.parseDuration (s)) if (*d > 0_td && *d != c->getPosition().getLength()) { tx ("Clip length"); c->setLength (*d, true); }
                refresh();
            };
            offset->onCommit = [this] (const juce::String& s)
            {
                if (auto* c = first()) if (auto d = vs.parseDuration (s)) if (*d >= 0_td && *d != c->getPosition().getOffset()) { tx ("Clip offset"); c->setOffset (*d); }
                refresh();
            };
            gain->onCommit = [this] (const juce::String& s)
            {
                const float db = s.trim().startsWithIgnoreCase ("-inf") ? -100.0f : juce::jlimit (-100.0f, 24.0f, s.getFloatValue());
                tx ("Clip gain");
                for (auto* c : clips()) c->setGainDB (db);
                refresh();
            };
            mute->onClick = [this] { tx ("Mute clip"); const bool m = ! mute->getToggleState(); for (auto* c : clips()) c->setMuted (m); refresh(); };
            reverse->onClick = [this] { tx ("Reverse clip"); const bool r = ! reverse->getToggleState(); for (auto* c : clips()) c->setIsReversed (r); refresh(); };
            fadesOn->onClick = [this] { tx ("Fades on/off"); const bool on = ! fadesOn->getToggleState(); for (auto* c : clips()) fades::setFadesOn (*c, on); refresh(); };

            // Time & pitch
            stretchTitle = makeLabel (owned, *this, "TIME & PITCH", true);
            algoL = makeLabel (owned, *this, "Mode");
            algo.addItemList ([] { juce::StringArray a; for (auto& x : tonkori::stretch::algorithms()) a.add (x.name); return a; }(), 1);
            algo.onChange = [this] { tx ("Stretch mode"); for (auto* c : clips()) tonkori::stretch::setAlgorithm (*c, algo.getSelectedItemIndex()); refresh(); };
            addAndMakeVisible (algo);
            stretchL = makeLabel (owned, *this, "Stretch"); stretch = makeField (owned, *this);
            pitchL = makeLabel (owned, *this, "Pitch");     pitch = makeField (owned, *this);
            stretch->setTooltip ("Length as % of the original: 200 = twice as long (half speed). Opt-drag a clip edge to stretch by eye (TCE).");
            pitch->setTooltip ("Semitones, decimals are cents: 7, -12, 0.25");
            stretch->onCommit = [this] (const juce::String& txt)
            {
                const double pc = txt.retainCharacters ("0123456789.-").getDoubleValue();
                if (pc > 0) { tx ("Stretch"); for (auto* c : clips()) tonkori::stretch::setPercent (*c, pc); }
                refresh();
            };
            pitch->onCommit = [this] (const juce::String& txt)
            {
                tx ("Pitch");
                for (auto* c : clips()) tonkori::stretch::setPitch (*c, txt.retainCharacters ("0123456789.-+").getDoubleValue());
                refresh();
            };
            resetStretch = makeToggle (owned, *this, "Reset time & pitch");
            resetStretch->onClick = [this] { tx ("Reset stretch"); for (auto* c : clips()) tonkori::stretch::reset (*c); refresh(); };

            // Gain line
            gainTitle = makeLabel (owned, *this, "CLIP GAIN LINE", true);
            gainInfo = makeLabel (owned, *this, "");
            clearGain = makeToggle (owned, *this, "Clear line");
            showGain = makeToggle (owned, *this, "Show lines");
            showGain->setColour (juce::TextButton::buttonOnColourId, colours::gain.withAlpha (0.5f));
            clearGain->onClick = [this] { tx ("Clear gain line"); for (auto* c : clips()) tonkori::gainline::clear (*c); refresh(); };
            showGain->onClick = [this] { tonkori::gainline::setShown (vs.edit, ! tonkori::gainline::isShown (vs.edit)); vs.changed(); refresh(); };

            fadeIn.onLayoutChange = fadeOut.onLayoutChange = [this] { if (onHeightChange) onHeightChange(); };
            addAndMakeVisible (fadeIn);
            addAndMakeVisible (fadeOut);
            refresh();
        }

        std::function<void()> onHeightChange;

        juce::Array<te::AudioClipBase*> clips() const
        {
            juce::Array<te::AudioClipBase*> r;
            for (auto* c : vs.selection.getItemsOfType<te::Clip>())
                if (auto* ac = dynamic_cast<te::AudioClipBase*> (c)) r.add (ac);
            return r;
        }

        te::AudioClipBase* first() const { auto c = clips(); return c.isEmpty() ? nullptr : c.getFirst(); }

        void refresh()
        {
            auto* c = first();
            if (c == nullptr) return;
            const auto n = clips().size();
            title->setText (n > 1 ? "CLIPS (" + juce::String (n) + ")  - edits apply to all" : "CLIP", juce::dontSendNotification);

            auto set = [] (Field* f, const juce::String& s) { if (! f->hasKeyboardFocus (true)) f->setText (s, false); };
            const auto pos = c->getPosition();
            set (name, c->getName());
            set (start, vs.format (pos.getStart()));
            set (end, vs.format (pos.getEnd()));
            set (length, vs.formatDuration (pos.getLength()));
            set (offset, vs.formatDuration (pos.getOffset()));
            set (gain, c->getGainDB() <= -99.0f ? juce::String ("-inf dB") : juce::String (c->getGainDB(), 2) + " dB");
            mute->setToggleState (c->isMuted(), juce::dontSendNotification);
            reverse->setToggleState (c->getIsReversed(), juce::dontSendNotification);
            fadesOn->setToggleState (fades::areFadesOn (*c), juce::dontSendNotification);
            algo.setSelectedItemIndex (tonkori::stretch::algorithmIndex (*c), juce::dontSendNotification);
            set (stretch, juce::String (tonkori::stretch::getPercent (*c), 2) + " %");
            set (pitch, (c->getPitchChange() > 0 ? "+" : "") + juce::String (c->getPitchChange(), 2) + " st");
            const auto np = (int) tonkori::gainline::getPoints (*c).size();
            gainInfo->setText (np == 0 ? juce::String ("No points. Click the line on the clip to add one; Cmd-drag moves the whole line, Opt-click deletes a point.")
                                       : juce::String (np) + " point" + (np > 1 ? "s" : ""), juce::dontSendNotification);
            showGain->setToggleState (tonkori::gainline::isShown (vs.edit), juce::dontSendNotification);
            if (! fadeIn.isMouseOverOrDragging (true) && ! fadeIn.hasKeyboardFocus (true)) fadeIn.refresh();
            if (! fadeOut.isMouseOverOrDragging (true) && ! fadeOut.hasKeyboardFocus (true)) fadeOut.refresh();
        }

        int idealHeight() const { return 40 + 6 * 28 + 34 + 16 + fadeIn.getIdealHeight() + 14 + fadeOut.getIdealHeight() + 20 + 22 + 4 * 28 + 16 + 22 + 40 + 30; }

        void resized() override
        {
            Form f { getLocalBounds().reduced (14, 10) };
            title->setBounds (f.row (18));
            f.labelled (*nameL, *name);
            f.labelled (*startL, *start);
            f.labelled (*endL, *end);
            f.labelled (*lenL, *length);
            f.labelled (*offL, *offset);
            f.labelled (*gainL, *gain);
            auto toggles = f.row (26);
            const int w = (toggles.getWidth() - 8) / 3;
            mute->setBounds (toggles.removeFromLeft (w)); toggles.removeFromLeft (4);
            reverse->setBounds (toggles.removeFromLeft (w)); toggles.removeFromLeft (4);
            fadesOn->setBounds (toggles);
            f.space (12);
            fadeIn.setBounds (f.row (fadeIn.getIdealHeight()));
            f.space (10);
            fadeOut.setBounds (f.row (fadeOut.getIdealHeight()));
            f.space (12);
            stretchTitle->setBounds (f.row (18));
            f.labelled (*algoL, algo);
            f.labelled (*stretchL, *stretch);
            f.labelled (*pitchL, *pitch);
            resetStretch->setBounds (f.row (24));
            f.space (12);
            gainTitle->setBounds (f.row (18));
            gainInfo->setBounds (f.row (36));
            auto gr = f.row (24);
            clearGain->setBounds (gr.removeFromLeft (gr.getWidth() / 2 - 2)); gr.removeFromLeft (4);
            showGain->setBounds (gr);
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (colours::line);
            g.drawHorizontalLine (fadeIn.getY() - 8, 14.0f, (float) getWidth() - 14);
            g.drawHorizontalLine (fadeOut.getY() - 6, 14.0f, (float) getWidth() - 14);
        }

        FadeEditorPanel& getFadePanel (bool in) { return in ? fadeIn : fadeOut; }

    private:
        ViewState& vs;
        juce::OwnedArray<juce::Component> owned;
        juce::Label *title, *nameL, *startL, *endL, *lenL, *offL, *gainL;
        Field *name, *start, *end, *length, *offset, *gain;
        juce::TextButton *mute, *reverse, *fadesOn, *resetStretch, *clearGain, *showGain;
        juce::Label *stretchTitle, *algoL, *stretchL, *pitchL, *gainTitle, *gainInfo;
        Field *stretch, *pitch;
        juce::ComboBox algo;
        FadeEditorPanel fadeIn, fadeOut;

        void tx (const juce::String& n) { vs.edit.getUndoManager().beginNewTransaction (n); }
    };

    //==============================================================================
    class TrackPanel : public juce::Component
    {
    public:
        TrackPanel (ViewState& v, te::AudioTrack& t) : vs (v), track (&t)
        {
            title = makeLabel (owned, *this, "TRACK", true);
            nameL = makeLabel (owned, *this, "Name");     name = makeField (owned, *this);
            volL = makeLabel (owned, *this, "Volume");    vol = makeField (owned, *this);
            panL = makeLabel (owned, *this, "Pan");       pan = makeField (owned, *this);
            insL = makeLabel (owned, *this, "INSERTS", true);
            add = makeToggle (owned, *this, "+ Add insert");
            outL = makeLabel (owned, *this, "Output");
            output = makeToggle (owned, *this, "Master");
            output->onClick = [this]
            {
                if (! track) return;
                auto dests = tonkori::routing::destinationsFor (*track);
                juce::PopupMenu m;
                for (int i = 0; i < (int) dests.size(); ++i) m.addItem (i + 1, dests[(size_t) i].label, true, tonkori::routing::currentDestination (*track) == dests[(size_t) i].label);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (output), [this, dests] (int r)
                {
                    if (r <= 0 || ! track) return;
                    tx ("Track output");
                    tonkori::routing::setDestination (*track, dests[(size_t) (r - 1)]);
                    refresh();
                });
            };

            vol->setTooltip ("Track volume in dB");
            pan->setTooltip ("-100 (L) ... 0 ... 100 (R)");

            name->onCommit = [this] (const juce::String& s) { if (track && s.isNotEmpty()) { tx ("Rename track"); track->setName (s); } };
            vol->onCommit = [this] (const juce::String& s)
            {
                if (auto* vp = track ? track->getVolumePlugin() : nullptr)
                { tx ("Track volume"); vp->setVolumeDb (s.trim().startsWithIgnoreCase ("-inf") ? -100.0f : juce::jlimit (-100.0f, 6.0f, s.getFloatValue())); }
                refresh();
            };
            pan->onCommit = [this] (const juce::String& s)
            {
                if (auto* vp = track ? track->getVolumePlugin() : nullptr)
                { tx ("Track pan"); vp->setPan (juce::jlimit (-1.0f, 1.0f, s.getFloatValue() / 100.0f)); }
                refresh();
            };
            add->onClick = [this] { if (track) showAddPluginMenu (*track, add, [this] { rebuildInserts(); }); };
            rebuildInserts();
            refresh();
        }

        void refresh()
        {
            if (! track) return;
            auto set = [] (Field* f, const juce::String& s) { if (! f->hasKeyboardFocus (true)) f->setText (s, false); };
            set (name, track->getName());
            if (auto* vp = track->getVolumePlugin())
            {
                set (vol, vp->getVolumeDb() <= -99.0f ? juce::String ("-inf dB") : juce::String (vp->getVolumeDb(), 2) + " dB");
                set (pan, juce::String (juce::roundToInt (vp->getPan() * 100.0f)));
            }
            output->setButtonText (tonkori::routing::currentDestination (*track));
            if (userPlugins (*track).size() != slots.size()) rebuildInserts();
            for (int i = 0; i < slots.size(); ++i)
                if (auto* p = userPlugins (*track)[i])
                    slots[i]->setButtonText ((p->isEnabled() ? "" : "[bypassed] ") + slotName (*p));
        }

        int idealHeight() const { return 40 + 4 * 28 + 30 + (slots.size() + 1) * 28 + 20; }

        void resized() override
        {
            Form f { getLocalBounds().reduced (14, 10) };
            title->setBounds (f.row (18));
            f.labelled (*nameL, *name);
            f.labelled (*volL, *vol);
            f.labelled (*panL, *pan);
            f.labelled (*outL, *output);
            f.space (10);
            insL->setBounds (f.row (18));
            for (auto* s : slots) s->setBounds (f.row (24));
            add->setBounds (f.row (24));
        }

        std::function<void()> onHeightChange;

    private:
        ViewState& vs;
        te::AudioTrack::Ptr track;
        juce::OwnedArray<juce::Component> owned;
        juce::OwnedArray<juce::TextButton> slots;
        juce::Label *title, *nameL, *volL, *panL, *insL, *outL;
        Field *name, *vol, *pan;
        juce::TextButton *add, *output;

        void tx (const juce::String& n) { vs.edit.getUndoManager().beginNewTransaction (n); }

        void rebuildInserts()
        {
            slots.clear();
            if (! track) return;
            for (auto* p : userPlugins (*track))
            {
                auto* b = slots.add (new juce::TextButton (slotName (*p)));
                b->setWantsKeyboardFocus (false);
                te::Plugin::Ptr ptr (p);
                b->onClick = [this, ptr, b] { showPluginSlotMenu (*ptr, b, [this] { refresh(); }); };
                addAndMakeVisible (b);
            }
            resized();
            if (onHeightChange) onHeightChange();
        }
    };

    //==============================================================================
    class SessionPanel : public juce::Component
    {
    public:
        explicit SessionPanel (ViewState& v) : vs (v)
        {
            title = makeLabel (owned, *this, "SESSION", true);
            tempoL = makeLabel (owned, *this, "Tempo");     tempo = makeField (owned, *this);
            sigL = makeLabel (owned, *this, "Meter");       sig = makeField (owned, *this);
            fmtL = makeLabel (owned, *this, "Display");
            fpsL = makeLabel (owned, *this, "Frames");
            autoL = makeLabel (owned, *this, "AUTO-FADES  (Pyramix / Cubase)", true);
            autoLenL = makeLabel (owned, *this, "Length");  autoLen = makeField (owned, *this);
            autoShapeL = makeLabel (owned, *this, "Shape"); autoShape = makeField (owned, *this);
            engineL = makeLabel (owned, *this, "ENGINE", true);
            engineInfo = makeLabel (owned, *this, "");
            engineInfo->setFont (mono (11.0f));
            engineInfo->setJustificationType (juce::Justification::topLeft);
            autoOn = makeToggle (owned, *this, "Auto-fades on");

            format.addItemList ({ "Bars | Beats", "Timecode", "Min:Sec", "Samples" }, 1);
            fps.addItemList (fpsNames(), 1);
            addAndMakeVisible (format);
            addAndMakeVisible (fps);
            format.onChange = [this] { vs.timeFormat = (TimeFormat) format.getSelectedItemIndex(); vs.changed(); };
            fps.onChange = [this] { vs.fpsIndex = fps.getSelectedItemIndex(); vs.changed(); };

            tempo->onCommit = [this] (const juce::String& s)
            {
                const double bpm = s.getDoubleValue();
                if (bpm >= 20.0 && bpm <= 400.0) { tx ("Tempo"); vs.edit.tempoSequence.getTempo (0)->setBpm (bpm); }
                refresh();
            };
            sig->onCommit = [this] (const juce::String& s)
            {
                const int n = s.upToFirstOccurrenceOf ("/", false, false).getIntValue();
                const int d = s.fromFirstOccurrenceOf ("/", false, false).getIntValue();
                if (n >= 1 && n <= 32 && (d == 2 || d == 4 || d == 8 || d == 16))
                {
                    tx ("Meter");
                    auto* ts = vs.edit.tempoSequence.getTimeSig (0);
                    ts->numerator = n; ts->denominator = d;
                }
                refresh();
            };
            autoOn->onClick = [this] { tx ("Auto-fades"); fades::setAutoFade (vs.edit, ! fades::isAutoFadeOn (vs.edit), fades::getAutoFadeMs (vs.edit)); refresh(); };
            autoLen->onCommit = [this] (const juce::String& s)
            {
                const double ms = s.getDoubleValue();
                if (ms >= 0.0 && ms <= 500.0) { tx ("Auto-fade length"); fades::setAutoFade (vs.edit, fades::isAutoFadeOn (vs.edit), ms); }
                refresh();
            };
            autoShape->onCommit = [this] (const juce::String& s)
            {
                auto spec = FadeSpec::fromString (s);
                tx ("Auto-fade shape");
                fades::setAutoFade (vs.edit, fades::isAutoFadeOn (vs.edit), fades::getAutoFadeMs (vs.edit), &spec);
                refresh();
            };
            refresh();
        }

        void refresh()
        {
            auto set = [] (Field* f, const juce::String& s) { if (! f->hasKeyboardFocus (true)) f->setText (s, false); };
            set (tempo, juce::String (vs.edit.tempoSequence.getTempo (0)->getBpm(), 2) + " bpm");
            auto* ts = vs.edit.tempoSequence.getTimeSig (0);
            set (sig, juce::String (ts->numerator.get()) + "/" + juce::String (ts->denominator.get()));
            format.setSelectedItemIndex ((int) vs.timeFormat, juce::dontSendNotification);
            fps.setSelectedItemIndex (vs.fpsIndex, juce::dontSendNotification);
            autoOn->setToggleState (fades::isAutoFadeOn (vs.edit), juce::dontSendNotification);
            set (autoLen, juce::String (fades::getAutoFadeMs (vs.edit), 2) + " ms");
            set (autoShape, fades::getAutoFadeSpec (vs.edit).toString());

            auto* dev = vs.edit.engine.getDeviceManager().deviceManager.getCurrentAudioDevice();
            juce::String info;
            info << "Processing  32-bit float\n"
                 << "Recording   WAV 32-bit float\n"
                 << "Device      " << (dev ? dev->getName() : juce::String ("none")) << "\n"
                 << "Rate        " << (dev ? juce::String (dev->getCurrentSampleRate() / 1000.0, 1) + " kHz" : juce::String ("-")) << "\n"
                 << "Buffer      " << (dev ? juce::String (dev->getCurrentBufferSizeSamples()) + " smp" : juce::String ("-")) << "\n"
                 << "Master      unity, linear pan law";
            engineInfo->setText (info, juce::dontSendNotification);
        }

        void resized() override
        {
            Form f { getLocalBounds().reduced (14, 10) };
            title->setBounds (f.row (18));
            f.labelled (*tempoL, *tempo);
            f.labelled (*sigL, *sig);
            f.labelled (*fmtL, format);
            f.labelled (*fpsL, fps);
            f.space (12);
            autoL->setBounds (f.row (18));
            autoOn->setBounds (f.row (26));
            f.labelled (*autoLenL, *autoLen);
            f.labelled (*autoShapeL, *autoShape);
            f.space (12);
            engineL->setBounds (f.row (18));
            engineInfo->setBounds (f.row (100));
        }

        int idealHeight() const { return 520; }

    private:
        ViewState& vs;
        juce::OwnedArray<juce::Component> owned;
        juce::Label *title, *tempoL, *sigL, *fmtL, *fpsL, *autoL, *autoLenL, *autoShapeL, *engineL, *engineInfo;
        Field *tempo, *sig, *autoLen, *autoShape;
        juce::TextButton* autoOn;
        juce::ComboBox format, fps;

        void tx (const juce::String& n) { vs.edit.getUndoManager().beginNewTransaction (n); }
    };
}

//==============================================================================
Inspector::Inspector (ViewState& v) : vs (v)
{
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    vs.addChangeListener (this);
    vs.selection.addChangeListener (this);
    rebuild (true);
    startTimerHz (6);
}

Inspector::~Inspector()
{
    vs.selection.removeChangeListener (this);
    vs.removeChangeListener (this);
}

void Inspector::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    g.setColour (colours::lineStrong);
    g.drawVerticalLine (0, 0.0f, (float) getHeight());
}

void Inspector::resized()
{
    viewport.setBounds (getLocalBounds().withTrimmedLeft (1));
    if (content)
    {
        int h = 600;
        if (auto* c = dynamic_cast<ClipPanel*> (content.get())) h = c->idealHeight();
        if (auto* t = dynamic_cast<TrackPanel*> (content.get())) h = t->idealHeight();
        if (auto* s = dynamic_cast<SessionPanel*> (content.get())) h = s->idealHeight();
        content->setSize (viewport.getMaximumVisibleWidth(), juce::jmax (h, viewport.getHeight()));
    }
}

void Inspector::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    rebuild (source == &vs.selection ? false : false);
}

void Inspector::rebuild (bool force)
{
    Mode newMode = Mode::session;
    juce::Array<te::EditItemID> ids;

    auto clips = vs.selection.getItemsOfType<te::Clip>();
    for (auto* c : clips) if (dynamic_cast<te::AudioClipBase*> (c)) ids.add (c->itemID);

    if (! ids.isEmpty()) newMode = Mode::clip;
    else if (auto tracks = vs.selection.getItemsOfType<te::AudioTrack>(); ! tracks.isEmpty())
    {
        newMode = Mode::track;
        ids.add (tracks.getFirst()->itemID);
    }

    if (! force && newMode == mode && ids == shownIDs)
    {
        if (auto* c = dynamic_cast<ClipPanel*> (content.get())) c->refresh();
        if (auto* t = dynamic_cast<TrackPanel*> (content.get())) t->refresh();
        if (auto* s = dynamic_cast<SessionPanel*> (content.get())) s->refresh();
        return;
    }

    mode = newMode;
    shownIDs = ids;
    viewport.setViewedComponent (nullptr, false);

    if (mode == Mode::clip)
    {
        auto cp = std::make_unique<ClipPanel> (vs);
        cp->onHeightChange = [this] { resized(); };
        content = std::move (cp);
    }
    else if (mode == Mode::track)
    {
        auto tp = std::make_unique<TrackPanel> (vs, *vs.selection.getItemsOfType<te::AudioTrack>().getFirst());
        tp->onHeightChange = [this] { resized(); };
        content = std::move (tp);
    }
    else
        content = std::make_unique<SessionPanel> (vs);

    viewport.setViewedComponent (content.get(), false);
    resized();
}

void Inspector::timerCallback()
{
    rebuild (false);
}

void Inspector::focusFades (bool fadeIn)
{
    if (auto* c = dynamic_cast<ClipPanel*> (content.get()))
    {
        auto& p = c->getFadePanel (fadeIn);
        viewport.setViewPosition (0, juce::jmax (0, p.getY() - 10));
    }
}
}
