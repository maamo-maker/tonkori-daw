#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "../engine/Loudness.h"

namespace te = tracktion;

namespace tonkori::ui
{
    /*  Taps the final device output (after the master bus) on the audio thread: K-weighting
        and true peak are computed there without allocating; every 100 ms sub-block goes
        through a lock-free FIFO to the meter, which does the gating on the message thread. */
    class OutputTap : public juce::AudioProcessor
    {
    public:
        OutputTap() : juce::AudioProcessor (BusesProperties()) {}

        struct Block { double meanSquare; float truePeak; };

        std::atomic<int> firstChannel { 0 }, numChannels { 2 };

        bool pop (Block& b)
        {
            int s1, n1, s2, n2;
            fifo.prepareToRead (1, s1, n1, s2, n2);
            if (n1 == 0) return false;
            b = blocks[(size_t) s1];
            fifo.finishedRead (1);
            return true;
        }

        void prepareToPlay (double sampleRate, int) override
        {
            rate = sampleRate;
            core.prepare (sampleRate, numChannels.load());
            preparedChannels = numChannels.load();
        }

        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
        {
            const int first = firstChannel.load(), nch = numChannels.load();
            if (nch != preparedChannels) { core.prepare (rate, nch); preparedChannels = nch; }
            const float* chans[tonkori::loudness::LoudnessCore::maxChannels] = {};
            int used = 0;
            for (int c = 0; c < nch && first + c < buffer.getNumChannels() && used < tonkori::loudness::LoudnessCore::maxChannels; ++c)
                chans[used++] = buffer.getReadPointer (first + c);
            if (used == 0) return;
            core.process (chans, used, buffer.getNumSamples(), [this] (double ms, float tp)
            {
                int s1, n1, s2, n2;
                fifo.prepareToWrite (1, s1, n1, s2, n2);
                if (n1 > 0) { blocks[(size_t) s1] = { ms, tp }; fifo.finishedWrite (1); }
            });
        }

        const juce::String getName() const override                     { return "Tonkori output tap"; }
        void releaseResources() override                                 {}
        double getTailLengthSeconds() const override                     { return 0; }
        bool acceptsMidi() const override                                { return false; }
        bool producesMidi() const override                               { return false; }
        juce::AudioProcessorEditor* createEditor() override              { return nullptr; }
        bool hasEditor() const override                                  { return false; }
        int getNumPrograms() override                                    { return 1; }
        int getCurrentProgram() override                                 { return 0; }
        void setCurrentProgram (int) override                            {}
        const juce::String getProgramName (int) override                 { return {}; }
        void changeProgramName (int, const juce::String&) override       {}
        void getStateInformation (juce::MemoryBlock&) override           {}
        void setStateInformation (const void*, int) override             {}

    private:
        tonkori::loudness::LoudnessCore core;
        double rate = 48000;
        int preparedChannels = 2;
        juce::AbstractFifo fifo { 256 };
        std::array<Block, 256> blocks {};
    };

    //==============================================================================
    /** One measurement shared by every loudness display. */
    class LoudnessHub : public juce::ChangeBroadcaster, private juce::Timer
    {
    public:
        LoudnessHub() { startTimerHz (10); }
        void setTap (OutputTap* t)      { tap = t; }
        void setEdit (te::Edit* e)      { edit = e; stats.reset(); }
        tonkori::loudness::LoudnessStats stats;

    private:
        OutputTap* tap = nullptr;
        te::Edit* edit = nullptr;

        void timerCallback() override
        {
            if (tap == nullptr) return;
            OutputTap::Block b;
            const bool playing = edit != nullptr && edit->getTransport().isPlaying();
            bool any = false;
            while (tap->pop (b))
                if (playing) { stats.addSubBlock (b.meanSquare, b.truePeak); any = true; }
            if (any) sendChangeMessage();
        }
    };

    //==============================================================================
    /** Loudness readout: momentary, short-term, integrated, range and true peak, with a
        target (EBU R128 −23, ATSC A/85 −24, streaming −14 / −16). Integrated and range
        accumulate while the transport runs; Reset starts a new measurement. */
    class LoudnessMeter : public juce::Component, private juce::ChangeListener
    {
    public:
        LoudnessMeter (LoudnessHub& h, bool compactView) : hub (h), stats (h.stats), compact (compactView)
        {
            hub.addChangeListener (this);
            if (! compact)
            {
                target.addItemList ({ "EBU R128  -23 LUFS", "ATSC A/85  -24 LKFS", "Apple Music  -16 LUFS", "Spotify / YouTube  -14 LUFS", "Netflix dialogue  -27 LUFS" }, 1);
                target.setSelectedItemIndex (0, juce::dontSendNotification);
                target.onChange = [this] { repaint(); };
                addAndMakeVisible (target);
                reset.onClick = [this] { stats.reset(); repaint(); };
                addAndMakeVisible (reset);
            }
            setInterceptsMouseClicks (! compact, true);
        }

        ~LoudnessMeter() override { hub.removeChangeListener (this); }

        double targetLufs() const
        {
            static const double t[] = { -23.0, -24.0, -16.0, -14.0, -27.0 };
            return t[juce::jlimit (0, 4, target.getSelectedItemIndex())];
        }

        void mouseDoubleClick (const juce::MouseEvent&) override { stats.reset(); }

        void paint (juce::Graphics& g) override
        {
            auto fmt = [] (double v) { return std::isfinite (v) && v > -70 ? juce::String (v, 1) : juce::String ("--"); };
            auto r = getLocalBounds().reduced (compact ? 2 : 10);

            if (compact)
            {
                g.setColour (colours::bg);
                g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
                g.setFont (mono (10.0f));
                auto line = [&] (const juce::String& k, const juce::String& v, juce::Colour c)
                {
                    auto lr = r.removeFromTop (13);
                    g.setColour (colours::textFaint); g.drawText (k, lr.removeFromLeft (14), juce::Justification::left);
                    g.setColour (c); g.drawText (v, lr, juce::Justification::right);
                };
                line ("M", fmt (stats.momentary()), colours::text);
                line ("S", fmt (stats.shortTerm()), colours::text);
                line ("I", fmt (stats.integrated()), colours::accent);
                const double tp = stats.truePeakDb();
                line ("TP", fmt (tp), tp > -1.0 ? colours::record : colours::text);
                return;
            }

            g.fillAll (colours::panel);
            auto top = r.removeFromTop (24);
            reset.setBounds (top.removeFromRight (64));
            top.removeFromRight (6);
            target.setBounds (top.removeFromRight (190));
            g.setColour (colours::text);
            g.setFont (font (13.0f, true));
            g.drawText ("LOUDNESS  (BS.1770-4)", top, juce::Justification::left);
            r.removeFromTop (8);

            const double integ = stats.integrated();
            auto big = r.removeFromTop (70);
            g.setFont (mono (38.0f));
            const double delta = std::isfinite (integ) ? integ - targetLufs() : 0.0;
            g.setColour (! std::isfinite (integ) ? colours::textFaint : (std::abs (delta) <= 1.0 ? colours::play : colours::warn));
            g.drawText (fmt (integ), big.removeFromLeft (big.getWidth() / 2), juce::Justification::centredLeft);
            g.setFont (font (12.0f));
            g.setColour (colours::textDim);
            g.drawText ("LUFS integrated" + (std::isfinite (integ) ? "   " + juce::String (delta >= 0 ? "+" : "") + juce::String (delta, 1) + " LU to target" : juce::String()),
                        big, juce::Justification::centredLeft);

            auto cell = [&] (juce::Rectangle<int> c, const juce::String& k, const juce::String& v, juce::Colour col)
            {
                g.setColour (colours::bg); g.fillRoundedRectangle (c.toFloat().reduced (2), 3);
                g.setColour (colours::textFaint); g.setFont (font (10.5f));
                g.drawText (k, c.reduced (8, 4).removeFromTop (12), juce::Justification::left);
                g.setColour (col); g.setFont (mono (17.0f));
                g.drawText (v, c.reduced (8, 4).withTrimmedTop (12), juce::Justification::left);
            };
            auto row = r.removeFromTop (52);
            const int cw = row.getWidth() / 5;
            cell (row.removeFromLeft (cw), "Momentary", fmt (stats.momentary()), colours::text);
            cell (row.removeFromLeft (cw), "Short-term", fmt (stats.shortTerm()), colours::text);
            cell (row.removeFromLeft (cw), "Max S", fmt (stats.maxShortTerm()), colours::text);
            cell (row.removeFromLeft (cw), "Range (LRA)", juce::String (stats.loudnessRange(), 1) + " LU", colours::text);
            const double tp = stats.truePeakDb();
            cell (row, "True peak max", fmt (tp) + " dBTP", tp > -1.0 ? colours::record : colours::text);

            r.removeFromTop (8);
            g.setColour (colours::textFaint);
            g.setFont (font (11.0f));
            g.drawText ("Integrated and range accumulate while playing. Double-click or Reset to start over. "
                        + juce::String (stats.seconds(), 0) + " s measured.", r.removeFromTop (16), juce::Justification::left);
        }

        void resized() override { repaint(); }

    private:
        LoudnessHub& hub;
        tonkori::loudness::LoudnessStats& stats;
        bool compact;
        juce::ComboBox target;
        juce::TextButton reset { "Reset" };

        void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    };
}
