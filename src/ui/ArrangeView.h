#pragma once

#include <JuceHeader.h>
#include "ViewState.h"

namespace tonkori::ui
{
    class Ruler;
    class TrackHeaderList;
    class ArrangeCanvas;

    /** The Edit window's arrangement: ruler, track headers, clip canvas and scrollbars.
        Everything is editable while playing or recording. */
    class ArrangeView : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        private juce::ChangeListener,
                        private juce::ValueTree::Listener,
                        private juce::AsyncUpdater,
                        private juce::Timer,
                        private juce::ScrollBar::Listener
    {
    public:
        explicit ArrangeView (ViewState&);
        ~ArrangeView() override;

        void resized() override;
        void paint (juce::Graphics&) override;

        /** Fired on double-click/Cmd+F over a fade, or from the clip menu. */
        std::function<void (te::AudioClipBase&, bool isFadeIn, juce::Rectangle<int> screenArea)> onOpenFadeEditor;
        /** Spot mode: ask for an exact clip start. */
        std::function<void (te::Clip&)> onSpotClip;

        /** Files dragged in from Finder: dropped at the pointer's time and track. */
        std::function<void (const juce::StringArray&, te::TimePosition, te::AudioTrack*)> onFilesDropped;
        bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
        void filesDropped (const juce::StringArray&, int x, int y) override;

        void fitToSession();
        juce::Rectangle<int> getScreenBoundsOfClip (te::Clip&) const;

        static constexpr int headerWidth = 236;
        static constexpr int rulerHeight = 38;

    private:
        ViewState& vs;
        std::unique_ptr<Ruler> ruler;
        std::unique_ptr<TrackHeaderList> headers;
        std::unique_ptr<ArrangeCanvas> canvas;
        juce::ScrollBar hScroll { false }, vScroll { true };
        juce::Label cornerLabel;
        int idleTicks = 0;

        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override        { triggerAsyncUpdate(); }
        void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { triggerAsyncUpdate(); }
        void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { triggerAsyncUpdate(); }
        void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override         { triggerAsyncUpdate(); }
        void handleAsyncUpdate() override;
        void timerCallback() override;
        void scrollBarMoved (juce::ScrollBar*, double) override;
        void updateScrollBars();

        friend class ArrangeCanvas;
        friend class TrackHeaderList;
        friend class Ruler;
    };
}
