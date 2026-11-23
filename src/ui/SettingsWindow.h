#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "KeyProfiles.h"
#include "../engine/LatencyProbe.h"

namespace te = tracktion;

namespace tonkori::ui
{
    /** What the settings window needs from the main window. */
    struct SettingsHost
    {
        virtual ~SettingsHost() = default;
        virtual te::Engine& getEngine() = 0;
        virtual te::Edit* getEdit() = 0;
        virtual juce::ApplicationCommandManager& getCommands() = 0;
        virtual keys::Profile getKeyProfile() = 0;
        virtual void setKeyProfile (keys::Profile) = 0;
        virtual void saveKeyMappings() = 0;
        virtual void resetKeyMappings() = 0;
        virtual double getPreRoll() = 0;   virtual void setPreRoll (double) = 0;
        virtual double getPostRoll() = 0;  virtual void setPostRoll (double) = 0;
        virtual double getVideoOffsetMs() = 0; virtual void setVideoOffsetMs (double) = 0;
    };

    //==============================================================================
    class AudioPage : public juce::Component
    {
    public:
        explicit AudioPage (SettingsHost& h) : host (h),
            selector (h.getEngine().getDeviceManager().deviceManager, 0, 512, 0, 512, false, false, false, false)
        {
            addAndMakeVisible (selector);
            layoutL.setText ("Channels as", juce::dontSendNotification);
            layoutL.setColour (juce::Label::textColourId, colours::textDim);
            addAndMakeVisible (layoutL);
            inLayout.addItemList ({ "Inputs: mono (MADI / multitrack)", "Inputs: stereo pairs" }, 1);
            auto& dm = h.getEngine().getDeviceManager();
            const auto props = &h.getEngine().getPropertyStorage().getPropertiesFile();
            inLayout.setSelectedItemIndex (props->getIntValue ("tkInputsMono", 1) ? 0 : 1, juce::dontSendNotification);
            inLayout.onChange = [this, &dm, props]
            {
                const bool mono = inLayout.getSelectedItemIndex() == 0;
                props->setValue ("tkInputsMono", mono ? 1 : 0);
                props->saveIfNeeded();
                dm.setAllWaveInputsToNumChannels (mono ? 1u : 2u);
            };
            addAndMakeVisible (inLayout);
            outNote.setText ("Outputs are stereo pairs; any track or bus can go straight to any pair.", juce::dontSendNotification);
            outNote.setColour (juce::Label::textColourId, colours::textDim);
            outNote.setFont (font (11.5f));
            addAndMakeVisible (outNote);

            note.setText ("RME MADIface / Pyramix-style rigs: choose mono inputs so every MADI channel is its own track input. "
                          "Set the sample rate here or in TotalMix; Tonkori follows the device.", juce::dontSendNotification);
            note.setColour (juce::Label::textColourId, colours::textFaint);
            note.setFont (font (11.5f));
            note.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (note);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (12);
            auto bottom = r.removeFromBottom (74);
            selector.setBounds (r);
            auto row = bottom.removeFromTop (26);
            layoutL.setBounds (row.removeFromLeft (90));
            inLayout.setBounds (row.removeFromLeft (row.getWidth() / 2 - 4)); row.removeFromLeft (8);
            outNote.setBounds (row);
            bottom.removeFromTop (6);
            note.setBounds (bottom);
        }

    private:
        SettingsHost& host;
        juce::AudioDeviceSelectorComponent selector;
        juce::Label layoutL, note, outNote;
        juce::ComboBox inLayout;
    };

    //==============================================================================
    class LatencyPage : public juce::Component, private juce::Timer
    {
    public:
        explicit LatencyPage (SettingsHost& h) : host (h)
        {
            for (auto* l : { &info, &result })
            {
                l->setFont (mono (12.0f));
                l->setColour (juce::Label::textColourId, colours::text);
                l->setJustificationType (juce::Justification::topLeft);
                addAndMakeVisible (*l);
            }
            auto label = [this] (juce::Label& l, const juce::String& t) { l.setText (t, juce::dontSendNotification); l.setColour (juce::Label::textColourId, colours::textDim); addAndMakeVisible (l); };
            label (outL, "Loop out");
            label (inL, "Loop in");
            label (offsetL, "Extra record offset (samples)");
            label (videoL, "Video sync offset (ms)");
            addAndMakeVisible (outCh);
            addAndMakeVisible (inCh);

            measure.setButtonText ("Measure round trip");
            measure.onClick = [this] { startMeasurement(); };
            addAndMakeVisible (measure);
            apply.setButtonText ("Use measured offset");
            apply.setEnabled (false);
            apply.onClick = [this] { offset.setText (juce::String (measuredExtra), juce::sendNotification); applyOffset(); };
            addAndMakeVisible (apply);

            offset.setFont (mono (12.0f));
            offset.onReturnKey = offset.onFocusLost = [this] { applyOffset(); };
            addAndMakeVisible (offset);
            video.setFont (mono (12.0f));
            video.setText (juce::String (host.getVideoOffsetMs(), 1));
            video.onReturnKey = video.onFocusLost = [this] { host.setVideoOffsetMs (video.getText().getDoubleValue()); };
            addAndMakeVisible (video);

            help.setText ("Patch an output to an input (a cable, or a TotalMix loopback), pick both channels and press Measure. "
                          "The engine already shifts recordings by the latency the driver reports; the measurement finds what the "
                          "driver can't know (external AD/DA on the MADI chain, outboard). Turn speakers down: the test sweep is -12 dBFS.",
                          juce::dontSendNotification);
            help.setFont (font (11.5f));
            help.setColour (juce::Label::textColourId, colours::textFaint);
            help.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (help);

            refreshDevice();
            startTimerHz (4);
        }

        ~LatencyPage() override { stopMeasurement(); }

        void resized() override
        {
            auto r = getLocalBounds().reduced (14);
            info.setBounds (r.removeFromTop (130));
            r.removeFromTop (6);
            auto row = r.removeFromTop (26);
            outL.setBounds (row.removeFromLeft (70)); outCh.setBounds (row.removeFromLeft (180)); row.removeFromLeft (12);
            inL.setBounds (row.removeFromLeft (60)); inCh.setBounds (row.removeFromLeft (180));
            r.removeFromTop (8);
            row = r.removeFromTop (28);
            measure.setBounds (row.removeFromLeft (170)); row.removeFromLeft (8);
            apply.setBounds (row.removeFromLeft (170));
            r.removeFromTop (6);
            result.setBounds (r.removeFromTop (54));
            row = r.removeFromTop (26);
            offsetL.setBounds (row.removeFromLeft (220)); offset.setBounds (row.removeFromLeft (100));
            r.removeFromTop (6);
            row = r.removeFromTop (26);
            videoL.setBounds (row.removeFromLeft (220)); video.setBounds (row.removeFromLeft (100));
            r.removeFromTop (10);
            help.setBounds (r);
        }

    private:
        SettingsHost& host;
        juce::Label info, result, outL, inL, offsetL, videoL, help;
        juce::ComboBox outCh, inCh;
        juce::TextButton measure, apply;
        juce::TextEditor offset, video;
        std::unique_ptr<tonkori::latency::Measurement> running;
        int measuredExtra = 0;
        juce::uint32 startedAt = 0;
        juce::String lastDevice;

        juce::AudioIODevice* device() { return host.getEngine().getDeviceManager().deviceManager.getCurrentAudioDevice(); }

        void refreshDevice()
        {
            auto* d = device();
            const juce::String name = d ? d->getName() + juce::String (d->getCurrentSampleRate()) + juce::String (d->getCurrentBufferSizeSamples()) : juce::String();
            if (name != lastDevice)
            {
                lastDevice = name;
                outCh.clear (juce::dontSendNotification);
                inCh.clear (juce::dontSendNotification);
                if (d)
                {
                    outCh.addItemList (d->getOutputChannelNames(), 1);
                    inCh.addItemList (d->getInputChannelNames(), 1);
                    outCh.setSelectedItemIndex (0, juce::dontSendNotification);
                    inCh.setSelectedItemIndex (0, juce::dontSendNotification);
                }
                auto& dm = host.getEngine().getDeviceManager();
                double adj = 0;
                if (dm.getNumWaveInDevices() > 0) if (auto* w = dm.getWaveInDevice (0)) adj = w->getRecordAdjustmentMs();
                offset.setText (juce::String (juce::roundToInt (adj / 1000.0 * (d ? d->getCurrentSampleRate() : 48000.0))), false);
            }

            juce::String s;
            if (d == nullptr) s = "No audio device open.";
            else
            {
                const double sr = d->getCurrentSampleRate();
                auto ms = [sr] (int n) { return juce::String (n * 1000.0 / sr, 2) + " ms"; };
                const int in = d->getInputLatencyInSamples(), out = d->getOutputLatencyInSamples(), buf = d->getCurrentBufferSizeSamples();
                s << "Device       " << d->getName() << "  (" << d->getTypeName() << ")\n"
                  << "Rate/buffer  " << juce::String (sr / 1000.0, 1) << " kHz, " << buf << " smp (" << ms (buf) << ")\n"
                  << "Input        " << in << " smp  " << ms (in) << "   (driver report: device + safety + buffer)\n"
                  << "Output       " << out << " smp  " << ms (out) << "\n"
                  << "Round trip   " << (in + out) << " smp  " << ms (in + out) << "   recordings are shifted by this automatically\n"
                  << "Plug-in delay " << sessionPluginLatency (sr) << " smp worst path   compensated on every track, send and bus";
            }
            info.setText (s, juce::dontSendNotification);
        }

        int sessionPluginLatency (double sr)
        {
            auto* e = host.getEdit();
            if (e == nullptr) return 0;
            double worst = 0;
            for (auto* t : te::getAudioTracks (*e))
            {
                double sum = 0;
                for (auto* p : t->pluginList) sum += p->getLatencySeconds();
                worst = juce::jmax (worst, sum);
            }
            double master = 0;
            for (auto* p : e->getMasterPluginList()) master += p->getLatencySeconds();
            return juce::roundToInt ((worst + master) * sr);
        }

        void applyOffset()
        {
            auto* d = device();
            const double sr = d ? d->getCurrentSampleRate() : 48000.0;
            const double ms = offset.getText().getIntValue() * 1000.0 / sr;
            auto& dm = host.getEngine().getDeviceManager();
            for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
                if (auto* w = dm.getWaveInDevice (i)) w->setRecordAdjustmentMs (ms);
        }

        void startMeasurement()
        {
            if (running) return;
            if (device() == nullptr || outCh.getNumItems() == 0 || inCh.getNumItems() == 0)
            {
                result.setText ("Open an audio device first (Audio tab).", juce::dontSendNotification);
                return;
            }
            startedAt = juce::Time::getMillisecondCounter();
            if (auto* e = host.getEdit()) e->getTransport().stop (false, false);
            running = std::make_unique<tonkori::latency::Measurement> (outCh.getSelectedItemIndex(), inCh.getSelectedItemIndex());
            host.getEngine().getDeviceManager().deviceManager.addAudioCallback (running.get());
            result.setText ("Measuring...", juce::dontSendNotification);
            measure.setEnabled (false);
        }

        void stopMeasurement()
        {
            if (! running) return;
            host.getEngine().getDeviceManager().deviceManager.removeAudioCallback (running.get());
            running.reset();
            measure.setEnabled (true);
        }

        void timerCallback() override
        {
            refreshDevice();
            if (running && ! running->isDone() && juce::Time::getMillisecondCounter() - startedAt > 6000)
            {
                stopMeasurement();
                result.setText ("The device stopped delivering audio; measurement cancelled.", juce::dontSendNotification);
                return;
            }
            if (running && running->isDone())
            {
                const auto est = running->result();
                const int reported = running->reportedIn + running->reportedOut;
                const double sr = running->sampleRate;
                stopMeasurement();
                if (est.confidence < 8.0)
                {
                    result.setText ("No clear signal came back (check the loop patch and levels).", juce::dontSendNotification);
                    return;
                }
                const int measured = juce::roundToInt (est.samples);
                measuredExtra = measured - reported;
                result.setText ("Measured round trip " + juce::String (est.samples, 1) + " smp (" + juce::String (est.samples * 1000.0 / sr, 3) + " ms)\n"
                                + "Driver reports " + juce::String (reported) + " smp: difference " + juce::String (measuredExtra) + " smp",
                                juce::dontSendNotification);
                apply.setEnabled (true);
            }
        }
    };

    //==============================================================================
    class ShortcutsPage : public juce::Component, private juce::ChangeListener
    {
    public:
        explicit ShortcutsPage (SettingsHost& h) : host (h), editor (*h.getCommands().getKeyMappings(), true)
        {
            label.setText ("Shortcut set", juce::dontSendNotification);
            label.setColour (juce::Label::textColourId, colours::textDim);
            addAndMakeVisible (label);
            profile.addItemList (keys::profileNames(), 1);
            profile.setSelectedItemIndex ((int) host.getKeyProfile(), juce::dontSendNotification);
            profile.onChange = [this] { host.setKeyProfile ((keys::Profile) profile.getSelectedItemIndex()); };
            addAndMakeVisible (profile);
            reset.setButtonText ("Reset this set");
            reset.onClick = [this] { host.resetKeyMappings(); };
            addAndMakeVisible (reset);
            note.setText ("Click a command's key to change it; + adds another key. Changes are saved per set and shown in the menus.",
                          juce::dontSendNotification);
            note.setColour (juce::Label::textColourId, colours::textFaint);
            note.setFont (font (11.5f));
            addAndMakeVisible (note);
            addAndMakeVisible (editor);
            h.getCommands().getKeyMappings()->addChangeListener (this);
        }

        ~ShortcutsPage() override { host.getCommands().getKeyMappings()->removeChangeListener (this); }

        void resized() override
        {
            auto r = getLocalBounds().reduced (12);
            auto top = r.removeFromTop (26);
            label.setBounds (top.removeFromLeft (90));
            profile.setBounds (top.removeFromLeft (240));
            top.removeFromLeft (10);
            reset.setBounds (top.removeFromLeft (120));
            r.removeFromTop (6);
            note.setBounds (r.removeFromTop (18));
            r.removeFromTop (4);
            editor.setBounds (r);
        }

    private:
        SettingsHost& host;
        juce::Label label, note;
        juce::ComboBox profile;
        juce::TextButton reset;
        juce::KeyMappingEditorComponent editor;
        void changeListenerCallback (juce::ChangeBroadcaster*) override { host.saveKeyMappings(); }
    };

    //==============================================================================
    class EditingPage : public juce::Component
    {
    public:
        explicit EditingPage (SettingsHost& h) : host (h)
        {
            auto field = [this] (juce::Label& l, juce::TextEditor& t, const juce::String& name, double value, std::function<void (double)> set)
            {
                l.setText (name, juce::dontSendNotification);
                l.setColour (juce::Label::textColourId, colours::textDim);
                addAndMakeVisible (l);
                t.setFont (mono (12.0f));
                t.setText (juce::String (value, 3));
                t.onReturnKey = t.onFocusLost = [&t, set] { set (t.getText().getDoubleValue()); };
                addAndMakeVisible (t);
            };
            field (preL, pre, "Pre-roll (s)", host.getPreRoll(), [this] (double v) { host.setPreRoll (juce::jlimit (0.0, 30.0, v)); });
            field (postL, post, "Post-roll (s)", host.getPostRoll(), [this] (double v) { host.setPostRoll (juce::jlimit (0.0, 30.0, v)); });

            countL.setText ("Count-in", juce::dontSendNotification);
            countL.setColour (juce::Label::textColourId, colours::textDim);
            addAndMakeVisible (countL);
            countIn.addItemList ({ "Off", "1 bar", "2 bars", "2 beats", "1 beat" }, 1);
            if (auto* e = host.getEdit()) countIn.setSelectedItemIndex ((int) e->getCountInMode(), juce::dontSendNotification);
            countIn.onChange = [this] { if (auto* e = host.getEdit()) e->setCountInMode ((te::Edit::CountIn) countIn.getSelectedItemIndex()); };
            addAndMakeVisible (countIn);

            field (compL, comp, "Comp crossfade (ms)", host.getEdit() ? (double) host.getEdit()->state.getProperty ("tkCompXfadeMs", 4.0) : 4.0,
                   [this] (double v) { if (auto* e = host.getEdit()) e->state.setProperty ("tkCompXfadeMs", juce::jlimit (0.0, 200.0, v), nullptr); });

            note.setText ("Pre/post-roll apply when playing a time selection and when recording with punch. "
                          "Count-in clicks before recording starts.", juce::dontSendNotification);
            note.setFont (font (11.5f));
            note.setColour (juce::Label::textColourId, colours::textFaint);
            note.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (note);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (14);
            auto row = [&r] (juce::Component& a, juce::Component& b) { auto rr = r.removeFromTop (26); a.setBounds (rr.removeFromLeft (170)); b.setBounds (rr.removeFromLeft (140)); r.removeFromTop (6); };
            row (preL, pre);
            row (postL, post);
            row (countL, countIn);
            row (compL, comp);
            r.removeFromTop (8);
            note.setBounds (r.removeFromTop (60));
        }

    private:
        SettingsHost& host;
        juce::Label preL, postL, countL, compL, note;
        juce::TextEditor pre, post, comp;
        juce::ComboBox countIn;
    };

    //==============================================================================
    class SettingsComponent : public juce::TabbedComponent
    {
    public:
        SettingsComponent (SettingsHost& h, int initialTab) : juce::TabbedComponent (juce::TabbedButtonBar::TabsAtTop)
        {
            const auto bg = colours::panel;
            addTab ("Audio", bg, new AudioPage (h), true);
            addTab ("Latency & sync", bg, new LatencyPage (h), true);
            addTab ("Shortcuts", bg, new ShortcutsPage (h), true);
            addTab ("Editing & transport", bg, new EditingPage (h), true);
            setCurrentTabIndex (initialTab);
            setSize (760, 620);
        }
    };
}
