#pragma once

#include <JuceHeader.h>

/*  Keyboard shortcut sets. Ableton Live 12 is the default; Pro Tools, Logic Pro and
    Pyramix sets put each command on that DAW's default Mac key where the DAW has the
    function (Pyramix: Windows Ctrl -> Cmd, Alt -> Option). A command a DAW doesn't have
    keeps its Live/Tonkori key unless that key means something else in the chosen set.
    Any key can be reassigned in Settings > Shortcuts; changes are kept per set. */

namespace tonkori::keys
{
    enum class Profile { ableton, protools, logic, pyramix };

    inline juce::StringArray profileNames()  { return { "Ableton Live (default)", "Pro Tools", "Logic Pro", "Pyramix" }; }
    inline juce::String profileId (Profile p) { return juce::StringArray { "ableton", "protools", "logic", "pyramix" }[(int) p]; }

    using KP = juce::KeyPress;
    constexpr int cmd   = juce::ModifierKeys::commandModifier;
    constexpr int shift = juce::ModifierKeys::shiftModifier;
    constexpr int opt   = juce::ModifierKeys::altModifier;
    constexpr int ctrl  = juce::ModifierKeys::ctrlModifier;

    struct Binding { juce::CommandID id; std::vector<KP> keys; };

    /** Profile-specific bindings, keyed by command name (names are stable, IDs are not). */
    struct NamedBinding { const char* command; std::vector<KP> keys; };

    inline const std::vector<NamedBinding>& overrides (Profile p)
    {
        static const std::vector<NamedBinding> none;

        static const std::vector<NamedBinding> protools
        {
            { "Save Session As...",      { KP ('s', cmd | shift, 0) } },
            { "Import Audio / DSD...",   { KP ('i', cmd | shift, 0) } },
            { "Import Video...",         { KP ('i', cmd | opt, 0) } },
            { "Export Mix...",           { KP ('b', cmd | opt, 0) } },
            { "Export Stems...",         { KP ('b', cmd | opt | shift, 0) } },
            { "Duplicate",               { KP ('d', cmd, 0) } },
            { "Split at Insert Marker / Selection", { KP ('e', cmd, 0), KP ('b', 0, 0) } },
            { "Heal Separation",         { KP ('h', cmd, 0) } },
            { "Trim Clip Start to Cursor", { KP ('a', 0, 0) } },
            { "Trim Clip End to Cursor",   { KP ('s', 0, 0) } },
            { "Trim Clip to Selection",  { KP ('t', cmd, 0) } },
            { "Fade Editor...",          { KP ('f', cmd, 0) } },
            { "Create Fades / Crossfade from Selection", { KP ('f', cmd | ctrl, 0), KP ('f', 0, 0) } },
            { "Insert Silence",          { KP ('e', cmd | shift, 0) } },
            { "Rename",                  { KP ('r', cmd | shift, 0) } },
            { "Deactivate Selection",    { KP ('m', cmd, 0) } },
            { "Nudge Left",              { KP (',', 0, 0), KP (KP::numberPadSubtract, 0, 0) } },
            { "Nudge Right",             { KP ('.', 0, 0), KP (KP::numberPadAdd, 0, 0) } },
            { "Clip Gain +1 dB",         { KP (KP::upKey, ctrl | shift, 0) } },
            { "Clip Gain -1 dB",         { KP (KP::downKey, ctrl | shift, 0) } },
            { "Show Clip Gain Lines",    { KP ('-', ctrl | shift, 0) } },
            { "Range Tool",              { KP (KP::F7Key, 0, 0) } },
            { "Grab Tool",               { KP (KP::F8Key, 0, 0) } },
            { "Trim Tool",               { KP (KP::F6Key, 0, 0) } },
            { "Shuffle Mode",            { KP (KP::F1Key, 0, 0) } },
            { "Slip Mode",               { KP (KP::F2Key, 0, 0) } },
            { "Spot Mode",               { KP (KP::F3Key, 0, 0) } },
            { "Grid Mode",               { KP (KP::F4Key, 0, 0) } },
            { "Loop Selection",          { KP ('l', cmd | shift, 0) } },
            { "Punch In/Out",            { KP ('p', cmd | shift, 0) } },
            { "Zoom In",                 { KP (']', cmd, 0) } },
            { "Zoom Out",                { KP ('[', cmd, 0) } },
            { "Optimize Arrangement Width", { KP ('a', opt, 0) } },
            { "Zoom to Selection",       { KP ('f', opt, 0) } },
            { "Show/Hide Mixer",         { KP ('=', cmd, 0) } },
            { "Show/Hide Video Window",  { KP ('9', cmd, 0) } },
            { "Marker List",             { KP ('5', cmd, 0) } },
            { "Insert Audio Track",      { KP ('n', cmd | shift, 0) } },
            { "Arm Selected Tracks",     { KP ('r', shift, 0) } },
            { "Solo Selected Tracks",    { KP ('s', shift, 0) } },
            { "Promote Selection to Main Lane", { KP ('v', ctrl | opt, 0) } },
            { "Record",                  { KP (KP::spaceKey, cmd, 0), KP (KP::F12Key, 0, 0), KP (KP::numberPad3, 0, 0) } },
            { "Move Insert Marker to Beginning", { KP (KP::returnKey, 0, 0), KP (KP::homeKey, 0, 0) } },
            { "Move Insert Marker to End", { KP (KP::returnKey, opt, 0), KP (KP::endKey, 0, 0) } },
            { "Pre-Roll On/Off",         { KP ('k', cmd, 0) } },
            { "Metronome",               { KP (KP::numberPad7, 0, 0) } },
            { "Count-In On/Off",         { KP (KP::numberPad8, 0, 0) } },
        };

        static const std::vector<NamedBinding> logic
        {
            { "Import Audio / DSD...",   { KP ('i', cmd | shift, 0) } },
            { "Export Mix...",           { KP ('b', cmd, 0) } },
            { "Export Stems...",         { KP ('e', cmd | shift, 0) } },
            { "Duplicate",               { KP ('r', cmd, 0) } },
            { "Split at Insert Marker / Selection", { KP ('t', cmd, 0) } },
            { "Heal Separation",         { KP ('j', cmd, 0) } },
            { "Trim Clip Start to Cursor", { KP ('[', cmd, 0) } },
            { "Trim Clip End to Cursor",   { KP (']', cmd, 0) } },
            { "Crossfade Editor...",     { KP ('x', opt, 0) } },
            { "Create Fades / Crossfade from Selection", { KP ('x', ctrl | opt, 0) } },
            { "Rename",                  { KP ('n', shift, 0) } },
            { "Deactivate Selection",    { KP ('m', ctrl, 0) } },
            { "Nudge Left",              { KP (KP::leftKey, opt, 0) } },
            { "Nudge Right",             { KP (KP::rightKey, opt, 0) } },
            { "Add Marker",              { KP ('\'', opt, 0) } },
            { "Previous Marker",         { KP (',', opt, 0) } },
            { "Next Marker",             { KP ('.', opt, 0) } },
            { "Loop Selection",          { KP ('c', 0, 0) } },
            { "Select Loop Range",       { KP ('u', cmd, 0) } },
            { "Punch In/Out",            { KP ('p', ctrl | opt | cmd, 0) } },
            { "Zoom In",                 { KP (KP::rightKey, cmd, 0) } },
            { "Zoom Out",                { KP (KP::leftKey, cmd, 0) } },
            { "Zoom to Selection",       { KP ('z', 0, 0) } },
            { "Zoom Back",               { KP ('z', shift, 0) } },
            { "Show/Hide Mixer",         { KP ('x', 0, 0) } },
            { "Show/Hide Inspector",     { KP ('i', 0, 0) } },
            { "Insert Audio Track",      { KP ('a', opt | cmd, 0) } },
            { "Insert MIDI Track",       { KP ('s', opt | cmd, 0) } },
            { "Arm Selected Tracks",     { KP ('r', ctrl, 0) } },
            { "Solo Selected Tracks",    { KP ('s', 0, 0) } },
            { "Show/Hide Take Lanes",    { KP ('f', opt, 0) } },
            { "Record",                  { KP ('r', 0, 0) } },
            { "Move Insert Marker to Beginning", { KP (KP::returnKey, 0, 0), KP (KP::homeKey, 0, 0) } },
            { "Metronome",               { KP ('k', 0, 0) } },
            { "Count-In On/Off",         { KP ('k', shift, 0) } },
            { "Reverse Audio Clip",      {} },
        };

        static const std::vector<NamedBinding> pyramix
        {
            { "Undo",                    { KP ('z', cmd, 0), KP (KP::F5Key, 0, 0) } },
            { "Redo",                    { KP ('z', cmd | shift, 0), KP (KP::F6Key, 0, 0) } },
            { "Cut",                     { KP ('x', cmd, 0), KP (KP::F2Key, 0, 0) } },
            { "Copy",                    { KP ('c', cmd, 0), KP (KP::F3Key, 0, 0) } },
            { "Paste",                   { KP ('v', cmd, 0), KP (KP::F4Key, 0, 0) } },
            { "Delete Time",             { KP ('x', cmd | opt, 0) } },
            { "Split at Insert Marker / Selection", { KP ('t', cmd, 0) } },
            { "Split Tool",              { KP ('c', 0, 0) } },
            { "Duplicate",               { KP ('d', 0, 0) } },
            { "Trim Clip to Selection",  { KP ('x', cmd | shift, 0) } },
            { "Create Fades / Crossfade from Selection", { KP (KP::F9Key, cmd, 0), KP (KP::F9Key, shift, 0), KP (KP::F9Key, opt, 0) } },
            { "Fade Editor...",          { KP (KP::F10Key, shift, 0), KP (KP::F10Key, opt, 0) } },
            { "Crossfade Editor...",     { KP (KP::F10Key, cmd, 0) } },
            { "Deactivate Selection",    { KP ('m', cmd | shift, 0) } },
            { "Insert Silence",          { KP ('s', cmd | opt, 0) } },
            { "Nudge Left",              { KP (KP::leftKey, opt, 0) } },
            { "Nudge Right",             { KP (KP::rightKey, opt, 0) } },
            { "Previous Marker",         { KP (KP::leftKey, opt | shift, 0) } },
            { "Next Marker",             { KP (KP::rightKey, opt | shift, 0) } },
            { "Add Marker",              { KP (KP::numberPad9, 0, 0) } },
            { "Loop Selection",          { KP (KP::F3Key, cmd, 0) } },
            { "Set Loop Start",          { KP (KP::F7Key, 0, 0), KP (KP::numberPad7, 0, 0) } },
            { "Set Loop End",            { KP (KP::F8Key, 0, 0), KP (KP::numberPad8, 0, 0) } },
            { "Zoom In",                 { KP ('3', opt, 0) } },
            { "Zoom Out",                { KP ('4', opt, 0) } },
            { "Optimize Arrangement Width", { KP ('1', opt, 0) } },
            { "Zoom to Selection",       { KP (KP::F4Key, shift, 0) } },
            { "Show/Hide Mixer",         { KP ('m', opt, 0) } },
            { "Marker List",             { KP (KP::F4Key, opt, 0) } },
            { "Stop",                    { KP (KP::numberPad0, 0, 0) } },
            { "Record",                  { KP (KP::numberPadDecimalPoint, 0, 0), KP (KP::F9Key, 0, 0) } },
            { "Move Insert Marker to Beginning", { KP (KP::homeKey, 0, 0), KP (KP::F5Key, shift, 0) } },
            { "Move Insert Marker to End", { KP (KP::endKey, 0, 0) } },
        };

        switch (p)
        {
            case Profile::protools: return protools;
            case Profile::logic:    return logic;
            case Profile::pyramix:  return pyramix;
            case Profile::ableton:  break;
        }
        return none;
    }

    /** Resolves the keys for one command in a set, given its Live/Tonkori keys. */
    inline std::vector<KP> keysFor (Profile p, const juce::String& commandName, const std::vector<KP>& liveKeys)
    {
        auto& ov = overrides (p);
        for (auto& b : ov)
            if (commandName == b.command) return b.keys;

        std::vector<KP> r;
        for (auto& k : liveKeys)
        {
            bool taken = false;
            for (auto& b : ov)
                for (auto& o : b.keys) taken = taken || o == k;
            if (! taken) r.push_back (k);
        }
        return r;
    }
}
