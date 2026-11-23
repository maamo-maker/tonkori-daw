#pragma once

#include <JuceHeader.h>

// Tracktion example helpers (GPL): plugin windows and device dialogs.
#include "Utilities.h"
#include "PluginWindow.h"

#include "EngineProfile.h"
#include "TaskRunner.h"
#include "StatusBar.h"
#include "dsd/DsdDecoder.h"
#include "model/ClipFades.h"
#include "ui/Theme.h"
#include "ui/ViewState.h"
#include "ui/Toolbar.h"
#include "ui/ArrangeView.h"
#include "ui/Inspector.h"
#include "ui/Mixer.h"
#include "ui/FadeEditor.h"

using namespace tracktion::literals;

//==============================================================================
/** Commands, keyed to Ableton Live 12's default Mac shortcuts (Live manual ch. 42).
    Tools use Cubase's number keys, edit modes Opt+1–4; see docs/SHORTCUTS.md. */
namespace Cmd
{
    enum : juce::CommandID
    {
        // File
        newSet = 0x2000, openSet, save, saveAs, exportAudio, importAudio, settings, pluginList, fullScreen,
        // Edit
        undo, redo, cut, copy, paste, duplicate, deleteSel, selectAll, clearSelection, rename,
        splitClip, insertSilence, deleteTime, createFades, deleteFades, reverseClip,
        nudgeLeft, nudgeRight, deactivate, fadeEditor, toggleClipFades, toggleAutoFade, spotClip,
        // Tools / modes / grid
        toolSmart, toolRange, toolGrab, toolTrim, toolFade, toolSplit,
        modeSlip, modeGrid, modeShuffle, modeSpot,
        gridNarrow, gridWiden, gridTriplet, snapToggle,
        // Loop / view
        loopSelection, selectLoopContents, setLoopStart, setLoopEnd, punch,
        zoomIn, zoomOut, fitWidth, zoomToSelection, zoomBack, follow, toggleMixer, toggleInspector,
        // Tracks
        insertAudioTrack, insertMidiTrack, armSelected, soloSelected,
        toggleTrack1, toggleTrack2, toggleTrack3, toggleTrack4,
        toggleTrack5, toggleTrack6, toggleTrack7, toggleTrack8,
        // Transport
        playStop, continuePlay, record, insertMarkerToStart, insertMarkerToPlayhead,
        metronome, engineOnOff
    };
}

//==============================================================================
/** Example plugin-window behaviour plus the job runner Tracktion needs for renders. */
class TonkoriUIBehaviour : public ExtendedUIBehaviour
{
public:
    void runTaskWithProgressBar (te::ThreadPoolJobWithProgress& job) override
    {
        tonkori::runJobWithProgressWindow (job);
    }
};

//==============================================================================
class MainComponent : public juce::Component,
                      public juce::ApplicationCommandTarget,
                      public juce::MenuBarModel,
                      private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

    void saveCurrentEdit();
    /** Imports files (Finder drop or launch arguments) at a time/track, one track per file when stacking. */
    void importFiles (const juce::StringArray&, std::optional<te::TimePosition> at = {}, te::AudioTrack* track = nullptr);
    juce::ApplicationCommandManager& getCommandManager() { return commandManager; }
    te::Edit* getEditForTesting() const { return edit.get(); }

    // ApplicationCommandTarget
    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands (juce::Array<juce::CommandID>&) override;
    void getCommandInfo (juce::CommandID, juce::ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int, const juce::String&) override;
    void menuItemSelected (int, int) override {}

private:
    tonkori::ui::LookAndFeel lookAndFeel;
    te::Engine engine { ProjectInfo::projectName, std::make_unique<TonkoriUIBehaviour>(),
                        std::make_unique<tonkori::EngineBehaviour>() };
    te::SelectionManager selectionManager { engine };
    std::unique_ptr<te::Edit> edit;
    std::unique_ptr<te::EditInsertPoint> insertPoint;
    juce::ApplicationCommandManager commandManager;
    juce::TooltipWindow tooltips { this, 600 };

    // The Edit window (rebuilt per session)
    std::unique_ptr<tonkori::ui::ViewState> view;
    std::unique_ptr<tonkori::ui::Toolbar> toolbar;
    std::unique_ptr<tonkori::ui::ArrangeView> arrange;
    std::unique_ptr<tonkori::ui::Inspector> inspector;
    std::unique_ptr<tonkori::ui::Mixer> mixer;
    StatusBar statusBar { engine };

    // Live transport model: Space plays from the insert marker and returns to it on stop;
    // Shift+Space continues from where playback stopped.
    te::TimePosition stopPosition;
    std::optional<std::pair<double, double>> previousView;

    std::shared_ptr<juce::FileChooser> chooser;

    // Session
    static juce::File getSessionsFolder();
    juce::File getSessionAudioFolder() const;
    void loadEdit (const juce::File&);
    void buildEditWindow();
    void newSession();
    void openSession();
    void saveSessionAs();
    void exportAudio();
    void importAudio();
    void importFile (const juce::File&, std::optional<te::TimePosition> at = {}, te::AudioTrack* track = nullptr);
    void showPluginList();

    // Transport (Live semantics)
    te::TimePosition insertMarker() const;
    void setInsertMarker (te::TimePosition);
    void playStop();
    void continuePlay();
    void toggleRecord();
    void toggleMetronome();
    void toggleEngine();

    // Editing
    juce::Array<te::Clip*> getSelectedClips() const;
    juce::Array<te::AudioClipBase*> getSelectedAudioClips() const;
    juce::Array<te::Track*> getSelectedTracks() const;
    juce::Array<te::Track*> getRangeTracks() const;
    te::AudioTrack* getTargetTrack() const;
    te::TimePosition cursor() const;
    std::optional<te::TimeRange> clipSelectionRange() const;
    te::TimeDuration sixteenthAt (te::TimePosition) const;

    void copySelection (bool alsoDelete);
    void pasteAtCursor();
    void duplicateSelection();
    void deleteSelection();
    void selectAllClips();
    void clearSelection();
    void renameSelection();
    void splitAtCursor();
    void insertSilence();
    void deleteTime();
    void createFades();
    void deleteFades();
    void reverseSelectedClips();
    void nudge (int direction);
    void deactivateSelection();
    void loopSelection();
    void ensureLoopRange();
    void selectLoopContents();
    void insertTrack (bool midi);
    void armSelectedTracks();
    void soloSelectedTracks();
    void toggleTrackActive (int index);
    void openFadeEditor (te::AudioClipBase&, bool fadeIn, juce::Rectangle<int> screenArea);
    void spotClip (te::Clip&);

    // View
    void zoom (double factor);
    void fitWidth();
    void zoomToSelection();
    void zoomBack();

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
