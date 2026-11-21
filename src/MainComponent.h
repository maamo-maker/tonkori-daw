#pragma once

#include <JuceHeader.h>

// Engine example UI (GPL, Tracktion) used as the first arrangement view.
// It will be replaced piece by piece by Tonkori's own views.
#include "Utilities.h"
#include "Components.h"
#include "PluginWindow.h"
#include "StatusBar.h"

using namespace tracktion::literals;

//==============================================================================
/** Milestone 0, the walking skeleton: one window with an arrangement,
    transport, recording, file import, plugin hosting (AU + VST3),
    audio device settings and session save/load. */
class MainComponent : public juce::Component,
                      public juce::ApplicationCommandTarget,
                      private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void saveCurrentEdit();

    // Keyboard commands
    enum CommandIDs
    {
        cmdPlayStop = 0x2000, cmdRecord, cmdReturnToStart, cmdToggleLoop,
        cmdNew, cmdOpen, cmdSave, cmdImportAudio, cmdAddTrack,
        cmdUndo, cmdRedo, cmdDelete
    };

    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands (juce::Array<juce::CommandID>&) override;
    void getCommandInfo (juce::CommandID, juce::ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;

private:
    te::Engine engine { ProjectInfo::projectName, std::make_unique<ExtendedUIBehaviour>(), nullptr };
    te::SelectionManager selectionManager { engine };
    std::unique_ptr<te::Edit> edit;
    std::unique_ptr<EditComponent> editComponent;
    juce::ApplicationCommandManager commandManager;

    juce::TextButton newButton { "New" }, openButton { "Open" }, saveButton { "Save" },
                     importButton { "Import Audio" },
                     startButton { "|<" }, playButton { "Play" }, recordButton { "Rec" },
                     loopButton { "Loop" },
                     addTrackButton { "+ Track" }, undoButton { "Undo" }, redoButton { "Redo" },
                     deleteButton { "Delete" },
                     pluginsButton { "Plugins" }, audioButton { "Audio" };
    juce::Label sessionLabel;
    StatusBar statusBar { engine };

    std::shared_ptr<juce::FileChooser> chooser;

    static juce::File getSessionsFolder();
    void loadEdit (const juce::File&);
    void newSession();
    void openSession();
    void importAudio();
    void togglePlay();
    void toggleRecord();
    void addTrack();
    void deleteSelection();
    void showPluginList();
    void assignInputsToArmedTracks();
    void updateButtons();

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
