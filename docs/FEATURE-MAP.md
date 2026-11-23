# Feature map: Pro Tools · Pyramix · Cubase · Live · Reaper · RipX → Tonkori

What the reference DAWs offer, and where each function stands in Tonkori.
**✓** = in Tonkori now · **◐** = partial · **M2…M6** = planned milestone · **—** = deliberately not planned.

Source: PT = Pro Tools, LG = Logic Pro, PX = Pyramix, CB = Cubase/Nuendo, LV = Ableton Live, RP = Reaper, RX = RipX.

## Recording

| Function | From | Status |
|---|---|---|
| Multitrack recording, per-track arm and input | all | ✓ |
| Input monitoring auto / on / off | PT, CB, LV | ✓ |
| 32-bit float WAV recording, rates up to 384 kHz / DXD | PX | ✓ |
| Punch in/out on a range | PT, CB, PX | ✓ (loop range, Cmd+Shift+P) |
| Edit other tracks while recording | PT, CB, LV | ✓ (tested) |
| Live recording waveform | all | ✓ |
| Loop recording → takes on take lanes | PT, CB, LV | ✓ |
| Pre-roll / post-roll, count-in | PT, CB, LV | ✓ (Settings → Editing & transport) |
| QuickPunch / on-the-fly punch | PT | M2 |
| Retrospective record (capture what you just played) | CB, LV | M2 |
| Background / continuous safety recording | PX | M6 |
| Destructive punch | PT | — |

## Editing

| Function | From | Status |
|---|---|---|
| Smart Tool (fade corners, trim edges, select top, move bottom) | PT | ✓ |
| Tools: Range, Grab, Trim, Fade, Split on 1–6 | PT, CB | ✓ |
| Edit modes Slip / Grid / Shuffle / Spot | PT | ✓ |
| Time selection across tracks; delete/cut/copy/split/duplicate on it | PT | ✓ |
| Spot dialog (type exact start in any format) | PT | ✓ |
| Numeric clip start/end/length/offset/gain | PX, CB | ✓ (Inspector) |
| Insert silence / delete time | LV, PT | ✓ |
| Nudge by grid | PT, LV | ✓ |
| Reverse, mute, rename, colour | all | ✓ |
| Every edit applies during playback and recording | all | ✓ (tested, 11–43 ms) |
| Drag & drop from Finder at the drop point | all | ✓ |
| Clip gain line (envelope), drawn on the clip | PT, CB | ✓ (sample-accurate, tested) |
| Take lanes and swipe comping, equal-power comp crossfades | PT, CB, LV | ✓ |
| Heal separation, trim to cursor / selection | PT | ✓ |
| Source–destination 3- and 4-point editing | PX | M2.5 |
| Edit groups, VCA | PT, CB | M3 |
| Strip silence, Tab-to-transient, hitpoints | PT, CB | M3 |
| Clip groups | PT | M3 |
| Time stretch and pitch per clip (Rubber Band R3, Signalsmith, SoundTouch, varispeed); Opt-drag an edge (TCE) | PT, CB, LV | ✓ |
| Warp markers inside a clip | PT, CB, LV | M3 |
| Beat Detective / quantise audio | PT, CB | M3 |
| Note-level editing of audio (VariAudio, RipX) | CB, RX | M4 |

## Fades

| Function | From | Status |
|---|---|---|
| Fade handles on clips, drag live while playing | LV, PT, CB, PX | ✓ |
| Shapes: linear, equal power, S-curve, exponential, dB-linear | PT, CB, PX | ✓ |
| Numeric programming: length, shape parameter, breakpoints (time % : dB) | CB (points), PX (numeric) | ✓ |
| Spline / damped spline / linear interpolation | CB | ✓ |
| Typed spec, e.g. `points damped 25:-12 75:-1` | Tonkori | ✓ |
| Per-clip fades on/off without losing them (F) | LV | ✓ |
| Session auto-fades with length and shape (Shift+F) | CB, PX | ✓ |
| Opt-drag a fade to bend its curve | LV | ✓ |
| Crossfade from a selection across a cut (equal power) | PT | ✓ |
| Fade presets, user presets, "set as auto-fade shape" | CB, PT | ✓ |
| Apply to all selected clips (batch fades) | PT | ✓ |
| Crossfade editor with splice-point offset, pre/centre/post alignment, per-side curves | CB, PX | ✓ |
| Audition crossfade with pre/post-roll | CB | ✓ |

## Mixing and routing

| Function | From | Status |
|---|---|---|
| Channel strips: inserts, pan, fader, peak-hold meter, M/S/R | all | ✓ |
| Master strip with inserts | all | ✓ |
| AU / VST3 hosting, built-in EQ, compressor, reverb, delay, etc. | all | ✓ |
| Plug-in delay compensation, also when bypassed and across sends | all | ✓ (null-tested) |
| Out-of-process plug-in scan (a crashing plug-in cannot take the app down) | LV, RP | ✓ |
| Generic editors for built-in plug-ins; missing plug-ins flagged | all | ✓ |
| Sends (pre/post fader), buses, subgroups, hardware outputs | all | ✓ |
| Automation: read / write / touch / latch, lanes | PT, PX, CB | M3 |
| Sidechain | all | M3 |
| VCA faders | PT, CB | M3 |
| Every track the same object, any routing | RP | M3 (spine) |
| Surround / Atmos / ADM, immersive buses | PX, PT, CB | M6 |
| Control room / monitoring section | CB, PX | M6 |

## Time, tempo, sync

| Function | From | Status |
|---|---|---|
| Bars, timecode (23.976–30 fps), min:sec, samples displays | all | ✓ |
| Type any position anywhere (counter, Inspector, Spot) | PT, PX | ✓ |
| Tempo and meter | all | ✓ (single tempo) |
| Tempo map, ramps, meter changes | all | M3 |
| Video track: MOV, MP4, M4V, MXF, AVI, MKV, WebM; ProRes, DNxHD/HR, H.264, HEVC, MPEG-2, DV and more | PT, CB | ✓ |
| Video export: ProRes Proxy–4444, DNxHR LB–444, H.264/HEVC (VideoToolbox), stream copy | PT, CB | ✓ |
| Video sync offset, thumbnails, video window | PT | ✓ |
| MTC / LTC sync | PT, PX | M6 |

## MIDI and instruments

| Function | From | Status |
|---|---|---|
| MIDI tracks | all | ◐ track type exists, no editor yet |
| Piano roll, key editor, instruments | all | M3 |

## Arrangement and performance

| Function | From | Status |
|---|---|---|
| Live transport (Space returns to insert marker, Shift+Space continues) | LV | ✓ |
| Loop brace, follow playhead, metronome | all | ✓ |
| Clip launcher (Session view) on the same clips | LV | M3 |
| Markers / memory locations, marker list | PT, PX, CB | ✓ |

## Analysis, restoration, decomposition

| Function | From | Status |
|---|---|---|
| Stem separation | LV, RX | M4 |
| Note decomposition with transparent-until-touched render | RX + Tonkori | M4 |
| Spectral editing | CB (SpectraLayers), RX | M5 |
| Loudness metering: momentary, short-term, integrated, LRA, true peak (EBU R128 / BS.1770-4) | PT, CB, PX | ✓ (EBU 3341/3342 tested) |

## Delivery

| Function | From | Status |
|---|---|---|
| Export WAV 32f, or 24/16-bit with TPDF dither | all | ✓ |
| Export time selection or whole session | all | ✓ |
| Stem / multitrack / bus export | all | ✓ (stems sum to the mix, tested) |
| DDP / CD mastering | PX | M6 |
| AAF / OMF interchange | PT, PX, CB | M6 |
| ADM BWF | PX, PT | M6 |

## Engine (the Pyramix profile)

| Function | From | Status |
|---|---|---|
| 32-bit float end to end, bit-exact unity path (null-tested) | PX | ✓ |
| DSD64/128/256 import as DXD | PX | ✓ |
| Real-time threads on Core Audio workgroups (MassCore counterpart) | PX | ✓ |
| RAVENNA / AES67 via Merging VAD | PX | ✓ (as Core Audio device) |

## Workflow

| Function | From | Status |
|---|---|---|
| Shortcut sets: Ableton Live (default), Pro Tools, Logic Pro, Pyramix; every key editable | LV, PT, LG, PX | ✓ |
| Full menu bar: File, Edit, Clip, Track, Markers, Tools, View, Transport | all | ✓ |
| Latency report, loopback measurement, record offset | PX, RP | ✓ |
| One undo step per gesture | all | ✓ |
| Inspector: everything numeric and typeable | PX, CB | ✓ |
| Session templates, track presets | PT, CB | M2 / M3 |
| Action list, scripting, macros | RP, CB | M5 |
