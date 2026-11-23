/*  Tonkori DAW engine self-test (headless).

    1. Null test: a file placed on a track at unity and rendered through the full
       engine (track volume/pan, master volume) must come out bit-identical,
       at 96 kHz and at DXD (352.8 kHz), from both float and 24-bit sources.
    2. DSD: a synthesized DSD64 stream written as DSF and as DSDIFF must decode to
       identical DXD; level, timing (group-delay compensation) and in-band SNR are measured.

    Exit code 0 = all pass. */

#include <JuceHeader.h>
#include "EngineProfile.h"
#include "dsd/DsdDecoder.h"
#include "TaskRunner.h"
#include "model/ClipFades.h"
#include <array>
#include <map>

namespace te = tracktion;
using namespace tracktion::literals;

static int failures = 0;

struct HeadlessUI : public te::UIBehaviour
{
    void runTaskWithProgressBar (te::ThreadPoolJobWithProgress& job) override
    {
        tonkori::runJobToCompletion (job);
        if (auto* rt = dynamic_cast<te::Renderer::RenderTask*> (&job); rt != nullptr && rt->errorMessage.isNotEmpty())
            std::cout << "        render error: " << rt->errorMessage << std::endl;
    }
    void showWarningMessage (const juce::String& m) override { std::cout << "        warning: " << m << std::endl; }
};

static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

//==============================================================================
static juce::AudioBuffer<float> readAll (const juce::File& f, double& rate)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) return {};
    rate = r->sampleRate;
    juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
    r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
    return b;
}

static bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double rate, int bits)
{
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (f);
    juce::WavAudioFormat wav;
    auto opts = juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (b.getNumChannels()).withBitsPerSample (bits);
    if (bits == 32) opts = opts.withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    auto w = wav.createWriterFor (os, opts);
    return w != nullptr && w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

//==============================================================================
static void nullTest (te::Engine& engine, const juce::File& dir, double rate, int sourceBits)
{
    const juce::String label = juce::String (rate / 1000.0, 1) + " kHz, " + (sourceBits == 32 ? "32f" : juce::String (sourceBits) + "-bit") + " source";
    const int n = (int) (rate * 3.0);

    juce::AudioBuffer<float> src (2, n);
    juce::Random rng (1234);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < n; ++i)
            src.setSample (c, i, (rng.nextFloat() * 2.0f - 1.0f) * 0.7f);

    auto srcFile = dir.getChildFile ("null_src_" + juce::String ((int) rate) + "_" + juce::String (sourceBits) + ".wav");
    writeWav (srcFile, src, rate, sourceBits);

    // Re-read so the reference is exactly what the file holds (24-bit quantised etc.)
    double r0 = 0;
    auto ref = readAll (srcFile, r0);

    auto editFile = dir.getChildFile ("null_" + juce::String ((int) rate) + ".tracktionedit");
    auto edit = te::createEmptyEdit (engine, editFile);
    edit->ensureNumberOfAudioTracks (1);
    tonkori::EngineProfile::applyToEdit (*edit);
    te::EditFileOperations (*edit).save (true, true, false);   // source paths resolve relative to the saved edit
    auto* track = te::getAudioTracks (*edit).getFirst();
    const auto len = te::TimeDuration::fromSeconds ((double) n / rate);
    auto clip = track->insertWaveClip ("src", srcFile, { { 0_tp, te::TimePosition() + len }, {} }, false);

    auto outFile = dir.getChildFile ("null_out_" + juce::String ((int) rate) + "_" + juce::String (sourceBits) + ".wav");
    outFile.deleteFile();

    te::Renderer::Parameters p (*edit);
    p.tracksToDo = te::toBitSet (te::getAllTracks (*edit));
    p.destFile = outFile;
    p.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
    p.bitDepth = 32;
    p.sampleRateForAudio = rate;
    p.blockSizeForAudio = 512;
    p.time = { 0_tp, te::TimePosition() + len };
    p.usePlugins = true;
    p.useMasterPlugins = true;
    p.canRenderInMono = false;
    p.ditheringEnabled = false;

    auto rendered = te::Renderer::renderToFile ("null test", p);
    double r1 = 0;
    auto out = readAll (rendered, r1);

    if (out.getNumSamples() == 0)
    {
        check (false, "Null test " + label + ": render produced no file");
        return;
    }

    int64_t differing = 0;
    float maxDiff = 0;
    const int m = juce::jmin (ref.getNumSamples(), out.getNumSamples());

    for (int c = 0; c < juce::jmin (ref.getNumChannels(), out.getNumChannels()); ++c)
        for (int i = 0; i < m; ++i)
        {
            const float d = std::abs (ref.getSample (c, i) - out.getSample (c, i));
            if (d != 0.0f) ++differing;
            maxDiff = juce::jmax (maxDiff, d);
        }

    std::cout << "        rendered " << out.getNumSamples() << " smp @ " << r1 << " Hz, "
              << out.getNumChannels() << " ch; differing samples: " << differing
              << ", max |diff| = " << (maxDiff > 0 ? juce::String (juce::Decibels::gainToDecibels (maxDiff), 1) + " dBFS" : juce::String ("0")) << std::endl;

    check (r1 == rate && m == n && differing == 0, "Null test " + label + ": bit-identical through engine");
}

//==============================================================================
// DSD synthesis: 2nd-order delta-sigma modulator (CIFB), ±1 output.
static std::vector<std::vector<uint8_t>> synthDsd (double dsdRate, double seconds,
                                                   const std::vector<std::pair<double, double>>& toneFreqAmp)
{
    const auto numBits = (int64_t) (dsdRate * seconds);
    std::vector<std::vector<uint8_t>> out;

    for (auto [freq, amp] : toneFreqAmp)
    {
        std::vector<uint8_t> bytes ((size_t) ((numBits + 7) / 8), 0);
        double i1 = 0, i2 = 0, y = 0;

        for (int64_t k = 0; k < numBits; ++k)
        {
            const double x = amp * std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) k / dsdRate);
            i1 += x - y;
            i2 += i1 - 2.0 * y;
            y = i2 >= 0 ? 1.0 : -1.0;
            if (y > 0)
                bytes[(size_t) (k / 8)] |= (uint8_t) (0x80 >> (k % 8));    // MSB first
        }
        out.push_back (std::move (bytes));
    }
    return out;
}

static uint8_t rev (uint8_t b) { uint8_t r = 0; for (int i = 0; i < 8; ++i) if (b & (1 << i)) r |= (uint8_t) (0x80 >> i); return r; }

static void writeDsf (const juce::File& f, const std::vector<std::vector<uint8_t>>& ch, double rate, uint64_t numBits)
{
    f.deleteFile();
    juce::FileOutputStream o (f);
    const uint32_t block = 4096;
    const int nc = (int) ch.size();
    const uint64_t bytesPerCh = ch[0].size();
    const uint64_t blocks = (bytesPerCh + block - 1) / block;
    const uint64_t dataLen = blocks * block * (uint64_t) nc;

    auto le = [&o] (uint64_t v, int n) { for (int i = 0; i < n; ++i) o.writeByte ((char) ((v >> (8 * i)) & 0xff)); };
    o.write ("DSD ", 4); le (28, 8); le (28 + 52 + 12 + dataLen, 8); le (0, 8);
    o.write ("fmt ", 4); le (52, 8); le (1, 4); le (0, 4); le (2, 4); le ((uint64_t) nc, 4);
    le ((uint64_t) rate, 4); le (1, 4); le (numBits, 8); le (block, 4); le (0, 4);
    o.write ("data", 4); le (12 + dataLen, 8);

    for (uint64_t b = 0; b < blocks; ++b)
        for (int c = 0; c < nc; ++c)
            for (uint32_t i = 0; i < block; ++i)
            {
                const auto idx = b * block + i;
                o.writeByte ((char) (idx < bytesPerCh ? rev (ch[(size_t) c][(size_t) idx]) : 0));   // LSB first
            }
}

static void writeDff (const juce::File& f, const std::vector<std::vector<uint8_t>>& ch, double rate)
{
    f.deleteFile();
    juce::FileOutputStream o (f);
    const int nc = (int) ch.size();
    const uint64_t bytesPerCh = ch[0].size();
    const uint64_t dsdLen = bytesPerCh * (uint64_t) nc;

    auto be = [&o] (uint64_t v, int n) { for (int i = n - 1; i >= 0; --i) o.writeByte ((char) ((v >> (8 * i)) & 0xff)); };
    const uint64_t fver = 12 + 4;
    const uint64_t fs = 12 + 4, chnl = 12 + 2 + 4 * (uint64_t) nc, cmpr = 12 + 4 + 1 + 14 + 1;
    const uint64_t prop = 12 + 4 + fs + chnl + cmpr;
    const uint64_t form = 4 + fver + prop + 12 + dsdLen;

    o.write ("FRM8", 4); be (form, 8); o.write ("DSD ", 4);
    o.write ("FVER", 4); be (4, 8); be (0x01050000, 4);
    o.write ("PROP", 4); be (prop - 12, 8); o.write ("SND ", 4);
    o.write ("FS  ", 4); be (4, 8); be ((uint64_t) rate, 4);
    o.write ("CHNL", 4); be (2 + 4 * (uint64_t) nc, 8); be ((uint64_t) nc, 2); o.write ("SLFTSRGT", 4 * (size_t) nc);
    o.write ("CMPR", 4); be (4 + 1 + 14 + 1, 8); o.write ("DSD ", 4); o.writeByte (14); o.write ("not compressed", 14); o.writeByte (0);
    o.write ("DSD ", 4); be (dsdLen, 8);
    for (uint64_t i = 0; i < bytesPerCh; ++i)
        for (int c = 0; c < nc; ++c)
            o.writeByte ((char) ch[(size_t) c][(size_t) i]);
}

// Least-squares fit of a sine at a known frequency; returns amplitude, phase (rad) and the residual.
static void fitSine (const float* x, int n, double rate, double freq, int skip,
                     double& amp, double& phase, std::vector<double>& residual)
{
    double ss = 0, sc = 0, cc = 0, xs = 0, xc = 0;
    for (int i = skip; i < n - skip; ++i)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * freq * i / rate;
        const double s = std::sin (w), c = std::cos (w);
        ss += s * s; sc += s * c; cc += c * c; xs += x[i] * s; xc += x[i] * c;
    }
    const double det = ss * cc - sc * sc;
    const double a = (xs * cc - xc * sc) / det;   // sin coefficient
    const double b = (xc * ss - xs * sc) / det;   // cos coefficient
    amp = std::sqrt (a * a + b * b);
    phase = std::atan2 (b, a);

    residual.assign ((size_t) n, 0.0);
    for (int i = 0; i < n; ++i)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * freq * i / rate;
        residual[(size_t) i] = x[i] - (a * std::sin (w) + b * std::cos (w));
    }
}

// In-band (≤ 20 kHz) RMS of a signal at 352.8 kHz via a windowed-sinc low-pass.
static double inBandRms (const std::vector<double>& x, double rate, int skip)
{
    const int taps = 1023, half = taps / 2;
    const double fc = 20000.0 / rate;
    std::vector<double> h ((size_t) taps);
    double sum = 0;
    for (int k = 0; k < taps; ++k)
    {
        const double t = k - half;
        const double s = t == 0 ? 2 * fc : std::sin (2 * juce::MathConstants<double>::pi * fc * t) / (juce::MathConstants<double>::pi * t);
        const double w = 0.42 - 0.5 * std::cos (2 * juce::MathConstants<double>::pi * k / (taps - 1)) + 0.08 * std::cos (4 * juce::MathConstants<double>::pi * k / (taps - 1));
        h[(size_t) k] = s * w; sum += h[(size_t) k];
    }
    for (auto& v : h) v /= sum;

    double acc = 0; int count = 0;
    for (int i = skip + half; i < (int) x.size() - skip - half; i += 3)
    {
        double y = 0;
        for (int k = 0; k < taps; ++k) y += h[(size_t) k] * x[(size_t) (i - half + k)];
        acc += y * y; ++count;
    }
    return std::sqrt (acc / juce::jmax (1, count));
}

static std::map<int, std::array<double, 2>> snrByRate;

static void dsdTest (const juce::File& dir, int multiple)
{
    const double dsdRate = 2822400.0 * multiple;
    const double seconds = 1.0;
    const std::vector<std::pair<double, double>> tones { { 1000.0, 0.5 }, { 3000.0, 0.25 } };
    auto bits = synthDsd (dsdRate, seconds, tones);
    const auto numBits = (uint64_t) (dsdRate * seconds);

    auto dsf = dir.getChildFile ("test_dsd" + juce::String (64 * multiple) + ".dsf");
    auto dff = dir.getChildFile ("test_dsd" + juce::String (64 * multiple) + ".dff");
    writeDsf (dsf, bits, dsdRate, numBits);
    writeDff (dff, bits, dsdRate);

    auto outA = dir.getChildFile ("dsf_out.wav"), outB = dir.getChildFile ("dff_out.wav");
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    auto ra = tonkori::dsd::convertToDxd (dsf, outA);
    const auto t1 = juce::Time::getMillisecondCounterHiRes();
    auto rb = tonkori::dsd::convertToDxd (dff, outB);

    const juce::String label = "DSD" + juce::String (64 * multiple);
    check (ra.ok, label + " DSF decode" + (ra.ok ? "" : ": " + ra.error));
    check (rb.ok, label + " DSDIFF decode" + (rb.ok ? "" : ": " + rb.error));
    if (! ra.ok || ! rb.ok) return;

    std::cout << "        " << ra.source.describe() << " -> " << ra.outputSamples << " smp @ 352.8 kHz in "
              << juce::String ((t1 - t0), 0) << " ms (" << juce::String (seconds * 1000.0 / (t1 - t0), 1) << "x realtime)" << std::endl;

    double rA = 0, rB = 0;
    auto a = readAll (outA, rA), b = readAll (outB, rB);
    const int expected = (int) ((numBits + (uint64_t) (8 * multiple) - 1) / (uint64_t) (8 * multiple));
    check (rA == 352800.0 && a.getNumSamples() == expected, label + " length/rate: " + juce::String (a.getNumSamples()) + " smp (expected " + juce::String (expected) + ")");

    bool identical = a.getNumSamples() == b.getNumSamples() && a.getNumChannels() == b.getNumChannels();
    for (int c = 0; identical && c < a.getNumChannels(); ++c)
        identical = std::memcmp (a.getReadPointer (c), b.getReadPointer (c), sizeof (float) * (size_t) a.getNumSamples()) == 0;
    check (identical, label + " DSF and DSDIFF decode bit-identical");

    for (int c = 0; c < 2; ++c)
    {
        double amp = 0, ph = 0;
        std::vector<double> res;
        const auto [freq, targetAmp] = tones[(size_t) c];
        fitSine (a.getReadPointer (c), a.getNumSamples(), 352800.0, freq, 2000, amp, ph, res);

        const double levelErrDb = juce::Decibels::gainToDecibels (amp / targetAmp);
        const double timeErrUs = ph / (2.0 * juce::MathConstants<double>::pi * freq) * 1.0e6;
        const double snr = juce::Decibels::gainToDecibels ((amp / std::sqrt (2.0)) / inBandRms (res, 352800.0, 2000));

        std::cout << "        ch" << c << " " << freq << " Hz: level " << juce::String (levelErrDb, 3) << " dB, timing "
                  << juce::String (timeErrUs, 3) << " us, in-band SNR " << juce::String (snr, 1) << " dB" << std::endl;

        check (std::abs (levelErrDb) < 0.05, label + " ch" + juce::String (c) + " level within 0.05 dB");
        check (std::abs (timeErrUs) < 1.0, label + " ch" + juce::String (c) + " time-aligned within 1 us");
        check (snr > 60.0, label + " ch" + juce::String (c) + " in-band SNR > 60 dB");
        snrByRate[multiple][c] = snr;
    }
}


//==============================================================================
// Fades: every shape rendered through the engine must match its formula sample by sample.
struct DcEdit
{
    std::unique_ptr<te::Edit> edit;
    te::WaveAudioClip* clip = nullptr;
};

static DcEdit makeDcEdit (te::Engine& engine, const juce::File& dir, const juce::String& name, double rate, double seconds, float level)
{
    const int n = (int) (rate * seconds);
    juce::AudioBuffer<float> dc (2, n);
    for (int c = 0; c < 2; ++c) juce::FloatVectorOperations::fill (dc.getWritePointer (c), level, n);

    auto src = dir.getChildFile (name + "_dc.wav");
    writeWav (src, dc, rate, 32);

    DcEdit d;
    d.edit = te::createEmptyEdit (engine, dir.getChildFile (name + ".tracktionedit"));
    d.edit->ensureNumberOfAudioTracks (1);
    tonkori::EngineProfile::applyToEdit (*d.edit);
    te::EditFileOperations (*d.edit).save (true, true, false);
    auto* track = te::getAudioTracks (*d.edit).getFirst();
    auto c = track->insertWaveClip ("dc", src, { { 0_tp, te::TimePosition::fromSeconds (seconds) }, {} }, false);
    d.clip = dynamic_cast<te::WaveAudioClip*> (c.get());
    return d;
}

static juce::AudioBuffer<float> renderEdit (te::Engine& engine, te::Edit& edit, const juce::File& out, double rate, double seconds)
{
    out.deleteFile();
    te::Renderer::Parameters p (edit);
    p.tracksToDo = te::toBitSet (te::getAllTracks (edit));
    p.destFile = out;
    p.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
    p.bitDepth = 32;
    p.sampleRateForAudio = rate;
    p.blockSizeForAudio = 512;
    p.time = { 0_tp, te::TimePosition::fromSeconds (seconds) };
    p.usePlugins = true;
    p.useMasterPlugins = true;
    p.canRenderInMono = false;
    auto f = te::Renderer::renderToFile ("render", p);
    double r = 0;
    return readAll (f, r);
}

static void fadeShapeTests (te::Engine& engine, const juce::File& dir)
{
    const double rate = 48000.0, seconds = 2.0;
    const float level = 0.5f;
    const double inLen = 0.5, outLen = 0.25;
    const int inN = (int) (inLen * rate), outN = (int) (outLen * rate), total = (int) (rate * seconds);

    const char* specs[] = { "linear", "equalPower", "sCurve 0.35", "exponential -0.6", "exponential 0.8",
                            "dBLinear -48", "points damped 25:-12 50:-4 75:-1", "points spline 30:-20 60:-3",
                            "points linear 50:-6" };

    for (auto* specText : specs)
    {
        auto d = makeDcEdit (engine, dir, "fade", rate, seconds, level);
        const auto spec = tonkori::FadeSpec::fromString (specText);
        const auto outSpec = tonkori::FadeSpec::fromString ("equalPower");

        tonkori::fades::setLength (*d.clip, true,  te::TimeDuration::fromSeconds (inLen));
        tonkori::fades::setLength (*d.clip, false, te::TimeDuration::fromSeconds (outLen));
        tonkori::fades::setSpec (*d.clip, true, spec);
        tonkori::fades::setSpec (*d.clip, false, outSpec);

        auto out = renderEdit (engine, *d.edit, dir.getChildFile ("fade_out.wav"), rate, seconds);
        if (out.getNumSamples() < total) { check (false, juce::String ("Fade ") + specText + ": no render"); continue; }

        double maxErr = 0;
        for (int i = 0; i < total; ++i)
        {
            double expected = level;
            if (i < inN)               expected = level * spec.gainAt ((double) i / inN);
            else if (i >= total - outN) expected = level * outSpec.gainAt (1.0 - (double) (i - (total - outN)) / outN);
            maxErr = std::max (maxErr, std::abs ((double) out.getSample (0, i) - expected));
        }

        std::cout << "        " << juce::String (specText).paddedRight (' ', 34) << " max error " << juce::String (maxErr, 8)
                  << "  (" << juce::String (juce::Decibels::gainToDecibels (maxErr / level + 1e-12), 1) << " dB re signal)" << std::endl;
        check (maxErr < 1.0e-4, juce::String ("Fade '") + specText + "' renders its formula (< -74 dB error)");
    }
}

static void fadeToggleTests (te::Engine& engine, const juce::File& dir)
{
    const double rate = 48000.0, seconds = 1.0;
    const float level = 0.5f;
    const int total = (int) (rate * seconds);
    auto d = makeDcEdit (engine, dir, "toggle", rate, seconds, level);
    tonkori::fades::setLength (*d.clip, true, te::TimeDuration::fromSeconds (0.3));
    tonkori::fades::setSpec (*d.clip, true, tonkori::FadeSpec::fromString ("linear"));

    auto on = renderEdit (engine, *d.edit, dir.getChildFile ("t1.wav"), rate, seconds);
    check (std::abs (on.getSample (0, 7200) - level * 0.5f) < 1e-4f, "Fade on: mid-fade sample at half gain");

    tonkori::fades::setFadesOn (*d.clip, false);
    auto off = renderEdit (engine, *d.edit, dir.getChildFile ("t2.wav"), rate, seconds);
    bool flat = true;
    for (int i = 0; i < total; ++i) flat = flat && off.getSample (0, i) == level;
    check (flat, "Fades off: clip plays untouched, bit-exact, fade length kept (" + juce::String (d.clip->getFadeIn().inSeconds(), 2) + " s)");

    const auto lin = tonkori::FadeSpec::fromString ("linear");
    tonkori::fades::setAutoFade (*d.edit, true, 5.0, &lin);
    auto autoOn = renderEdit (engine, *d.edit, dir.getChildFile ("t3.wav"), rate, seconds);
    const int an = (int) (0.005 * rate);
    bool autoOk = std::abs (autoOn.getSample (0, an / 2) - level * 0.5f) < 1e-4f
               && autoOn.getSample (0, an + 10) == level
               && std::abs (autoOn.getSample (0, total - an / 2) - level * 0.5f) < 2e-3f;
    check (autoOk, "Auto-fades on: 5 ms edge fades applied to a clip whose own fades are off");

    tonkori::fades::setAutoFade (*d.edit, false, 5.0);
    tonkori::fades::setFadesOn (*d.clip, true);
    auto back = renderEdit (engine, *d.edit, dir.getChildFile ("t4.wav"), rate, seconds);
    check (std::abs (back.getSample (0, 7200) - level * 0.5f) < 1e-4f, "Fades back on: original fade restored");
}

//==============================================================================
// Live edits: the transport keeps running (hosted device) while clips are changed.
static void liveEditTests (te::Engine& engine, const juce::File& dir)
{
    const double rate = 48000.0;
    const int block = 512;
    const float level = 0.5f;

    auto& hostedIface = engine.getDeviceManager().getHostedAudioDeviceInterface();
    hostedIface.prepareToPlay (rate, block);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    auto d = makeDcEdit (engine, dir, "live", rate, 20.0, level);
    auto& transport = d.edit->getTransport();
    transport.ensureContextAllocated();
    transport.setPosition (0_tp);
    transport.play (false);

    auto& hosted = engine.getDeviceManager().getHostedAudioDeviceInterface();
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    double playedSeconds = 0;
    float lastSample = 0;

    auto runFor = [&] (double seconds)
    {
        const int blocks = (int) std::ceil (seconds * rate / block);
        for (int b = 0; b < blocks; ++b)
        {
            buf.clear();
            hosted.processBlock (buf, midi);
            lastSample = buf.getSample (0, block - 1);
            playedSeconds += block / rate;
            juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
        }
    };

    // Wait until audio flows (graph built), then measure.
    for (int i = 0; i < 400 && std::abs (lastSample - level) > 1e-6f; ++i) runFor (block / rate);
    check (std::abs (lastSample - level) < 1e-6f, "Live: playback running through the hosted device");

    auto waitFor = [&] (std::function<bool (float)> condition, const juce::String& what)
    {
        const double t0 = playedSeconds;
        for (int i = 0; i < 2000; ++i)
        {
            runFor (block / rate);
            if (condition (lastSample))
            {
                std::cout << "        " << what << " heard after " << juce::String ((playedSeconds - t0) * 1000.0, 1) << " ms of playback" << std::endl;
                return true;
            }
        }
        return false;
    };

    // 1. Mute the clip while playing.
    d.clip->setMuted (true);
    check (waitFor ([] (float s) { return s == 0.0f; }, "Clip mute"), "Live: muting a clip while playing takes effect");
    d.clip->setMuted (false);
    check (waitFor ([&] (float s) { return s == level; }, "Clip unmute"), "Live: unmuting while playing takes effect");

    // 2. Change clip gain while playing.
    d.clip->setGainDB (-6.0f);
    const float g6 = level * juce::Decibels::decibelsToGain (-6.0f);
    check (waitFor ([&] (float s) { return std::abs (s - g6) < 1e-5f; }, "Clip gain -6 dB"), "Live: clip gain change while playing takes effect");
    d.clip->setGainDB (0.0f);
    waitFor ([&] (float s) { return s == level; }, "Clip gain 0 dB");

    // 3. Program a long fade-out ahead of the playhead while playing; hear it when we reach it.
    const auto now = transport.getPosition().inSeconds();
    const double fadeStart = now + 1.0, clipEnd = 20.0;
    tonkori::fades::setLength (*d.clip, false, te::TimeDuration::fromSeconds (clipEnd - fadeStart));
    tonkori::fades::setSpec (*d.clip, false, tonkori::FadeSpec::fromString ("linear"));
    runFor (2.0);                                       // now inside the fade
    const float s1 = lastSample;
    const int blocks = 94;                              // exactly 94 × 512 samples later
    for (int b = 0; b < blocks; ++b) runFor (block / rate);
    const float s2 = lastSample;
    const double dt = blocks * block / rate;
    const double measuredSlope = (s2 - s1) / dt;
    const double formulaSlope  = -level / (clipEnd - fadeStart);
    std::cout << "        fade-out programmed live: slope " << juce::String (measuredSlope, 6) << " /s, formula "
              << juce::String (formulaSlope, 6) << " /s (level " << juce::String (s1, 4) << " -> " << juce::String (s2, 4) << ")" << std::endl;
    check (s1 < level && std::abs (measuredSlope - formulaSlope) < 1e-4, "Live: fade programmed during playback plays its curve");

    // 4. Toggle that fade off and on while inside it.
    tonkori::fades::setFadesOn (*d.clip, false);
    check (waitFor ([&] (float s) { return s == level; }, "Fade toggled off"), "Live: toggling fades off mid-fade");
    tonkori::fades::setFadesOn (*d.clip, true);
    check (waitFor ([&] (float s) { return s < level * 0.999f; }, "Fade toggled on"), "Live: toggling fades back on mid-fade");

    // 5. Move the clip away from the playhead while playing.
    d.clip->setStart (transport.getPosition() + te::TimeDuration::fromSeconds (30.0), false, true);
    check (waitFor ([] (float s) { return s == 0.0f; }, "Clip moved away"), "Live: moving a clip while playing takes effect");

    check (transport.isPlaying(), "Live: transport never stopped during the edits");
    transport.stop (false, false);
}

#include "M2Tests.inc"

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    tonkori::EngineProfile::applyGlobal();

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("tonkori-selftest");
    dir.deleteRecursively();
    dir.createDirectory();

    std::cout << "Tonkori DAW engine self-test" << std::endl;

    {
        te::Engine engine { "TonkoriSelfTest", std::make_unique<HeadlessUI>(), std::make_unique<tonkori::EngineBehaviour>() };
        engine.getPluginManager().createBuiltInType<LatencyTestPlugin>();

        // CI machines have no sound card: give the engine a virtual stereo output so
        // tracks have a destination (as they do on a real rig).
        te::HostedAudioDeviceInterface::Parameters hp;
        hp.sampleRate = 96000.0;
        hp.blockSize = 512;
        hp.inputChannels = 0;
        hp.outputChannels = 2;
        engine.getDeviceManager().getHostedAudioDeviceInterface().initialise (hp);

        std::cout << "[1] Null tests (render through track + master at unity)" << std::endl;
        nullTest (engine, dir, 96000.0, 32);
        nullTest (engine, dir, 96000.0, 24);
        nullTest (engine, dir, 352800.0, 32);

        std::cout << "[3] Fade curves: engine output vs formula, sample by sample" << std::endl;
        fadeShapeTests (engine, dir);

        std::cout << "[4] Fade toggles (per clip on/off, session auto-fades)" << std::endl;
        fadeToggleTests (engine, dir);

        std::cout << "[5] Edits while the transport runs" << std::endl;
        hp.sampleRate = 48000.0;
        engine.getDeviceManager().getHostedAudioDeviceInterface().initialise (hp);
        liveEditTests (engine, dir);

        std::cout << "[7] Clip gain line" << std::endl;
        gainLineTests (engine, dir);
        std::cout << "[8] Crossfades: splice point, length, alignment (null tests)" << std::endl;
        crossfadeTests (engine, dir);
        std::cout << "[9] Take lanes and comping" << std::endl;
        takeLaneTests (engine, dir);
        std::cout << "[10] Plug-in delay compensation" << std::endl;
        pdcTests (engine, dir);
        std::cout << "[11] Stems" << std::endl;
        stemTests (engine, dir);
        std::cout << "[12] Time-stretch and pitch" << std::endl;
        stretchTests (engine, dir);
        std::cout << "[15] Video import / export" << std::endl;
        videoTests (engine, dir);
    }

    std::cout << "[13] Loudness (ITU-R BS.1770-4, EBU 3341/3342)" << std::endl;
    loudnessTests();
    std::cout << "[14] Round-trip latency measurement" << std::endl;
    latencyProbeTests();

    std::cout << "[6] DSD -> DXD decoding" << std::endl;
    dsdTest (dir, 1);
    dsdTest (dir, 2);
    dsdTest (dir, 4);

    // The test modulator is 2nd-order, so its in-band noise falls 15 dB per doubling of the
    // DSD rate. If the decoder had its own noise floor, the gain would flatten out.
    for (int c = 0; c < 2; ++c)
    {
        const double gain = snrByRate[4][c] - snrByRate[1][c];
        std::cout << "        ch" << c << " SNR gain DSD64 -> DSD256: " << juce::String (gain, 1) << " dB (2nd-order theory: 30.1 dB)" << std::endl;
        check (gain > 27.0, "Decoder is transparent: SNR tracks the modulator (ch" + juce::String (c) + ")");
    }

    std::cout << (failures == 0 ? "ALL PASS" : juce::String (failures) + " FAILURE(S)") << std::endl;
    if (juce::SystemStats::getEnvironmentVariable ("TONKORI_KEEP_TEST_FILES", {}).isEmpty())
        dir.deleteRecursively();
    return failures == 0 ? 0 : 1;
}
