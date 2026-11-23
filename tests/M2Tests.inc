// Included by EngineSelfTest.cpp. Milestone 2 engine checks: gain line, crossfades,
// take lanes, loudness (EBU test vectors), latency probe, plug-in delay compensation
// (inserts, bypass, sends into buses), stems, time-stretching, video round trips.

#include "model/GainLine.h"
#include "model/Crossfade.h"
#include "model/Takes.h"
#include "model/Stretch.h"
#include "model/Routing.h"
#include "engine/Loudness.h"
#include "engine/LatencyProbe.h"
#include "video/Video.h"

//==============================================================================
static juce::File writeNoise (const juce::File& dir, const juce::String& name, double rate, double seconds, int seed, float level = 0.5f, int channels = 2)
{
    const int n = (int) (rate * seconds);
    juce::AudioBuffer<float> b (channels, n);
    juce::Random rng (seed);
    for (int c = 0; c < channels; ++c)
        for (int i = 0; i < n; ++i) b.setSample (c, i, (rng.nextFloat() * 2.0f - 1.0f) * level);
    auto f = dir.getChildFile (name + ".wav");
    writeWav (f, b, rate, 32);
    return f;
}

static std::unique_ptr<te::Edit> newEdit (te::Engine& engine, const juce::File& dir, const juce::String& name, int tracks)
{
    auto e = te::createEmptyEdit (engine, dir.getChildFile (name + ".tracktionedit"));
    e->ensureNumberOfAudioTracks (tracks);
    tonkori::EngineProfile::applyToEdit (*e);
    te::EditFileOperations (*e).save (true, true, false);
    return e;
}

static juce::AudioBuffer<float> renderTracks (te::Engine& engine, te::Edit& edit, juce::Array<te::Track*> tracks,
                                              const juce::File& out, double rate, double seconds, bool master = true)
{
    out.deleteFile();
    te::Renderer::Parameters p (edit);
    p.tracksToDo = te::toBitSet (tracks);
    p.destFile = out;
    p.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
    p.bitDepth = 32;
    p.sampleRateForAudio = rate;
    p.blockSizeForAudio = 512;
    p.time = { 0_tp, te::TimePosition::fromSeconds (seconds) };
    p.usePlugins = true;
    p.useMasterPlugins = master;
    p.canRenderInMono = false;
    auto f = te::Renderer::renderToFile ("render", p);
    double r = 0;
    return readAll (f, r);
}

static double maxAbsDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int from, int to, int channels = 2)
{
    double m = 0;
    for (int c = 0; c < channels; ++c)
        for (int i = from; i < to && i < a.getNumSamples() && i < b.getNumSamples(); ++i)
            m = std::max (m, (double) std::abs (a.getSample (c, i) - b.getSample (c, i)));
    return m;
}

static juce::String dbStr (double v) { return v > 0 ? juce::String (juce::Decibels::gainToDecibels (v), 1) + " dBFS" : juce::String ("0 (exact)"); }

//==============================================================================
static void gainLineTests (te::Engine& engine, const juce::File& dir)
{
    const double rate = 48000.0, seconds = 3.0;
    const float level = 0.5f;
    auto d = makeDcEdit (engine, dir, "gain", rate, seconds, level);
    using P = tonkori::gainline::Point;
    tonkori::gainline::setPoints (*d.clip, { P { 0.2, 0.0 }, P { 0.6, -12.0 }, P { 1.0, -12.0 }, P { 1.2, 6.0 } });
    const auto pts = tonkori::gainline::getPoints (*d.clip);

    auto out = renderEdit (engine, *d.edit, dir.getChildFile ("gain_out.wav"), rate, seconds);
    double maxErr = 0;
    for (int i = 0; i < (int) (rate * seconds); i += 7)
    {
        const double expected = level * juce::Decibels::decibelsToGain (tonkori::gainline::dbAt (pts, i / rate));
        maxErr = std::max (maxErr, std::abs ((double) out.getSample (0, i) - expected));
    }
    std::cout << "        gain line (0 dB -> -12 dB -> +6 dB) max error " << juce::String (maxErr, 8) << std::endl;
    check (maxErr < 1.0e-5, "Clip gain line renders its breakpoints (linear in dB)");

    // Trim the clip start by 0.5 s (sync kept): the line must stay on the same audio.
    d.clip->setStart (0.5_tp, true, false);
    auto out2 = renderEdit (engine, *d.edit, dir.getChildFile ("gain_out2.wav"), rate, seconds);
    double maxErr2 = 0;
    for (int i = (int) (0.5 * rate); i < (int) (rate * seconds); i += 7)
    {
        const double expected = level * juce::Decibels::decibelsToGain (tonkori::gainline::dbAt (pts, i / rate));
        maxErr2 = std::max (maxErr2, std::abs ((double) out2.getSample (0, i) - expected));
    }
    check (maxErr2 < 1.0e-5, "Gain line stays locked to the audio after a trim");

    tonkori::gainline::clear (*d.clip);
    auto out3 = renderEdit (engine, *d.edit, dir.getChildFile ("gain_out3.wav"), rate, seconds);
    bool flat = true;
    for (int i = (int) (0.5 * rate) + 10; i < (int) (rate * seconds) - 10; ++i) flat = flat && out3.getSample (0, i) == level;
    check (flat, "Clip without a gain line renders bit-exact");
}

//==============================================================================
static void crossfadeTests (te::Engine& engine, const juce::File& dir)
{
    const double rate = 48000.0, seconds = 4.0;
    auto src = writeNoise (dir, "xf_src", rate, seconds, 77);
    double r0 = 0;
    auto ref = readAll (src, r0);

    struct Case { double len, align, spliceOffset; const char* name; };
    const Case cases[] = { { 0.010, 0.5, 0.0, "10 ms centred" },
                           { 0.200, 0.0, 0.3, "200 ms pre, splice +300 ms" },
                           { 0.050, 1.0, -0.12, "50 ms post, splice -120 ms" },
                           { 1.000, 0.25, 0.05, "1 s, 25 % after splice" } };

    for (auto& cs : cases)
    {
        auto e = newEdit (engine, dir, "xf", 1);
        auto* track = te::getAudioTracks (*e).getFirst();
        auto clip = track->insertWaveClip ("src", src, { { 0_tp, te::TimePosition::fromSeconds (seconds) }, {} }, false);
        te::SelectableList l; l.add (clip.get());
        te::splitClips (l, te::TimePosition::fromSeconds (2.0));

        auto clips = track->getClips();
        tonkori::xfade::Pair pr { dynamic_cast<te::AudioClipBase*> (clips[0]), dynamic_cast<te::AudioClipBase*> (clips[1]) };
        if (pr.out->getPosition().getStart() > pr.in->getPosition().getStart()) std::swap (pr.out, pr.in);

        tonkori::xfade::Params prm;
        prm.length = cs.len; prm.align = cs.align; prm.splice = 2.0 + cs.spliceOffset;
        prm.outSpec = prm.inSpec = tonkori::FadeSpec::fromString ("linear");
        const double used = tonkori::xfade::apply (pr, prm);
        const auto back = tonkori::xfade::read (pr);

        auto out = renderTracks (engine, *e, te::getAllTracks (*e), dir.getChildFile ("xf_out.wav"), rate, seconds);
        const double diff = maxAbsDiff (ref, out, 0, (int) (rate * seconds) - 1);
        std::cout << "        " << juce::String (cs.name).paddedRight (' ', 30) << " length " << juce::String (used * 1000.0, 1)
                  << " ms, splice " << juce::String (back.splice, 3) << " s, null residual " << dbStr (diff) << std::endl;
        check (diff < 2.0e-6 && std::abs (used - cs.len) < 1e-9 && std::abs (back.splice - prm.splice) < 1e-9,
               juce::String ("Crossfade ") + cs.name + ": equal-gain splice nulls against the original");
    }
}

//==============================================================================
static void takeLaneTests (te::Engine& engine, const juce::File& dir)
{
    const double rate = 48000.0, seconds = 3.0;
    auto e = newEdit (engine, dir, "takes", 1);
    auto* track = te::getAudioTracks (*e).getFirst();

    auto mk = [&] (const juce::String& n, float v)
    {
        juce::AudioBuffer<float> b (2, (int) (rate * seconds));
        for (int c = 0; c < 2; ++c) juce::FloatVectorOperations::fill (b.getWritePointer (c), v, b.getNumSamples());
        auto f = dir.getChildFile (n + ".wav");
        writeWav (f, b, rate, 32);
        return f;
    };
    const auto fa = mk ("takeA", 0.5f), fb = mk ("takeB", -0.25f);
    const te::ClipPosition pos { { 0_tp, te::TimePosition::fromSeconds (seconds) }, {} };

    // Two recording passes.
    auto a = track->insertWaveClip ("A", fa, pos, false);
    tonkori::takes::absorbRecording (*a);
    auto b = track->insertWaveClip ("B", fb, pos, false);
    tonkori::takes::absorbRecording (*b);

    const int lanes = tonkori::takes::numLanes (*track);
    check (lanes == 2 && tonkori::takes::clipsOnLane (*track, 0).size() == 1, "Takes: two passes give two take lanes and one playing clip (" + juce::String (lanes) + " lanes)");

    auto out = renderTracks (engine, *e, te::getAllTracks (*e), dir.getChildFile ("takes1.wav"), rate, seconds);
    check (std::abs (out.getSample (0, 24000) + 0.25f) < 1e-6f, "Takes: the latest pass plays; take lanes stay silent");

    // Comp: 1.0–2.0 s from take A.
    te::Clip* laneA = nullptr;
    for (auto* c : tonkori::takes::clipsOnLane (*track, 1)) laneA = c;
    auto* seg = laneA ? tonkori::takes::promote (*laneA, { 1.0_tp, 2.0_tp }) : nullptr;
    auto comp = renderTracks (engine, *e, te::getAllTracks (*e), dir.getChildFile ("takes2.wav"), rate, seconds);

    const int xf = (int) (tonkori::takes::compXfadeMs (*e) / 1000.0 * rate);
    auto at = [&] (double s) { return comp.getSample (0, (int) (s * rate)); };
    const bool regions = std::abs (at (0.5) + 0.25f) < 1e-6f && std::abs (at (1.5) - 0.5f) < 1e-6f && std::abs (at (2.5) + 0.25f) < 1e-6f;
    float lo = 1, hi = -1;
    for (int i = (int) (1.0 * rate) - xf; i < (int) (1.0 * rate) + xf; ++i) { lo = juce::jmin (lo, comp.getSample (0, i)); hi = juce::jmax (hi, comp.getSample (0, i)); }
    std::cout << "        comp edge at 1.000 s: " << xf << "-sample equal-power crossfade, range " << juce::String (lo, 3) << " .. " << juce::String (hi, 3) << std::endl;
    check (seg != nullptr && regions && lo > -0.26f && hi < 0.56f, "Comping: swiped range plays from the chosen take with crossfaded edges");
    check (tonkori::takes::clipsOnLane (*track, 1).size() == 1 && tonkori::takes::clipsOnLane (*track, 2).size() == 1, "Comping: the takes themselves stay intact");
}

//==============================================================================
static juce::AudioBuffer<float> sine (double rate, double seconds, double freq, double dbfs, int channels = 2, double phase = 0)
{
    juce::AudioBuffer<float> b (channels, (int) (rate * seconds));
    const double a = juce::Decibels::decibelsToGain (dbfs);
    for (int c = 0; c < channels; ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (c, i, (float) (a * std::sin (2 * juce::MathConstants<double>::pi * freq * i / rate + phase)));
    return b;
}

static juce::AudioBuffer<float> concat (std::initializer_list<juce::AudioBuffer<float>> parts)
{
    int n = 0; for (auto& p : parts) n += p.getNumSamples();
    juce::AudioBuffer<float> r (parts.begin()->getNumChannels(), n);
    int at = 0;
    for (auto& p : parts) { for (int c = 0; c < r.getNumChannels(); ++c) r.copyFrom (c, at, p, c, 0, p.getNumSamples()); at += p.getNumSamples(); }
    return r;
}

static void loudnessTests()
{
    namespace L = tonkori::loudness;
    for (double rate : { 48000.0, 44100.0, 96000.0 })
    {
        const auto label = juce::String (rate / 1000.0, 1) + " kHz";
        auto r1 = L::analyse (sine (rate, 20, 1000, -23), rate);
        auto r2 = L::analyse (sine (rate, 20, 1000, -33), rate);
        auto r3 = L::analyse (concat ({ sine (rate, 10, 1000, -36), sine (rate, 60, 1000, -23), sine (rate, 10, 1000, -36) }), rate);
        auto r5 = L::analyse (concat ({ sine (rate, 20, 1000, -26), sine (rate, 20.1, 1000, -20), sine (rate, 20, 1000, -26) }), rate);
        std::cout << "        " << label << ": EBU 3341 #1 " << juce::String (r1.integrated, 2) << ", #2 " << juce::String (r2.integrated, 2)
                  << ", #3 " << juce::String (r3.integrated, 2) << ", #5 " << juce::String (r5.integrated, 2) << " LUFS" << std::endl;
        check (std::abs (r1.integrated + 23) < 0.1 && std::abs (r1.momentaryMax + 23) < 0.1 && std::abs (r1.shortTermMax + 23) < 0.1,
               "Loudness " + label + ": EBU 3341 case 1 (-23 dBFS sine) = -23.0 LUFS (I, M, S)");
        check (std::abs (r2.integrated + 33) < 0.1, "Loudness " + label + ": EBU 3341 case 2 = -33.0 LUFS");
        check (std::abs (r3.integrated + 23) < 0.1, "Loudness " + label + ": EBU 3341 case 3 (absolute/relative gating) = -23.0 LUFS");
        check (std::abs (r5.integrated + 23) < 0.1, "Loudness " + label + ": EBU 3341 case 5 (relative gate) = -23.0 LUFS");

        auto lra = L::analyse (concat ({ sine (rate, 20, 1000, -20), sine (rate, 20, 1000, -30) }), rate);
        std::cout << "        " << label << ": EBU 3342 case 1 LRA " << juce::String (lra.lra, 2) << " LU" << std::endl;
        check (std::abs (lra.lra - 10.0) < 1.0, "Loudness range " + label + ": EBU 3342 case 1 = 10 +/- 1 LU");
    }

    // True peak: fs/4 sine with 45° phase — samples peak at -3.01 dBFS, the waveform at 0 dBTP.
    const double rate = 48000.0;
    auto tp = sine (rate, 2, rate / 4.0, 0.0, 2, juce::MathConstants<double>::pi / 4);
    float samplePeak = tp.getMagnitude (0, tp.getNumSamples());
    auto r = L::analyse (tp, rate);
    std::cout << "        fs/4 sine, 45 deg: sample peak " << juce::String (juce::Decibels::gainToDecibels (samplePeak), 2)
              << " dBFS, true peak " << juce::String (r.truePeakDb, 2) << " dBTP" << std::endl;
    check (std::abs (r.truePeakDb) < 0.3, "True peak: inter-sample peak found within 0.3 dB (4x oversampling)");
    auto tp2 = L::analyse (sine (rate, 2, 997, -6.0), rate);
    check (std::abs (tp2.truePeakDb + 6.0) < 0.1, "True peak: -6 dBFS 997 Hz sine reads -6.0 dBTP");
}

//==============================================================================
static void latencyProbeTests()
{
    namespace LP = tonkori::latency;
    const double rate = 48000.0;
    auto sweep = LP::Probe::makeSweep (rate);
    juce::Random rng (5);

    for (int delay : { 0, 64, 1234, 7777 })
    {
        std::vector<float> rec (sweep.size() + 48000, 0.0f);
        for (size_t i = 0; i < sweep.size(); ++i) rec[i + (size_t) delay] = sweep[i] * 0.4f;
        for (auto& v : rec) v += (rng.nextFloat() - 0.5f) * 0.002f;             // −60 dB noise floor
        auto est = LP::Probe::estimate (sweep, rec, 43000);
        check (std::abs (est.samples - delay) < 0.5, "Latency probe finds a " + juce::String (delay) + "-sample loop: "
                                                     + juce::String (est.samples, 2) + " smp (confidence " + juce::String (est.confidence, 0) + ")");
    }

    // Through the device callback, with a simulated converter loop of 3 x 256 + 41 samples.
    LP::Measurement m (0, 0);
    struct FakeDevice : juce::AudioIODevice
    {
        FakeDevice() : juce::AudioIODevice ("fake", "fake") {}
        juce::StringArray getOutputChannelNames() override { return { "1" }; }
        juce::StringArray getInputChannelNames() override  { return { "1" }; }
        juce::Array<double> getAvailableSampleRates() override { return { 48000.0 }; }
        juce::Array<int> getAvailableBufferSizes() override { return { 256 }; }
        int getDefaultBufferSize() override { return 256; }
        juce::String open (const juce::BigInteger&, const juce::BigInteger&, double, int) override { return {}; }
        void close() override {}
        bool isOpen() override { return true; }
        void start (juce::AudioIODeviceCallback*) override {}
        void stop() override {}
        bool isPlaying() override { return true; }
        juce::String getLastError() override { return {}; }
        int getCurrentBufferSizeSamples() override { return 256; }
        double getCurrentSampleRate() override { return 48000.0; }
        int getCurrentBitDepth() override { return 32; }
        juce::BigInteger getActiveOutputChannels() const override { return 1; }
        juce::BigInteger getActiveInputChannels() const override { return 1; }
        int getOutputLatencyInSamples() override { return 256 + 22; }
        int getInputLatencyInSamples() override { return 256 + 31; }
    } dev;

    m.audioDeviceAboutToStart (&dev);
    const int loop = 3 * 256 + 41;
    std::vector<float> line ((size_t) loop + 256, 0.0f);
    std::vector<float> in (256), out (256);
    std::deque<float> pipe (loop, 0.0f);
    for (int b = 0; b < 1000 && ! m.isDone(); ++b)
    {
        for (int i = 0; i < 256; ++i) { in[(size_t) i] = pipe.front(); pipe.pop_front(); }
        const float* ins[] = { in.data() };
        float* outs[] = { out.data() };
        m.audioDeviceIOCallbackWithContext (ins, 1, outs, 1, 256, {});
        for (int i = 0; i < 256; ++i) pipe.push_back (out[(size_t) i]);
    }
    auto est = m.result();
    check (m.isDone() && std::abs (est.samples - loop) < 0.5, "Latency measurement through a device callback: " + juce::String (est.samples, 2)
                                                               + " smp (loop " + juce::String (loop) + ")");
}

//==============================================================================
// A processor that delays by N samples and reports it, like a look-ahead limiter.
class LatencyTestPlugin : public te::Plugin
{
public:
    LatencyTestPlugin (te::PluginCreationInfo info) : te::Plugin (info) {}
    ~LatencyTestPlugin() override { notifyListenersOfDeletion(); }

    static const char* getPluginName() { return "Latency Test"; }
    static inline const char* xmlTypeName = "tkLatencyTest";
    static inline int latencySamples = 1000;

    juce::String getName() const override                     { return getPluginName(); }
    juce::String getPluginType() override                     { return xmlTypeName; }
    juce::String getSelectableDescription() override          { return getName(); }
    BusLayout getBusses() const override                      { return BusLayout::singlePassThrough(); }
    int getNumOutputChannelsGivenInputs (int n) override      { return n; }
    double getLatencySeconds() override                       { return latencySamples / rate; }
    void initialise (const te::PluginInitialisationInfo& i) override
    {
        rate = i.sampleRate;
        line.setSize (2, latencySamples + 8192);
        line.clear();
        pos = 0;
    }
    void deinitialise() override {}
    void applyToBuffer (const te::PluginRenderContext& rc) override
    {
        if (rc.destBuffer == nullptr) return;
        const int len = line.getNumSamples();
        for (int i = 0; i < rc.bufferNumSamples; ++i)
        {
            for (int c = 0; c < juce::jmin (2, rc.destBuffer->getNumChannels()); ++c)
            {
                auto* d = rc.destBuffer->getWritePointer (c, rc.bufferStartSample);
                line.setSample (c, pos, d[i]);
                d[i] = line.getSample (c, (pos - latencySamples + len) % len);
            }
            pos = (pos + 1) % len;
        }
    }

private:
    double rate = 48000;
    juce::AudioBuffer<float> line;
    int pos = 0;
};

static void pdcTests (te::Engine& engine, const juce::File& dir)
{
    const double rate = 48000.0, seconds = 3.0;
    auto src = writeNoise (dir, "pdc_src", rate, seconds, 9);
    auto inv = writeNoise (dir, "pdc_inv", rate, seconds, 9, -0.5f);      // exact negative of src

    auto build = [&] (std::function<void (te::Edit&, te::AudioTrack&, te::AudioTrack&)> setup)
    {
        auto e = newEdit (engine, dir, "pdc", 2);
        auto tracks = te::getAudioTracks (*e);
        // Track 2 plays the polarity-inverted copy: if both paths line up, the mix is silent.
        tracks[0]->insertWaveClip ("n", src, { { 0_tp, te::TimePosition::fromSeconds (seconds) }, {} }, false);
        tracks[1]->insertWaveClip ("i", inv, { { 0_tp, te::TimePosition::fromSeconds (seconds) }, {} }, false);
        setup (*e, *tracks[0], *tracks[1]);
        auto out = renderTracks (engine, *e, te::getAllTracks (*e), dir.getChildFile ("pdc_out.wav"), rate, seconds);
        return out.getNumSamples() > 0 ? (double) out.getMagnitude ((int) (0.2 * rate), (int) (2.0 * rate)) : 1.0;
    };

    auto addLatent = [] (te::Edit& e, te::Track& t)
    {
        auto p = e.getPluginCache().createNewPlugin (LatencyTestPlugin::xmlTypeName, {});
        t.pluginList.insertPlugin (p, 0, nullptr);
        return p;
    };

    const double base = build ([] (te::Edit&, te::AudioTrack&, te::AudioTrack&) {});
    check (base == 0.0, "PDC reference: two polarity-opposed copies cancel exactly");

    const double ins = build ([&] (te::Edit& e, te::AudioTrack& a, te::AudioTrack&) { addLatent (e, a); });
    std::cout << "        1000-sample insert on one track: residual " << dbStr (ins) << std::endl;
    check (ins == 0.0, "PDC: a 1000-sample latent insert is compensated (sample-exact)");

    const double byp = build ([&] (te::Edit& e, te::AudioTrack& a, te::AudioTrack&) { addLatent (e, a)->setEnabled (false); });
    check (byp == 0.0, "PDC: bypassing the plug-in keeps the compensation (no jump)");

    const double send = build ([&] (te::Edit& e, te::AudioTrack& a, te::AudioTrack&)
    {
        auto* bus = tonkori::routing::createBus (e, "FX");
        addLatent (e, *bus);
        tonkori::routing::addSend (a, tonkori::routing::busNumberOf (*bus), true, 0.0f);
        if (auto* vp = a.getVolumePlugin()) vp->volParam->setParameter (0.0f, juce::sendNotification);   // direct path -inf
    });
    std::cout << "        pre-fader send into a bus with a 1000-sample insert: residual " << dbStr (send) << std::endl;
    check (send < 1.0e-6, "PDC: send -> bus with latent insert lines up with a dry track");
}

//==============================================================================
static void stemTests (te::Engine& engine, const juce::File& dir)
{
    const double rate = 48000.0, seconds = 2.0;
    auto e = newEdit (engine, dir, "stems", 3);
    auto tracks = te::getAudioTracks (*e);
    const float vols[] = { 0.0f, -6.0f, 3.0f }, pans[] = { 0.0f, -0.6f, 0.4f };
    for (int i = 0; i < 3; ++i)
    {
        tracks[i]->insertWaveClip ("n", writeNoise (dir, "stem" + juce::String (i), rate, seconds, 100 + i, 0.2f),
                                   { { 0_tp, te::TimePosition::fromSeconds (seconds) }, {} }, false);
        tracks[i]->getVolumePlugin()->setVolumeDb (vols[i]);
        tracks[i]->getVolumePlugin()->setPan (pans[i]);
    }
    auto mix = renderTracks (engine, *e, te::getAllTracks (*e), dir.getChildFile ("mix.wav"), rate, seconds);
    juce::AudioBuffer<float> sum (2, mix.getNumSamples());
    sum.clear();
    for (int i = 0; i < 3; ++i)
    {
        auto s = renderTracks (engine, *e, { tracks[i] }, dir.getChildFile ("stem_out" + juce::String (i) + ".wav"), rate, seconds);
        for (int c = 0; c < 2; ++c) sum.addFrom (c, 0, s, c, 0, juce::jmin (s.getNumSamples(), sum.getNumSamples()));
    }
    const double d = maxAbsDiff (mix, sum, 0, mix.getNumSamples());
    std::cout << "        3 stems (faders 0/-6/+3 dB, panned) summed vs mix: residual " << dbStr (d) << std::endl;
    check (mix.getNumSamples() > 0 && d < 1.0e-6, "Stems: the stems sum back to the mix (< -120 dBFS)");
}

//==============================================================================
static double dominantFrequency (const juce::AudioBuffer<float>& b, int from, int n, double rate)
{
    // zero crossings with linear interpolation
    const float* x = b.getReadPointer (0, from);
    double first = -1, last = -1; int count = 0;
    for (int i = 1; i < n; ++i)
        if (x[i - 1] < 0 && x[i] >= 0)
        {
            const double t = (i - 1) + (-x[i - 1]) / (x[i] - x[i - 1]);
            if (first < 0) first = t; else { last = t; ++count; }
        }
    return count > 0 ? count / ((last - first) / rate) : 0.0;
}

static void stretchTests (te::Engine& engine, const juce::File& dir)
{
    const double rate = 48000.0, seconds = 4.0;
    auto f = dir.getChildFile ("stretch_src.wav");
    writeWav (f, sine (rate, seconds, 440.0, -12.0), rate, 32);

    for (int a = 0; a < (int) tonkori::stretch::algorithms().size(); ++a)
    {
        auto& alg = tonkori::stretch::algorithms()[(size_t) a];
        auto e = newEdit (engine, dir, "stretch", 1);
        auto* track = te::getAudioTracks (*e).getFirst();
        auto c = track->insertWaveClip ("s", f, { { 0_tp, te::TimePosition::fromSeconds (seconds) }, {} }, false);
        auto* ac = dynamic_cast<te::AudioClipBase*> (c.get());
        tonkori::stretch::setAlgorithm (*ac, a);
        tonkori::stretch::setPercent (*ac, 150.0);

        const double expectLen = seconds * 1.5;
        auto out = renderTracks (engine, *e, te::getAllTracks (*e), dir.getChildFile ("stretch_out.wav"), rate, expectLen + 0.5);
        const double clipLen = ac->getPosition().getLength().inSeconds();
        const double freq = out.getNumSamples() > 0 ? dominantFrequency (out, (int) (1.0 * rate), (int) (3.0 * rate), rate) : 0;
        const bool varispeed = alg.mode == te::TimeStretcher::disabled;
        const double expectFreq = varispeed ? 440.0 / 1.5 : 440.0;
        float tailLevel = out.getNumSamples() > 0 ? out.getMagnitude (0, (int) ((expectLen - 0.6) * rate), (int) (0.3 * rate)) : 0;

        std::cout << "        " << juce::String (alg.name).paddedRight (' ', 32) << " 150 %: length " << juce::String (clipLen, 3)
                  << " s, pitch " << juce::String (freq, 2) << " Hz (expected " << juce::String (expectFreq, 2) << ")" << std::endl;
        check (std::abs (clipLen - expectLen) < 1e-6 && std::abs (freq - expectFreq) < 1.0 && tailLevel > 0.1f,
               juce::String ("Stretch: ") + alg.name + " to 150 % keeps " + (varispeed ? "tape behaviour" : "pitch"));

        if (! varispeed)
        {
            tonkori::stretch::setPercent (*ac, 100.0);
            tonkori::stretch::setPitch (*ac, 7.0);
            auto p = renderTracks (engine, *e, te::getAllTracks (*e), dir.getChildFile ("pitch_out.wav"), rate, seconds);
            const double pf = p.getNumSamples() > 0 ? dominantFrequency (p, (int) (1.0 * rate), (int) (2.0 * rate), rate) : 0;
            const double target = 440.0 * std::pow (2.0, 7.0 / 12.0);
            check (std::abs (pf - target) < 2.0, juce::String ("Pitch: ") + alg.name + " +7 semitones = " + juce::String (pf, 1) + " Hz (" + juce::String (target, 1) + ")");
        }
    }
}

//==============================================================================
static int readCode (const juce::Image& img)
{
    // 8 blocks across the top: white = 1
    int code = 0;
    for (int bit = 0; bit < 8; ++bit)
    {
        const int cx = (int) ((bit + 0.5) * img.getWidth() / 8.0), cy = img.getHeight() / 8;
        if (img.getPixelAt (cx, cy).getBrightness() > 0.5f) code |= 1 << bit;
    }
    return code;
}

static void videoTests (te::Engine& engine, const juce::File& dir)
{
    namespace V = tonkori::video;
    juce::ignoreUnused (engine);
    auto frames = dir.getChildFile ("frames");
    frames.createDirectory();
    const int numFrames = 100, w = 640, h = 360;
    juce::PNGImageFormat png;
    for (int i = 0; i < numFrames; ++i)
    {
        juce::Image img (juce::Image::RGB, w, h, true);
        juce::Graphics g (img);
        g.fillAll (juce::Colour (40, 40, 40));
        for (int bit = 0; bit < 8; ++bit)
        {
            g.setColour ((i >> bit) & 1 ? juce::Colours::white : juce::Colours::black);
            g.fillRect (bit * w / 8, 0, w / 8, h / 4);
        }
        juce::FileOutputStream os (frames.getChildFile ("f" + juce::String (i).paddedLeft ('0', 4) + ".png"));
        png.writeImageToStream (img, os);
    }

    // Picture source: PNG sequence -> ProRes 422 HQ .mov (25 fps), via our own exporter.
    const double rate = 48000.0;
    auto mixA = dir.getChildFile ("vid_mix.wav");
    writeWav (mixA, sine (rate, 4.0, 1000.0, -20.0), rate, 32);

    V::ExportParams base;
    base.source = juce::File (frames.getFullPathName() + "/f%04d.png");
    base.videoStart = 0.0; base.rangeStart = 0.0; base.rangeEnd = 4.0;
    base.audioWav = mixA;
    base.timecode = "01:00:00:00";

    auto proresSrc = dir.getChildFile ("src_prores.mov");
    juce::String err;
    auto p1 = base; p1.codec = V::Codec::proresHQ; p1.output = proresSrc;
    const bool ok1 = V::exportVideo (p1, {}, err);
    check (ok1, "Video export: ProRes 422 HQ .mov from an image sequence" + (ok1 ? juce::String() : ": " + err));
    if (! ok1) return;

    auto vf = V::VideoFile::open (proresSrc, err);
    check (vf != nullptr && vf->getInfo().hasVideo && vf->getInfo().width == w && std::abs (vf->getInfo().fps - 25.0) < 1e-6
           && vf->getInfo().timecode == "01:00:00:00" && vf->getInfo().audio.size() == 1,
           "Video import: " + (vf ? vf->getInfo().describe() : err));
    if (vf == nullptr) return;

    // Frame accuracy: forward play, random jumps, backwards.
    int wrong = 0;
    for (int i = 0; i < numFrames; ++i)             // sequential
        if (readCode (vf->frameAt ((i + 0.5) / 25.0, w, h)) != i) ++wrong;
    juce::Random rng (3);
    for (int k = 0; k < 60; ++k)
    {
        const int i = rng.nextInt (numFrames);
        if (readCode (vf->frameAt ((i + 0.2) / 25.0, w, h)) != i) ++wrong;
    }
    for (int i = numFrames - 1; i >= 0; i -= 7)
        if (readCode (vf->frameAt (i / 25.0 + 0.001, w, h)) != i) ++wrong;
    check (wrong == 0, "Video decode: frame-accurate playback, seeks and reverse jumps (" + juce::String (wrong) + " wrong of 175)");

    // Audio extraction keeps level and length.
    auto ex = dir.getChildFile ("extracted.wav");
    const bool okA = vf->extractAudio (vf->getInfo().audio[0].index, ex, 48000.0, {}, err);
    double rr = 0;
    auto exb = readAll (ex, rr);
    const double peakDb = exb.getNumSamples() > 0 ? juce::Decibels::gainToDecibels ((double) exb.getMagnitude (0, 0, exb.getNumSamples())) : -100;
    check (okA && std::abs (exb.getNumSamples() - 4.0 * rate) < 2 && std::abs (peakDb + 20.0) < 0.01,
           "Video audio extraction: " + juce::String (exb.getNumSamples()) + " smp, peak " + juce::String (peakDb, 3) + " dBFS (-20)");

    // Mix laid back to picture: DNxHR HQ in MXF and MOV, ProRes 4444, offset range, and stream copy.
    auto mixB = dir.getChildFile ("vid_mix2.wav");
    writeWav (mixB, sine (rate, 2.0, 440.0, -10.0), rate, 32);

    struct Case { V::Codec codec; const char* ext; const char* name; };
    const Case cases[] = { { V::Codec::dnxhrHQ, ".mxf", "DNxHR HQ .mxf" }, { V::Codec::dnxhrHQX, ".mov", "DNxHR HQX .mov" },
                           { V::Codec::prores4444, ".mov", "ProRes 4444 .mov" }, { V::Codec::proresProxy, ".mov", "ProRes Proxy .mov" },
                           { V::Codec::h264, ".mp4", "H.264 .mp4" }, { V::Codec::hevc, ".mp4", "HEVC .mp4" } };
    for (auto& c : cases)
    {
        if (! V::isEncoderAvailable (c.codec)) { std::cout << "        (" << c.name << ": encoder not available on this machine, skipped)" << std::endl; continue; }
        auto p = base;
        p.source = proresSrc; p.videoStart = 0.5; p.rangeStart = 1.5; p.rangeEnd = 3.5; p.audioWav = mixB;
        p.codec = c.codec; p.output = dir.getChildFile (juce::String ("out_") + juce::String (c.name).upToFirstOccurrenceOf (".", false, false).retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") + c.ext);
        p.width = 320; p.height = 180;
        const bool ok = V::exportVideo (p, {}, err);
        const bool hardware = c.codec == V::Codec::h264 || c.codec == V::Codec::hevc;
        if (! ok && hardware && err.startsWith ("Encoder setup failed"))
        {
            // VideoToolbox needs the Mac's media engine; CI virtual machines may not expose it.
            std::cout << "        (" << c.name << ": hardware encoder not available on this machine: " << err << ", skipped)" << std::endl;
            continue;
        }
        auto o = ok ? V::VideoFile::open (p.output, err) : nullptr;
        int bad = 0;
        if (o != nullptr)
            for (int i = 0; i < 50; i += 3)                         // output frame i = source frame 25 + i
                if (readCode (o->frameAt ((i + 0.5) / 25.0, 320, 180)) != 25 + i) ++bad;
        double peak = -100;
        if (o != nullptr && ! o->getInfo().audio.empty())
        {
            auto ax = dir.getChildFile ("ax.wav");
            o->extractAudio (o->getInfo().audio[0].index, ax, 48000.0, {}, err);
            double r2 = 0; auto ab = readAll (ax, r2);
            if (ab.getNumSamples() > 0) peak = juce::Decibels::gainToDecibels ((double) ab.getMagnitude (0, 0, ab.getNumSamples()));
        }
        const bool lossy = c.codec == V::Codec::h264 || c.codec == V::Codec::hevc;
        if (o != nullptr)
        {
            std::cout << "        " << c.name << ": " << o->getInfo().describe() << ", frames off: " << bad << std::endl;
            if (bad > 0)
            {
                std::cout << "          decoded codes:";
                for (int i = 0; i < 8; ++i) std::cout << " " << readCode (o->frameAt ((i + 0.5) / 25.0, 320, 180));
                std::cout << std::endl;
            }
        }
        else
            std::cout << "        " << c.name << ": " << err << std::endl;
        check (ok && o != nullptr && o->getInfo().width == 320 && std::abs (o->getInfo().durationSec - 2.0) < 0.05 && bad == 0
               && std::abs (peak + 10.0) < (lossy ? 0.6 : 0.01),
               juce::String ("Video export ") + c.name + ": 320x180, 2.00 s, picture in sync, audio " + juce::String (peak, 2) + " dBFS"
               + (ok ? juce::String() : " (" + err + ")"));
    }

    auto pc = base;
    pc.source = proresSrc; pc.codec = V::Codec::sameAsSource; pc.output = dir.getChildFile ("copy.mov");
    pc.rangeStart = 0; pc.rangeEnd = 4.0; pc.audioWav = mixA;
    const bool okc = V::exportVideo (pc, {}, err);
    auto oc = okc ? V::VideoFile::open (pc.output, err) : nullptr;
    int badc = 0;
    if (oc) for (int i = 0; i < numFrames; i += 9) if (readCode (oc->frameAt ((i + 0.5) / 25.0, w, h)) != i) ++badc;
    std::cout << "        copy: " << (oc ? oc->getInfo().describe() : err) << ", frames off: " << badc << std::endl;
    check (okc && oc != nullptr && badc == 0 && oc->getInfo().codec.containsIgnoreCase ("prores"),
           "Video export: same-as-source stream copy keeps every frame" + (okc ? juce::String() : ": " + err));
}
