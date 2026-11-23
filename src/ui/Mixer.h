#pragma once

#include <JuceHeader.h>
#include "ViewState.h"

namespace tonkori::ui
{
    class ChannelStrip;

    /** Bottom mixer: a strip per audio track (inserts, pan, fader, meter, M/S/R) and the master. */
    class Mixer : public juce::Component, private juce::Timer
    {
    public:
        explicit Mixer (ViewState&);
        ~Mixer() override;

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        ViewState& vs;
        juce::Viewport viewport;
        juce::TextButton addBus;
        juce::Component strips;
        juce::OwnedArray<ChannelStrip> channelStrips;
        std::unique_ptr<ChannelStrip> master;
        juce::Array<te::AudioTrack*> shown;

        void timerCallback() override;
        void rebuild();
    };
}
