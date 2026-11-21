# Tonkori DAW

A personal DAW for the Tonkori studio rig. It is not a commercial product.

**Get the app:** open the [latest release](../../releases/latest), download the `.zip` and unzip it. Move **Tonkori DAW** to Applications and open it. The first time, macOS may block it: go to **System Settings → Privacy & Security** and click **Open Anyway**.

Target: Mac Studio (Apple M1 Max), macOS 13.7.1 (Ventura) and later, Apple Silicon only.

## Thesis

One object model, several views. Every capability comes from a mechanism borrowed from one of five DAWs:

| From | Mechanism | Layer |
|---|---|---|
| Reaper | One uniform track type; any routing; every action scriptable | Spine |
| Pyramix | High-rate engine (to DXD), source–destination editing, immersive buses | Engine |
| Pro Tools | Edit grammar: Shuffle/Slip/Spot/Grid, playlists and comping | Editing |
| Ableton Live | Clip launcher as a second view of the same clips; warp | View |
| RipX | Audio decomposed into editable note objects | Data |

The new idea is **transparent-until-touched decomposition**. The original PCM is always the source of truth. Note objects are a derived layer. The render rule is:

    output = original − (original versions of edited notes) + (resynthesized edited notes)

Untouched audio stays bit-identical, so null tests pass. Artifacts can only appear where something was actually edited.

## Milestones

- **M0, walking skeleton** (this build): arrangement, transport, recording, audio import, AU/VST3 hosting, device settings, session save/load.
- **M1, spine:** Tonkori's own timeline and track model, uniform tracks, routing matrix, high sample rates.
- **M2, edit grammar:** edit modes, playlists/comping, 3- and 4-point source–destination editing.
- **M3, launcher:** clip-grid view on the same clips.
- **M4, decomposition:** stem separation, note objects, the transparent render rule.
- **M5, scripting:** action list and a script console.
- **M6, delivery:** ADM/immersive buses, DDP, stem delivery, AAF.

## Build

Fully automated: every push to `main` builds on GitHub's Apple Silicon machines (see `.github/workflows/build-macos.yml`) and publishes a release.

Built on [Tracktion Engine](https://github.com/Tracktion/tracktion_engine) (GPLv3) and [JUCE](https://juce.com) (AGPLv3), pinned in `deps.lock`. `src/vendor/tracktion_examples` holds Tracktion's example UI code (GPLv3). It serves as the M0 arrangement view and will be replaced.
