# Tonkori DAW keyboard shortcuts

Tonkori has four shortcut sets: **Ableton Live (default)**, **Pro Tools**, **Logic Pro** and **Pyramix**. Pick one in **File → Shortcut Set**, or in **Settings (Cmd ,) → Shortcuts**. In that tab you can also click any command's key to change it, or press + to add a second key. Your changes are saved separately for each set, and **Reset this set** restores it. The menu bar always shows the keys of the current set.

Each set starts from the Live keys below. The set then puts its own keys on the commands it knows (tables at the end). A Live key that the set uses for something else is removed from the other command, so no key ever does two things.

# Ableton Live set (default)

Live 12's default Mac shortcuts (Live manual, ch. 42 "Live Keyboard Shortcuts"). The "Live" column says whether the binding is Live's own or a Tonkori addition placed on a key Live leaves free.

## Transport

| Action | Keys | Live |
|---|---|---|
| Play from insert marker / Stop (playhead returns to the insert marker) | Space | ✓ |
| Continue play from stop point | Shift Space | ✓ |
| Record | F9 | ✓ |
| Move insert marker to beginning | Home | ✓ |
| Move insert marker to playhead | Cmd Shift Space | ✓ |
| Metronome | O | ✓ |
| Audio engine on/off | Cmd Opt Shift E | ✓ |

Clicking on an empty part of a track, or on a clip, sets the insert marker while stopped.

## Tools and edit modes

| Action | Keys | From |
|---|---|---|
| Smart / Range / Grab / Trim / Fade / Split tool | 1 / 2 / 3 / 4 / 5 / 6 | Cubase |
| Slip / Grid / Shuffle / Spot mode | Opt 1 / 2 / 3 / 4 | Pro Tools modes |
| Narrow / widen grid, triplet, snap on/off | Cmd 1 / 2 / 3 / 4 | Live |
| Clear selection | Esc | |

Smart Tool (mouse): the top corners of a clip drag its fades, Opt-drag bends the curve, the edges trim, the upper half selects time, the lower half moves (Opt copies), and a double-click on a fade opens the fade editor.

## Fades

| Action | Keys | From |
|---|---|---|
| Fade editor (numeric) | Cmd F | Pro Tools |
| Create fades / crossfade from the time selection | Cmd Opt F | Live + Pro Tools |
| Delete fades | Cmd Opt Delete | Live |
| Fades on/off for the selected clips (kept, bypassed) | F | Tonkori |
| Session auto-fades on/off | Shift F | Tonkori |

## Editing

| Action | Keys | Live |
|---|---|---|
| Undo / Redo | Cmd Z / Cmd Shift Z | ✓ |
| Cut / Copy / Paste (paste goes to the insert marker on the selected track) | Cmd X / C / V | ✓ |
| Duplicate (placed after the selection) | Cmd D | ✓ |
| Delete clip or track | Delete | ✓ |
| Select all clips | Cmd A | ✓ |
| Rename clip or track | Cmd R | ✓ |
| Split at insert marker, or at both edges of the time selection | Cmd E | ✓ |
| Spot clip (type its exact start) | Cmd Opt P | Pro Tools |
| Insert silence (loop length if the loop is on, else 1 bar) | Cmd I | ✓ |
| Delete time (loop range, closes the gap) | Cmd Shift Delete | ✓ |
| Create fades (10 ms in/out) / Delete fades | Cmd Opt F / Cmd Opt Delete | ✓ |
| Reverse audio clip | R | ✓ |
| Nudge selected clips (1/16 note) | ← / → | ✓ |
| Deactivate selection (clip or track mute) | 0 | ✓ |
| Cmd-click / Shift-click a clip to add it to the selection | | ✓ |

## Loop and view

| Action | Keys | Live |
|---|---|---|
| Loop selection / toggle loop (time selection, clips, or a 4-bar default) | Cmd L | ✓ |
| Punch in/out on the loop range | Cmd Shift P | Tonkori |
| Follow playhead | Cmd Shift F | ✓ |
| Show/hide Inspector, Mixer | Cmd Opt I, Cmd Opt M | ✓ |
| Select loop range as a time selection | Cmd Shift L | ✓ |
| Set loop start / end at insert marker | Cmd F10 / Cmd F11 | ✓ |
| Zoom in / out | + / − | ✓ |
| Optimize arrangement width | W | ✓ |
| Zoom to selection / zoom back | Z / X | ✓ |
| Full screen | Ctrl Cmd F | ✓ |

## Tracks

| Action | Keys | Live |
|---|---|---|
| Insert audio track | Cmd T | ✓ |
| Insert MIDI track | Cmd Shift T | ✓ |
| Arm selected tracks (assigns a free input if none) | C | ✓ |
| Solo selected tracks | S | ✓ |
| Activate/deactivate tracks 1–8 | F1 … F8 | ✓ |

## Files

| Action | Keys | Live |
|---|---|---|
| New / Open / Save / Save As | Cmd N / O / S / Shift S | ✓ |
| Export mix | Cmd Shift R | ✓ |
| Settings (audio, latency, shortcuts, editing) | Cmd , | ✓ |
| Import audio or DSD | Cmd Shift O | Tonkori |

## Recording, takes and comping

| Action | Keys | From |
|---|---|---|
| Pre-roll / post-roll / count-in on/off | Transport menu | Pro Tools |
| Show/hide take lanes on the selected track | Cmd Opt L | Tonkori |
| Promote the selected take (or its time selection) to the main lane | Cmd Opt V | Tonkori |
| Swipe across a take lane to comp that range | mouse | Logic, Pro Tools |
| Double-click a take to use all of it | mouse | |

## Clip gain, crossfades, healing

| Action | Keys | From |
|---|---|---|
| Crossfade editor for the selected overlap (or double-click the overlap) | Cmd Opt X | Tonkori |
| Clip gain +1 / −1 dB | Cmd Opt ↑ / ↓ | Tonkori |
| Show/hide clip gain lines | Cmd Opt G | Tonkori |
| Heal separation | Cmd J | Pro Tools |
| Trim clip start / end to the insert marker | Opt [ / Opt ] | Pro Tools |
| Time-stretch: Opt-drag a clip edge | mouse | Pro Tools TCE |

Gain line (mouse): click to add a point, drag to move, Opt-click to delete, Cmd-drag to move the whole line, Shift to move vertically only.

## Markers

| Action | Keys | From |
|---|---|---|
| Add marker at the insert marker | M | Tonkori |
| Previous / next marker | Opt ← / Opt → | |
| Marker list | Cmd Opt K | |

Ruler (mouse): double-click the marker strip to add or rename a marker, drag to move, Opt-click to delete, right-click for a menu.

## Buses, video, metering, export

| Action | Keys | From |
|---|---|---|
| Insert bus | Cmd Opt T | Live (return track) |
| Import video | Cmd Opt O | Tonkori |
| Video window | Cmd Opt 9 | |
| Loudness meter window | Cmd Opt U | |
| Export mix | Cmd Shift R | Live |
| Export stems | Cmd Opt R | Tonkori |
| Export video | Cmd Opt Shift R | Tonkori |
| Move insert marker to end | End | |

## Not mapped yet in the Live set (arrive with their features)

- Tab: Session/Arrangement toggle (M3, clip launcher)
- Cmd Shift J: crop. Cmd 5: adaptive grid. Cmd 6–0: global quantization
- Cmd G: group tracks. A: automation mode. B: draw mode
- Cmd M / Cmd K: MIDI and key map modes

# Pro Tools set

Pro Tools' own keys for these commands. Everything else keeps its Live key.

| Command | Keys |
|---|---|
| Save Session As... | Shift Cmd S |
| Import Audio / DSD... | Shift Cmd I |
| Import Video... | Opt Cmd I |
| Export Mix... | Opt Cmd B |
| Export Stems... | Opt Shift Cmd B |
| Duplicate | Cmd D |
| Split at Insert Marker / Selection | Cmd E or B |
| Heal Separation | Cmd H |
| Trim Clip Start to Cursor | A |
| Trim Clip End to Cursor | S |
| Trim Clip to Selection | Cmd T |
| Fade Editor... | Cmd F |
| Create Fades / Crossfade from Selection | Ctrl Cmd F or F |
| Insert Silence | Shift Cmd E |
| Rename | Shift Cmd R |
| Deactivate Selection | Cmd M |
| Nudge Left | , or Keypad − |
| Nudge Right | . or Keypad + |
| Clip Gain +1 dB | Ctrl Shift ↑ |
| Clip Gain -1 dB | Ctrl Shift ↓ |
| Show Clip Gain Lines | Ctrl Shift - |
| Range Tool | F7 |
| Grab Tool | F8 |
| Trim Tool | F6 |
| Shuffle Mode | F1 |
| Slip Mode | F2 |
| Spot Mode | F3 |
| Grid Mode | F4 |
| Loop Selection | Shift Cmd L |
| Punch In/Out | Shift Cmd P |
| Zoom In | Cmd ] |
| Zoom Out | Cmd [ |
| Optimize Arrangement Width | Opt A |
| Zoom to Selection | Opt F |
| Show/Hide Mixer | Cmd = |
| Show/Hide Video Window | Cmd 9 |
| Marker List | Cmd 5 |
| Insert Audio Track | Shift Cmd N |
| Arm Selected Tracks | Shift R |
| Solo Selected Tracks | Shift S |
| Promote Selection to Main Lane | Ctrl Opt V |
| Record | Cmd Space or F12 or Keypad 3 |
| Move Insert Marker to Beginning | Return or Home |
| Move Insert Marker to End | Opt Return or End |
| Pre-Roll On/Off | Cmd K |
| Metronome | Keypad 7 |
| Count-In On/Off | Keypad 8 |

# Logic Pro set

| Command | Keys |
|---|---|
| Import Audio / DSD... | Shift Cmd I |
| Export Mix... | Cmd B |
| Export Stems... | Shift Cmd E |
| Duplicate | Cmd R |
| Split at Insert Marker / Selection | Cmd T |
| Heal Separation | Cmd J |
| Trim Clip Start to Cursor | Cmd [ |
| Trim Clip End to Cursor | Cmd ] |
| Crossfade Editor... | Opt X |
| Create Fades / Crossfade from Selection | Ctrl Opt X |
| Rename | Shift N |
| Deactivate Selection | Ctrl M |
| Nudge Left | Opt ← |
| Nudge Right | Opt → |
| Add Marker | Opt ' |
| Previous Marker | Opt , |
| Next Marker | Opt . |
| Loop Selection | C |
| Select Loop Range | Cmd U |
| Punch In/Out | Ctrl Opt Cmd P |
| Zoom In | Cmd → |
| Zoom Out | Cmd ← |
| Zoom to Selection | Z |
| Zoom Back | Shift Z |
| Show/Hide Mixer | X |
| Show/Hide Inspector | I |
| Insert Audio Track | Opt Cmd A |
| Insert MIDI Track | Opt Cmd S |
| Arm Selected Tracks | Ctrl R |
| Solo Selected Tracks | S |
| Show/Hide Take Lanes | Opt F |
| Record | R |
| Move Insert Marker to Beginning | Return or Home |
| Metronome | K |
| Count-In On/Off | Shift K |
| Reverse Audio Clip | (unmapped) |

# Pyramix set

Pyramix's default keyboard layout for these commands (keypad transport, F-key edits).

| Command | Keys |
|---|---|
| Undo | Cmd Z or F5 |
| Redo | Shift Cmd Z or F6 |
| Cut | Cmd X or F2 |
| Copy | Cmd C or F3 |
| Paste | Cmd V or F4 |
| Delete Time | Opt Cmd X |
| Split at Insert Marker / Selection | Cmd T |
| Split Tool | C |
| Duplicate | D |
| Trim Clip to Selection | Shift Cmd X |
| Create Fades / Crossfade from Selection | Cmd F9 or Shift F9 or Opt F9 |
| Fade Editor... | Shift F10 or Opt F10 |
| Crossfade Editor... | Cmd F10 |
| Deactivate Selection | Shift Cmd M |
| Insert Silence | Opt Cmd S |
| Nudge Left | Opt ← |
| Nudge Right | Opt → |
| Previous Marker | Opt Shift ← |
| Next Marker | Opt Shift → |
| Add Marker | Keypad 9 |
| Loop Selection | Cmd F3 |
| Set Loop Start | F7 or Keypad 7 |
| Set Loop End | F8 or Keypad 8 |
| Zoom In | Opt 3 |
| Zoom Out | Opt 4 |
| Optimize Arrangement Width | Opt 1 |
| Zoom to Selection | Shift F4 |
| Show/Hide Mixer | Opt M |
| Marker List | Opt F4 |
| Stop | Keypad 0 |
| Record | Keypad . or F9 |
| Move Insert Marker to Beginning | Home or Shift F5 |
| Move Insert Marker to End | End |
