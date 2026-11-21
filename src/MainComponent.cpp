#include "MainComponent.h"

using KP = juce::KeyPress;
using MK = juce::ModifierKeys;

namespace
{
    constexpr int cmdKey   = MK::commandModifier;   // ⌘ on Mac
    constexpr int shiftKey = MK::shiftModifier;
    constexpr int altKey   = MK::altModifier;       // ⌥ on Mac
    constexpr int ctrlKey  = MK::ctrlModifier;      // ⌃ on Mac

    struct CommandDef
    {
        juce::CommandID id;
        const char* name;
        const char* category;
        std::vector<KP> keys;
    };

    // Ableton Live 12 default Mac shortcuts (Live manual, "Live Keyboard Shortcuts").
    // Entries marked "Tonkori" have no Live default and use keys Live leaves free.
    const std::vector<CommandDef>& commandTable()
    {
        static const std::vector<CommandDef> t =
        {
            // File ---------------------------------------------------------------------
            { Cmd::newSet,       "New Session",              "File", { KP ('n', cmdKey, 0) } },
            { Cmd::openSet,      "Open Session...",          "File", { KP ('o', cmdKey, 0) } },
            { Cmd::save,         "Save Session",             "File", { KP ('s', cmdKey, 0) } },
            { Cmd::saveAs,       "Save Session As...",       "File", { KP ('s', cmdKey | shiftKey, 0) } },
            { Cmd::importAudio,  "Import Audio / DSD...",    "File", { KP ('o', cmdKey | shiftKey, 0) } },            // Tonkori
            { Cmd::exportAudio,  "Export Audio...",          "File", { KP ('r', cmdKey | shiftKey, 0) } },
            { Cmd::settings,     "Audio Settings...",        "File", { KP (',', cmdKey, 0) } },
            { Cmd::pluginList,   "Plug-in Manager...",       "File", {} },
            { Cmd::fullScreen,   "Toggle Full Screen",       "View", { KP ('f', cmdKey | ctrlKey, 0) } },

            // Edit ---------------------------------------------------------------------
            { Cmd::undo,         "Undo",                     "Edit", { KP ('z', cmdKey, 0) } },
            { Cmd::redo,         "Redo",                     "Edit", { KP ('z', cmdKey | shiftKey, 0) } },
            { Cmd::cut,          "Cut",                      "Edit", { KP ('x', cmdKey, 0) } },
            { Cmd::copy,         "Copy",                     "Edit", { KP ('c', cmdKey, 0) } },
            { Cmd::paste,        "Paste",                    "Edit", { KP ('v', cmdKey, 0) } },
            { Cmd::duplicate,    "Duplicate",                "Edit", { KP ('d', cmdKey, 0) } },
            { Cmd::deleteSel,    "Delete",                   "Edit", { KP (KP::backspaceKey, 0, 0), KP (KP::deleteKey, 0, 0) } },
            { Cmd::selectAll,    "Select All",               "Edit", { KP ('a', cmdKey, 0) } },
            { Cmd::rename,       "Rename",                   "Edit", { KP ('r', cmdKey, 0) } },
            { Cmd::splitClip,    "Split Clip at Insert Marker", "Edit", { KP ('e', cmdKey, 0) } },
            { Cmd::insertSilence,"Insert Silence",           "Edit", { KP ('i', cmdKey, 0) } },
            { Cmd::deleteTime,   "Delete Time (loop range)", "Edit", { KP (KP::backspaceKey, cmdKey | shiftKey, 0), KP (KP::deleteKey, cmdKey | shiftKey, 0) } },
            { Cmd::createFades,  "Create Fades",             "Edit", { KP ('f', cmdKey | altKey, 0) } },
            { Cmd::deleteFades,  "Delete Fades",             "Edit", { KP (KP::backspaceKey, cmdKey | altKey, 0), KP (KP::deleteKey, cmdKey | altKey, 0) } },
            { Cmd::reverseClip,  "Reverse Audio Clip",       "Edit", { KP ('r', 0, 0) } },
            { Cmd::nudgeLeft,    "Nudge Left",               "Edit", { KP (KP::leftKey, 0, 0) } },
            { Cmd::nudgeRight,   "Nudge Right",              "Edit", { KP (KP::rightKey, 0, 0) } },
            { Cmd::deactivate,   "Deactivate Selection",     "Edit", { KP ('0', 0, 0) } },

            // Loop / View ---------------------------------------------------------------
            { Cmd::loopSelection,      "Loop Selection",          "View", { KP ('l', cmdKey, 0) } },
            { Cmd::selectLoopContents, "Select Loop Contents",    "View", { KP ('l', cmdKey | shiftKey, 0) } },
            { Cmd::setLoopStart,       "Set Loop Start",          "View", { KP (KP::F10Key, cmdKey, 0) } },
            { Cmd::setLoopEnd,         "Set Loop End",            "View", { KP (KP::F11Key, cmdKey, 0) } },
            { Cmd::zoomIn,             "Zoom In",                 "View", { KP ('+', 0, 0), KP ('=', 0, 0), KP ('+', shiftKey, 0), KP ('=', shiftKey, 0) } },
            { Cmd::zoomOut,            "Zoom Out",                "View", { KP ('-', 0, 0) } },
            { Cmd::fitWidth,           "Optimize Arrangement Width", "View", { KP ('w', 0, 0) } },
            { Cmd::zoomToSelection,    "Zoom to Selection",       "View", { KP ('z', 0, 0) } },
            { Cmd::zoomBack,           "Zoom Back",               "View", { KP ('x', 0, 0) } },

            // Tracks --------------------------------------------------------------------
            { Cmd::insertAudioTrack, "Insert Audio Track",      "Create", { KP ('t', cmdKey, 0) } },
            { Cmd::insertMidiTrack,  "Insert MIDI Track",       "Create", { KP ('t', cmdKey | shiftKey, 0) } },
            { Cmd::armSelected,      "Arm Selected Tracks",     "Create", { KP ('c', 0, 0) } },
            { Cmd::soloSelected,     "Solo Selected Tracks",    "Create", { KP ('s', 0, 0) } },
            { Cmd::toggleTrack1,     "Activate/Deactivate Track 1", "Create", { KP (KP::F1Key, 0, 0) } },
            { Cmd::toggleTrack2,     "Activate/Deactivate Track 2", "Create", { KP (KP::F2Key, 0, 0) } },
            { Cmd::toggleTrack3,     "Activate/Deactivate Track 3", "Create", { KP (KP::F3Key, 0, 0) } },
            { Cmd::toggleTrack4,     "Activate/Deactivate Track 4", "Create", { KP (KP::F4Key, 0, 0) } },
            { Cmd::toggleTrack5,     "Activate/Deactivate Track 5", "Create", { KP (KP::F5Key, 0, 0) } },
            { Cmd::toggleTrack6,     "Activate/Deactivate Track 6", "Create", { KP (KP::F6Key, 0, 0) } },
            { Cmd::toggleTrack7,     "Activate/Deactivate Track 7", "Create", { KP (KP::F7Key, 0, 0) } },
            { Cmd::toggleTrack8,     "Activate/Deactivate Track 8", "Create", { KP (KP::F8Key, 0, 0) } },

            // Transport -----------------------------------------------------------------
            { Cmd::playStop,              "Play from Insert Marker / Stop", "Transport", { KP (KP::spaceKey, 0, 0) } },
            { Cmd::continuePlay,          "Continue Play from Stop Point",  "Transport", { KP (KP::spaceKey, shiftKey, 0) } },
            { Cmd::record,                "Record",                         "Transport", { KP (KP::F9Key, 0, 0) } },
            { Cmd::insertMarkerToStart,   "Move Insert Marker to Beginning","Transport", { KP (KP::homeKey, 0, 0) } },
            { Cmd::insertMarkerToPlayhead,"Move Insert Marker to Playhead", "Transport", { KP (KP::spaceKey, cmdKey | shiftKey, 0) } },
            { Cmd::metronome,             "Metronome",                      "Transport", { KP ('o', 0, 0) } },
            { Cmd::engineOnOff,           "Audio Engine On/Off",            "Transport", { KP ('e', cmdKey | altKey | shiftKey, 0) } },
        };
        return t;
    }

    const CommandDef* findCommand (juce::CommandID id)
    {
        for (auto& c : commandTable())
            if (c.id == id) return &c;
        return nullptr;
    }
}

//==============================================================================
MainComponent::MainComponent()
{
    tonkori::EngineProfile::applyGlobal();

    for (auto* b : { &newButton, &openButton, &saveButton, &importButton,
                     &startButton, &playButton, &recordButton, &loopButton, &metroButton,
                     &addTrackButton, &undoButton, &redoButton, &deleteButton,
                     &exportButton, &pluginsButton, &audioButton })
    {
        b->setWantsKeyboardFocus (false);   // Space must reach the transport, not a focused button
        addAndMakeVisible (*b);
    }

    recordButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::red.darker (0.2f));
    for (auto* b : { &loopButton, &metroButton })
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3d7bd9));

    newButton.onClick      = [this] { newSession(); };
    openButton.onClick     = [this] { openSession(); };
    saveButton.onClick     = [this] { saveCurrentEdit(); };
    importButton.onClick   = [this] { importAudio(); };
    startButton.onClick    = [this] { commandManager.invokeDirectly (Cmd::insertMarkerToStart, false); };
    playButton.onClick     = [this] { playStop(); };
    recordButton.onClick   = [this] { toggleRecord(); };
    loopButton.onClick     = [this] { if (edit) { edit->getUndoManager().beginNewTransaction ("Loop"); loopSelection(); } };
    metroButton.onClick    = [this] { toggleMetronome(); };
    addTrackButton.onClick = [this] { insertTrack (false); };
    undoButton.onClick     = [this] { commandManager.invokeDirectly (Cmd::undo, false); };
    redoButton.onClick     = [this] { commandManager.invokeDirectly (Cmd::redo, false); };
    deleteButton.onClick   = [this] { commandManager.invokeDirectly (Cmd::deleteSel, false); };
    exportButton.onClick   = [this] { exportAudio(); };
    pluginsButton.onClick  = [this] { showPluginList(); };
    audioButton.onClick    = [this] { EngineHelpers::showAudioDeviceSettings (engine); };

    sessionLabel.setJustificationType (juce::Justification::centred);
    sessionLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    addAndMakeVisible (sessionLabel);
    addAndMakeVisible (statusBar);

    selectionManager.addChangeListener (this);
    engine.getDeviceManager().addChangeListener (this);

    // Inputs: enable every input the interface exposes, recording as 32-bit float WAV.
    auto& dm = engine.getDeviceManager();
    for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
        if (auto* wip = dm.getWaveInDevice (i))
        {
            wip->setMonitorMode (te::InputDevice::MonitorMode::automatic);
            wip->setEnabled (true);
        }
    tonkori::EngineProfile::applyToInputs (engine);

    auto folder = getSessionsFolder();
    auto recent = Helpers::findRecentEdit (folder);
    loadEdit (recent.existsAsFile() ? recent
                                    : folder.getNonexistentChildFile ("Untitled", te::editFileSuffix, false));

    commandManager.registerAllCommandsForTarget (this);
    commandManager.setFirstCommandTarget (this);
    addKeyListener (commandManager.getKeyMappings());
    setWantsKeyboardFocus (true);

   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (this);
   #endif
    setApplicationCommandManagerToWatch (&commandManager);

    setSize (1400, 850);
}

MainComponent::~MainComponent()
{
   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (nullptr);
   #endif
    saveCurrentEdit();
    engine.getDeviceManager().removeChangeListener (this);
    selectionManager.removeChangeListener (this);
    editComponent = nullptr;
    insertPoint = nullptr;
    edit = nullptr;
    engine.getTemporaryFileManager().getTempDirectory().deleteRecursively();
}

void MainComponent::parentHierarchyChanged()
{
    // Make the window itself route keys to our command map, whatever has focus.
    if (auto* top = getTopLevelComponent(); top != nullptr && top != this)
    {
        top->removeKeyListener (commandManager.getKeyMappings());
        top->addKeyListener (commandManager.getKeyMappings());
    }
}

//==============================================================================
juce::File MainComponent::getSessionsFolder()
{
    auto d = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                .getChildFile ("Tonkori DAW").getChildFile ("Sessions");
    d.createDirectory();
    return d;
}

juce::File MainComponent::getSessionAudioFolder() const
{
    if (edit == nullptr) return getSessionsFolder();
    auto f = edit->editFileRetriever();
    auto d = f.getParentDirectory().getChildFile (f.getFileNameWithoutExtension() + " Audio");
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
    insertPoint = nullptr;

    if (edit != nullptr)
        edit->getTransport().removeChangeListener (this);

    edit = file.existsAsFile() ? te::loadEditFromFile (engine, file)
                               : te::createEmptyEdit (engine, file);

    edit->playInStopEnabled = true;
    edit->getTransport().addChangeListener (this);
    edit->getTransport().ensureContextAllocated();
    insertPoint = std::make_unique<te::EditInsertPoint> (*edit);

    if (te::getAudioTracks (*edit).isEmpty())
        edit->ensureNumberOfAudioTracks (2);

    tonkori::EngineProfile::applyToEdit (*edit);

    insertMarker = stopPosition = edit->getTransport().getPosition();
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

void MainComponent::saveSessionAs()
{
    if (edit == nullptr) return;

    chooser = std::make_shared<juce::FileChooser> ("Save session as", getSessionsFolder(), "*" + juce::String (te::editFileSuffix));
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File() || edit == nullptr) return;
                              f = f.withFileExtension (te::editFileSuffix);
                              te::EditFileOperations (*edit).saveAs (f, true, [this, f] (bool ok)
                              {
                                  if (ok) juce::MessageManager::callAsync ([this, f] { loadEdit (f); });
                              });
                          });
}

//==============================================================================
void MainComponent::importAudio()
{
    if (edit == nullptr) return;

    auto wildcard = engine.getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats() + ";*.dsf;*.dff";
    chooser = std::make_shared<juce::FileChooser> ("Import audio or DSD", juce::File::getSpecialLocation (juce::File::userMusicDirectory), wildcard);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult().existsAsFile())
                                  importFile (fc.getResult());
                          });
}

void MainComponent::importFile (const juce::File& source)
{
    auto f = source;

    if (tonkori::dsd::isDsdFile (source))
    {
        auto dest = getSessionAudioFolder().getNonexistentChildFile (source.getFileNameWithoutExtension() + " (DXD)", ".wav", false);

        struct Job : public juce::ThreadWithProgressWindow
        {
            Job (juce::File s, juce::File d)
                : ThreadWithProgressWindow ("Converting DSD to DXD (352.8 kHz, 32-bit float)...", true, true),
                  src (std::move (s)), dst (std::move (d)) {}

            void run() override
            {
                result = tonkori::dsd::convertToDxd (src, dst, [this] (double p)
                {
                    setProgress (p);
                    return ! threadShouldExit();
                });
            }

            juce::File src, dst;
            tonkori::dsd::ConversionResult result;
        };

        Job job (source, dest);
        job.runThread();

        if (! job.result.ok)
        {
            dest.deleteFile();
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "DSD import failed", job.result.error);
            return;
        }

        f = dest;
        statusBar.flashMessage ("Imported " + job.result.source.describe() + " as DXD");
    }

    te::AudioFile audioFile (engine, f);
    if (! audioFile.isValid())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import failed", "Unreadable audio file:\n" + f.getFullPathName());
        return;
    }

    auto* target = getTargetTrack();
    if (target == nullptr)
    {
        edit->ensureNumberOfAudioTracks (te::getAudioTracks (*edit).size() + 1);
        target = te::getAudioTracks (*edit).getLast();
    }

    edit->getUndoManager().beginNewTransaction ("Import");
    const auto start = cursor();
    const auto length = te::TimeDuration::fromSeconds (audioFile.getLength());

    if (auto clip = target->insertWaveClip (f.getFileNameWithoutExtension(), f, { { start, start + length }, {} }, false))
        selectionManager.selectOnly (clip.get());
}

void MainComponent::exportAudio()
{
    if (edit == nullptr) return;

    const auto length = edit->getLength();
    if (length <= te::TimeDuration())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Export", "The session is empty.");
        return;
    }

    auto* aw = new juce::AlertWindow ("Export Audio", "Renders the whole session through the master bus at "
                                        + juce::String (engine.getDeviceManager().getSampleRate() / 1000.0, 1) + " kHz.",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addComboBox ("format", { "WAV 32-bit float (no dither)", "WAV 24-bit (TPDF dither)", "WAV 16-bit (TPDF dither)" });
    aw->addButton ("Choose File...", 1, KP (KP::returnKey));
    aw->addButton ("Cancel", 0, KP (KP::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
    {
        const int choice = aw->getComboBoxComponent ("format")->getSelectedItemIndex();
        if (r == 0 || edit == nullptr) return;

        const int bits = choice == 0 ? 32 : (choice == 1 ? 24 : 16);
        auto start = getSessionsFolder().getParentDirectory().getChildFile ("Exports");
        start.createDirectory();

        chooser = std::make_shared<juce::FileChooser> ("Export audio", start.getChildFile (edit->getName() + ".wav"), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, bits] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult().withFileExtension ("wav");
                                  if (f == juce::File() || edit == nullptr) return;

                                  te::Renderer::Parameters p (*edit);
                                  p.tracksToDo = te::toBitSet (te::getAllTracks (*edit));
                                  p.destFile = f;
                                  p.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
                                  p.bitDepth = bits;
                                  p.ditheringEnabled = bits < 32;
                                  p.sampleRateForAudio = engine.getDeviceManager().getSampleRate();
                                  p.blockSizeForAudio = 512;
                                  p.time = { 0_tp, te::TimePosition::fromSeconds (edit->getLength().inSeconds()) };
                                  p.usePlugins = true;
                                  p.useMasterPlugins = true;
                                  p.canRenderInMono = false;

                                  f.deleteFile();
                                  auto out = te::Renderer::renderToFile ("Exporting " + f.getFileName(), p);
                                  statusBar.flashMessage (out.existsAsFile() ? "Exported " + out.getFileName() : "Export failed");
                              });
    }), true);
}

void MainComponent::showPluginList()
{
    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle                  = "Plug-in Manager (AU / VST3)";
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

//==============================================================================
// Transport: Live semantics
void MainComponent::playStop()
{
    if (edit == nullptr) return;
    auto& t = edit->getTransport();

    if (t.isPlaying())
    {
        const bool wasRecording = t.isRecording();
        stopPosition = t.getPosition();
        t.stop (false, false);
        t.setPosition (insertMarker);
        if (wasRecording) saveCurrentEdit();
    }
    else
    {
        insertMarker = t.getPosition();
        t.play (false);
    }
}

void MainComponent::continuePlay()
{
    if (edit == nullptr) return;
    auto& t = edit->getTransport();

    if (t.isPlaying())
    {
        stopPosition = t.getPosition();
        t.stop (false, false);
        return;
    }

    t.setPosition (stopPosition);
    t.play (false);
}

void MainComponent::toggleRecord()
{
    if (edit == nullptr) return;
    auto& t = edit->getTransport();

    if (t.isRecording())
    {
        stopPosition = t.getPosition();
        t.stop (false, false);          // keep the take
        t.setPosition (insertMarker);
        saveCurrentEdit();
    }
    else
    {
        if (! t.isPlaying())
            insertMarker = t.getPosition();
        t.record (false);
    }
}

void MainComponent::toggleMetronome()
{
    if (edit == nullptr) return;
    edit->clickTrackEnabled = ! edit->clickTrackEnabled.get();
    updateButtons();
}

void MainComponent::toggleEngine()
{
    auto& adm = engine.getDeviceManager().deviceManager;

    if (adm.getCurrentAudioDevice() != nullptr)
    {
        if (edit) edit->getTransport().stop (false, false);
        adm.closeAudioDevice();
        statusBar.flashMessage ("Audio engine OFF  (Cmd+Opt+Shift+E to restart)");
    }
    else
    {
        adm.restartLastAudioDevice();
        statusBar.flashMessage ("Audio engine ON");
    }
}

//==============================================================================
// Editing helpers
juce::Array<te::Clip*> MainComponent::getSelectedClips() const
{
    return selectionManager.getItemsOfType<te::Clip>();
}

juce::Array<te::Track*> MainComponent::getSelectedTracks() const
{
    auto tracks = selectionManager.getItemsOfType<te::Track>();

    if (tracks.isEmpty())
        for (auto* c : getSelectedClips())
            if (auto* t = c->getTrack())
                tracks.addIfNotAlreadyThere (t);

    return tracks;
}

te::AudioTrack* MainComponent::getTargetTrack() const
{
    for (auto* t : getSelectedTracks())
        if (auto* at = dynamic_cast<te::AudioTrack*> (t))
            return at;

    return edit != nullptr ? te::getAudioTracks (*edit).getFirst() : nullptr;
}

te::TimePosition MainComponent::cursor() const
{
    return edit != nullptr ? edit->getTransport().getPosition() : te::TimePosition();
}

std::optional<te::TimeRange> MainComponent::clipSelectionRange() const
{
    std::optional<te::TimeRange> r;
    for (auto* c : getSelectedClips())
    {
        auto cr = c->getPosition().time;
        r = r ? r->getUnionWith (cr) : cr;
    }
    return r;
}

te::TimeDuration MainComponent::sixteenthAt (te::TimePosition t) const
{
    auto& ts = edit->tempoSequence;
    auto b = ts.toBeats (t);
    return ts.toTime (b + te::BeatDuration::fromBeats (0.25)) - t;
}

void MainComponent::copySelection (bool alsoDelete)
{
    auto clips = getSelectedClips();
    auto range = clipSelectionRange();
    if (clips.isEmpty() || ! range) return;

    auto content = std::make_unique<te::Clipboard::Clips>();
    content->addSelectedClips (selectionManager.getSelectedObjects(), *range, te::Clipboard::Clips::AutomationLocked::no);
    te::Clipboard::getInstance()->setContent (std::move (content));

    if (alsoDelete)
        for (auto* c : clips)
            c->removeFromParent();
}

void MainComponent::pasteAtCursor()
{
    auto* content = te::Clipboard::getInstance()->getContentWithType<te::Clipboard::Clips>();
    if (content == nullptr || edit == nullptr) return;

    te::Clipboard::ContentType::EditPastingOptions opts (*edit, *insertPoint, &selectionManager);
    opts.startTime = cursor();
    opts.startTrack = getTargetTrack();
    content->pasteIntoEdit (opts);
}

void MainComponent::duplicateSelection()
{
    auto clips = getSelectedClips();
    auto range = clipSelectionRange();
    if (clips.isEmpty() || ! range) return;

    // Live: duplicates are placed directly after the selection as a block.
    const auto offset = range->getLength();
    te::SelectableList newOnes;

    for (auto* c : clips)
        if (auto n = te::duplicateClip (*c))
        {
            n->setStart (c->getPosition().getStart() + offset, false, true);
            newOnes.add (n.get());
        }

    selectionManager.select (newOnes);
}

void MainComponent::deleteSelection()
{
    if (edit == nullptr) return;

    auto clips = getSelectedClips();
    if (! clips.isEmpty())
    {
        for (auto* c : clips)
            c->removeFromParent();
        return;
    }

    for (auto* track : selectionManager.getItemsOfType<te::Track>())
        if (! (track->isMasterTrack() || track->isMarkerTrack() || track->isTempoTrack() || track->isChordTrack()))
            edit->deleteTrack (track);
}

void MainComponent::selectAllClips()
{
    if (edit == nullptr) return;
    te::SelectableList all;
    for (auto* t : te::getClipTracks (*edit))
        for (auto* c : t->getClips())
            all.add (c);
    selectionManager.select (all);
}

void MainComponent::renameSelection()
{
    te::EditItemID targetID;
    bool isClip = false;
    juce::String current;

    if (auto clips = getSelectedClips(); ! clips.isEmpty())        { targetID = clips.getFirst()->itemID;  isClip = true; current = clips.getFirst()->getName(); }
    else if (auto tracks = getSelectedTracks(); ! tracks.isEmpty()) { targetID = tracks.getFirst()->itemID; current = tracks.getFirst()->getName(); }
    if (! targetID.isValid()) return;

    auto* aw = new juce::AlertWindow ("Rename", {}, juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", current);
    aw->addButton ("Rename", 1, KP (KP::returnKey));
    aw->addButton ("Cancel", 0, KP (KP::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, targetID, isClip] (int r)
    {
        if (r == 0 || edit == nullptr) return;
        const auto name = aw->getTextEditorContents ("name").trim();
        if (name.isEmpty()) return;

        edit->getUndoManager().beginNewTransaction ("Rename");
        if (isClip)
        {
            if (auto* c = te::findClipForID (*edit, targetID)) c->setName (name);
        }
        else if (auto* t = te::findTrackForID (*edit, targetID))
        {
            t->setName (name);
        }
    }), true);
}

void MainComponent::splitAtCursor()
{
    if (edit == nullptr) return;
    const auto pos = cursor();

    te::SelectableList targets;
    for (auto* c : getSelectedClips())
        targets.add (c);

    if (targets.isEmpty())
    {
        auto tracks = getSelectedTracks();
        for (auto* t : te::getClipTracks (*edit))
            if (tracks.isEmpty() || tracks.contains (t))
                for (auto* c : t->getClips())
                    if (c->getPosition().time.contains (pos))
                        targets.add (c);
    }

    if (! targets.isEmpty())
        selectionManager.select (te::splitClips (targets, pos));
}

void MainComponent::insertSilence()
{
    if (edit == nullptr) return;
    auto& t = edit->getTransport();
    const auto loop = t.getLoopRange();
    const auto pos = cursor();

    // Live inserts the length of the time selection; until M1's time selection we use the
    // loop brace if it is on, otherwise one bar.
    auto len = (t.looping.get() && loop.getLength() > te::TimeDuration()) ? loop.getLength()
             : edit->tempoSequence.toTime (edit->tempoSequence.toBeats (pos) + te::BeatDuration::fromBeats (4)) - pos;

    te::insertSpaceIntoEdit (*edit, { pos, pos + len });
}

void MainComponent::deleteTime()
{
    if (edit == nullptr) return;
    const auto loop = edit->getTransport().getLoopRange();
    if (loop.getLength() <= te::TimeDuration()) return;

    te::deleteRegionOfTracks (*edit, loop, false, te::CloseGap::yes, &selectionManager);
}

void MainComponent::setFades (bool create)
{
    const auto fade = te::TimeDuration::fromSeconds (create ? 0.010 : 0.0);
    for (auto* c : getSelectedClips())
        if (auto* ac = dynamic_cast<te::AudioClipBase*> (c))
        {
            ac->setFadeIn (fade);
            ac->setFadeOut (fade);
        }
}

void MainComponent::reverseSelectedClips()
{
    for (auto* c : getSelectedClips())
        if (auto* ac = dynamic_cast<te::AudioClipBase*> (c))
            ac->setIsReversed (! ac->getIsReversed());
}

void MainComponent::nudge (int direction)
{
    for (auto* c : getSelectedClips())
    {
        const auto start = c->getPosition().getStart();
        const auto step = sixteenthAt (start);
        auto newStart = direction > 0 ? start + step : start - step;
        if (newStart < 0_tp) newStart = 0_tp;
        c->setStart (newStart, false, true);
    }
}

void MainComponent::deactivateSelection()
{
    auto clips = getSelectedClips();
    if (! clips.isEmpty())
    {
        for (auto* c : clips) c->setMuted (! c->isMuted());
        return;
    }

    for (auto* t : selectionManager.getItemsOfType<te::Track>())
        t->setMute (! t->isMuted (false));
}

void MainComponent::ensureLoopRange()
{
    // Live always has a loop brace; Tracktion starts with an empty one and refuses to
    // play looped. Default: 4 bars from the insert marker.
    auto& t = edit->getTransport();
    if (t.getLoopRange().getLength() > te::TimeDuration::fromSeconds (0.01))
        return;

    auto& ts = edit->tempoSequence;
    const auto start = cursor();
    t.setLoopRange ({ start, ts.toTime (ts.toBeats (start) + te::BeatDuration::fromBeats (16)) });
}

void MainComponent::loopSelection()
{
    if (edit == nullptr) return;
    auto& t = edit->getTransport();

    if (auto r = clipSelectionRange())
    {
        t.setLoopRange (*r);
        t.looping = true;
    }
    else
    {
        if (! t.looping.get())
            ensureLoopRange();
        t.looping = ! t.looping.get();
    }
    updateButtons();
}

void MainComponent::selectLoopContents()
{
    if (edit == nullptr) return;
    const auto loop = edit->getTransport().getLoopRange();
    te::SelectableList inLoop;

    for (auto* t : te::getClipTracks (*edit))
        for (auto* c : t->getClips())
            if (c->getPosition().time.overlaps (loop))
                inLoop.add (c);

    selectionManager.select (inLoop);
}

void MainComponent::insertTrack (bool midi)
{
    if (edit == nullptr) return;
    const int n = te::getAudioTracks (*edit).size();
    edit->ensureNumberOfAudioTracks (n + 1);

    if (auto* t = te::getAudioTracks (*edit).getLast())
    {
        t->setName ((midi ? "MIDI " : "Audio ") + juce::String (n + 1));
        selectionManager.selectOnly (t);
    }
}

void MainComponent::armSelectedTracks()
{
    if (edit == nullptr) return;

    for (auto* t : getSelectedTracks())
        if (auto* at = dynamic_cast<te::AudioTrack*> (t))
        {
            const bool armed = EngineHelpers::isTrackArmed (*at);

            if (! armed)
            {
                // No input assigned yet: give it the first audio input nobody else uses.
                bool hasInput = false;
                for (auto* in : edit->getAllInputDevices())
                    if (in->getTargets().contains (at->itemID)) hasInput = true;

                if (! hasInput)
                    for (auto* in : edit->getAllInputDevices())
                        if (in->getInputDevice().getDeviceType() == te::InputDevice::waveDevice && in->getTargets().isEmpty())
                        {
                            [[maybe_unused]] auto r = in->setTarget (at->itemID, true, &edit->getUndoManager(), 0);
                            break;
                        }
            }

            EngineHelpers::armTrack (*at, ! armed);
        }
}

void MainComponent::soloSelectedTracks()
{
    for (auto* t : getSelectedTracks())
        t->setSolo (! t->isSolo (false));
}

void MainComponent::toggleTrackActive (int index)
{
    if (edit == nullptr) return;
    auto tracks = te::getAudioTracks (*edit);
    if (auto* t = tracks[index])
        t->setMute (! t->isMuted (false));
}

//==============================================================================
// View
void MainComponent::zoom (double factor)
{
    if (editComponent == nullptr) return;
    auto& evs = editComponent->getEditViewState();
    const auto x1 = evs.viewX1.get(), x2 = evs.viewX2.get();
    const auto centre = cursor();
    const double len = juce::jlimit (0.05, 3600.0, (x2 - x1).inSeconds() * factor);
    const double frac = juce::jlimit (0.0, 1.0, (centre - x1).inSeconds() / juce::jmax (1.0e-9, (x2 - x1).inSeconds()));
    const double start = juce::jmax (0.0, centre.inSeconds() - len * frac);

    evs.viewX1 = te::TimePosition::fromSeconds (start);
    evs.viewX2 = te::TimePosition::fromSeconds (start + len);
}

void MainComponent::fitWidth()
{
    if (editComponent == nullptr || edit == nullptr) return;
    auto& evs = editComponent->getEditViewState();
    const double len = juce::jmax (10.0, edit->getLength().inSeconds() * 1.05);
    evs.viewX1 = 0_tp;
    evs.viewX2 = te::TimePosition::fromSeconds (len);
}

void MainComponent::zoomToSelection()
{
    if (editComponent == nullptr) return;
    auto r = clipSelectionRange();
    if (! r) return;

    auto& evs = editComponent->getEditViewState();
    previousView = std::make_pair (evs.viewX1.get(), evs.viewX2.get());
    const auto pad = te::TimeDuration::fromSeconds (r->getLength().inSeconds() * 0.05);
    evs.viewX1 = r->getStart() - pad < 0_tp ? 0_tp : r->getStart() - pad;
    evs.viewX2 = r->getEnd() + pad;
}

void MainComponent::zoomBack()
{
    if (editComponent == nullptr || ! previousView) return;
    auto& evs = editComponent->getEditViewState();
    evs.viewX1 = previousView->first;
    evs.viewX2 = previousView->second;
    previousView.reset();
}

//==============================================================================
void MainComponent::updateButtons()
{
    if (edit == nullptr) return;
    auto& t = edit->getTransport();
    playButton.setButtonText (t.isPlaying() ? "Stop" : "Play");
    recordButton.setToggleState (t.isRecording(), juce::dontSendNotification);
    loopButton.setToggleState (t.looping.get(), juce::dontSendNotification);
    metroButton.setToggleState (edit->clickTrackEnabled.get(), juce::dontSendNotification);
    deleteButton.setEnabled (! selectionManager.getSelectedObjects().isEmpty());
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &engine.getDeviceManager())
        tonkori::EngineProfile::applyToInputs (engine);   // new device → re-assert 32f recording

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

    place (newButton, 50); place (openButton, 52); place (saveButton, 50); place (importButton, 62); place (exportButton, 62);
    gap (14);
    place (startButton, 36); place (playButton, 58); place (recordButton, 50); place (loopButton, 50); place (metroButton, 50);
    gap (14);
    place (addTrackButton, 68); place (deleteButton, 60); place (undoButton, 50); place (redoButton, 50);

    audioButton.setBounds (bar.removeFromRight (60)); bar.removeFromRight (4);
    pluginsButton.setBounds (bar.removeFromRight (66)); bar.removeFromRight (4);
    sessionLabel.setBounds (bar);

    if (editComponent != nullptr)
        editComponent->setBounds (r);
}

//==============================================================================
void MainComponent::getAllCommands (juce::Array<juce::CommandID>& c)
{
    for (auto& d : commandTable())
        c.add (d.id);
}

void MainComponent::getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    if (auto* d = findCommand (id))
    {
        info.setInfo (d->name, d->name, d->category, 0);
        for (auto& k : d->keys)
            info.addDefaultKeypress (k.getKeyCode(), k.getModifiers());
    }

    if (edit == nullptr) return;
    auto& t = edit->getTransport();

    switch (id)
    {
        case Cmd::undo:      info.setActive (edit->getUndoManager().canUndo()); break;
        case Cmd::redo:      info.setActive (edit->getUndoManager().canRedo()); break;
        case Cmd::paste:     info.setActive (te::Clipboard::getInstance()->hasContentWithType<te::Clipboard::Clips>()); break;
        case Cmd::metronome: info.setTicked (edit->clickTrackEnabled.get()); break;
        case Cmd::loopSelection: info.setTicked (t.looping.get()); break;
        case Cmd::zoomBack:  info.setActive (previousView.has_value()); break;
        default: break;
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    if (edit == nullptr) return false;

    // One undo step per command.
    if (auto* d = findCommand (info.commandID); d != nullptr && info.commandID != Cmd::undo && info.commandID != Cmd::redo)
        edit->getUndoManager().beginNewTransaction (d->name);

    switch (info.commandID)
    {
        case Cmd::newSet:        newSession(); break;
        case Cmd::openSet:       openSession(); break;
        case Cmd::save:          saveCurrentEdit(); statusBar.flashMessage ("Saved"); break;
        case Cmd::saveAs:        saveSessionAs(); break;
        case Cmd::importAudio:   importAudio(); break;
        case Cmd::exportAudio:   exportAudio(); break;
        case Cmd::settings:      EngineHelpers::showAudioDeviceSettings (engine); break;
        case Cmd::pluginList:    showPluginList(); break;
        case Cmd::fullScreen:
            if (auto* peer = getPeer()) peer->setFullScreen (! peer->isFullScreen());
            break;

        case Cmd::undo:          edit->getUndoManager().undo(); break;
        case Cmd::redo:          edit->getUndoManager().redo(); break;
        case Cmd::cut:           copySelection (true); break;
        case Cmd::copy:          copySelection (false); break;
        case Cmd::paste:         pasteAtCursor(); break;
        case Cmd::duplicate:     duplicateSelection(); break;
        case Cmd::deleteSel:     deleteSelection(); break;
        case Cmd::selectAll:     selectAllClips(); break;
        case Cmd::rename:        renameSelection(); break;
        case Cmd::splitClip:     splitAtCursor(); break;
        case Cmd::insertSilence: insertSilence(); break;
        case Cmd::deleteTime:    deleteTime(); break;
        case Cmd::createFades:   setFades (true); break;
        case Cmd::deleteFades:   setFades (false); break;
        case Cmd::reverseClip:   reverseSelectedClips(); break;
        case Cmd::nudgeLeft:     nudge (-1); break;
        case Cmd::nudgeRight:    nudge (1); break;
        case Cmd::deactivate:    deactivateSelection(); break;

        case Cmd::loopSelection:      loopSelection(); break;
        case Cmd::selectLoopContents: selectLoopContents(); break;
        case Cmd::setLoopStart:
        {
            auto& t = edit->getTransport();
            auto r = t.getLoopRange();
            t.setLoopRange ({ cursor(), r.getEnd() > cursor() ? r.getEnd() : cursor() + sixteenthAt (cursor()) * 16 });
            break;
        }
        case Cmd::setLoopEnd:
        {
            auto& t = edit->getTransport();
            auto r = t.getLoopRange();
            if (cursor() > r.getStart()) t.setLoopRange ({ r.getStart(), cursor() });
            break;
        }
        case Cmd::zoomIn:          zoom (0.5); break;
        case Cmd::zoomOut:         zoom (2.0); break;
        case Cmd::fitWidth:        fitWidth(); break;
        case Cmd::zoomToSelection: zoomToSelection(); break;
        case Cmd::zoomBack:        zoomBack(); break;

        case Cmd::insertAudioTrack: insertTrack (false); break;
        case Cmd::insertMidiTrack:  insertTrack (true); break;
        case Cmd::armSelected:      armSelectedTracks(); break;
        case Cmd::soloSelected:     soloSelectedTracks(); break;
        case Cmd::toggleTrack1: case Cmd::toggleTrack2: case Cmd::toggleTrack3: case Cmd::toggleTrack4:
        case Cmd::toggleTrack5: case Cmd::toggleTrack6: case Cmd::toggleTrack7: case Cmd::toggleTrack8:
            toggleTrackActive ((int) (info.commandID - Cmd::toggleTrack1));
            break;

        case Cmd::playStop:               playStop(); break;
        case Cmd::continuePlay:           continuePlay(); break;
        case Cmd::record:                 toggleRecord(); break;
        case Cmd::insertMarkerToStart:    insertMarker = 0_tp; edit->getTransport().setPosition (0_tp); break;
        case Cmd::insertMarkerToPlayhead: insertMarker = cursor(); break;
        case Cmd::metronome:              toggleMetronome(); break;
        case Cmd::engineOnOff:            toggleEngine(); break;

        default: return false;
    }

    updateButtons();
    return true;
}

//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "Create", "View", "Transport" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu m;
    auto add = [&] (std::initializer_list<juce::CommandID> ids)
    {
        for (auto id : ids) m.addCommandItem (&commandManager, id);
    };

    switch (index)
    {
        case 0: add ({ Cmd::newSet, Cmd::openSet }); m.addSeparator();
                add ({ Cmd::save, Cmd::saveAs }); m.addSeparator();
                add ({ Cmd::importAudio, Cmd::exportAudio }); m.addSeparator();
                add ({ Cmd::settings, Cmd::pluginList });
                break;
        case 1: add ({ Cmd::undo, Cmd::redo }); m.addSeparator();
                add ({ Cmd::cut, Cmd::copy, Cmd::paste, Cmd::duplicate, Cmd::deleteSel }); m.addSeparator();
                add ({ Cmd::selectAll, Cmd::rename }); m.addSeparator();
                add ({ Cmd::splitClip, Cmd::insertSilence, Cmd::deleteTime }); m.addSeparator();
                add ({ Cmd::createFades, Cmd::deleteFades, Cmd::reverseClip, Cmd::deactivate }); m.addSeparator();
                add ({ Cmd::nudgeLeft, Cmd::nudgeRight });
                break;
        case 2: add ({ Cmd::insertAudioTrack, Cmd::insertMidiTrack }); m.addSeparator();
                add ({ Cmd::armSelected, Cmd::soloSelected }); m.addSeparator();
                add ({ Cmd::toggleTrack1, Cmd::toggleTrack2, Cmd::toggleTrack3, Cmd::toggleTrack4,
                       Cmd::toggleTrack5, Cmd::toggleTrack6, Cmd::toggleTrack7, Cmd::toggleTrack8 });
                break;
        case 3: add ({ Cmd::zoomIn, Cmd::zoomOut, Cmd::fitWidth, Cmd::zoomToSelection, Cmd::zoomBack }); m.addSeparator();
                add ({ Cmd::loopSelection, Cmd::selectLoopContents, Cmd::setLoopStart, Cmd::setLoopEnd }); m.addSeparator();
                add ({ Cmd::fullScreen });
                break;
        case 4: add ({ Cmd::playStop, Cmd::continuePlay, Cmd::record }); m.addSeparator();
                add ({ Cmd::insertMarkerToStart, Cmd::insertMarkerToPlayhead }); m.addSeparator();
                add ({ Cmd::metronome, Cmd::engineOnOff });
                break;
        default: break;
    }

    return m;
}
