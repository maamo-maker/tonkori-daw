#pragma once

#include <JuceHeader.h>

// Engine example UI (GPL, Tracktion) used as the first arrangement view.
// It will be replaced piece by piece by Tonkori's own views (M1).
#include "Utilities.h"
#include "Components.h"
#include "PluginWindow.h"

#include "EngineProfile.h"
#include "TaskRunner.h"
#include "StatusBar.h"
#include "dsd/DsdDecoder.h"

using namespace tracktion::literals;

//==============================================================================
/** Commands, keyed to Ableton Live 12's default Mac shortcuts (Live manual ch. 42).
    Where Live has no equivalent, the shortcut avoids Live's assignments. */
namespace Cmd
{
    enum : juce::CommandID
    {
        // File
        newSet = 0x2000, openSet, save, saveAs, exportAudio, importAudio, settings, pluginList, fullScreen,
        // Edit
        undo, redo, cut, copy, paste, duplicate, deleteSel, selectAll, rename,
        splitClip, insertSilence, deleteTime, createFades, deleteFades, reverseClip,
        nudgeLeft, nudgeRight, deactivate,
        // Loop / view
        loopSelection, selectLoopContents, setLoopStart, setLoopEnd,
        zoomIn, zoomOut, fitWidth, zoomToSelection, zoomBack,
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
    juce::ApplicationCommandManager& getCommandManager() { return commandManager; }

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
    te::Engine engine { ProjectInfo::projectName, std::make_unique<TonkoriUIBehaviour>(),
                        std::make_unique<tonkori::EngineBehaviour>() };
    te::SelectionManager selectionManager { engine };
    std::unique_ptr<te::Edit> edit;
    std::unique_ptr<te::EditInsertPoint> insertPoint;
    std::unique_ptr<EditComponent> editComponent;
    juce::ApplicationCommandManager commandManager;

    // Live transport model: Space plays from the insert marker and returns to it on stop;
    // Shift+Space continues from where playback stopped.
    te::TimePosition insertMarker, stopPosition;
    std::optional<std::pair<te::TimePosition, te::TimePosition>> previousView;

    juce::TextButton newButton { "New" }, openButton { "Open" }, saveButton { "Save" },
                     importButton { "Import" },
                     startButton { "|<" }, playButton { "Play" }, recordButton { "Rec" },
                     loopButton { "Loop" }, metroButton { "Click" },
                     addTrackButton { "+ Track" }, undoButton { "Undo" }, redoButton { "Redo" },
                     deleteButton { "Delete" }, exportButton { "Export" },
                     pluginsButton { "Plugins" }, audioButton { "Audio" };
    juce::Label sessionLabel;
    StatusBar statusBar { engine };

    std::shared_ptr<juce::FileChooser> chooser;

    // Session
    static juce::File getSessionsFolder();
    juce::File getSessionAudioFolder() const;
    void loadEdit (const juce::File&);
    void newSession();
    void openSession();
    void saveSessionAs();
    void exportAudio();
    void importAudio();
    void importFile (const juce::File&);
    void showPluginList();

    // Transport (Live semantics)
    void playStop();
    void continuePlay();
    void toggleRecord();
    void toggleMetronome();
    void toggleEngine();

    // Editing
    juce::Array<te::Clip*> getSelectedClips() const;
    juce::Array<te::Track*> getSelectedTracks() const;
    te::AudioTrack* getTargetTrack() const;
    te::TimePosition cursor() const;
    std::optional<te::TimeRange> clipSelectionRange() const;
    te::TimeDuration sixteenthAt (te::TimePosition) const;

    void copySelection (bool alsoDelete);
    void pasteAtCursor();
    void duplicateSelection();
    void deleteSelection();
    void selectAllClips();
    void renameSelection();
    void splitAtCursor();
    void insertSilence();
    void deleteTime();
    void setFades (bool create);
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

    // View
    void zoom (double factor);
    void fitWidth();
    void zoomToSelection();
    void zoomBack();

    void updateButtons();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
