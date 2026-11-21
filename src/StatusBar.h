#pragma once

#include <JuceHeader.h>
#include "EngineProfile.h"

namespace te = tracktion;

//==============================================================================
/** Bottom strip: timecode, bars|beats, tempo | device, rate, buffer, latency |
    engine profile, xruns, CPU. Polls at 30 Hz from the message thread. */
class StatusBar : public juce::Component,
                  private juce::Timer
{
public:
    explicit StatusBar (te::Engine& e) : engine (e)
    {
        for (auto* l : { &timeLabel, &deviceLabel, &cpuLabel })
        {
            l->setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
            l->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
            l->setInterceptsMouseClicks (false, false);
            addAndMakeVisible (*l);
        }

        timeLabel.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 17.0f, juce::Font::bold));
        cpuLabel.setJustificationType (juce::Justification::centredRight);
        startTimerHz (30);
    }

    void setEdit (te::Edit* e) { edit = e; }

    /** Shows a short confirmation in place of the device line for ~3 s. */
    void flashMessage (const juce::String& m)
    {
        message = m;
        messageUntil = juce::Time::getMillisecondCounter() + 3000;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff15171a));
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 2);
        timeLabel.setBounds (r.removeFromLeft (400));
        cpuLabel.setBounds (r.removeFromRight (470));
        deviceLabel.setBounds (r);
    }

private:
    te::Engine& engine;
    te::Edit* edit = nullptr;
    juce::Label timeLabel, deviceLabel, cpuLabel;
    juce::String message;
    juce::uint32 messageUntil = 0;

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
                                 + juce::String::formatted ("  %d|%d|%03d  ", bb.bars + 1, beat, tick)
                                 + juce::String (bpm, 2) + " bpm" + state,
                               juce::dontSendNotification);
        }

        auto* dev = dm.deviceManager.getCurrentAudioDevice();

        if (juce::Time::getMillisecondCounter() < messageUntil)
        {
            deviceLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8fd18f));
            deviceLabel.setText (message, juce::dontSendNotification);
        }
        else
        {
            deviceLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));

            if (dev == nullptr)
            {
                deviceLabel.setText ("Audio engine off / no device", juce::dontSendNotification);
            }
            else
            {
                const auto sr = dev->getCurrentSampleRate();
                const auto lat = (double) (dev->getOutputLatencyInSamples() + dev->getCurrentBufferSizeSamples()) / sr;
                deviceLabel.setText (dev->getName() + "  " + juce::String (sr / 1000.0, 1) + " kHz  "
                                       + juce::String (dev->getCurrentBufferSizeSamples()) + " smp  out "
                                       + juce::String (lat * 1000.0, 1) + " ms",
                                     juce::dontSendNotification);
            }
        }

        const int xruns = dev != nullptr ? dev->getXRunCount() : 0;
        cpuLabel.setColour (juce::Label::textColourId, xruns > 0 ? juce::Colour (0xffe0a040) : juce::Colours::white.withAlpha (0.85f));
        cpuLabel.setText (tonkori::EngineProfile::describe (engine)
                            + "  xruns " + juce::String (xruns)
                            + "  CPU " + juce::String (juce::roundToInt (dm.getCpuUsage() * 100.0f)) + "%",
                          juce::dontSendNotification);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StatusBar)
};
