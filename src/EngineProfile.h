#pragma once

#include <JuceHeader.h>
#include "model/ClipFades.h"

namespace te = tracktion;

/*  The "Pyramix profile": how the engine is configured to behave like
    Merging's Pyramix on macOS.

    - 32-bit floating-point processing end to end (Pyramix's own internal format).
    - Recording written as 32-bit float WAV at the device rate, up to 384 kHz / DXD.
    - Real-time worker threads joined to Core Audio's audio workgroup, so macOS
      schedules them against the device deadline on the performance cores: the
      macOS counterpart of MassCore's dedicated real-time cores.
    - Unity path: linear pan law (centre = 0 dB, no attenuation), no automatic
      clip-edge fades, so an untouched clip at 0 dB renders bit-identical.
    - DSD imported as DXD (352.8 kHz float), see dsd/DsdDecoder.h. */

namespace tonkori
{
    class EngineBehaviour : public te::EngineBehaviour
    {
    public:
        int getNumberOfCPUsToUseForAudio() override     { return juce::jmax (1, juce::SystemStats::getNumCpus()); }
        bool autoAddClipEdgeFades() override            { return false; }
        bool shouldOpenAudioInputByDefault() override   { return true; }

        // Numeric fade curves, per-clip fade on/off and session auto-fades (engine patch 0001).
        te::FadeOverride getFadeOverride (const te::AudioClipBase& clip, bool isFadeIn) override
        {
            return fades::resolve (clip, isFadeIn);
        }

        // %projectdir% resolves to the session's folder; takes go in "<Session> Audio/".
        juce::String getDefaultAudioRecordingFilePattern() override
        {
            return "%projectdir%/%edit% Audio/%track% Take %take%";
        }
    };

    struct EngineProfile
    {
        static constexpr int recordBitDepth = 32;   // IEEE float WAV

        /** Must be called before any Edit's playback context is created. */
        static void applyGlobal()
        {
            te::EditPlaybackContext::enableAudioWorkgroup (true);
            te::EditPlaybackContext::setThreadPoolStrategy ((int) tracktion::graph::ThreadPoolStrategy::realTime);
            te::setDefaultPanLaw (te::PanLawLinear);
        }

        /** Applies recording format to every audio input. Call after device changes too. */
        static void applyToInputs (te::Engine& engine)
        {
            auto& dm = engine.getDeviceManager();
            const auto wavName = engine.getAudioFileFormatManager().getWavFormat()->getFormatName();

            for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
                if (auto* wip = dm.getWaveInDevice (i))
                {
                    wip->setOutputFormat (wavName);
                    wip->setBitDepth (recordBitDepth);
                }
        }

        /** Unity master (Tracktion creates new Edits with master at −3 dB; Pyramix is 0 dB).
            Applied once per session; a session tagged with the profile keeps the user's level. */
        static void applyToEdit (te::Edit& edit)
        {
            static const juce::Identifier tag ("tonkoriProfile");

            if ((int) edit.state.getProperty (tag, 0) >= 1)
                return;

            if (auto mv = edit.getMasterVolumePlugin())
            {
                mv->setVolumeDb (0.0f);
                mv->setPan (0.0f);
            }

            edit.state.setProperty (tag, 1, nullptr);
        }

        static juce::String describe (te::Engine& engine)
        {
            auto& dm = engine.getDeviceManager();
            const bool wg = (bool) dm.deviceManager.getDeviceAudioWorkgroup();

            return juce::String ("32f engine | rec WAV 32f | ")
                 + juce::String (juce::SystemStats::getNumCpus()) + " thr"
                 + (wg ? " +workgroup" : "");
        }
    };
}
