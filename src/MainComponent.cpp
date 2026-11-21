#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    for (auto* b : { &newButton, &openButton, &saveButton, &importButton,
                     &startButton, &playButton, &recordButton, &loopButton,
                     &addTrackButton, &undoButton, &redoButton, &deleteButton,
                     &pluginsButton, &audioButton })
        addAndMakeVisible (*b);

    recordButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::red.darker (0.2f));
    loopButton.setClickingTogglesState (false);
    loopButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3d7bd9));

    newButton.onClick      = [this] { newSession(); };
    openButton.onClick     = [this] { openSession(); };
    saveButton.onClick     = [this] { saveCurrentEdit(); };
    importButton.onClick   = [this] { importAudio(); };
    startButton.onClick    = [this] { if (edit) edit->getTransport().setPosition (0_tp); };
    playButton.onClick     = [this] { togglePlay(); };
    recordButton.onClick   = [this] { toggleRecord(); };
    loopButton.onClick     = [this] { if (edit) { edit->getTransport().looping = ! edit->getTransport().looping.get(); updateButtons(); } };
    addTrackButton.onClick = [this] { addTrack(); };
    undoButton.onClick     = [this] { if (edit) edit->getUndoManager().undo(); };
    redoButton.onClick     = [this] { if (edit) edit->getUndoManager().redo(); };
    deleteButton.onClick   = [this] { deleteSelection(); };
    pluginsButton.onClick  = [this] { showPluginList(); };
    audioButton.onClick    = [this] { EngineHelpers::showAudioDeviceSettings (engine); };

    sessionLabel.setJustificationType (juce::Justification::centred);
    sessionLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    addAndMakeVisible (sessionLabel);
    addAndMakeVisible (statusBar);

    selectionManager.addChangeListener (this);

    // Inputs: enable every audio input the interface exposes. Each track's "I"
    // button picks its source; "A" arms it.
    auto& dm = engine.getDeviceManager();
    for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
        if (auto* wip = dm.getWaveInDevice (i))
        {
            wip->setMonitorMode (te::InputDevice::MonitorMode::automatic);
            wip->setEnabled (true);
        }

    // Reopen the most recent session, or start a fresh one.
    auto folder = getSessionsFolder();
    auto recent = Helpers::findRecentEdit (folder);
    loadEdit (recent.existsAsFile() ? recent
                                    : folder.getNonexistentChildFile ("Untitled", te::editFileSuffix, false));

    commandManager.registerAllCommandsForTarget (this);
    addKeyListener (commandManager.getKeyMappings());
    setWantsKeyboardFocus (true);

    setSize (1400, 850);
}

MainComponent::~MainComponent()
{
    saveCurrentEdit();
    selectionManager.removeChangeListener (this);
    editComponent = nullptr;
    edit = nullptr;
    engine.getTemporaryFileManager().getTempDirectory().deleteRecursively();
}

//==============================================================================
juce::File MainComponent::getSessionsFolder()
{
    auto d = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                .getChildFile ("Tonkori DAW").getChildFile ("Sessions");
    d.createDirectory();
    return d;
}

void MainComponent::saveCurrentEdit()
{
    if (edit != nullptr)
        te::EditFileOperations (*edit).save (true, true, false);
}

void MainComponent::loadEdit (const juce::File& file)
{
    saveCurrentEdit();
    selectionManager.deselectAll();
    statusBar.setEdit (nullptr);
    editComponent = nullptr;

    if (edit != nullptr)
        edit->getTransport().removeChangeListener (this);

    edit = file.existsAsFile() ? te::loadEditFromFile (engine, file)
                               : te::createEmptyEdit (engine, file);

    edit->playInStopEnabled = true;
    edit->getTransport().addChangeListener (this);
    edit->getTransport().ensureContextAllocated();

    if (te::getAudioTracks (*edit).isEmpty())
        edit->ensureNumberOfAudioTracks (2);

    saveCurrentEdit();

    editComponent = std::make_unique<EditComponent> (*edit, selectionManager);
    editComponent->getEditViewState().showMidiDevices = true;
    editComponent->getEditViewState().showWaveDevices = true;
    addAndMakeVisible (*editComponent);

    statusBar.setEdit (edit.get());
    sessionLabel.setText (file.getFileNameWithoutExtension(), juce::dontSendNotification);
    updateButtons();
    resized();
}

void MainComponent::newSession()
{
    chooser = std::make_shared<juce::FileChooser> ("New session", getSessionsFolder(), "*" + juce::String (te::editFileSuffix));
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              f = f.withFileExtension (te::editFileSuffix);
                              f.deleteFile();
                              loadEdit (f);
                          });
}

void MainComponent::openSession()
{
    chooser = std::make_shared<juce::FileChooser> ("Open session", getSessionsFolder(), "*" + juce::String (te::editFileSuffix));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult().existsAsFile())
                                  loadEdit (fc.getResult());
                          });
}

void MainComponent::importAudio()
{
    if (edit == nullptr) return;

    EngineHelpers::browseForAudioFile (engine, [this] (const juce::File& f)
    {
        if (! f.existsAsFile() || edit == nullptr) return;

        // Import onto the selected track if there is one, else a new track, at the playhead.
        te::AudioTrack* target = nullptr;

        if (auto* t = dynamic_cast<te::AudioTrack*> (selectionManager.getSelectedObject (0)))
            target = t;

        if (target == nullptr)
        {
            edit->ensureNumberOfAudioTracks (te::getAudioTracks (*edit).size() + 1);
            target = te::getAudioTracks (*edit).getLast();
        }

        te::AudioFile audioFile (engine, f);
        const auto start = edit->getTransport().getPosition();
        const auto length = te::TimeDuration::fromSeconds (audioFile.getLength());

        if (auto clip = target->insertWaveClip (f.getFileNameWithoutExtension(), f,
                                                { { start, start + length }, {} }, false))
            selectionManager.selectOnly (clip.get());
    });
}

void MainComponent::togglePlay()
{
    if (edit == nullptr) return;
    const bool wasRecording = edit->getTransport().isRecording();
    EngineHelpers::togglePlay (*edit);
    if (wasRecording) saveCurrentEdit();
}

void MainComponent::toggleRecord()
{
    if (edit == nullptr) return;
    const bool wasRecording = edit->getTransport().isRecording();
    EngineHelpers::toggleRecord (*edit);
    if (wasRecording) saveCurrentEdit();
}

void MainComponent::addTrack()
{
    if (edit != nullptr)
        edit->ensureNumberOfAudioTracks (te::getAudioTracks (*edit).size() + 1);
}

void MainComponent::deleteSelection()
{
    if (edit == nullptr) return;

    auto* sel = selectionManager.getSelectedObject (0);

    if (auto* clip = dynamic_cast<te::Clip*> (sel))
        clip->removeFromParent();
    else if (auto* track = dynamic_cast<te::Track*> (sel))
        if (! (track->isMasterTrack() || track->isMarkerTrack() || track->isTempoTrack() || track->isChordTrack()))
            edit->deleteTrack (track);
}

void MainComponent::showPluginList()
{
    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle                  = "Plugins (scan AU / VST3)";
    o.dialogBackgroundColour       = juce::Colour (0xff1c1f23);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar            = true;
    o.resizable                    = true;

    auto* v = new juce::PluginListComponent (engine.getPluginManager().pluginFormatManager,
                                             engine.getPluginManager().knownPluginList,
                                             engine.getTemporaryFileManager().getTempFile ("PluginScanDeadMansPedal"),
                                             std::addressof (engine.getPropertyStorage().getPropertiesFile()),
                                             true);
    v->setSize (900, 600);
    o.content.setOwned (v);
    o.launchAsync();
}

void MainComponent::updateButtons()
{
    if (edit == nullptr) return;
    auto& t = edit->getTransport();
    playButton.setButtonText (t.isPlaying() ? "Stop" : "Play");
    recordButton.setToggleState (t.isRecording(), juce::dontSendNotification);
    loopButton.setToggleState (t.looping.get(), juce::dontSendNotification);

    auto* sel = selectionManager.getSelectedObject (0);
    deleteButton.setEnabled (dynamic_cast<te::Clip*> (sel) != nullptr || dynamic_cast<te::Track*> (sel) != nullptr);
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateButtons();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1f23));
    g.setColour (juce::Colour (0xff24282d));
    g.fillRect (getLocalBounds().removeFromTop (40));
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    statusBar.setBounds (r.removeFromBottom (30));

    auto bar = r.removeFromTop (40).reduced (6, 5);
    auto place = [&bar] (juce::Component& c, int w) { c.setBounds (bar.removeFromLeft (w)); bar.removeFromLeft (4); };
    auto gap   = [&bar] (int w) { bar.removeFromLeft (w); };

    place (newButton, 56); place (openButton, 56); place (saveButton, 56); place (importButton, 104);
    gap (16);
    place (startButton, 40); place (playButton, 64); place (recordButton, 56); place (loopButton, 56);
    gap (16);
    place (addTrackButton, 72); place (deleteButton, 64); place (undoButton, 56); place (redoButton, 56);

    audioButton.setBounds (bar.removeFromRight (64)); bar.removeFromRight (4);
    pluginsButton.setBounds (bar.removeFromRight (72)); bar.removeFromRight (4);
    sessionLabel.setBounds (bar);

    if (editComponent != nullptr)
        editComponent->setBounds (r);
}

//==============================================================================
void MainComponent::getAllCommands (juce::Array<juce::CommandID>& c)
{
    c.addArray ({ cmdPlayStop, cmdRecord, cmdReturnToStart, cmdToggleLoop, cmdNew, cmdOpen, cmdSave,
                  cmdImportAudio, cmdAddTrack, cmdUndo, cmdRedo, cmdDelete });
}

void MainComponent::getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    using KP = juce::KeyPress;
    using MK = juce::ModifierKeys;
    const auto cmd = MK::commandModifier;

    switch (id)
    {
        case cmdPlayStop:      info.setInfo ("Play / Stop", {}, "Transport", 0);   info.addDefaultKeypress (KP::spaceKey, 0); break;
        case cmdRecord:        info.setInfo ("Record", {}, "Transport", 0);        info.addDefaultKeypress ('r', 0); break;
        case cmdReturnToStart: info.setInfo ("Return to start", {}, "Transport", 0); info.addDefaultKeypress (KP::returnKey, 0); break;
        case cmdToggleLoop:    info.setInfo ("Loop", {}, "Transport", 0);          info.addDefaultKeypress ('l', 0); break;
        case cmdNew:           info.setInfo ("New session", {}, "File", 0);        info.addDefaultKeypress ('n', cmd); break;
        case cmdOpen:          info.setInfo ("Open session", {}, "File", 0);       info.addDefaultKeypress ('o', cmd); break;
        case cmdSave:          info.setInfo ("Save", {}, "File", 0);               info.addDefaultKeypress ('s', cmd); break;
        case cmdImportAudio:   info.setInfo ("Import audio", {}, "File", 0);       info.addDefaultKeypress ('i', cmd); break;
        case cmdAddTrack:      info.setInfo ("Add track", {}, "Edit", 0);          info.addDefaultKeypress ('t', cmd); break;
        case cmdUndo:          info.setInfo ("Undo", {}, "Edit", 0);               info.addDefaultKeypress ('z', cmd); break;
        case cmdRedo:          info.setInfo ("Redo", {}, "Edit", 0);               info.addDefaultKeypress ('z', cmd | MK::shiftModifier); break;
        case cmdDelete:        info.setInfo ("Delete", {}, "Edit", 0);
                               info.addDefaultKeypress (KP::deleteKey, 0);
                               info.addDefaultKeypress (KP::backspaceKey, 0); break;
        default: break;
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    switch (info.commandID)
    {
        case cmdPlayStop:      togglePlay(); return true;
        case cmdRecord:        toggleRecord(); return true;
        case cmdReturnToStart: if (edit) edit->getTransport().setPosition (0_tp); return true;
        case cmdToggleLoop:    loopButton.onClick(); return true;
        case cmdNew:           newSession(); return true;
        case cmdOpen:          openSession(); return true;
        case cmdSave:          saveCurrentEdit(); return true;
        case cmdImportAudio:   importAudio(); return true;
        case cmdAddTrack:      addTrack(); return true;
        case cmdUndo:          if (edit) edit->getUndoManager().undo(); return true;
        case cmdRedo:          if (edit) edit->getUndoManager().redo(); return true;
        case cmdDelete:        deleteSelection(); return true;
        default:               return false;
    }
}
