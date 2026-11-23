#pragma once

#include <JuceHeader.h>
#include <array>
#include <deque>

/*  ITU-R BS.1770-4 / EBU R128 loudness and true-peak metering.

    LoudnessCore runs on the audio thread (no allocation after prepare): K-weighting
    (two biquads designed for the actual sample rate), channel weighting, 100 ms
    sub-block energy, and 4x-oversampled true peak (2x at 88.2/96 kHz, 1x at 176.4 kHz+).
    LoudnessStats turns 100 ms sub-blocks into momentary (400 ms), short-term (3 s),
    gated integrated loudness (absolute −70 LUFS, relative −10 LU) and loudness range
    (EBU Tech 3342: 3 s windows, −20 LU relative gate, 10th–95th percentile). */

namespace tonkori::loudness
{
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;
        double process (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void reset() noexcept { z1 = z2 = 0; }
    };

    class LoudnessCore
    {
    public:
        static constexpr int maxChannels = 8;

        void prepare (double sampleRate, int numChannels);
        void reset();

        /** Calls onSubBlock (weightedMeanSquare, truePeakLinear) every 100 ms of audio. */
        template <typename Fn>
        void process (const float* const* data, int numChannels, int numSamples, Fn&& onSubBlock)
        {
            numChannels = juce::jmin (numChannels, channels);
            for (int i = 0; i < numSamples; ++i)
            {
                double sum = 0;
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const float x = data[ch][i];
                    const double y = stage2[(size_t) ch].process (stage1[(size_t) ch].process (x));
                    sum += weights[(size_t) ch] * y * y;
                    peak = juce::jmax (peak, truePeakSample (ch, x));
                }
                energy += sum;
                if (++count >= subBlockSize)
                {
                    onSubBlock (energy / count, peak);
                    energy = 0; count = 0; peak = 0;
                }
            }
        }

        int getSubBlockSize() const noexcept { return subBlockSize; }

    private:
        double fs = 48000;
        int channels = 2, subBlockSize = 4800, count = 0, factor = 4, tapsPerPhase = 24;
        double energy = 0;
        float peak = 0;
        std::array<Biquad, maxChannels> stage1, stage2;
        std::array<double, maxChannels> weights {};
        std::vector<float> phases;                                    // factor x tapsPerPhase
        std::array<std::array<float, 64>, maxChannels> history {};    // ring of last taps
        std::array<int, maxChannels> histPos {};

        float truePeakSample (int ch, float x) noexcept;
    };

    class LoudnessStats
    {
    public:
        void reset();
        void addSubBlock (double meanSquare, float truePeak);

        double momentary() const;          // LUFS, −inf if no signal
        double shortTerm() const;
        double integrated() const;
        double loudnessRange() const;      // LU
        double truePeakDb() const          { return juce::Decibels::gainToDecibels ((double) maxTruePeak, -200.0); }
        double maxMomentary() const        { return maxM; }
        double maxShortTerm() const        { return maxS; }
        double seconds() const             { return (double) totalSubBlocks * 0.1; }

        static double toLufs (double meanSquare) { return meanSquare > 0 ? -0.691 + 10.0 * std::log10 (meanSquare) : -std::numeric_limits<double>::infinity(); }

    private:
        static constexpr int bins = 1000;                 // 0.1 LU bins from −70 to +30 LUFS
        static constexpr double lo = -70.0, step = 0.1;
        std::deque<double> recent;                        // last 30 sub-blocks
        std::array<double, bins> blockCount {}, blockPower {}, stCount {}, stPower {};
        float maxTruePeak = 0;
        double maxM = -std::numeric_limits<double>::infinity(), maxS = -std::numeric_limits<double>::infinity();
        juce::int64 totalSubBlocks = 0;

        double meanOfLast (int n) const;
        static int binOf (double lufs) { return juce::jlimit (0, bins - 1, (int) std::floor ((lufs - lo) / step)); }
    };

    /** Offline analysis of a whole buffer (used for exports and the self-test). */
    struct Result { double integrated, shortTermMax, momentaryMax, lra, truePeakDb; };
    Result analyse (const juce::AudioBuffer<float>&, double sampleRate);
    Result analyse (const juce::File&, juce::AudioFormatManager&);

    juce::String describe (const Result&);
}
