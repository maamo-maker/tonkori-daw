#include "MainComponent.h"

using KP = juce::KeyPress;
using MK = juce::ModifierKeys;
namespace tui = tonkori::ui;
namespace fades = tonkori::fades;

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

    // Ableton Live 12 default Mac shortcuts where Live has the function; tools on 1–6
    // (Cubase); edit modes on Opt+1–4; Pro Tools' Cmd+F for the fade dialog.
    const std::vector<CommandDef>& commandTable()
    {
        static const std::vector<CommandDef> t =
        {
            // File
            { Cmd::newSet,       "New Session",              "File", { KP ('n', cmdKey, 0) } },
            { Cmd::openSet,      "Open Session...",          "File", { KP ('o', cmdKey, 0) } },
            { Cmd::save,         "Save Session",             "File", { KP ('s', cmdKey, 0) } },
            { Cmd::saveAs,       "Save Session As...",       "File", { KP ('s', cmdKey | shiftKey, 0) } },
            { Cmd::importAudio,  "Import Audio / DSD...",    "File", { KP ('o', cmdKey | shiftKey, 0) } },
            { Cmd::exportAudio,  "Export Audio...",          "File", { KP ('r', cmdKey | shiftKey, 0) } },
            { Cmd::settings,     "Audio Settings...",        "File", { KP (',', cmdKey, 0) } },
            { Cmd::pluginList,   "Plug-in Manager...",       "File", {} },
            { Cmd::fullScreen,   "Toggle Full Screen",       "View", { KP ('f', cmdKey | ctrlKey, 0) } },

            // Edit
            { Cmd::undo,         "Undo",                     "Edit", { KP ('z', cmdKey, 0) } },
            { Cmd::redo,         "Redo",                     "Edit", { KP ('z', cmdKey | shiftKey, 0) } },
            { Cmd::cut,          "Cut",                      "Edit", { KP ('x', cmdKey, 0) } },
            { Cmd::copy,         "Copy",                     "Edit", { KP ('c', cmdKey, 0) } },
            { Cmd::paste,        "Paste",                    "Edit", { KP ('v', cmdKey, 0) } },
            { Cmd::duplicate,    "Duplicate",                "Edit", { KP ('d', cmdKey, 0) } },
            { Cmd::deleteSel,    "Delete",                   "Edit", { KP (KP::backspaceKey, 0, 0), KP (KP::deleteKey, 0, 0) } },
            { Cmd::selectAll,    "Select All",               "Edit", { KP ('a', cmdKey, 0) } },
            { Cmd::clearSelection, "Clear Selection",        "Edit", { KP (KP::escapeKey, 0, 0) } },
            { Cmd::rename,       "Rename",                   "Edit", { KP ('r', cmdKey, 0) } },
            { Cmd::splitClip,    "Split at Insert Marker / Selection", "Edit", { KP ('e', cmdKey, 0) } },
            { Cmd::insertSilence,"Insert Silence",           "Edit", { KP ('i', cmdKey, 0) } },
            { Cmd::deleteTime,   "Delete Time",              "Edit", { KP (KP::backspaceKey, cmdKey | shiftKey, 0), KP (KP::deleteKey, cmdKey | shiftKey, 0) } },
            { Cmd::createFades,  "Create Fades / Crossfade from Selection", "Edit", { KP ('f', cmdKey | altKey, 0) } },
            { Cmd::deleteFades,  "Delete Fades",             "Edit", { KP (KP::backspaceKey, cmdKey | altKey, 0), KP (KP::deleteKey, cmdKey | altKey, 0) } },
            { Cmd::fadeEditor,   "Fade Editor...",           "Edit", { KP ('f', cmdKey, 0) } },
            { Cmd::toggleClipFades, "Fades On/Off (selected clips)", "Edit", { KP ('f', 0, 0) } },
            { Cmd::toggleAutoFade,  "Session Auto-Fades On/Off",     "Edit", { KP ('f', shiftKey, 0) } },
            { Cmd::reverseClip,  "Reverse Audio Clip",       "Edit", { KP ('r', 0, 0) } },
            { Cmd::nudgeLeft,    "Nudge Left",               "Edit", { KP (KP::leftKey, 0, 0) } },
            { Cmd::nudgeRight,   "Nudge Right",              "Edit", { KP (KP::rightKey, 0, 0) } },
            { Cmd::deactivate,   "Deactivate Selection",     "Edit", { KP ('0', 0, 0) } },
            { Cmd::spotClip,     "Spot Clip...",             "Edit", { KP ('p', cmdKey | altKey, 0) } },

            // Tools (Cubase number keys) and edit modes (Pro Tools)
            { Cmd::toolSmart,    "Smart Tool",               "Tools", { KP ('1', 0, 0) } },
            { Cmd::toolRange,    "Range Tool",               "Tools", { KP ('2', 0, 0) } },
            { Cmd::toolGrab,     "Grab Tool",                "Tools", { KP ('3', 0, 0) } },
            { Cmd::toolTrim,     "Trim Tool",                "Tools", { KP ('4', 0, 0) } },
            { Cmd::toolFade,     "Fade Tool",                "Tools", { KP ('5', 0, 0) } },
            { Cmd::toolSplit,    "Split Tool",               "Tools", { KP ('6', 0, 0) } },
            { Cmd::modeSlip,     "Slip Mode",                "Tools", { KP ('1', altKey, 0) } },
            { Cmd::modeGrid,     "Grid Mode",                "Tools", { KP ('2', altKey, 0) } },
            { Cmd::modeShuffle,  "Shuffle Mode",             "Tools", { KP ('3', altKey, 0) } },
            { Cmd::modeSpot,     "Spot Mode",                "Tools", { KP ('4', altKey, 0) } },
            { Cmd::gridNarrow,   "Narrow Grid",              "Tools", { KP ('1', cmdKey, 0) } },
            { Cmd::gridWiden,    "Widen Grid",               "Tools", { KP ('2', cmdKey, 0) } },
            { Cmd::gridTriplet,  "Triplet Grid",             "Tools", { KP ('3', cmdKey, 0) } },
            { Cmd::snapToggle,   "Snap to Grid",             "Tools", { KP ('4', cmdKey, 0) } },

            // Loop / View
            { Cmd::loopSelection,      "Loop Selection",          "View", { KP ('l', cmdKey, 0) } },
            { Cmd::selectLoopContents, "Select Loop Range",       "View", { KP ('l', cmdKey | shiftKey, 0) } },
            { Cmd::setLoopStart,       "Set Loop Start",          "View", { KP (KP::F10Key, cmdKey, 0) } },
            { Cmd::setLoopEnd,         "Set Loop End",            "View", { KP (KP::F11Key, cmdKey, 0) } },
            { Cmd::punch,              "Punch In/Out",            "View", { KP ('p', cmdKey | shiftKey, 0) } },
            { Cmd::zoomIn,             "Zoom In",                 "View", { KP ('+', 0, 0), KP ('=', 0, 0), KP ('+', shiftKey, 0), KP ('=', shiftKey, 0) } },
            { Cmd::zoomOut,            "Zoom Out",                "View", { KP ('-', 0, 0) } },
            { Cmd::fitWidth,           "Optimize Arrangement Width", "View", { KP ('w', 0, 0) } },
            { Cmd::zoomToSelection,    "Zoom to Selection",       "View", { KP ('z', 0, 0) } },
            { Cmd::zoomBack,           "Zoom Back",               "View", { KP ('x', 0, 0) } },
            { Cmd::follow,             "Follow Playhead",         "View", { KP ('f', cmdKey | shiftKey, 0) } },
            { Cmd::toggleMixer,        "Show/Hide Mixer",         "View", { KP ('m', cmdKey | altKey, 0) } },
            { Cmd::toggleInspector,    "Show/Hide Inspector",     "View", { KP ('i', cmdKey | altKey, 0) } },

            // Tracks
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

            // Transport
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
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    tonkori::EngineProfile::applyGlobal();

    addAndMakeVisible (statusBar);
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

    commandManager.registerAllCommandsForTarget (this);
    commandManager.setFirstCommandTarget (this);
    addKeyListener (commandManager.getKeyMappings());
    setWantsKeyboardFocus (true);

    auto folder = getSessionsFolder();
    auto recent = Helpers::findRecentEdit (folder);
    loadEdit (recent.existsAsFile() ? recent
                                    : folder.getNonexistentChildFile ("Untitled", te::editFileSuffix, false));

   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (this);
   #endif
    setApplicationCommandManagerToWatch (&commandManager);

    setSize (1480, 900);
}

MainComponent::~MainComponent()
{
   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu (nullptr);
   #endif
    saveCurrentEdit();
    engine.getDeviceManager().removeChangeListener (this);
    mixer = nullptr;
    inspector = nullptr;
    arrange = nullptr;
    toolbar = nullptr;
    view = nullptr;
    insertPoint = nullptr;
    edit = nullptr;
    engine.getTemporaryFileManager().getTempDirectory().deleteRecursively();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

void MainComponent::parentHierarchyChanged()
{
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
    statusBar.setInsertMarkerSource (nullptr);

    // Keep display preferences across sessions.
    tui::TimeFormat fmt = tui::TimeFormat::barsBeats;
    int fps = 4;
    bool showInspector = true, showMixer = false;
    if (view) { fmt = view->timeFormat; fps = view->fpsIndex; showInspector = view->showInspector; showMixer = view->showMixer; }

    mixer = nullptr;
    inspector = nullptr;
    arrange = nullptr;
    toolbar = nullptr;
    if (view) view->removeChangeListener (this);
    view = nullptr;
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
    stopPosition = edit->getTransport().getPosition();
    saveCurrentEdit();

    view = std::make_unique<tui::ViewState> (*edit, selectionManager);
    view->timeFormat = fmt;
    view->fpsIndex = fps;
    view->showInspector = showInspector;
    view->showMixer = showMixer;
    view->insertMarker = stopPosition;
    view->addChangeListener (this);

    buildEditWindow();
    statusBar.setEdit (edit.get());
    statusBar.setInsertMarkerSource (&view->insertMarker);
    resized();
}

void MainComponent::buildEditWindow()
{
    tui::ToolbarCommands ids { Cmd::insertMarkerToStart, Cmd::playStop, Cmd::record, Cmd::loopSelection, Cmd::punch,
                               Cmd::metronome, Cmd::follow, Cmd::toggleInspector, Cmd::toggleMixer, Cmd::settings,
                               Cmd::pluginList, Cmd::importAudio, Cmd::exportAudio, Cmd::toggleAutoFade };
    toolbar = std::make_unique<tui::Toolbar> (*view, commandManager, ids);
    arrange = std::make_unique<tui::ArrangeView> (*view);
    inspector = std::make_unique<tui::Inspector> (*view);
    mixer = std::make_unique<tui::Mixer> (*view);

    arrange->onOpenFadeEditor = [this] (te::AudioClipBase& c, bool in, juce::Rectangle<int> area) { openFadeEditor (c, in, area); };
    arrange->onSpotClip = [this] (te::Clip& c) { spotClip (c); };
    arrange->onFilesDropped = [this] (const juce::StringArray& files, te::TimePosition t, te::AudioTrack* track)
    {
        auto tracks = te::getAudioTracks (*edit);
        importFiles (files, t, track != nullptr ? track : (tracks.isEmpty() ? nullptr : tracks.getLast()));
    };

    addAndMakeVisible (*toolbar);
    addAndMakeVisible (*arrange);
    addChildComponent (*inspector);
    addChildComponent (*mixer);
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

void MainComponent::importFiles (const juce::StringArray& paths, std::optional<te::TimePosition> at, te::AudioTrack* track)
{
    if (edit == nullptr) return;
    auto tracks = te::getAudioTracks (*edit);
    int trackIndex = track ? tracks.indexOf (track) : -1;

    for (auto& p : paths)
    {
        juce::File f (p);
        if (! f.existsAsFile()) continue;

        // Several files stack onto consecutive tracks (Pro Tools "one track per file").
        te::AudioTrack* target = nullptr;
        if (trackIndex >= 0)
        {
            edit->ensureNumberOfAudioTracks (trackIndex + 1);
            target = te::getAudioTracks (*edit)[trackIndex++];
        }
        importFile (f, at, target);
    }
}

void MainComponent::importFile (const juce::File& source, std::optional<te::TimePosition> at, te::AudioTrack* explicitTrack)
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

    auto* target = explicitTrack != nullptr ? explicitTrack : getTargetTrack();
    if (target == nullptr)
    {
        edit->ensureNumberOfAudioTracks (te::getAudioTracks (*edit).size() + 1);
        target = te::getAudioTracks (*edit).getLast();
    }

    edit->getUndoManager().beginNewTransaction ("Import");
    const auto start = at ? *at : insertMarker();
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

    const bool hasRange = view && view->timeSelection.has_value();
    auto* aw = new juce::AlertWindow ("Export Audio", "Renders through the master bus at "
                                        + juce::String (engine.getDeviceManager().getSampleRate() / 1000.0, 1) + " kHz.",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addComboBox ("format", { "WAV 32-bit float (no dither)", "WAV 24-bit (TPDF dither)", "WAV 16-bit (TPDF dither)" });
    aw->addComboBox ("range", hasRange ? juce::StringArray { "Time selection", "Whole session" } : juce::StringArray { "Whole session" });
    aw->addButton ("Choose File...", 1, KP (KP::returnKey));
    aw->addButton ("Cancel", 0, KP (KP::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, hasRange] (int r)
    {
        const int choice = aw->getComboBoxComponent ("format")->getSelectedItemIndex();
        const bool useRange = hasRange && aw->getComboBoxComponent ("range")->getSelectedItemIndex() == 0;
        if (r == 0 || edit == nullptr) return;

        const int bits = choice == 0 ? 32 : (choice == 1 ? 24 : 16);
        const te::TimeRange range = useRange ? *view->timeSelection
                                             : te::TimeRange (0_tp, te::TimePosition::fromSeconds (edit->getLength().inSeconds()));
        auto start = getSessionsFolder().getParentDirectory().getChildFile ("Exports");
        start.createDirectory();

        chooser = std::make_shared<juce::FileChooser> ("Export audio", start.getChildFile (edit->getName() + ".wav"), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, bits, range] (const juce::FileChooser& fc)
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
                                  p.time = range;
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
    o.dialogBackgroundColour       = tui::colours::panel;
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
te::TimePosition MainComponent::insertMarker() const        { return view ? view->insertMarker : te::TimePosition(); }
void MainComponent::setInsertMarker (te::TimePosition t)     { if (view) { view->insertMarker = t; view->changed(); } }

void MainComponent::playStop()
{
    if (edit == nullptr) return;
    auto& t = edit->getTransport();

    if (t.isPlaying())
    {
        const bool wasRecording = t.isRecording();
        stopPosition = t.getPosition();
        t.stop (false, false);
        t.setPosition (insertMarker());
        if (wasRecording) saveCurrentEdit();
    }
    else
    {
        t.setPosition (insertMarker());
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
        t.setPosition (insertMarker());
        saveCurrentEdit();
    }
    else
    {
        if (! t.isPlaying()) t.setPosition (insertMarker());
        if (edit->recordingPunchInOut.get()) ensureLoopRange();
        t.record (false);
    }
}

void MainComponent::toggleMetronome()
{
    if (edit == nullptr) return;
    edit->clickTrackEnabled = ! edit->clickTrackEnabled.get();
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

juce::Array<te::AudioClipBase*> MainComponent::getSelectedAudioClips() const
{
    juce::Array<te::AudioClipBase*> r;
    for (auto* c : getSelectedClips())
        if (auto* ac = dynamic_cast<te::AudioClipBase*> (c)) r.add (ac);
    return r;
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

juce::Array<te::Track*> MainComponent::getRangeTracks() const
{
    juce::Array<te::Track*> r;
    if (view == nullptr) return r;
    for (auto& id : view->rangeTracks)
        if (auto* t = te::findTrackForID (*edit, id)) r.add (t);
    return r;
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
    if (edit == nullptr) return {};
    return edit->getTransport().isPlaying() ? edit->getTransport().getPosition() : insertMarker();
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
    // Range selection (Pro Tools): copy what lies inside the range on the selected tracks.
    if (view && view->timeSelection)
    {
        const auto range = *view->timeSelection;
        te::SelectableList inRange;
        for (auto* t : getRangeTracks())
            if (auto* ct = dynamic_cast<te::ClipTrack*> (t))
                for (auto* c : ct->getClips())
                    if (c->getPosition().time.overlaps (range)) inRange.add (c);

        if (! inRange.isEmpty())
        {
            auto content = std::make_unique<te::Clipboard::Clips>();
            content->addSelectedClips (inRange, range, te::Clipboard::Clips::AutomationLocked::no);
            te::Clipboard::getInstance()->setContent (std::move (content));
        }

        if (alsoDelete)
            te::deleteRegionOfTracks (*edit, range, false,
                                      view->mode == tui::EditMode::shuffle ? te::CloseGap::yes : te::CloseGap::no, &selectionManager);
        return;
    }

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
    opts.startTime = (view && view->timeSelection) ? view->timeSelection->getStart() : cursor();
    auto rangeTracks = getRangeTracks();
    opts.startTrack = (view && view->timeSelection && ! rangeTracks.isEmpty()) ? rangeTracks.getFirst() : getTargetTrack();
    content->pasteIntoEdit (opts);
}

void MainComponent::duplicateSelection()
{
    if (view && view->timeSelection)
    {
        // Duplicate the selected time on the selected tracks, right after it.
        const auto range = *view->timeSelection;
        copySelection (false);
        if (auto* content = te::Clipboard::getInstance()->getContentWithType<te::Clipboard::Clips>())
        {
            te::Clipboard::ContentType::EditPastingOptions opts (*edit, *insertPoint, &selectionManager);
            opts.startTime = range.getEnd();
            auto rt = getRangeTracks();
            opts.startTrack = rt.isEmpty() ? getTargetTrack() : rt.getFirst();
            content->pasteIntoEdit (opts);
            view->timeSelection = te::TimeRange (range.getEnd(), range.getEnd() + range.getLength());
            view->changed();
        }
        return;
    }

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

    if (view && view->timeSelection && ! view->rangeTracks.isEmpty())
    {
        const auto range = *view->timeSelection;
        auto tracks = getRangeTracks();

        // Clear only the selected tracks: split at the range edges, then remove what's inside.
        te::SelectableList toSplit;
        for (auto* t : tracks)
            if (auto* ct = dynamic_cast<te::ClipTrack*> (t))
                for (auto* c : ct->getClips())
                    if (c->getPosition().time.overlaps (range)) toSplit.add (c);
        if (! toSplit.isEmpty())
        {
            auto afterFirst = te::splitClips (toSplit, range.getStart());
            te::SelectableList all;
            for (auto* t : tracks)
                if (auto* ct = dynamic_cast<te::ClipTrack*> (t))
                    for (auto* c : ct->getClips())
                        if (c->getPosition().time.overlaps (range)) all.add (c);
            te::splitClips (all, range.getEnd());
        }

        for (auto* t : tracks)
            if (auto* ct = dynamic_cast<te::ClipTrack*> (t))
            {
                juce::Array<te::Clip*> doomed;
                for (auto* c : ct->getClips())
                    if (range.contains (c->getPosition().time)) doomed.add (c);
                for (auto* c : doomed) c->removeFromParent();

                if (view->mode == tui::EditMode::shuffle)
                    for (auto* c : ct->getClips())
                        if (c->getPosition().getStart() >= range.getEnd())
                            c->setStart (c->getPosition().getStart() - range.getLength(), false, true);
            }

        view->timeSelection.reset();
        view->changed();
        return;
    }

    auto clips = getSelectedClips();
    if (! clips.isEmpty())
    {
        for (auto* c : clips)
        {
            const auto pos = c->getPosition();
            auto* ct = dynamic_cast<te::ClipTrack*> (c->getTrack());
            c->removeFromParent();

            if (view && view->mode == tui::EditMode::shuffle && ct != nullptr)
                for (auto* o : ct->getClips())
                    if (o->getPosition().getStart() >= pos.getEnd())
                        o->setStart (o->getPosition().getStart() - pos.getLength(), false, true);
        }
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

void MainComponent::clearSelection()
{
    selectionManager.deselectAll();
    if (view) { view->timeSelection.reset(); view->rangeTracks.clear(); view->changed(); }
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

    // With a time selection: split at both edges on the selected tracks.
    if (view && view->timeSelection)
    {
        for (auto t : { view->timeSelection->getStart(), view->timeSelection->getEnd() })
        {
            te::SelectableList targets;
            for (auto* tr : getRangeTracks())
                if (auto* ct = dynamic_cast<te::ClipTrack*> (tr))
                    for (auto* c : ct->getClips())
                        if (c->getPosition().time.contains (t) && c->getPosition().getStart() != t) targets.add (c);
            if (! targets.isEmpty()) te::splitClips (targets, t);
        }
        return;
    }

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
    const auto pos = view && view->timeSelection ? view->timeSelection->getStart() : cursor();

    te::TimeDuration len;
    if (view && view->timeSelection) len = view->timeSelection->getLength();
    else if (t.looping.get() && t.getLoopRange().getLength() > te::TimeDuration()) len = t.getLoopRange().getLength();
    else len = edit->tempoSequence.toTime (edit->tempoSequence.toBeats (pos) + te::BeatDuration::fromBeats (4)) - pos;

    juce::Array<te::Track*> only;
    if (view && view->timeSelection) only = getRangeTracks();
    te::insertSpaceIntoEdit (*edit, { pos, pos + len }, only);
}

void MainComponent::deleteTime()
{
    if (edit == nullptr) return;
    auto range = view && view->timeSelection ? *view->timeSelection : edit->getTransport().getLoopRange();
    if (range.getLength() <= te::TimeDuration()) return;

    te::deleteRegionOfTracks (*edit, range, false, te::CloseGap::yes, &selectionManager);
    if (view) { view->timeSelection.reset(); view->changed(); }
}

void MainComponent::createFades()
{
    if (edit == nullptr) return;

    // Pro Tools-style: a time selection defines the fades. Across a boundary between two
    // clips it builds an equal-power crossfade, extending both clips into the overlap.
    if (view && view->timeSelection)
    {
        const auto r = *view->timeSelection;
        const auto xfade = tonkori::FadeSpec::fromString ("equalPower");
        int made = 0;

        for (auto* t : getRangeTracks())
            if (auto* ct = dynamic_cast<te::ClipTrack*> (t))
            {
                te::AudioClipBase *outgoing = nullptr, *incoming = nullptr;
                for (auto* c : ct->getClips())
                    if (auto* ac = dynamic_cast<te::AudioClipBase*> (c))
                    {
                        const auto p = ac->getPosition();
                        if (p.getEnd() > r.getStart() && p.getEnd() <= r.getEnd() && p.getStart() < r.getStart()) outgoing = ac;
                        if (p.getStart() >= r.getStart() && p.getStart() < r.getEnd() && p.getEnd() > r.getEnd()) incoming = ac;
                    }

                if (outgoing && incoming)
                {
                    // extend both clips across the selection where the source allows
                    const auto outMax = outgoing->getPosition().getStart() + (outgoing->getSourceLength() - outgoing->getPosition().getOffset());
                    outgoing->setEnd (juce::jmin (r.getEnd(), outMax), true);
                    const auto inMin = incoming->getPosition().getStart() - incoming->getPosition().getOffset();
                    incoming->setStart (juce::jmax (r.getStart(), inMin), true, false);

                    fades::setSpec (*outgoing, false, xfade);
                    fades::setSpec (*incoming, true, xfade);
                    fades::setLength (*outgoing, false, outgoing->getPosition().getEnd() - r.getStart());
                    fades::setLength (*incoming, true, r.getEnd() - incoming->getPosition().getStart());
                    ++made;
                    continue;
                }

                for (auto* c : ct->getClips())
                    if (auto* ac = dynamic_cast<te::AudioClipBase*> (c))
                    {
                        const auto p = ac->getPosition();
                        if (! p.time.overlaps (r)) continue;
                        if (r.contains (p.getStart()) || (r.getStart() <= p.getStart() && r.getEnd() > p.getStart()))
                        { fades::setLength (*ac, true, r.getEnd() - p.getStart()); ++made; }
                        else if (r.getEnd() >= p.getEnd() && r.getStart() < p.getEnd())
                        { fades::setLength (*ac, false, p.getEnd() - r.getStart()); ++made; }
                        else if (p.time.contains (r))
                        {
                            const bool firstHalf = r.getEnd() < p.getStart() + p.getLength() * 0.5;
                            if (firstHalf) fades::setLength (*ac, true, r.getEnd() - p.getStart());
                            else           fades::setLength (*ac, false, p.getEnd() - r.getStart());
                            ++made;
                        }
                    }
            }

        statusBar.flashMessage (made > 0 ? "Fades created from selection" : "No clip edges inside the selection");
        return;
    }

    for (auto* ac : getSelectedAudioClips())
    {
        if (ac->getFadeIn() <= te::TimeDuration())  fades::setLength (*ac, true, te::TimeDuration::fromSeconds (0.010));
        if (ac->getFadeOut() <= te::TimeDuration()) fades::setLength (*ac, false, te::TimeDuration::fromSeconds (0.010));
    }
}

void MainComponent::deleteFades()
{
    for (auto* ac : getSelectedAudioClips())
    {
        fades::setLength (*ac, true, {});
        fades::setLength (*ac, false, {});
    }
}

void MainComponent::reverseSelectedClips()
{
    for (auto* ac : getSelectedAudioClips())
        ac->setIsReversed (! ac->getIsReversed());
}

void MainComponent::nudge (int direction)
{
    for (auto* c : getSelectedClips())
    {
        const auto start = c->getPosition().getStart();
        const auto step = view ? (edit->tempoSequence.toTime (edit->tempoSequence.toBeats (start) + view->gridBeats (start)) - start)
                               : sixteenthAt (start);
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
    auto& t = edit->getTransport();
    if (t.getLoopRange().getLength() > te::TimeDuration::fromSeconds (0.01))
        return;

    auto& ts = edit->tempoSequence;
    const auto start = insertMarker();
    t.setLoopRange ({ start, ts.toTime (ts.toBeats (start) + te::BeatDuration::fromBeats (16)) });
}

void MainComponent::loopSelection()
{
    if (edit == nullptr) return;
    auto& t = edit->getTransport();

    if (view && view->timeSelection)
    {
        t.setLoopRange (*view->timeSelection);
        t.looping = true;
    }
    else if (auto r = clipSelectionRange())
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
}

void MainComponent::selectLoopContents()
{
    if (edit == nullptr || view == nullptr) return;
    view->timeSelection = edit->getTransport().getLoopRange();
    view->rangeTracks.clear();
    for (auto* t : te::getAudioTracks (*edit)) view->rangeTracks.add (t->itemID);
    view->changed();
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

void MainComponent::openFadeEditor (te::AudioClipBase& clip, bool fadeIn, juce::Rectangle<int> screenArea)
{
    if (! selectionManager.isSelected (&clip))
        selectionManager.selectOnly (&clip);

    auto panel = std::make_unique<tui::FadeEditorPanel> (*view, fadeIn, [this]
    {
        return getSelectedAudioClips();
    });
    panel->setSize (330, panel->getIdealHeight());
    auto* p = panel.get();
    panel->onLayoutChange = [p] { p->setSize (330, p->getIdealHeight()); };
    juce::CallOutBox::launchAsynchronously (std::move (panel), screenArea, nullptr);
}

void MainComponent::spotClip (te::Clip& clip)
{
    const auto id = clip.itemID;
    auto* aw = new juce::AlertWindow ("Spot clip", "\"" + clip.getName() + "\" start  (bars.beats.ticks, hh:mm:ss:ff, m:ss.mmm, samples, or 1.5 s)",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("start", view->format (clip.getPosition().getStart()));
    aw->addButton ("Spot", 1, KP (KP::returnKey));
    aw->addButton ("Cancel", 0, KP (KP::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, id] (int r)
    {
        if (r == 0 || edit == nullptr) return;
        if (auto* c = te::findClipForID (*edit, id))
            if (auto t = view->parse (aw->getTextEditorContents ("start")))
            {
                edit->getUndoManager().beginNewTransaction ("Spot");
                c->setStart (*t, false, true);
            }
    }), true);
}

//==============================================================================
// View
void MainComponent::zoom (double factor)
{
    if (view == nullptr || arrange == nullptr) return;
    const float x = view->timeToX (cursor());
    const float w = (float) (arrange->getWidth() - tui::ArrangeView::headerWidth);
    view->setZoom (view->pixelsPerSecond / factor, juce::jlimit (0.0f, w, x));
}

void MainComponent::fitWidth()
{
    if (arrange) arrange->fitToSession();
}

void MainComponent::zoomToSelection()
{
    if (view == nullptr || arrange == nullptr) return;
    auto r = view->timeSelection ? view->timeSelection : clipSelectionRange();
    if (! r) return;

    previousView = std::make_pair (view->viewStart, view->pixelsPerSecond);
    const double w = arrange->getWidth() - tui::ArrangeView::headerWidth - 10;
    const double len = juce::jmax (0.01, r->getLength().inSeconds() * 1.1);
    view->pixelsPerSecond = w / len;
    view->viewStart = juce::jmax (0.0, r->getStart().inSeconds() - r->getLength().inSeconds() * 0.05);
    view->changed();
}

void MainComponent::zoomBack()
{
    if (view == nullptr || ! previousView) return;
    view->viewStart = previousView->first;
    view->pixelsPerSecond = previousView->second;
    previousView.reset();
    view->changed();
}

//==============================================================================
void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &engine.getDeviceManager())
        tonkori::EngineProfile::applyToInputs (engine);   // new device → re-assert 32f recording

    if (view && source == view.get())
    {
        if (inspector && inspector->isVisible() != view->showInspector) resized();
        if (mixer && mixer->isVisible() != view->showMixer) resized();
    }
    commandManager.commandStatusChanged();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (tui::colours::bg);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    statusBar.setBounds (r.removeFromBottom (26));
    if (toolbar) toolbar->setBounds (r.removeFromTop (tui::Toolbar::totalHeight));

    if (view && mixer)
    {
        mixer->setVisible (view->showMixer);
        if (view->showMixer) mixer->setBounds (r.removeFromBottom (juce::jmin (300, r.getHeight() / 2)));
    }
    if (view && inspector)
    {
        inspector->setVisible (view->showInspector);
        if (view->showInspector) inspector->setBounds (r.removeFromRight (318));
    }
    if (arrange) arrange->setBounds (r);
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

    if (edit == nullptr || view == nullptr) return;
    auto& t = edit->getTransport();

    switch (id)
    {
        case Cmd::undo:          info.setActive (edit->getUndoManager().canUndo()); break;
        case Cmd::redo:          info.setActive (edit->getUndoManager().canRedo()); break;
        case Cmd::paste:         info.setActive (te::Clipboard::getInstance()->hasContentWithType<te::Clipboard::Clips>()); break;
        case Cmd::metronome:     info.setTicked (edit->clickTrackEnabled.get()); break;
        case Cmd::loopSelection: info.setTicked (t.looping.get()); break;
        case Cmd::punch:         info.setTicked (edit->recordingPunchInOut.get()); break;
        case Cmd::zoomBack:      info.setActive (previousView.has_value()); break;
        case Cmd::follow:        info.setTicked (view->follow); break;
        case Cmd::toggleMixer:   info.setTicked (view->showMixer); break;
        case Cmd::toggleInspector: info.setTicked (view->showInspector); break;
        case Cmd::toggleAutoFade:  info.setTicked (fades::isAutoFadeOn (*edit)); break;
        case Cmd::snapToggle:    info.setTicked (view->mode == tui::EditMode::grid); break;
        case Cmd::gridTriplet:   info.setTicked (view->triplet); break;
        case Cmd::toolSmart: case Cmd::toolRange: case Cmd::toolGrab: case Cmd::toolTrim: case Cmd::toolFade: case Cmd::toolSplit:
            info.setTicked ((int) view->tool == (int) (id - Cmd::toolSmart)); break;
        case Cmd::modeSlip: case Cmd::modeGrid: case Cmd::modeShuffle: case Cmd::modeSpot:
            info.setTicked ((int) view->mode == (int) (id - Cmd::modeSlip)); break;
        default: break;
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    if (edit == nullptr || view == nullptr) return false;

    // One undo step per command.
    if (auto* d = findCommand (info.commandID); d != nullptr && info.commandID != Cmd::undo && info.commandID != Cmd::redo)
        edit->getUndoManager().beginNewTransaction (d->name);

    auto& vs = *view;

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
        case Cmd::clearSelection: clearSelection(); break;
        case Cmd::rename:        renameSelection(); break;
        case Cmd::splitClip:     splitAtCursor(); break;
        case Cmd::insertSilence: insertSilence(); break;
        case Cmd::deleteTime:    deleteTime(); break;
        case Cmd::createFades:   createFades(); break;
        case Cmd::deleteFades:   deleteFades(); break;
        case Cmd::reverseClip:   reverseSelectedClips(); break;
        case Cmd::nudgeLeft:     nudge (-1); break;
        case Cmd::nudgeRight:    nudge (1); break;
        case Cmd::deactivate:    deactivateSelection(); break;
        case Cmd::fadeEditor:
            if (auto clips = getSelectedAudioClips(); ! clips.isEmpty())
            {
                const bool in = cursor() < clips.getFirst()->getPosition().getStart() + clips.getFirst()->getPosition().getLength() * 0.5;
                if (vs.showInspector && inspector) inspector->focusFades (in);
                else openFadeEditor (*clips.getFirst(), in, arrange->getScreenBoundsOfClip (*clips.getFirst()));
            }
            break;
        case Cmd::toggleClipFades:
        {
            auto clips = getSelectedAudioClips();
            const bool on = clips.isEmpty() ? true : ! fades::areFadesOn (*clips.getFirst());
            for (auto* c : clips) fades::setFadesOn (*c, on);
            if (! clips.isEmpty()) statusBar.flashMessage (on ? "Fades ON" : "Fades OFF (kept, bypassed)");
            break;
        }
        case Cmd::toggleAutoFade:
            fades::setAutoFade (*edit, ! fades::isAutoFadeOn (*edit), fades::getAutoFadeMs (*edit));
            statusBar.flashMessage (fades::isAutoFadeOn (*edit) ? "Session auto-fades ON (" + juce::String (fades::getAutoFadeMs (*edit), 1) + " ms)" : "Session auto-fades OFF");
            break;
        case Cmd::spotClip:
            if (auto clips = getSelectedClips(); ! clips.isEmpty()) spotClip (*clips.getFirst());
            break;

        case Cmd::toolSmart: case Cmd::toolRange: case Cmd::toolGrab: case Cmd::toolTrim: case Cmd::toolFade: case Cmd::toolSplit:
            vs.tool = (tui::Tool) (info.commandID - Cmd::toolSmart);
            break;
        case Cmd::modeSlip: case Cmd::modeGrid: case Cmd::modeShuffle: case Cmd::modeSpot:
            vs.mode = (tui::EditMode) (info.commandID - Cmd::modeSlip);
            break;
        case Cmd::gridNarrow:   vs.gridIndex = juce::jmin ((int) tui::gridDivisions().size() - 1, vs.gridIndex + 1); break;
        case Cmd::gridWiden:    vs.gridIndex = juce::jmax (0, vs.gridIndex - 1); break;
        case Cmd::gridTriplet:  vs.triplet = ! vs.triplet; break;
        case Cmd::snapToggle:   vs.mode = vs.mode == tui::EditMode::grid ? tui::EditMode::slip : tui::EditMode::grid; break;

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
        case Cmd::punch:
            edit->recordingPunchInOut = ! edit->recordingPunchInOut.get();
            if (edit->recordingPunchInOut.get()) ensureLoopRange();
            statusBar.flashMessage (edit->recordingPunchInOut.get() ? "Punch ON: records only inside the loop range" : "Punch OFF");
            break;
        case Cmd::zoomIn:          zoom (0.5); break;
        case Cmd::zoomOut:         zoom (2.0); break;
        case Cmd::fitWidth:        fitWidth(); break;
        case Cmd::zoomToSelection: zoomToSelection(); break;
        case Cmd::zoomBack:        zoomBack(); break;
        case Cmd::follow:          vs.follow = ! vs.follow; break;
        case Cmd::toggleMixer:     vs.showMixer = ! vs.showMixer; resized(); break;
        case Cmd::toggleInspector: vs.showInspector = ! vs.showInspector; resized(); break;

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
        case Cmd::insertMarkerToStart:    setInsertMarker (0_tp); if (! edit->getTransport().isPlaying()) edit->getTransport().setPosition (0_tp); break;
        case Cmd::insertMarkerToPlayhead: setInsertMarker (edit->getTransport().getPosition()); break;
        case Cmd::metronome:              toggleMetronome(); break;
        case Cmd::engineOnOff:            toggleEngine(); break;

        default: return false;
    }

    vs.changed();
    return true;
}

//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "Tools", "Create", "View", "Transport" };
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
                add ({ Cmd::selectAll, Cmd::clearSelection, Cmd::rename }); m.addSeparator();
                add ({ Cmd::splitClip, Cmd::insertSilence, Cmd::deleteTime, Cmd::spotClip }); m.addSeparator();
                add ({ Cmd::fadeEditor, Cmd::createFades, Cmd::deleteFades, Cmd::toggleClipFades, Cmd::toggleAutoFade }); m.addSeparator();
                add ({ Cmd::reverseClip, Cmd::deactivate, Cmd::nudgeLeft, Cmd::nudgeRight });
                break;
        case 2: add ({ Cmd::toolSmart, Cmd::toolRange, Cmd::toolGrab, Cmd::toolTrim, Cmd::toolFade, Cmd::toolSplit }); m.addSeparator();
                add ({ Cmd::modeSlip, Cmd::modeGrid, Cmd::modeShuffle, Cmd::modeSpot }); m.addSeparator();
                add ({ Cmd::gridNarrow, Cmd::gridWiden, Cmd::gridTriplet, Cmd::snapToggle });
                break;
        case 3: add ({ Cmd::insertAudioTrack, Cmd::insertMidiTrack }); m.addSeparator();
                add ({ Cmd::armSelected, Cmd::soloSelected }); m.addSeparator();
                add ({ Cmd::toggleTrack1, Cmd::toggleTrack2, Cmd::toggleTrack3, Cmd::toggleTrack4,
                       Cmd::toggleTrack5, Cmd::toggleTrack6, Cmd::toggleTrack7, Cmd::toggleTrack8 });
                break;
        case 4: add ({ Cmd::toggleInspector, Cmd::toggleMixer, Cmd::follow }); m.addSeparator();
                add ({ Cmd::zoomIn, Cmd::zoomOut, Cmd::fitWidth, Cmd::zoomToSelection, Cmd::zoomBack }); m.addSeparator();
                add ({ Cmd::loopSelection, Cmd::selectLoopContents, Cmd::setLoopStart, Cmd::setLoopEnd, Cmd::punch }); m.addSeparator();
                add ({ Cmd::fullScreen });
                break;
        case 5: add ({ Cmd::playStop, Cmd::continuePlay, Cmd::record }); m.addSeparator();
                add ({ Cmd::insertMarkerToStart, Cmd::insertMarkerToPlayhead }); m.addSeparator();
                add ({ Cmd::metronome, Cmd::engineOnOff });
                break;
        default: break;
    }

    return m;
}
