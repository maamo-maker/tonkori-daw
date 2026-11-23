#pragma once

#include <JuceHeader.h>
#include "ViewState.h"

namespace tonkori::ui
{
    /** Right-hand panel. Shows what is selected and lets every value be typed:
        clip (position, gain, fades), track (level, pan, inserts) or session (tempo,
        meter, time display, auto-fades). Edits apply live. */
    class Inspector : public juce::Component,
                      private juce::ChangeListener,
                      private juce::Timer
    {
    public:
        explicit Inspector (ViewState&);
        ~Inspector() override;

        void resized() override;
        void paint (juce::Graphics&) override;

        /** Brings the fade editor for the selected clip into view. */
        void focusFades (bool fadeIn);

    private:
        ViewState& vs;
        juce::Viewport viewport;
        std::unique_ptr<juce::Component> content;
        enum class Mode { none, clip, track, session } mode = Mode::none;
        juce::Array<te::EditItemID> shownIDs;

        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void timerCallback() override;
        void rebuild (bool force);
    };
}
