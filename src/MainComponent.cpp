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
            { Cmd::importVideo,  "Import Video...",          "File", { KP ('o', cmdKey | altKey, 0) } },
            { Cmd::exportAudio,  "Export Mix...",            "File", { KP ('r', cmdKey | shiftKey, 0) } },
            { Cmd::exportStems,  "Export Stems...",          "File", { KP ('r', cmdKey | altKey, 0) } },
            { Cmd::exportVideo,  "Export Video...",          "File", { KP ('r', cmdKey | altKey | shiftKey, 0) } },
            { Cmd::settings,     "Settings...",              "File", { KP (',', cmdKey, 0) } },
            { Cmd::shortcutSettings, "Keyboard Shortcuts...", "File", { KP ('k', cmdKey | altKey | shiftKey, 0) } },
            { Cmd::latencySettings,  "Latency & Sync...",     "File", {} },
            { Cmd::pluginList,   "Plug-in Manager...",       "File", {} },
            { Cmd::removeVideo,  "Remove Video",             "File", {} },
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
            { Cmd::heal,         "Heal Separation",          "Edit", { KP ('j', cmdKey, 0) } },
            { Cmd::trimStartToCursor, "Trim Clip Start to Cursor", "Edit", { KP ('[', altKey, 0) } },
            { Cmd::trimEndToCursor,   "Trim Clip End to Cursor",   "Edit", { KP (']', altKey, 0) } },
            { Cmd::trimToSelection,   "Trim Clip to Selection",    "Edit", { KP ('t', cmdKey | altKey | shiftKey, 0) } },
            { Cmd::crossfadeEditor,   "Crossfade Editor...",       "Edit", { KP ('x', cmdKey | altKey, 0) } },
            { Cmd::clipGainUp,        "Clip Gain +1 dB",           "Edit", { KP (KP::upKey, cmdKey | altKey, 0) } },
            { Cmd::clipGainDown,      "Clip Gain -1 dB",           "Edit", { KP (KP::downKey, cmdKey | altKey, 0) } },
            { Cmd::toggleGainLines,   "Show Clip Gain Lines",      "Edit", { KP ('g', cmdKey | altKey, 0) } },
            { Cmd::clearGainLine,     "Clear Clip Gain Line",      "Edit", {} },
            { Cmd::resetStretch,      "Reset Time & Pitch",        "Edit", {} },
            { Cmd::toggleLanes,       "Show/Hide Take Lanes",      "Edit", { KP ('l', cmdKey | altKey, 0) } },
            { Cmd::promoteToMain,     "Promote Selection to Main Lane", "Edit", { KP ('v', cmdKey | altKey, 0) } },

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
            { Cmd::videoWindow,        "Show/Hide Video Window",  "View", { KP ('9', cmdKey | altKey, 0) } },
            { Cmd::loudnessWindow,     "Loudness Meter",          "View", { KP ('u', cmdKey | altKey, 0) } },
            { Cmd::markerList,         "Marker List",             "View", { KP ('k', cmdKey | altKey, 0) } },

            // Tracks
            { Cmd::insertAudioTrack, "Insert Audio Track",      "Create", { KP ('t', cmdKey, 0) } },
            { Cmd::insertMidiTrack,  "Insert MIDI Track",       "Create", { KP ('t', cmdKey | shiftKey, 0) } },
            { Cmd::insertBus,        "Insert Bus",              "Create", { KP ('t', cmdKey | altKey, 0) } },
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
            { Cmd::stopOnly,              "Stop",                           "Transport", {} },
            { Cmd::insertMarkerToEnd,     "Move Insert Marker to End",      "Transport", { KP (KP::endKey, 0, 0) } },
            { Cmd::addMarker,             "Add Marker",                     "Transport", { KP ('m', 0, 0) } },
            { Cmd::prevMarker,            "Previous Marker",                "Transport", { KP (KP::leftKey, altKey, 0) } },
            { Cmd::nextMarker,            "Next Marker",                    "Transport", { KP (KP::rightKey, altKey, 0) } },
            { Cmd::preRollToggle,         "Pre-Roll On/Off",                "Transport", {} },
            { Cmd::postRollToggle,        "Post-Roll On/Off",               "Transport", {} },
            { Cmd::countInToggle,         "Count-In On/Off",                "Transport", {} },
            { Cmd::engineOnOff,           "Audio Engine On/Off",            "Transport", { KP ('e', cmdKey | altKey | shiftKey, 0) } },
        };
        return t;
    }

    // Range edits act on what plays (the main lane); take lanes are edited by comping.
    juce::Array<te::Clip*> mainClips (te::ClipTrack& t) { return tonkori::takes::clipsOnLane (t, 0); }

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

    // Inputs: mono channels (MADI-friendly) unless set otherwise, all enabled, 32-bit float takes.
    configureInputs (true);

    // Master loudness / true-peak tap on the final output.
    {
        auto tap = std::make_unique<tonkori::ui::OutputTap>();
        outputTap = tap.get();
        engine.getDeviceManager().setGlobalOutputAudioProcessor (std::move (tap));
        loudnessHub.setTap (outputTap);
        updateTapChannels();
    }

    // Plug-ins are scanned in a separate process: a crashing plug-in can't take the session down.
    engine.getPluginManager().knownPluginList.setCustomScanner (std::make_unique<tonkori::scan::OutOfProcessScanner>());

    // Shortcut set (Ableton Live by default) and transport preferences.
    {
        const auto id = props().getValue ("tkKeyProfile", "ableton");
        for (int i = 0; i < tonkori::keys::profileNames().size(); ++i)
            if (tonkori::keys::profileId ((tonkori::keys::Profile) i) == id) keyProfile = (tonkori::keys::Profile) i;
        preRollSeconds = props().getDoubleValue ("tkPreRoll", 2.0);
        postRollSeconds = props().getDoubleValue ("tkPostRoll", 1.0);
        preRollOn = props().getBoolValue ("tkPreRollOn", false);
        postRollOn = props().getBoolValue ("tkPostRollOn", false);
    }
    commandManager.setFirstCommandTarget (this);
    applyKeyProfile (keyProfile, true);
    addKeyListener (commandManager.getKeyMappings());
    startTimerHz (30);
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
    stopTimer();
    settingsWindow = nullptr;
    loudnessWindow = nullptr;
    videoWindow = nullptr;
    markerWindow = nullptr;
    videoThumbs = nullptr;
    loudnessHub.setTap (nullptr);
    loudnessHub.setEdit (nullptr);
    engine.getDeviceManager().setGlobalOutputAudioProcessor (nullptr);
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
    videoWindow = nullptr;
    markerWindow = nullptr;
    loudnessWindow = nullptr;
    videoThumbs = nullptr;
    loudnessHub.setEdit (nullptr);
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

    view->loudness = &loudnessHub;
    loudnessHub.setEdit (edit.get());
    videoWindow = nullptr;
    markerWindow = nullptr;
    attachSessionVideo();

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
    view->onOpenCrossfade = [this] (te::AudioClipBase& a, te::AudioClipBase& b) { openCrossfadeEditor (a, b); };
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
        if (tonkori::video::isVideoFile (f)) { importVideoFile (f); continue; }

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
                                  if (! out.existsAsFile()) { statusBar.flashMessage ("Export failed"); return; }
                                  const auto lr = tonkori::loudness::analyse (out, engine.getAudioFileFormatManager().readFormatManager);
                                  statusBar.flashMessage ("Exported " + out.getFileName() + ":  " + tonkori::loudness::describe (lr));
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
        stopAt.reset();
        t.setPosition (insertMarker());
        if (wasRecording) { absorbNewRecordings(); saveCurrentEdit(); }
        return;
    }

    // Pro Tools: a time selection plays from its start (minus pre-roll) to its end (plus post-roll);
    // without one, playback starts at the insert marker (minus pre-roll) and runs on.
    auto start = insertMarker();
    stopAt.reset();
    if (view && view->timeSelection)
    {
        start = view->timeSelection->getStart();
        if (postRollOn || preRollOn)
            stopAt = view->timeSelection->getEnd() + te::TimeDuration::fromSeconds (postRollOn ? postRollSeconds : 0.0);
    }
    if (preRollOn) start = juce::jmax (0_tp, start - te::TimeDuration::fromSeconds (preRollSeconds));
    t.setPosition (start);
    t.play (false);
}

void MainComponent::continuePlay()
{
    if (edit == nullptr) return;
    auto& t = edit->getTransport();

    if (t.isPlaying())
    {
        stopPosition = t.getPosition();
        const bool wasRecording = t.isRecording();
        t.stop (false, false);
        stopAt.reset();
        if (wasRecording) { absorbNewRecordings(); saveCurrentEdit(); }
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
        stopAt.reset();
        t.setPosition (insertMarker());
        absorbNewRecordings();
        saveCurrentEdit();
    }
    else
    {
        beginRecordingSnapshot();
        if (! t.isPlaying())
        {
            auto start = insertMarker();
            if (edit->recordingPunchInOut.get())
            {
                ensureLoopRange();
                start = t.getLoopRange().getStart();
                if (postRollOn) stopAt = t.getLoopRange().getEnd() + te::TimeDuration::fromSeconds (postRollSeconds);
            }
            if (preRollOn) start = juce::jmax (0_tp, start - te::TimeDuration::fromSeconds (preRollSeconds));
            t.setPosition (start);
        }
        else if (edit->recordingPunchInOut.get()) ensureLoopRange();
        t.record (false);
    }
}

void MainComponent::beginRecordingSnapshot()
{
    clipsBeforeRecording.clear();
    for (auto* track : te::getAudioTracks (*edit))
    {
        juce::Array<te::EditItemID> ids;
        for (auto* c : track->getClips()) ids.add (c->itemID);
        clipsBeforeRecording[track->itemID] = ids;
    }
}

void MainComponent::absorbNewRecordings()
{
    // Runs once the engine has added the new clips: keep every pass as a take lane.
    auto snapshot = clipsBeforeRecording;
    clipsBeforeRecording.clear();
    juce::Component::SafePointer<MainComponent> safe (this);
    juce::Timer::callAfterDelay (250, [safe, snapshot]
    {
        if (safe == nullptr || safe->edit == nullptr) return;
        auto& e = *safe->edit;
        bool any = false;
        for (auto* track : te::getAudioTracks (e))
        {
            auto it = snapshot.find (track->itemID);
            if (it == snapshot.end()) continue;
            juce::Array<te::Clip*> fresh;
            for (auto* c : track->getClips()) if (! it->second.contains (c->itemID)) fresh.add (c);

            for (auto* c : fresh)
            {
                bool overlaps = tonkori::takes::numLanes (*track) > 0;
                for (auto* o : track->getClips())
                    if (o != c && ! fresh.contains (o) && tonkori::takes::laneOf (*o) == 0 && o->getPosition().time.overlaps (c->getPosition().time)) overlaps = true;
                auto* wc = dynamic_cast<te::WaveAudioClip*> (c);
                const bool loopTakes = wc != nullptr && wc->getNumTakes (false) > 1;
                if (overlaps || loopTakes) { tonkori::takes::absorbRecording (*c); any = true; }
            }
        }
        if (any) { safe->statusBar.flashMessage ("Take kept on a new lane (L on the track shows the takes)"); safe->view->changed(); }
    });
}

void MainComponent::timerCallback()
{
    if (edit == nullptr) return;
    auto& t = edit->getTransport();
    if (stopAt && t.isPlaying() && t.getPosition() >= *stopAt)
    {
        const bool wasRecording = t.isRecording();
        stopPosition = t.getPosition();
        t.stop (false, false);
        stopAt.reset();
        t.setPosition (insertMarker());
        if (wasRecording) { absorbNewRecordings(); saveCurrentEdit(); }
    }
    if (! t.isPlaying()) stopAt.reset();
}

void MainComponent::saveTransportPrefs()
{
    props().setValue ("tkPreRoll", preRollSeconds);
    props().setValue ("tkPostRoll", postRollSeconds);
    props().setValue ("tkPreRollOn", preRollOn);
    props().setValue ("tkPostRollOn", postRollOn);
    props().saveIfNeeded();
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
                for (auto* c : mainClips (*ct))
                    if (c->getPosition().time.overlaps (range)) inRange.add (c);

        if (! inRange.isEmpty())
        {
            auto content = std::make_unique<te::Clipboard::Clips>();
            content->addSelectedClips (inRange, range, te::Clipboard::Clips::AutomationLocked::no);
            te::Clipboard::getInstance()->setContent (std::move (content));
        }

        // Cut removes the range from the selected tracks only (it used to clear every track).
        if (alsoDelete)
            deleteSelection();
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
        for (auto* t : tracks)
            if (auto* ct = dynamic_cast<te::ClipTrack*> (t))
            {
                tonkori::takes::clearMainLane (*ct, range);

                // Shuffle closes the gap on every lane, so takes stay aligned with the comp.
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
        for (auto* c : mainClips (*t))
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
                    for (auto* c : mainClips (*ct))
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
                for (auto* c : mainClips (*t))
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
                for (auto* c : mainClips (*ct))
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

                for (auto* c : mainClips (*ct))
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
// Shortcut sets
void MainComponent::applyKeyProfile (tonkori::keys::Profile p, bool loadUserChanges)
{
    keyProfile = p;
    commandManager.clearCommands();
    commandManager.registerAllCommandsForTarget (this);
    auto* km = commandManager.getKeyMappings();
    km->resetToDefaultMappings();
    if (loadUserChanges)
        if (auto xml = juce::parseXML (props().getValue ("tkKeys_" + tonkori::keys::profileId (p))))
            km->restoreFromXml (*xml);
    props().setValue ("tkKeyProfile", tonkori::keys::profileId (p));
    props().saveIfNeeded();
    commandManager.commandStatusChanged();
    menuItemsChanged();
}

void MainComponent::saveKeyMappings()
{
    if (auto xml = commandManager.getKeyMappings()->createXml (true))
        props().setValue ("tkKeys_" + tonkori::keys::profileId (keyProfile), xml->toString());
    props().saveIfNeeded();
    menuItemsChanged();
}

void MainComponent::resetKeyMappings()
{
    props().removeValue ("tkKeys_" + tonkori::keys::profileId (keyProfile));
    applyKeyProfile (keyProfile, false);
}

//==============================================================================
// Inputs and metering
void MainComponent::configureInputs (bool applyLayout)
{
    auto& dm = engine.getDeviceManager();
    if (applyLayout)
        dm.setAllWaveInputsToNumChannels (props().getIntValue ("tkInputsMono", 1) ? 1u : 2u);

    for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
        if (auto* wip = dm.getWaveInDevice (i))
        {
            if (! wip->isEnabled()) wip->setEnabled (true);
            static std::set<juce::String> configured;              // auto monitoring once per device, then the user's choice stands
            if (configured.insert (wip->getDeviceID()).second)
                wip->setMonitorMode (te::InputDevice::MonitorMode::automatic);
        }
    tonkori::EngineProfile::applyToInputs (engine);
}

void MainComponent::updateTapChannels()
{
    if (outputTap == nullptr) return;
    if (auto* out = engine.getDeviceManager().getDefaultWaveOutDevice())
    {
        const auto& ch = out->getChannels();
        if (ch.getNumChannels() > 0)
        {
            outputTap->firstChannel = juce::jmax (0, ch[0].indexInDevice);
            outputTap->numChannels = juce::jmin (2, ch.getNumChannels());
        }
    }
}

//==============================================================================
// Tool windows
void MainComponent::showSettings (int tab)
{
    settingsWindow = std::make_unique<ToolWindow> ("Tonkori Settings", new tui::SettingsComponent (*this, tab), true);
    settingsWindow->setVisible (true);
    settingsWindow->toFront (true);
}

void MainComponent::showLoudness()
{
    if (loudnessWindow == nullptr)
    {
        auto* m = new tui::LoudnessMeter (loudnessHub, false);
        m->setSize (620, 230);
        loudnessWindow = std::make_unique<ToolWindow> ("Loudness", m, true, commandManager.getKeyMappings());
        loudnessWindow->setAlwaysOnTop (true);
    }
    loudnessWindow->setVisible (! loudnessWindow->isVisible());
}

namespace markerlist
{
    /** Marker list (Pro Tools memory locations / Pyramix markers tab). */
    class MarkerList : public juce::Component, private juce::TableListBoxModel, private juce::Timer
    {
    public:
        MarkerList (te::Edit& e, tui::ViewState& v) : edit (e), vs (v)
        {
            table.setModel (this);
            table.getHeader().addColumn ("#", 1, 40);
            table.getHeader().addColumn ("Name", 2, 220);
            table.getHeader().addColumn ("Position", 3, 140);
            table.setColour (juce::ListBox::backgroundColourId, tui::colours::bg);
            table.setMultipleSelectionEnabled (false);
            addAndMakeVisible (table);
            add.setButtonText ("+ Add at insert marker");
            add.onClick = [this]
            {
                edit.getUndoManager().beginNewTransaction ("Add marker");
                edit.ensureMarkerTrack();
                auto& mm = edit.getMarkerManager();
                const int n = mm.getNextUniqueID();
                if (auto m = mm.createMarker (n, vs.insertMarker, te::TimeDuration(), nullptr)) m->setName ("Marker " + juce::String (n));
                vs.changed();
            };
            addAndMakeVisible (add);
            del.setButtonText ("Delete");
            del.onClick = [this]
            {
                if (auto m = markers()[table.getSelectedRow()]) { edit.getUndoManager().beginNewTransaction ("Delete marker"); m->removeFromParent(); vs.changed(); }
            };
            addAndMakeVisible (del);
            setSize (420, 360);
            startTimerHz (4);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            auto b = r.removeFromBottom (28);
            add.setBounds (b.removeFromLeft (190));
            b.removeFromLeft (6);
            del.setBounds (b.removeFromLeft (90));
            r.removeFromBottom (6);
            table.setBounds (r);
        }

    private:
        te::Edit& edit;
        tui::ViewState& vs;
        juce::TableListBox table;
        juce::TextButton add, del;
        int lastCount = -1;

        juce::ReferenceCountedArray<te::MarkerClip> markers() const
        {
            auto m = edit.getMarkerManager().getMarkers();
            std::sort (m.begin(), m.end(), [] (auto* a, auto* b) { return a->getPosition().getStart() < b->getPosition().getStart(); });
            return m;
        }

        int getNumRows() override { return markers().size(); }

        void paintRowBackground (juce::Graphics& g, int, int, int, bool selected) override
        {
            g.fillAll (selected ? tui::colours::accent.withAlpha (0.35f) : tui::colours::bg);
        }

        void paintCell (juce::Graphics& g, int row, int col, int w, int h, bool) override
        {
            auto m = markers()[row];
            if (m == nullptr) return;
            g.setColour (tui::colours::text);
            g.setFont (col == 3 ? tui::mono (12.0f) : tui::font (12.5f));
            const auto text = col == 1 ? juce::String (m->getMarkerID()) : (col == 2 ? m->getName() : vs.format (m->getPosition().getStart()));
            g.drawText (text, 6, 0, w - 8, h, juce::Justification::centredLeft, true);
        }

        void cellDoubleClicked (int row, int col, const juce::MouseEvent&) override
        {
            auto m = markers()[row];
            if (m == nullptr) return;
            if (col == 2)
            {
                auto* aw = new juce::AlertWindow ("Rename marker", {}, juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor ("name", m->getName());
                aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                te::Clip::Ptr keep (m.get());
                aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, keep] (int r)
                {
                    if (r == 1 && keep) { edit.getUndoManager().beginNewTransaction ("Rename marker"); keep->setName (aw->getTextEditorContents ("name")); }
                }), true);
                return;
            }
            vs.insertMarker = m->getPosition().getStart();
            edit.getTransport().setPosition (vs.insertMarker);
            vs.changed();
        }

        void deleteKeyPressed (int row) override
        {
            if (auto m = markers()[row]) { edit.getUndoManager().beginNewTransaction ("Delete marker"); m->removeFromParent(); vs.changed(); }
        }

        void timerCallback() override { table.updateContent(); table.repaint(); }
    };
}

void MainComponent::showMarkerList()
{
    if (edit == nullptr || view == nullptr) return;
    if (markerWindow == nullptr)
        markerWindow = std::make_unique<ToolWindow> ("Markers", new markerlist::MarkerList (*edit, *view), true, commandManager.getKeyMappings());
    markerWindow->setVisible (! markerWindow->isVisible());
    if (markerWindow->isVisible()) markerWindow->toFront (true);
}

//==============================================================================
// Video
double MainComponent::getVideoOffsetMs()
{
    return edit ? tonkori::video::SessionVideo::get (*edit).syncOffsetMs : 0.0;
}

void MainComponent::setVideoOffsetMs (double ms)
{
    if (edit == nullptr) return;
    auto v = tonkori::video::SessionVideo::get (*edit);
    if (! v.isValid()) return;
    v.syncOffsetMs = juce::jlimit (-2000.0, 2000.0, ms);
    tonkori::video::SessionVideo::set (*edit, v);
}

void MainComponent::importVideo()
{
    if (edit == nullptr) return;
    chooser = std::make_shared<juce::FileChooser> ("Import video", juce::File::getSpecialLocation (juce::File::userMoviesDirectory),
                                                   tonkori::video::fileExtensions().joinIntoString (";"));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) importVideoFile (fc.getResult()); });
}

void MainComponent::importVideoFile (const juce::File& f)
{
    if (edit == nullptr) return;
    juce::String err;
    auto vf = tonkori::video::VideoFile::open (f, err);
    if (vf == nullptr || ! vf->getInfo().hasVideo)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Video import", err.isNotEmpty() ? err : "No picture stream in " + f.getFileName());
        return;
    }
    const auto info = vf->getInfo();

    auto* aw = new juce::AlertWindow ("Import Video", f.getFileName() + "\n" + info.describe(), juce::MessageBoxIconType::NoIcon);
    aw->addComboBox ("at", { "At session start (0)", "At the insert marker" });
    juce::StringArray audioChoices { "Picture only" };
    if (! info.audio.empty()) audioChoices.add ("Picture + import its audio (" + juce::String ((int) info.audio.size()) + " stream" + (info.audio.size() > 1 ? "s" : "") + ")");
    aw->addComboBox ("audio", audioChoices);
    if (! info.audio.empty()) aw->getComboBoxComponent ("audio")->setSelectedItemIndex (1);
    aw->addButton ("Import", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, f, info] (int r)
    {
        if (r == 0 || edit == nullptr) return;
        const double start = aw->getComboBoxComponent ("at")->getSelectedItemIndex() == 1 ? insertMarker().inSeconds() : 0.0;
        const bool withAudio = aw->getComboBoxComponent ("audio")->getSelectedItemIndex() == 1;

        edit->getUndoManager().beginNewTransaction ("Import video");
        tonkori::video::SessionVideo sv;
        sv.file = f;
        sv.start = start;
        tonkori::video::SessionVideo::set (*edit, sv);
        attachSessionVideo();

        if (withAudio)
        {
            const double rate = engine.getDeviceManager().getSampleRate() > 0 ? engine.getDeviceManager().getSampleRate() : 48000.0;
            for (size_t i = 0; i < info.audio.size(); ++i)
            {
                auto dest = getSessionAudioFolder().getNonexistentChildFile (f.getFileNameWithoutExtension() + " audio " + juce::String ((int) i + 1), ".wav", false);
                struct Job : public juce::ThreadWithProgressWindow
                {
                    Job (juce::File s, juce::File d, int idx, double rt) : ThreadWithProgressWindow ("Extracting audio from video...", true, true), src (s), dst (d), index (idx), rate (rt) {}
                    void run() override
                    {
                        juce::String e;
                        auto v = tonkori::video::VideoFile::open (src, e);
                        ok = v != nullptr && v->extractAudio (index, dst, rate, [this] (double p) { setProgress (p); return ! threadShouldExit(); }, error);
                        if (v == nullptr) error = e;
                    }
                    juce::File src, dst; int index; double rate; bool ok = false; juce::String error;
                };
                Job job (f, dest, info.audio[i].index, rate);
                job.runThread();
                if (! job.ok) { statusBar.flashMessage ("Audio extraction failed: " + job.error); continue; }

                const int n = te::getAudioTracks (*edit).size();
                edit->ensureNumberOfAudioTracks (n + 1);
                if (auto* t = te::getAudioTracks (*edit).getLast())
                {
                    t->setName (f.getFileNameWithoutExtension() + (info.audio.size() > 1 ? " A" + juce::String ((int) i + 1) : juce::String (" audio")));
                    te::AudioFile af (engine, dest);
                    t->insertWaveClip (t->getName(), dest, { { te::TimePosition::fromSeconds (start), te::TimePosition::fromSeconds (start + af.getLength()) }, {} }, false);
                }
            }
        }
        saveCurrentEdit();
        showVideoWindow();
        statusBar.flashMessage ("Video: " + info.describe());
    }), true);
}

void MainComponent::attachSessionVideo()
{
    videoThumbs = nullptr;
    if (edit == nullptr || view == nullptr) return;
    auto sv = tonkori::video::SessionVideo::get (*edit);
    auto& v = view->video;
    v = {};
    if (! sv.isValid()) { if (arrange) arrange->repaint(); return; }

    juce::String err;
    auto vf = tonkori::video::VideoFile::open (sv.file, err);
    if (vf == nullptr) { statusBar.flashMessage ("Video missing or unreadable: " + sv.file.getFileName()); return; }
    const auto info = vf->getInfo();

    videoThumbs = std::make_unique<tui::ThumbnailCache> (sv.file);
    juce::Component::SafePointer<MainComponent> safe (this);
    videoThumbs->onReady = [safe] { if (safe && safe->arrange) safe->arrange->repaint(); };

    v.present = true;
    v.start = sv.start;
    v.length = info.durationSec;
    v.fps = info.fps;
    v.aspect = info.height > 0 ? (double) info.width / info.height : 16.0 / 9.0;
    v.name = sv.file.getFileName() + "  " + juce::String (info.width) + "x" + juce::String (info.height) + " " + juce::String (info.fps, 2) + " fps";
    v.thumbAt = [safe] (double s) { return safe && safe->videoThumbs ? safe->videoThumbs->get (s) : juce::Image(); };
    v.setStart = [safe] (double ns)
    {
        if (safe == nullptr || safe->edit == nullptr) return;
        auto cur = tonkori::video::SessionVideo::get (*safe->edit);
        cur.start = ns;
        tonkori::video::SessionVideo::set (*safe->edit, cur);
        safe->view->video.start = ns;
    };
    v.openWindow = [safe] { if (safe) safe->showVideoWindow(); };
    v.remove = [safe] { if (safe) safe->removeVideo(); };
    v.spot = [safe] { if (safe) safe->spotVideo(); };
    if (arrange) { arrange->resized(); arrange->repaint(); }
    view->changed();
}

void MainComponent::showVideoWindow()
{
    if (edit == nullptr || ! view || ! view->video.present) { statusBar.flashMessage ("No video in this session: File > Import Video"); return; }
    if (videoWindow == nullptr)
    {
        auto sv = tonkori::video::SessionVideo::get (*edit);
        juce::Component::SafePointer<MainComponent> safe (this);
        auto* panel = new tui::VideoPanel (*edit, sv.file,
                                           [safe] { return safe && safe->view ? safe->view->video.start : 0.0; },
                                           [safe] { return safe ? safe->getVideoOffsetMs() : 0.0; },
                                           [safe] { return safe ? safe->insertMarker() : te::TimePosition(); });
        videoWindow = std::make_unique<ToolWindow> ("Video - " + sv.file.getFileName(), panel, true, commandManager.getKeyMappings());
        videoWindow->setResizeLimits (320, 200, 7680, 4400);
        videoWindow->setAlwaysOnTop (true);
    }
    videoWindow->setVisible (! videoWindow->isVisible());
}

void MainComponent::spotVideo()
{
    if (edit == nullptr || ! view->video.present) return;
    auto* aw = new juce::AlertWindow ("Spot video", "Timeline position of the first frame", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("start", view->format (te::TimePosition::fromSeconds (view->video.start)));
    aw->addButton ("Spot", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
    {
        if (r == 0 || edit == nullptr) return;
        if (auto t = view->parse (aw->getTextEditorContents ("start")))
        {
            edit->getUndoManager().beginNewTransaction ("Spot video");
            if (view->video.setStart) view->video.setStart (t->inSeconds());
            view->changed();
        }
    }), true);
}

void MainComponent::removeVideo()
{
    if (edit == nullptr) return;
    edit->getUndoManager().beginNewTransaction ("Remove video");
    videoWindow = nullptr;
    tonkori::video::SessionVideo::clear (*edit);
    attachSessionVideo();
    if (arrange) arrange->resized();
}

juce::File MainComponent::renderRange (te::TimeRange range, const juce::File& dest, int bits, juce::Array<te::Track*> tracks, bool masterPlugins)
{
    te::Renderer::Parameters p (*edit);
    p.tracksToDo = te::toBitSet (tracks);
    p.destFile = dest;
    p.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
    p.bitDepth = bits;
    p.ditheringEnabled = bits < 32;
    p.sampleRateForAudio = engine.getDeviceManager().getSampleRate() > 0 ? engine.getDeviceManager().getSampleRate() : 48000.0;
    p.blockSizeForAudio = 512;
    p.time = range;
    p.usePlugins = true;
    p.useMasterPlugins = masterPlugins;
    p.canRenderInMono = false;
    dest.deleteFile();
    return te::Renderer::renderToFile ("Rendering " + dest.getFileName(), p);
}

void MainComponent::exportVideo()
{
    if (edit == nullptr || view == nullptr) return;
    auto sv = tonkori::video::SessionVideo::get (*edit);
    if (! sv.isValid()) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Export Video", "Import a video first (File > Import Video)."); return; }

    namespace V = tonkori::video;
    juce::String err;
    auto vf = V::VideoFile::open (sv.file, err);
    if (vf == nullptr) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export Video", err); return; }
    const auto info = vf->getInfo();

    std::vector<V::Codec> codecs;
    juce::StringArray codecLabels;
    for (int i = 0; i < V::codecNames().size(); ++i)
        if (V::isEncoderAvailable ((V::Codec) i)) { codecs.push_back ((V::Codec) i); codecLabels.add (V::codecNames()[i]); }

    auto* aw = new juce::AlertWindow ("Export Video", "Picture: " + info.describe() + "\nAudio: the mix through the master bus.", juce::MessageBoxIconType::NoIcon);
    aw->addComboBox ("codec", codecLabels, "Codec");
    for (int i = 0; i < (int) codecs.size(); ++i) if (codecs[(size_t) i] == V::Codec::proresHQ) aw->getComboBoxComponent ("codec")->setSelectedItemIndex (i);
    aw->addComboBox ("size", { "Source (" + juce::String (info.width) + "x" + juce::String (info.height) + ")", "UHD 3840 wide", "HD 1920 wide", "1280 wide", "Half size" }, "Resolution");
    aw->addComboBox ("audio", { "PCM 24-bit", "PCM 32-bit float", "AAC 320 kb/s (MP4)" }, "Audio");
    aw->addComboBox ("container", { "QuickTime .mov", "MXF (DNxHR, OP1a)" }, "DNxHR container");
    const bool hasSel = view->timeSelection.has_value();
    juce::StringArray ranges { "Length of the video" };
    if (hasSel) ranges.add ("Time selection");
    aw->addComboBox ("range", ranges, "Range");
    aw->addTextEditor ("tc", info.timecode.isNotEmpty() ? info.timecode : juce::String ("00:00:00:00"), "Start timecode");
    aw->addButton ("Choose File...", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, codecs, info, sv, hasSel] (int r)
    {
        if (r == 0 || edit == nullptr) return;
        V::ExportParams p;
        p.source = sv.file;
        p.videoStart = sv.start;
        p.codec = codecs[(size_t) juce::jmax (0, aw->getComboBoxComponent ("codec")->getSelectedItemIndex())];
        const int size = aw->getComboBoxComponent ("size")->getSelectedItemIndex();
        const double aspect = info.height > 0 ? (double) info.width / info.height : 16.0 / 9.0;
        auto widthFor = [&] (int w) { p.width = w; p.height = ((int) std::lround (w / aspect)) & ~1; };
        if (size == 1) widthFor (3840); else if (size == 2) widthFor (1920); else if (size == 3) widthFor (1280);
        else if (size == 4) widthFor ((info.width / 2) & ~1);
        const int audio = aw->getComboBoxComponent ("audio")->getSelectedItemIndex();
        p.audio = audio == 1 ? V::AudioFormat::pcm32f : (audio == 2 ? V::AudioFormat::aac320 : V::AudioFormat::pcm24);
        const bool useSel = hasSel && aw->getComboBoxComponent ("range")->getSelectedItemIndex() == 1;
        p.rangeStart = useSel ? view->timeSelection->getStart().inSeconds() : sv.start;
        p.rangeEnd = useSel ? view->timeSelection->getEnd().inSeconds() : sv.start + info.durationSec;
        if (p.codec == V::Codec::sameAsSource) { p.rangeStart = sv.start; p.rangeEnd = sv.start + info.durationSec; }
        p.timecode = aw->getTextEditorContents ("tc").trim();
        const bool mxf = aw->getComboBoxComponent ("container")->getSelectedItemIndex() == 1;
        if (p.audio == V::AudioFormat::aac320 && V::containerFor (p.codec, false) != ".mp4") p.audio = V::AudioFormat::pcm24;

        auto ext = V::containerFor (p.codec, mxf);
        if (p.codec == V::Codec::sameAsSource) ext = sv.file.getFileExtension().isNotEmpty() ? sv.file.getFileExtension() : juce::String (".mov");
        auto start = getSessionsFolder().getParentDirectory().getChildFile ("Exports");
        start.createDirectory();
        chooser = std::make_shared<juce::FileChooser> ("Export video", start.getChildFile (edit->getName() + ext), "*" + ext);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, p, ext] (const juce::FileChooser& fc) mutable
        {
            auto out = fc.getResult();
            if (out == juce::File() || edit == nullptr) return;
            p.output = out.withFileExtension (ext);

            // 1. the mix for exactly the picture range
            auto wav = engine.getTemporaryFileManager().getTempFile ("video-mix.wav");
            p.audioWav = renderRange ({ te::TimePosition::fromSeconds (p.rangeStart), te::TimePosition::fromSeconds (p.rangeEnd) }, wav, 32, te::getAllTracks (*edit), true);
            if (! p.audioWav.existsAsFile()) { statusBar.flashMessage ("Mix render failed"); return; }

            // 2. picture + mix
            struct Job : public juce::ThreadWithProgressWindow
            {
                Job (V::ExportParams pp) : ThreadWithProgressWindow ("Exporting video...", true, true), params (pp) {}
                void run() override { ok = V::exportVideo (params, [this] (double x) { setProgress (x); return ! threadShouldExit(); }, error); }
                V::ExportParams params; bool ok = false; juce::String error;
            };
            Job job (p);
            job.runThread();
            wav.deleteFile();
            if (! job.ok) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Video export failed", job.error); return; }
            statusBar.flashMessage ("Exported " + p.output.getFileName() + "  (" + V::codecNames()[(int) p.codec] + ")");
        });
    }), true);
}

void MainComponent::exportStems()
{
    if (edit == nullptr || view == nullptr) return;
    const auto length = edit->getLength();
    if (length <= te::TimeDuration()) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Export Stems", "The session is empty."); return; }

    const bool hasRange = view->timeSelection.has_value();
    auto* aw = new juce::AlertWindow ("Export Stems", "One file per track, post-fader and post-insert, time-aligned from the same start.\n"
                                      "Without master processing, the stems sum exactly to the mix (null-tested).", juce::MessageBoxIconType::NoIcon);
    aw->addComboBox ("which", { "All audio tracks", "All tracks and buses", "Selected tracks" }, "Tracks");
    aw->addComboBox ("format", { "WAV 32-bit float", "WAV 24-bit (TPDF dither)", "WAV 16-bit (TPDF dither)" }, "Format");
    aw->addComboBox ("master", { "Without master bus processing", "Through master bus processing" }, "Master");
    aw->addComboBox ("range", hasRange ? juce::StringArray { "Time selection", "Whole session" } : juce::StringArray { "Whole session" }, "Range");
    aw->addButton ("Choose Folder...", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, hasRange] (int r)
    {
        if (r == 0 || edit == nullptr) return;
        const int which = aw->getComboBoxComponent ("which")->getSelectedItemIndex();
        const int fmt = aw->getComboBoxComponent ("format")->getSelectedItemIndex();
        const bool master = aw->getComboBoxComponent ("master")->getSelectedItemIndex() == 1;
        const bool useRange = hasRange && aw->getComboBoxComponent ("range")->getSelectedItemIndex() == 0;
        const int bits = fmt == 0 ? 32 : (fmt == 1 ? 24 : 16);
        const te::TimeRange range = useRange ? *view->timeSelection : te::TimeRange (0_tp, te::TimePosition::fromSeconds (edit->getLength().inSeconds()));

        juce::Array<te::AudioTrack*> tracks;
        for (auto* t : te::getAudioTracks (*edit))
        {
            const bool bus = tonkori::routing::isBus (*t);
            if (which == 0 && ! bus) tracks.add (t);
            if (which == 1) tracks.add (t);
            if (which == 2 && getSelectedTracks().contains (t)) tracks.add (t);
        }
        if (tracks.isEmpty()) { statusBar.flashMessage ("No tracks to export"); return; }

        auto start = getSessionsFolder().getParentDirectory().getChildFile ("Exports");
        start.createDirectory();
        chooser = std::make_shared<juce::FileChooser> ("Folder for stems", start, "*");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this, tracks, bits, master, range] (const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (! dir.isDirectory() || edit == nullptr) return;
            auto folder = dir.getNonexistentChildFile (edit->getName() + " stems", {}, false);
            folder.createDirectory();
            int done = 0, idx = 1;
            for (auto* t : tracks)
            {
                auto name = juce::String (idx++).paddedLeft ('0', 2) + " " + juce::File::createLegalFileName (t->getName());
                auto f = renderRange (range, folder.getChildFile (edit->getName() + " - " + name + ".wav"), bits, { t }, master);
                if (f.existsAsFile()) ++done;
            }
            statusBar.flashMessage ("Exported " + juce::String (done) + " stem" + (done == 1 ? "" : "s") + " to " + folder.getFileName());
            folder.revealToUser();
        });
    }), true);
}

//==============================================================================
// Editing commands
juce::Array<te::Clip*> MainComponent::clipsForCommand() const
{
    auto clips = getSelectedClips();
    if (! clips.isEmpty() || edit == nullptr) return clips;
    const auto pos = cursor();
    auto tracks = getSelectedTracks();
    for (auto* t : te::getClipTracks (*edit))
        if (tracks.isEmpty() || tracks.contains (t))
            for (auto* c : mainClips (*t))
                if (c->getPosition().time.contains (pos)) clips.add (c);
    return clips;
}

void MainComponent::healSelection()
{
    if (edit == nullptr) return;
    int healed = 0;
    juce::Array<te::ClipTrack*> tracks;
    for (auto* c : clipsForCommand()) if (auto* ct = dynamic_cast<te::ClipTrack*> (c->getTrack())) tracks.addIfNotAlreadyThere (ct);

    for (auto* ct : tracks)
    {
        bool again = true;
        while (again)
        {
            again = false;
            auto clips = mainClips (*ct);
            for (auto* a : clips)
            {
                auto* wa = dynamic_cast<te::WaveAudioClip*> (a);
                if (wa == nullptr || ! (getSelectedClips().isEmpty() || getSelectedClips().contains (a))) continue;
                for (auto* b : clips)
                {
                    auto* wb = dynamic_cast<te::WaveAudioClip*> (b);
                    if (wb == nullptr || wb == wa) continue;
                    const auto pa = wa->getPosition(), pb = wb->getPosition();
                    const bool touching = std::abs ((pb.getStart() - pa.getEnd()).inSeconds()) < 1.0e-6;
                    const bool sameSource = wa->getOriginalFile() == wb->getOriginalFile() && wa->getSpeedRatio() == wb->getSpeedRatio()
                                            && wa->getGainDB() == wb->getGainDB() && wa->getIsReversed() == wb->getIsReversed();
                    const bool contiguous = std::abs ((pb.getOffset() - (pa.getOffset() + pa.getLength())).inSeconds()) < 1.0e-6;
                    if (touching && sameSource && contiguous)
                    {
                        const auto fadeOut = wb->getFadeOut();
                        const auto spec = tonkori::fades::getSpec (*wb, false);
                        wb->removeFromParent();
                        wa->setEnd (pb.getEnd(), true);
                        tonkori::fades::setLength (*wa, false, fadeOut);
                        if (fadeOut > te::TimeDuration()) tonkori::fades::setSpec (*wa, false, spec);
                        ++healed;
                        again = true;
                        break;
                    }
                }
                if (again) break;
            }
        }
    }
    statusBar.flashMessage (healed > 0 ? "Healed " + juce::String (healed) + " separation" + (healed > 1 ? "s" : "") : "Nothing to heal (clips must be adjacent pieces of the same audio)");
}

void MainComponent::trimToCursor (bool start)
{
    const auto pos = cursor();
    for (auto* c : clipsForCommand())
    {
        const auto p = c->getPosition();
        if (! p.time.contains (pos) || pos == p.getStart()) continue;
        if (start) c->setStart (pos, true, false);
        else c->setEnd (pos, true);
    }
}

void MainComponent::trimToSelection()
{
    if (view == nullptr || ! view->timeSelection) { statusBar.flashMessage ("Trim to selection needs a time selection"); return; }
    const auto r = *view->timeSelection;
    for (auto* t : getRangeTracks())
        if (auto* ct = dynamic_cast<te::ClipTrack*> (t))
            for (auto* c : mainClips (*ct))
            {
                const auto p = c->getPosition();
                if (! p.time.overlaps (r)) continue;
                if (p.getStart() < r.getStart()) c->setStart (r.getStart(), true, false);
                if (c->getPosition().getEnd() > r.getEnd()) c->setEnd (r.getEnd(), true);
            }
}

void MainComponent::nudgeClipGain (double db)
{
    for (auto* c : getSelectedAudioClips())
    {
        auto pts = tonkori::gainline::getPoints (*c);
        if (! pts.empty())
        {
            for (auto& pt : pts) pt.db += db;
            tonkori::gainline::setPoints (*c, pts);
        }
        else c->setGainDB (juce::jlimit (-100.0f, 24.0f, c->getGainDB() + (float) db));
    }
}

void MainComponent::openCrossfadeEditor (te::AudioClipBase& a, te::AudioClipBase& b)
{
    tonkori::xfade::Pair pr { &a, &b };
    if (! a.getPosition().time.overlaps (b.getPosition().time) && std::abs ((b.getPosition().getStart() - a.getPosition().getEnd()).inSeconds()) > 1e-6)
    {
        statusBar.flashMessage ("Those clips don't meet: the crossfade editor works on a cut or an overlap");
        return;
    }
    // A butt edit gets a short centred crossfade to start from.
    if (! a.getPosition().time.overlaps (b.getPosition().time))
    {
        edit->getUndoManager().beginNewTransaction ("Crossfade");
        tonkori::xfade::Params prm;
        prm.splice = b.getPosition().getStart().inSeconds();
        prm.length = 0.010;
        tonkori::xfade::apply (pr, prm);
    }
    auto ed = std::make_unique<tui::CrossfadeEditor> (*view, pr);
    auto area = arrange->getScreenBoundsOfClip (b).withWidth (40).translated (-20, 0);
    juce::CallOutBox::launchAsynchronously (std::move (ed), area, nullptr);
}

void MainComponent::openCrossfadeForSelection()
{
    auto clips = getSelectedAudioClips();
    if (clips.size() >= 2)
    {
        std::sort (clips.begin(), clips.end(), [] (auto* x, auto* y) { return x->getPosition().getStart() < y->getPosition().getStart(); });
        openCrossfadeEditor (*clips[0], *clips[1]);
        return;
    }
    if (clips.size() == 1)
    {
        auto* c = clips.getFirst();
        const bool nearStart = cursor() < c->getPosition().getStart() + c->getPosition().getLength() * 0.5;
        const int lane = tonkori::takes::laneOf (*c);
        if (nearStart) { if (auto* o = tonkori::xfade::findOutgoing (*c, lane)) { openCrossfadeEditor (*o, *c); return; } }
        else if (auto* i = tonkori::xfade::findIncoming (*c, lane)) { openCrossfadeEditor (*c, *i); return; }
        if (auto* o = tonkori::xfade::findOutgoing (*c, lane)) { openCrossfadeEditor (*o, *c); return; }
        if (auto* i = tonkori::xfade::findIncoming (*c, lane)) { openCrossfadeEditor (*c, *i); return; }
    }
    statusBar.flashMessage ("Select a clip next to a cut, or double-click an overlap");
}

void MainComponent::toggleLanesForSelection()
{
    auto tracks = getSelectedTracks();
    if (tracks.isEmpty() && edit) for (auto* t : te::getAudioTracks (*edit)) if (tonkori::takes::numLanes (*t) > 0) tracks.add (t);
    bool anyShown = false;
    for (auto* t : tracks) anyShown = anyShown || tonkori::takes::lanesShown (*t);
    for (auto* t : tracks) tonkori::takes::showLanes (*t, ! anyShown);
    if (view) view->changed();
}

void MainComponent::promoteSelectionToMain()
{
    int n = 0;
    for (auto* c : getSelectedClips())
        if (tonkori::takes::isTake (*c))
        {
            auto range = view && view->timeSelection ? *view->timeSelection : c->getPosition().time;
            if (tonkori::takes::promote (*c, range)) ++n;
        }
    statusBar.flashMessage (n > 0 ? "Comped from " + juce::String (n) + " take" + (n > 1 ? "s" : "") : "Select a clip on a take lane (show lanes with L)");
}

void MainComponent::addMarkerAtCursor()
{
    if (edit == nullptr) return;
    edit->ensureMarkerTrack();
    auto& mm = edit->getMarkerManager();
    const int n = mm.getNextUniqueID();
    if (auto m = mm.createMarker (n, cursor(), te::TimeDuration(), nullptr))
    {
        m->setName ("Marker " + juce::String (n));
        statusBar.flashMessage ("Marker " + juce::String (n) + " at " + view->format (cursor()));
    }
}

void MainComponent::jumpMarker (int direction)
{
    if (edit == nullptr) return;
    auto& mm = edit->getMarkerManager();
    const auto now = cursor();
    auto* m = direction > 0 ? mm.getNextMarker (now + te::TimeDuration::fromSeconds (1e-4))
                            : mm.getPrevMarker (now - te::TimeDuration::fromSeconds (1e-4));
    if (m == nullptr) return;
    setInsertMarker (m->getPosition().getStart());
    edit->getTransport().setPosition (m->getPosition().getStart());
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
    {
        configureInputs (false);                          // new device: enable its inputs, 32f takes
        updateTapChannels();
    }

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
        for (auto& k : tonkori::keys::keysFor (keyProfile, d->name, d->keys))
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
        case Cmd::toggleGainLines: info.setTicked (tonkori::gainline::isShown (*edit)); break;
        case Cmd::preRollToggle:   info.setTicked (preRollOn); break;
        case Cmd::postRollToggle:  info.setTicked (postRollOn); break;
        case Cmd::countInToggle:   info.setTicked (edit->getCountInMode() != te::Edit::CountIn::none); break;
        case Cmd::videoWindow:     info.setActive (view->video.present); info.setTicked (videoWindow && videoWindow->isVisible()); break;
        case Cmd::removeVideo:     info.setActive (view->video.present); break;
        case Cmd::exportVideo:     info.setActive (view->video.present); break;
        case Cmd::loudnessWindow:  info.setTicked (loudnessWindow && loudnessWindow->isVisible()); break;
        case Cmd::markerList:      info.setTicked (markerWindow && markerWindow->isVisible()); break;
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
        case Cmd::settings:      showSettings (0); break;
        case Cmd::latencySettings: showSettings (1); break;
        case Cmd::shortcutSettings: showSettings (2); break;
        case Cmd::pluginList:    showPluginList(); break;
        case Cmd::importVideo:   importVideo(); break;
        case Cmd::exportStems:   exportStems(); break;
        case Cmd::exportVideo:   exportVideo(); break;
        case Cmd::removeVideo:   removeVideo(); break;
        case Cmd::videoWindow:   showVideoWindow(); break;
        case Cmd::loudnessWindow: showLoudness(); break;
        case Cmd::markerList:    showMarkerList(); break;
        case Cmd::heal:          healSelection(); break;
        case Cmd::trimStartToCursor: trimToCursor (true); break;
        case Cmd::trimEndToCursor:   trimToCursor (false); break;
        case Cmd::trimToSelection:   trimToSelection(); break;
        case Cmd::crossfadeEditor:   openCrossfadeForSelection(); break;
        case Cmd::clipGainUp:        nudgeClipGain (1.0); break;
        case Cmd::clipGainDown:      nudgeClipGain (-1.0); break;
        case Cmd::toggleGainLines:   tonkori::gainline::setShown (*edit, ! tonkori::gainline::isShown (*edit)); break;
        case Cmd::clearGainLine:     for (auto* c : getSelectedAudioClips()) tonkori::gainline::clear (*c); break;
        case Cmd::resetStretch:      for (auto* c : getSelectedAudioClips()) tonkori::stretch::reset (*c); break;
        case Cmd::toggleLanes:       toggleLanesForSelection(); break;
        case Cmd::promoteToMain:     promoteSelectionToMain(); break;
        case Cmd::insertBus:         if (auto* b = tonkori::routing::createBus (*edit)) selectionManager.selectOnly (b); break;
        case Cmd::addMarker:         addMarkerAtCursor(); break;
        case Cmd::prevMarker:        jumpMarker (-1); break;
        case Cmd::nextMarker:        jumpMarker (1); break;
        case Cmd::preRollToggle:     preRollOn = ! preRollOn; saveTransportPrefs(); statusBar.flashMessage (preRollOn ? "Pre-roll ON (" + juce::String (preRollSeconds, 1) + " s)" : "Pre-roll OFF"); break;
        case Cmd::postRollToggle:    postRollOn = ! postRollOn; saveTransportPrefs(); statusBar.flashMessage (postRollOn ? "Post-roll ON (" + juce::String (postRollSeconds, 1) + " s)" : "Post-roll OFF"); break;
        case Cmd::countInToggle:
            edit->setCountInMode (edit->getCountInMode() == te::Edit::CountIn::none ? te::Edit::CountIn::oneBar : te::Edit::CountIn::none);
            statusBar.flashMessage (edit->getCountInMode() == te::Edit::CountIn::none ? "Count-in OFF" : "Count-in: 1 bar before recording");
            break;
        case Cmd::insertMarkerToEnd:
        {
            const auto end = te::TimePosition::fromSeconds (edit->getLength().inSeconds());
            setInsertMarker (end);
            if (! edit->getTransport().isPlaying()) edit->getTransport().setPosition (end);
            break;
        }
        case Cmd::stopOnly:
            if (edit->getTransport().isPlaying()) playStop();
            break;
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
    return { "File", "Edit", "Clip", "Track", "Markers", "Tools", "View", "Transport", "Help" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu m;
    auto add = [&] (juce::PopupMenu& menu, std::initializer_list<juce::CommandID> ids)
    {
        for (auto id : ids) menu.addCommandItem (&commandManager, id);
    };

    switch (index)
    {
        case 0:   // File
        {
            add (m, { Cmd::newSet, Cmd::openSet }); m.addSeparator();
            add (m, { Cmd::save, Cmd::saveAs }); m.addSeparator();
            add (m, { Cmd::importAudio, Cmd::importVideo }); m.addSeparator();
            add (m, { Cmd::exportAudio, Cmd::exportStems, Cmd::exportVideo }); m.addSeparator();
            add (m, { Cmd::settings, Cmd::latencySettings, Cmd::shortcutSettings, Cmd::pluginList });
            juce::PopupMenu sets;
            for (int i = 0; i < tonkori::keys::profileNames().size(); ++i)
                sets.addItem (tonkori::keys::profileNames()[i], true, (int) keyProfile == i,
                              [this, i] { applyKeyProfile ((tonkori::keys::Profile) i, true); statusBar.flashMessage ("Shortcuts: " + tonkori::keys::profileNames()[i]); });
            m.addSubMenu ("Shortcut Set", sets);
            break;
        }
        case 1:   // Edit
            add (m, { Cmd::undo, Cmd::redo }); m.addSeparator();
            add (m, { Cmd::cut, Cmd::copy, Cmd::paste, Cmd::duplicate, Cmd::deleteSel }); m.addSeparator();
            add (m, { Cmd::selectAll, Cmd::clearSelection, Cmd::rename }); m.addSeparator();
            add (m, { Cmd::splitClip, Cmd::heal, Cmd::trimStartToCursor, Cmd::trimEndToCursor, Cmd::trimToSelection }); m.addSeparator();
            add (m, { Cmd::insertSilence, Cmd::deleteTime }); m.addSeparator();
            add (m, { Cmd::nudgeLeft, Cmd::nudgeRight, Cmd::spotClip });
            break;
        case 2:   // Clip
        {
            juce::PopupMenu fadesMenu, gainMenu, stretchMenu;
            add (fadesMenu, { Cmd::fadeEditor, Cmd::crossfadeEditor, Cmd::createFades, Cmd::deleteFades }); fadesMenu.addSeparator();
            add (fadesMenu, { Cmd::toggleClipFades, Cmd::toggleAutoFade });
            add (gainMenu, { Cmd::toggleGainLines, Cmd::clipGainUp, Cmd::clipGainDown, Cmd::clearGainLine });
            add (stretchMenu, { Cmd::resetStretch });
            stretchMenu.addItem ("Opt-drag a clip edge to stretch it (TCE); set % and pitch in the Inspector", false, false, nullptr);
            m.addSubMenu ("Fades", fadesMenu);
            m.addSubMenu ("Clip Gain", gainMenu);
            m.addSubMenu ("Time & Pitch", stretchMenu);
            m.addSeparator();
            add (m, { Cmd::reverseClip, Cmd::deactivate });
            break;
        }
        case 3:   // Track
            add (m, { Cmd::insertAudioTrack, Cmd::insertMidiTrack, Cmd::insertBus }); m.addSeparator();
            add (m, { Cmd::armSelected, Cmd::soloSelected }); m.addSeparator();
            add (m, { Cmd::toggleLanes, Cmd::promoteToMain }); m.addSeparator();
            add (m, { Cmd::toggleTrack1, Cmd::toggleTrack2, Cmd::toggleTrack3, Cmd::toggleTrack4,
                      Cmd::toggleTrack5, Cmd::toggleTrack6, Cmd::toggleTrack7, Cmd::toggleTrack8 });
            break;
        case 4:   // Markers
            add (m, { Cmd::addMarker, Cmd::prevMarker, Cmd::nextMarker, Cmd::markerList });
            if (edit)
            {
                auto markers = edit->getMarkerManager().getMarkers();
                if (! markers.isEmpty()) m.addSeparator();
                for (auto* mk : markers)
                {
                    const auto t = mk->getPosition().getStart();
                    m.addItem (mk->getName() + "    " + (view ? view->format (t) : juce::String()), [this, t] { setInsertMarker (t); if (edit) edit->getTransport().setPosition (t); });
                }
            }
            break;
        case 5:   // Tools
            add (m, { Cmd::toolSmart, Cmd::toolRange, Cmd::toolGrab, Cmd::toolTrim, Cmd::toolFade, Cmd::toolSplit }); m.addSeparator();
            add (m, { Cmd::modeSlip, Cmd::modeGrid, Cmd::modeShuffle, Cmd::modeSpot }); m.addSeparator();
            add (m, { Cmd::gridNarrow, Cmd::gridWiden, Cmd::gridTriplet, Cmd::snapToggle });
            break;
        case 6:   // View
            add (m, { Cmd::toggleInspector, Cmd::toggleMixer, Cmd::videoWindow, Cmd::loudnessWindow, Cmd::markerList, Cmd::follow }); m.addSeparator();
            add (m, { Cmd::zoomIn, Cmd::zoomOut, Cmd::fitWidth, Cmd::zoomToSelection, Cmd::zoomBack }); m.addSeparator();
            add (m, { Cmd::fullScreen });
            break;
        case 7:   // Transport
            add (m, { Cmd::playStop, Cmd::continuePlay, Cmd::stopOnly, Cmd::record }); m.addSeparator();
            add (m, { Cmd::loopSelection, Cmd::selectLoopContents, Cmd::setLoopStart, Cmd::setLoopEnd, Cmd::punch }); m.addSeparator();
            add (m, { Cmd::preRollToggle, Cmd::postRollToggle, Cmd::countInToggle, Cmd::metronome }); m.addSeparator();
            add (m, { Cmd::insertMarkerToStart, Cmd::insertMarkerToEnd, Cmd::insertMarkerToPlayhead }); m.addSeparator();
            add (m, { Cmd::engineOnOff });
            break;
        case 8:   // Help
            m.addItem ("Keyboard shortcuts...", [this] { showSettings (2); });
            m.addItem ("Shortcut set: " + tonkori::keys::profileNames()[(int) keyProfile], false, false, nullptr);
            m.addItem ("Engine: " + tonkori::EngineProfile::describe (engine), false, false, nullptr);
            break;
        default: break;
    }

    return m;
}
