#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

/*  Time-stretching and pitch for audio clips (Pro Tools TCE / Elastic Audio, Live warp,
    Cubase AudioWarp, Pyramix time-stretch).

    Stretch is a percentage of the original length (200 % = twice as long, half speed).
    Algorithms: Rubber Band R3 (formant-preserving, best for music and voice), Rubber Band
    percussive (transient-tight), Signalsmith Stretch, SoundTouch, or Varispeed (pitch
    follows speed, like tape). Stretched clips are processed in real time with the
    best sinc resampler, so changes are heard immediately, also while playing. */

namespace tonkori::stretch
{
    struct Algorithm { const char* name; te::TimeStretcher::Mode mode; };

    inline const std::vector<Algorithm>& algorithms()
    {
        static const std::vector<Algorithm> a
        {
            { "Rubber Band R3 (music, voice)", te::TimeStretcher::rubberbandMelodic },
            { "Rubber Band (percussive)",      te::TimeStretcher::rubberbandPercussive },
            { "Signalsmith Stretch",           te::TimeStretcher::signalsmithDefault },
            { "SoundTouch",                    te::TimeStretcher::soundtouchBetter },
            { "Varispeed (pitch follows)",     te::TimeStretcher::disabled },
        };
        return a;
    }

    inline int algorithmIndex (const te::AudioClipBase& c)
    {
        const auto m = c.getTimeStretchMode();
        for (int i = 0; i < (int) algorithms().size(); ++i)
            if (algorithms()[(size_t) i].mode == m) return i;
        return 0;
    }

    inline double getPercent (const te::AudioClipBase& c)  { return 100.0 / juce::jmax (0.0001, c.getSpeedRatio()); }

    inline void prepare (te::AudioClipBase& c)
    {
        c.setUsesProxy (false);
        c.setResamplingQuality (te::ResamplingQuality::sincBest);
        if (c.getTimeStretchMode() == te::TimeStretcher::disabled && ! c.state.hasProperty ("tkStretchSet"))
            c.setTimeStretchMode (te::TimeStretcher::rubberbandMelodic);
        c.state.setProperty ("tkStretchSet", true, nullptr);
    }

    inline void setAlgorithm (te::AudioClipBase& c, int index)
    {
        prepare (c);
        c.setTimeStretchMode (algorithms()[(size_t) juce::jlimit (0, (int) algorithms().size() - 1, index)].mode);
        c.edit.restartPlayback();
    }

    /** Sets the stretch, keeping the clip's start and content start; the length follows. */
    inline void setPercent (te::AudioClipBase& c, double percent)
    {
        percent = juce::jlimit (5.0, 2000.0, percent);
        prepare (c);
        const auto pos = c.getPosition();
        const double oldRatio = c.getSpeedRatio();
        const double newRatio = 100.0 / percent;
        const double sourceSecs = pos.getLength().inSeconds() * oldRatio;      // source material shown
        const double offsetSrc = pos.getOffset().inSeconds() * oldRatio;
        c.setSpeedRatio (newRatio);
        c.setOffset (te::TimeDuration::fromSeconds (offsetSrc / newRatio));
        c.setLength (te::TimeDuration::fromSeconds (sourceSecs / newRatio), true);
        c.edit.restartPlayback();
    }

    /** TCE trim: drag the clip end (or start) and the content is stretched to fit. */
    inline void tceTrim (te::AudioClipBase& c, const te::ClipPosition& original, double originalRatio,
                         te::TimePosition newEdge, bool isEnd)
    {
        prepare (c);
        const double origLen = original.getLength().inSeconds();
        const double newLen = isEnd ? (newEdge - original.getStart()).inSeconds()
                                    : (original.getEnd() - newEdge).inSeconds();
        if (newLen < 0.005 || origLen <= 0) return;
        const double ratio = juce::jlimit (0.05, 20.0, originalRatio * origLen / newLen);
        const double offsetSrc = original.getOffset().inSeconds() * originalRatio;

        c.setSpeedRatio (ratio);
        c.setOffset (te::TimeDuration::fromSeconds (offsetSrc / ratio));
        if (isEnd)
            c.setLength (te::TimeDuration::fromSeconds (newLen), true);
        else
        {
            c.setStart (newEdge, false, true);
            c.setLength (te::TimeDuration::fromSeconds (newLen), true);
        }
        c.edit.restartPlayback();
    }

    inline void setPitch (te::AudioClipBase& c, double semitones)
    {
        prepare (c);
        c.setPitchChange ((float) juce::jlimit (-24.0, 24.0, semitones));
        c.edit.restartPlayback();
    }

    inline void reset (te::AudioClipBase& c)
    {
        setPercent (c, 100.0);
        c.setPitchChange (0.0f);
        c.edit.restartPlayback();
    }

    inline bool isStretched (const te::AudioClipBase& c)
    {
        return std::abs (c.getSpeedRatio() - 1.0) > 1.0e-6 || std::abs (c.getPitchChange()) > 1.0e-4f;
    }
}
