#pragma once

#include <JuceHeader.h>
#include <optional>

namespace te = tracktion;

namespace tonkori::ui
{
    enum class TimeFormat { barsBeats, timecode, minSec, samples };
    enum class Tool       { smart, range, grab, trim, fade, split };      // keys 1–6 (Cubase-style)
    enum class EditMode   { slip, grid, shuffle, spot };                  // Pro Tools edit modes

    inline juce::StringArray toolNames()     { return { "Smart", "Range", "Grab", "Trim", "Fade", "Split" }; }
    inline juce::StringArray editModeNames() { return { "Slip", "Grid", "Shuffle", "Spot" }; }

    struct GridDivision { const char* name; double beats; };
    inline const std::vector<GridDivision>& gridDivisions()
    {
        static const std::vector<GridDivision> g {
            { "1 bar", -1.0 }, { "1/2", 2.0 }, { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 }, { "1/32", 0.125 }, { "1/64", 0.0625 } };
        return g;
    }

    inline juce::StringArray fpsNames() { return { "23.976", "24", "25", "29.97 DF", "30" }; }
    inline double fpsValue (int i)      { static const double v[] = { 24000.0 / 1001.0, 24.0, 25.0, 30000.0 / 1001.0, 30.0 }; return v[juce::jlimit (0, 4, i)]; }

    //==============================================================================
    /** Shared view model for the Edit window: zoom/scroll, tools, grid, time display,
        range selection and the insert marker. Components listen for changes. */
    class ViewState : public juce::ChangeBroadcaster
    {
    public:
        ViewState (te::Edit& e, te::SelectionManager& sm) : edit (e), selection (sm) {}

        te::Edit& edit;
        te::SelectionManager& selection;

        // Horizontal view
        double viewStart = 0.0;            // seconds at the left edge of the arrangement
        double pixelsPerSecond = 60.0;
        int scrollY = 0;

        // Tools and modes
        Tool tool = Tool::smart;
        EditMode mode = EditMode::slip;
        int gridIndex = 4;                 // 1/16
        bool triplet = false;
        bool follow = true;

        // Time display
        TimeFormat timeFormat = TimeFormat::barsBeats;
        TimeFormat secondaryFormat = TimeFormat::timecode;
        int fpsIndex = 4;                  // 30 fps

        // Selection
        std::optional<te::TimeRange> timeSelection;
        juce::Array<te::EditItemID> rangeTracks;
        te::TimePosition insertMarker;

        // Panels
        bool showInspector = true;
        bool showMixer = false;

        void changed() { sendChangeMessage(); }

        //==============================================================================
        float timeToX (te::TimePosition t) const        { return (float) ((t.inSeconds() - viewStart) * pixelsPerSecond); }
        float durationToWidth (te::TimeDuration d) const { return (float) (d.inSeconds() * pixelsPerSecond); }
        te::TimePosition xToTime (double x) const        { return te::TimePosition::fromSeconds (juce::jmax (0.0, viewStart + x / pixelsPerSecond)); }

        void setZoom (double newPps, double anchorX)
        {
            newPps = juce::jlimit (0.5, 20000.0, newPps);
            const double anchorTime = viewStart + anchorX / pixelsPerSecond;
            pixelsPerSecond = newPps;
            viewStart = juce::jmax (0.0, anchorTime - anchorX / pixelsPerSecond);
            changed();
        }

        //==============================================================================
        bool isSnapping() const     { return mode == EditMode::grid; }

        te::BeatDuration gridBeats (te::TimePosition at) const
        {
            auto& d = gridDivisions()[(size_t) juce::jlimit (0, (int) gridDivisions().size() - 1, gridIndex)];
            double beats = d.beats;
            if (beats < 0) beats = (double) edit.tempoSequence.getTimeSigAt (at).numerator;
            if (triplet) beats *= 2.0 / 3.0;
            return te::BeatDuration::fromBeats (beats);
        }

        te::TimePosition snap (te::TimePosition t, bool force = false) const
        {
            if (! (force || isSnapping())) return t;
            auto& ts = edit.tempoSequence;
            const auto beat = ts.toBeats (t).inBeats();
            const auto g = gridBeats (t).inBeats();
            return ts.toTime (te::BeatPosition::fromBeats (std::round (beat / g) * g));
        }

        //==============================================================================
        double sampleRate() const
        {
            auto sr = edit.engine.getDeviceManager().getSampleRate();
            return sr > 0 ? sr : 48000.0;
        }

        juce::String format (te::TimePosition t, TimeFormat f) const
        {
            const double s = t.inSeconds();
            switch (f)
            {
                case TimeFormat::barsBeats:
                {
                    auto bb = edit.tempoSequence.toBarsAndBeats (t);
                    const int tick = juce::roundToInt (bb.getFractionalBeats().inBeats() * 960.0) % 960;
                    return juce::String (bb.bars + 1) + "." + juce::String (bb.getWholeBeats() + 1) + "." + juce::String (tick).paddedLeft ('0', 3);
                }
                case TimeFormat::timecode:
                {
                    const double fps = fpsValue (fpsIndex);
                    const int nominal = juce::roundToInt (fps);
                    auto frames = (juce::int64) std::floor (s * fps + 1e-9);
                    const auto ff = frames % nominal; frames /= nominal;
                    return juce::String::formatted ("%02d:%02d:%02d:%02d", (int) (frames / 3600), (int) ((frames / 60) % 60), (int) (frames % 60), (int) ff);
                }
                case TimeFormat::minSec:
                {
                    const auto ms = (juce::int64) std::llround (s * 1000.0);
                    return juce::String::formatted ("%d:%02d.%03d", (int) (ms / 60000), (int) ((ms / 1000) % 60), (int) (ms % 1000));
                }
                case TimeFormat::samples:
                    return juce::String ((juce::int64) std::llround (s * sampleRate()));
            }
            return {};
        }

        juce::String format (te::TimePosition t) const     { return format (t, timeFormat); }

        juce::String formatDuration (te::TimeDuration d) const
        {
            switch (timeFormat)
            {
                case TimeFormat::samples:   return juce::String ((juce::int64) std::llround (d.inSeconds() * sampleRate())) + " smp";
                case TimeFormat::barsBeats:
                {
                    auto& ts = edit.tempoSequence;
                    const auto beats = ts.toBeats (te::TimePosition() + d).inBeats();
                    const int num = edit.tempoSequence.getTimeSigAt (te::TimePosition()).numerator;
                    const int whole = (int) std::floor (beats + 1e-9);
                    const int tick = juce::roundToInt ((beats - whole) * 960.0) % 960;
                    return juce::String (whole / num) + "." + juce::String (whole % num) + "." + juce::String (tick).paddedLeft ('0', 3);
                }
                default:                    return juce::String (d.inSeconds() * 1000.0, 3) + " ms";
            }
        }

        /** Parses a position typed in any format: "5.2.480" (bars.beats.ticks), "00:01:02:12"
            (timecode), "1:02.345" (min:sec), "123456" or "123456 smp" (samples), "1.5 s", "250 ms". */
        std::optional<te::TimePosition> parse (juce::String text) const
        {
            text = text.trim().toLowerCase();
            if (text.isEmpty()) return {};

            if (text.endsWith ("ms"))   return te::TimePosition::fromSeconds (text.dropLastCharacters (2).getDoubleValue() / 1000.0);
            if (text.endsWith ("smp"))  return te::TimePosition::fromSeconds (text.dropLastCharacters (3).getDoubleValue() / sampleRate());
            if (text.endsWith ("s") && ! text.containsChar (':'))
                return te::TimePosition::fromSeconds (text.dropLastCharacters (1).getDoubleValue());

            auto colons = juce::StringArray::fromTokens (text, ":", "");
            if (colons.size() == 4)
            {
                const double fps = fpsValue (fpsIndex);
                const double frames = ((colons[0].getIntValue() * 60.0 + colons[1].getIntValue()) * 60.0 + colons[2].getIntValue()) * std::round (fps)
                                    + colons[3].getIntValue();
                return te::TimePosition::fromSeconds (frames / fps);
            }
            if (colons.size() == 2)
                return te::TimePosition::fromSeconds (colons[0].getIntValue() * 60.0 + colons[1].getDoubleValue());

            auto dots = juce::StringArray::fromTokens (text, ".", "");
            if (dots.size() == 3 || (dots.size() == 2 && timeFormat == TimeFormat::barsBeats))
            {
                const int bar = juce::jmax (1, dots[0].getIntValue());
                const int beat = juce::jmax (1, dots[1].getIntValue());
                const int tick = dots.size() == 3 ? dots[2].getIntValue() : 0;
                const int num = edit.tempoSequence.getTimeSigAt (te::TimePosition()).numerator;
                const double beats = (bar - 1) * num + (beat - 1) + tick / 960.0;
                return edit.tempoSequence.toTime (te::BeatPosition::fromBeats (beats));
            }

            if (timeFormat == TimeFormat::samples)
                return te::TimePosition::fromSeconds (text.getDoubleValue() / sampleRate());

            return te::TimePosition::fromSeconds (text.getDoubleValue());
        }

        /** Durations: same syntax; a bare number means milliseconds unless in samples/bars mode. */
        std::optional<te::TimeDuration> parseDuration (juce::String text) const
        {
            text = text.trim().toLowerCase();
            if (text.isEmpty()) return {};

            if (timeFormat == TimeFormat::barsBeats && text.containsChar ('.') && ! text.endsWith ("s"))
            {
                auto dots = juce::StringArray::fromTokens (text, ".", "");
                const int num = edit.tempoSequence.getTimeSigAt (te::TimePosition()).numerator;
                const double beats = dots[0].getIntValue() * num + dots[1].getIntValue() + dots[2].getIntValue() / 960.0;
                return edit.tempoSequence.toTime (te::BeatPosition::fromBeats (beats)) - te::TimePosition();
            }

            const bool unitless = text.containsOnly ("0123456789.-");
            if (unitless)
            {
                if (timeFormat == TimeFormat::samples) return te::TimeDuration::fromSeconds (text.getDoubleValue() / sampleRate());
                return te::TimeDuration::fromSeconds (text.getDoubleValue() / 1000.0);
            }

            if (auto p = parse (text))
                return *p - te::TimePosition();
            return {};
        }

        /** Fade lengths read best in ms (samples when the display is in samples). */
        juce::String formatFadeLength (te::TimeDuration d) const
        {
            if (timeFormat == TimeFormat::samples) return juce::String ((juce::int64) std::llround (d.inSeconds() * sampleRate())) + " smp";
            return juce::String (d.inSeconds() * 1000.0, 2) + " ms";
        }

        /** Bare numbers are milliseconds; also accepts "s", "ms", "smp", and bars.beats.ticks. */
        std::optional<te::TimeDuration> parseFadeLength (juce::String text) const
        {
            text = text.trim().toLowerCase();
            if (text.isEmpty()) return {};
            if (text.containsOnly ("0123456789.") && text.indexOfChar ('.') == text.lastIndexOfChar ('.'))
                return te::TimeDuration::fromSeconds (text.getDoubleValue() / 1000.0);
            return parseDuration (text);
        }

        //==============================================================================
        int getTrackHeight (const te::Track& t) const
        {
            return juce::jlimit (36, 400, (int) t.state.getProperty ("tkHeight", 84));
        }

        void setTrackHeight (te::Track& t, int h)
        {
            t.state.setProperty ("tkHeight", juce::jlimit (36, 400, h), nullptr);
            changed();
        }
    };
}
