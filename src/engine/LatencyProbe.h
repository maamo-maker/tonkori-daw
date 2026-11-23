#pragma once

#include <JuceHeader.h>

/*  Round-trip latency measurement (Reaper / Pyramix style loopback test).

    Plays a 1 s logarithmic sweep on one output channel and records one input channel;
    the cross-correlation peak (FFT, with parabolic interpolation) gives the true round
    trip in samples: driver + safety offsets + converters + any outboard on the loop.
    Compared with what the driver reports, the difference is the offset recordings need. */

namespace tonkori::latency
{
    /** The probe signal and the delay estimator, independent of any device. */
    struct Probe
    {
        static std::vector<float> makeSweep (double sampleRate, double seconds = 1.0, float gain = 0.25f)
        {
            const int n = (int) (sampleRate * seconds);
            std::vector<float> s ((size_t) n);
            const double f1 = 40.0, f2 = juce::jmin (18000.0, sampleRate * 0.4);
            const double k = std::log (f2 / f1);
            for (int i = 0; i < n; ++i)
            {
                const double t = i / sampleRate;
                const double ph = 2 * juce::MathConstants<double>::pi * f1 * seconds / k * (std::exp (t / seconds * k) - 1.0);
                const double env = juce::jmin (1.0, juce::jmin (i, n - 1 - i) / (0.01 * sampleRate));   // 10 ms tapers
                s[(size_t) i] = (float) (gain * env * std::sin (ph));
            }
            return s;
        }

        struct Estimate { double samples = -1; double confidence = 0; };

        /** Delay of 'received' relative to 'sent' (0 … maxLag samples). */
        static Estimate estimate (const std::vector<float>& sent, const std::vector<float>& received, int maxLag)
        {
            const int need = (int) sent.size() + maxLag;
            int order = 1;
            while ((1 << order) < need * 2) ++order;
            const int N = 1 << order;

            juce::dsp::FFT fft (order);
            std::vector<float> a ((size_t) N * 2, 0.0f), b ((size_t) N * 2, 0.0f);
            std::copy (sent.begin(), sent.end(), a.begin());
            std::copy (received.begin(), received.begin() + juce::jmin ((int) received.size(), N), b.begin());
            fft.performRealOnlyForwardTransform (a.data());
            fft.performRealOnlyForwardTransform (b.data());

            // cross-spectrum B · conj(A), phase transform weighting for a sharp peak
            for (int i = 0; i <= N / 2; ++i)
            {
                const float ar = a[(size_t) (2 * i)], ai = a[(size_t) (2 * i + 1)];
                const float br = b[(size_t) (2 * i)], bi = b[(size_t) (2 * i + 1)];
                float re = br * ar + bi * ai, im = bi * ar - br * ai;
                const float mag = std::sqrt (re * re + im * im) + 1.0e-12f;
                b[(size_t) (2 * i)] = re / mag; b[(size_t) (2 * i + 1)] = im / mag;
            }
            fft.performRealOnlyInverseTransform (b.data());

            int best = 0;
            float bestV = -1, sum = 0;
            for (int lag = 0; lag <= maxLag && lag < N; ++lag)
            {
                const float v = b[(size_t) lag];
                sum += std::abs (v);
                if (v > bestV) { bestV = v; best = lag; }
            }

            double frac = 0;
            if (best > 0 && best < maxLag)
            {
                const double y0 = b[(size_t) (best - 1)], y1 = b[(size_t) best], y2 = b[(size_t) (best + 1)];
                const double d = y0 - 2 * y1 + y2;
                if (std::abs (d) > 1e-12) frac = 0.5 * (y0 - y2) / d;
            }
            const float mean = sum / (float) (maxLag + 1);
            return { best + frac, mean > 0 ? bestV / mean : 0.0 };
        }
    };

    /** Runs the probe through a live device (added as an extra AudioDeviceManager callback). */
    class Measurement : public juce::AudioIODeviceCallback
    {
    public:
        Measurement (int outChannel, int inChannel) : outCh (outChannel), inCh (inChannel) {}

        void audioDeviceAboutToStart (juce::AudioIODevice* d) override
        {
            sampleRate = d->getCurrentSampleRate();
            reportedIn = d->getInputLatencyInSamples();
            reportedOut = d->getOutputLatencyInSamples();
            sweep = Probe::makeSweep (sampleRate);
            recorded.assign (sweep.size() + (size_t) (sampleRate * 1.0), 0.0f);
            pos = 0;
            done = false;
        }

        void audioDeviceStopped() override {}

        void audioDeviceIOCallbackWithContext (const float* const* ins, int numIns, float* const* outs, int numOuts,
                                               int n, const juce::AudioIODeviceCallbackContext&) override
        {
            for (int ch = 0; ch < numOuts; ++ch)
                if (outs[ch] != nullptr) juce::FloatVectorOperations::clear (outs[ch], n);
            if (done) return;

            for (int i = 0; i < n; ++i, ++pos)
            {
                if (pos < recorded.size())
                {
                    if (outCh < numOuts && outs[outCh] != nullptr && pos < sweep.size()) outs[outCh][i] = sweep[pos];
                    if (inCh < numIns && ins[inCh] != nullptr) recorded[pos] = ins[inCh][i];
                }
            }
            if (pos >= recorded.size()) done = true;
        }

        bool isDone() const { return done; }

        Probe::Estimate result() const { return Probe::estimate (sweep, recorded, (int) (sampleRate * 0.9)); }

        double sampleRate = 48000;
        int reportedIn = 0, reportedOut = 0;

    private:
        int outCh, inCh;
        std::vector<float> sweep, recorded;
        size_t pos = 0;
        std::atomic<bool> done { false };
    };
}
