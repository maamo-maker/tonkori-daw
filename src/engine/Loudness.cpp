#include "Loudness.h"

namespace tonkori::loudness
{
    //==============================================================================
    void LoudnessCore::prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate > 0 ? sampleRate : 48000.0;
        channels = juce::jlimit (1, maxChannels, numChannels);
        subBlockSize = juce::jmax (1, (int) std::lround (fs * 0.1));

        // K-weighting, BS.1770-4, re-derived for this sample rate (as in libebur128).
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            Biquad b;
            b.b0 = (Vh + Vb * K / Q + K * K) / a0;
            b.b1 = 2.0 * (K * K - Vh) / a0;
            b.b2 = (Vh - Vb * K / Q + K * K) / a0;
            b.a1 = 2.0 * (K * K - 1.0) / a0;
            b.a2 = (1.0 - K / Q + K * K) / a0;
            stage1.fill (b);
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            Biquad b;
            b.b0 = 1.0; b.b1 = -2.0; b.b2 = 1.0;
            b.a1 = 2.0 * (K * K - 1.0) / a0;
            b.a2 = (1.0 - K / Q + K * K) / a0;
            stage2.fill (b);
        }

        // Channel weights for 5.1 (L R C LFE Ls Rs): surrounds +1.5 dB, LFE excluded.
        for (int ch = 0; ch < maxChannels; ++ch)
            weights[(size_t) ch] = (channels == 6 && ch == 3) ? 0.0 : ((channels == 6 && ch >= 4) ? 1.41 : 1.0);

        // True-peak interpolator: Kaiser-windowed sinc, polyphase.
        factor = fs < 88000.0 ? 4 : (fs < 176000.0 ? 2 : 1);
        tapsPerPhase = 24;
        const int N = factor * tapsPerPhase;
        phases.assign ((size_t) N, 0.0f);
        if (factor > 1)
        {
            const double centre = (N - 1) / 2.0, cutoff = 0.5 / factor * 0.96, beta = 8.0;
            auto bessel = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
            std::vector<double> h ((size_t) N);
            for (int n = 0; n < N; ++n)
            {
                const double m = n - centre;
                const double sinc = m == 0 ? 2 * cutoff : std::sin (2 * juce::MathConstants<double>::pi * cutoff * m) / (juce::MathConstants<double>::pi * m);
                const double r = 2.0 * n / (N - 1) - 1.0;
                h[(size_t) n] = sinc * bessel (beta * std::sqrt (juce::jmax (0.0, 1 - r * r))) / bessel (beta);
            }
            for (int p = 0; p < factor; ++p)
            {
                double s = 0;
                for (int k = 0; k < tapsPerPhase; ++k) s += h[(size_t) (k * factor + p)];
                for (int k = 0; k < tapsPerPhase; ++k)
                    phases[(size_t) (p * tapsPerPhase + k)] = (float) (h[(size_t) (k * factor + p)] / s);
            }
        }
        reset();
    }

    void LoudnessCore::reset()
    {
        for (auto& b : stage1) b.reset();
        for (auto& b : stage2) b.reset();
        for (auto& h : history) h.fill (0.0f);
        histPos.fill (0);
        energy = 0; count = 0; peak = 0;
    }

    float LoudnessCore::truePeakSample (int ch, float x) noexcept
    {
        if (factor == 1) return std::abs (x);

        auto& h = history[(size_t) ch];
        auto& pos = histPos[(size_t) ch];
        h[(size_t) pos] = x;
        float best = std::abs (x);

        for (int p = 0; p < factor; ++p)
        {
            const float* c = phases.data() + p * tapsPerPhase;
            float acc = 0;
            int idx = pos;
            for (int k = 0; k < tapsPerPhase; ++k)
            {
                acc += c[k] * h[(size_t) idx];
                idx = idx == 0 ? 63 : idx - 1;
            }
            best = juce::jmax (best, std::abs (acc));
        }
        pos = (pos + 1) & 63;
        return best;
    }

    //==============================================================================
    void LoudnessStats::reset()
    {
        recent.clear();
        blockCount.fill (0); blockPower.fill (0); stCount.fill (0); stPower.fill (0);
        maxTruePeak = 0;
        maxM = maxS = -std::numeric_limits<double>::infinity();
        totalSubBlocks = 0;
    }

    double LoudnessStats::meanOfLast (int n) const
    {
        if ((int) recent.size() < n) return 0.0;
        double s = 0;
        for (size_t i = recent.size() - (size_t) n; i < recent.size(); ++i) s += recent[i];
        return s / n;
    }

    void LoudnessStats::addSubBlock (double meanSquare, float tp)
    {
        recent.push_back (meanSquare);
        if (recent.size() > 30) recent.pop_front();
        maxTruePeak = juce::jmax (maxTruePeak, tp);
        ++totalSubBlocks;

        if (recent.size() >= 4)
        {
            const double p = meanOfLast (4);
            const double l = toLufs (p);
            maxM = juce::jmax (maxM, l);
            if (l > lo) { const int b = binOf (l); blockCount[(size_t) b] += 1; blockPower[(size_t) b] += p; }
        }
        if (recent.size() >= 30)
        {
            const double p = meanOfLast (30);
            const double l = toLufs (p);
            maxS = juce::jmax (maxS, l);
            if (l > lo) { const int b = binOf (l); stCount[(size_t) b] += 1; stPower[(size_t) b] += p; }
        }
    }

    double LoudnessStats::momentary() const  { return recent.size() >= 4 ? toLufs (meanOfLast (4)) : -std::numeric_limits<double>::infinity(); }
    double LoudnessStats::shortTerm() const  { return recent.size() >= 30 ? toLufs (meanOfLast (30)) : -std::numeric_limits<double>::infinity(); }

    double LoudnessStats::integrated() const
    {
        double n = 0, p = 0;
        for (int b = 0; b < bins; ++b) { n += blockCount[(size_t) b]; p += blockPower[(size_t) b]; }
        if (n <= 0) return -std::numeric_limits<double>::infinity();

        const double relGate = toLufs (p / n) - 10.0;
        n = 0; p = 0;
        for (int b = 0; b < bins; ++b)
        {
            const double binLo = lo + b * step;
            if (binLo + step <= relGate) continue;
            if (binLo < relGate)
            {
                // Partially gated bin: count only blocks above the gate (mean power of bin decides).
                if (blockCount[(size_t) b] > 0 && toLufs (blockPower[(size_t) b] / blockCount[(size_t) b]) < relGate) continue;
            }
            n += blockCount[(size_t) b]; p += blockPower[(size_t) b];
        }
        return n > 0 ? toLufs (p / n) : -std::numeric_limits<double>::infinity();
    }

    double LoudnessStats::loudnessRange() const
    {
        double n = 0, p = 0;
        for (int b = 0; b < bins; ++b) { n += stCount[(size_t) b]; p += stPower[(size_t) b]; }
        if (n <= 0) return 0.0;
        const double relGate = toLufs (p / n) - 20.0;

        double total = 0;
        for (int b = 0; b < bins; ++b)
            if (lo + (b + 0.5) * step >= relGate) total += stCount[(size_t) b];
        if (total <= 0) return 0.0;

        auto percentile = [&] (double q)
        {
            const double target = q * total;
            double acc = 0;
            for (int b = 0; b < bins; ++b)
            {
                if (lo + (b + 0.5) * step < relGate) continue;
                acc += stCount[(size_t) b];
                if (acc >= target) return lo + (b + 0.5) * step;
            }
            return lo + bins * step;
        };
        return juce::jmax (0.0, percentile (0.95) - percentile (0.10));
    }

    //==============================================================================
    Result analyse (const juce::AudioBuffer<float>& buf, double sampleRate)
    {
        LoudnessCore core;
        LoudnessStats stats;
        core.prepare (sampleRate, buf.getNumChannels());
        core.process (buf.getArrayOfReadPointers(), buf.getNumChannels(), buf.getNumSamples(),
                      [&] (double ms, float tp) { stats.addSubBlock (ms, tp); });
        return { stats.integrated(), stats.maxShortTerm(), stats.maxMomentary(), stats.loudnessRange(), stats.truePeakDb() };
    }

    Result analyse (const juce::File& f, juce::AudioFormatManager& fm)
    {
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        if (r == nullptr) return { -INFINITY, -INFINITY, -INFINITY, 0, -INFINITY };

        LoudnessCore core;
        LoudnessStats stats;
        const int nch = (int) juce::jmin (r->numChannels, (unsigned int) LoudnessCore::maxChannels);
        core.prepare (r->sampleRate, nch);
        juce::AudioBuffer<float> buf (nch, 65536);
        for (juce::int64 pos = 0; pos < r->lengthInSamples; pos += buf.getNumSamples())
        {
            const int n = (int) juce::jmin ((juce::int64) buf.getNumSamples(), r->lengthInSamples - pos);
            r->read (&buf, 0, n, pos, true, true);
            core.process (buf.getArrayOfReadPointers(), nch, n, [&] (double ms, float tp) { stats.addSubBlock (ms, tp); });
        }
        return { stats.integrated(), stats.maxShortTerm(), stats.maxMomentary(), stats.loudnessRange(), stats.truePeakDb() };
    }

    juce::String describe (const Result& r)
    {
        auto f = [] (double v) { return std::isfinite (v) ? juce::String (v, 1) : juce::String ("-inf"); };
        return f (r.integrated) + " LUFS integrated, LRA " + juce::String (r.lra, 1) + " LU, true peak " + f (r.truePeakDb) + " dBTP";
    }
}
