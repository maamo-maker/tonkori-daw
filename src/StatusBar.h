#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

//==============================================================================
/** Bottom strip: timecode, bars|beats, tempo, device format, latency, CPU.
    Polls at 30 Hz from the message thread; never touches the audio thread. */
class StatusBar : public juce::Component,
                  private juce::Timer
{
public:
    explicit StatusBar (te::Engine& e) : engine (e)
    {
        for (auto* l : { &timeLabel, &deviceLabel, &cpuLabel })
        {
            l->setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));
            l->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
            addAndMakeVisible (*l);
        }

        timeLabel.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::bold));
        cpuLabel.setJustificationType (juce::Justification::centredRight);
        startTimerHz (30);
    }

    void setEdit (te::Edit* e) { edit = e; }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff15171a));
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 2);
        timeLabel.setBounds (r.removeFromLeft (420));
        cpuLabel.setBounds (r.removeFromRight (160));
        deviceLabel.setBounds (r);
    }

private:
    te::Engine& engine;
    te::Edit* edit = nullptr;
    juce::Label timeLabel, deviceLabel, cpuLabel;

    static juce::String timecode (double seconds)
    {
        const auto ms = juce::roundToInt (seconds * 1000.0);
        const auto a  = std::abs (ms);
        return juce::String::formatted ("%s%02d:%02d:%02d.%03d", ms < 0 ? "-" : "",
                                        a / 3600000, (a / 60000) % 60, (a / 1000) % 60, a % 1000);
    }

    void timerCallback() override
    {
        auto& dm = engine.getDeviceManager();

        if (edit != nullptr)
        {
            auto& transport = edit->getTransport();
            const auto pos  = transport.getPosition();
            const auto bb   = edit->tempoSequence.toBarsAndBeats (pos);
            const auto bpm  = edit->tempoSequence.getBpmAt (pos);
            const auto beat = bb.getWholeBeats() + 1;
            const auto tick = juce::roundToInt (bb.getFractionalBeats().inBeats() * 960.0) % 960;

            juce::String state = transport.isRecording() ? "  REC" : (transport.isPlaying() ? "  PLAY" : "");
            timeLabel.setText (timecode (pos.inSeconds())
                                 + juce::String::formatted ("   %d|%d|%03d   ", bb.bars + 1, beat, tick)
                                 + juce::String (bpm, 2) + " bpm" + state,
                               juce::dontSendNotification);
        }

        const auto sr = dm.getSampleRate();
        juce::String devName = "No audio device";

        if (auto* dev = dm.deviceManager.getCurrentAudioDevice())
            devName = dev->getName();

        deviceLabel.setText (devName + "   " + juce::String (sr / 1000.0, 1) + " kHz   "
                               + juce::String (dm.getBlockSize()) + " smp   out latency "
                               + juce::String (dm.getOutputLatencySeconds() * 1000.0, 1) + " ms",
                             juce::dontSendNotification);

        cpuLabel.setText ("CPU " + juce::String (juce::roundToInt (dm.getCpuUsage() * 100.0f)) + "%",
                          juce::dontSendNotification);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StatusBar)
};
