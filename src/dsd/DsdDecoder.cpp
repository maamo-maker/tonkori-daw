#include "DsdDecoder.h"

namespace tonkori::dsd
{
//==============================================================================
namespace
{
    uint64_t readLE (juce::InputStream& s, int bytes)
    {
        uint64_t v = 0;
        for (int i = 0; i < bytes; ++i)
            v |= (uint64_t) (uint8_t) s.readByte() << (8 * i);
        return v;
    }

    uint64_t readBE (juce::InputStream& s, int bytes)
    {
        uint64_t v = 0;
        for (int i = 0; i < bytes; ++i)
            v = (v << 8) | (uint8_t) s.readByte();
        return v;
    }

    juce::String readId (juce::InputStream& s)
    {
        char c[5] = {};
        s.read (c, 4);
        return juce::String (c, 4);
    }

    uint8_t reverseBits (uint8_t b) noexcept
    {
        b = (uint8_t) ((b & 0xF0) >> 4 | (b & 0x0F) << 4);
        b = (uint8_t) ((b & 0xCC) >> 2 | (b & 0x33) << 2);
        b = (uint8_t) ((b & 0xAA) >> 1 | (b & 0x55) << 1);
        return b;
    }

    double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 64; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < sum * 1e-17) break;
        }
        return sum;
    }

    bool isSupportedDsdRate (double r)
    {
        for (double base : { 2822400.0, 3072000.0 })
            for (int m : { 1, 2, 4 })
                if (std::abs (r - base * m) < 1.0)
                    return true;
        return false;
    }
}

juce::String StreamInfo::describe() const
{
    const int multiple = juce::roundToInt (sampleRate / 44100.0);
    return juce::String (container == Container::dsf ? "DSF" : "DSDIFF")
         + " DSD" + juce::String (multiple) + " (" + juce::String (sampleRate / 1.0e6, 4) + " MHz), "
         + juce::String (numChannels) + " ch, "
         + juce::String ((double) samplesPerChannel / sampleRate, 2) + " s";
}

//==============================================================================
bool Reader::open (const juce::File& f, juce::String& error)
{
    in = f.createInputStream();

    if (in == nullptr || in->failedToOpen())
    {
        error = "Cannot open " + f.getFullPathName();
        return false;
    }

    const auto magic = readId (*in);
    in->setPosition (0);

    if (magic == "DSD ") return openDsf (error);
    if (magic == "FRM8") return openDff (error);

    error = "Not a DSF or DSDIFF file";
    return false;
}

bool Reader::openDsf (juce::String& error)
{
    info.container = StreamInfo::Container::dsf;

    readId (*in);                       // "DSD "
    const auto dsdChunkSize = readLE (*in, 8);
    in->setPosition ((int64_t) dsdChunkSize);

    if (readId (*in) != "fmt ")         { error = "DSF: missing fmt chunk"; return false; }

    const auto fmtSize    = readLE (*in, 8);
    const auto fmtStart   = in->getPosition() - 12;
    readLE (*in, 4);                    // format version
    const auto formatId   = readLE (*in, 4);
    readLE (*in, 4);                    // channel type
    info.numChannels      = (int) readLE (*in, 4);
    info.sampleRate       = (double) readLE (*in, 4);
    const auto bitsPerSmp = readLE (*in, 4);
    info.samplesPerChannel = readLE (*in, 8);
    dsfBlockSize          = (uint32_t) readLE (*in, 4);

    if (formatId != 0)                  { error = "DSF: unsupported format id"; return false; }
    if (bitsPerSmp != 1 && bitsPerSmp != 8) { error = "DSF: unsupported bits per sample"; return false; }
    dsfLsbFirst = (bitsPerSmp == 1);

    in->setPosition (fmtStart + (int64_t) fmtSize);

    if (readId (*in) != "data")         { error = "DSF: missing data chunk"; return false; }
    readLE (*in, 8);
    dataStart = in->getPosition();

    if (info.numChannels < 1 || info.numChannels > 64 || dsfBlockSize == 0)
    {
        error = "DSF: bad header";
        return false;
    }

    bytesPerChannelTotal = (info.samplesPerChannel + 7) / 8;
    dsfBlock.resize ((size_t) dsfBlockSize * (size_t) info.numChannels);
    pending.assign ((size_t) info.numChannels, {});
    pendingPos = 0;

    if (! isSupportedDsdRate (info.sampleRate))
    {
        error = "Unsupported DSD rate " + juce::String (info.sampleRate);
        return false;
    }

    return true;
}

bool Reader::openDff (juce::String& error)
{
    info.container = StreamInfo::Container::dff;

    readId (*in);                                   // FRM8
    const auto formSize = readBE (*in, 8);
    if (readId (*in) != "DSD ")                     { error = "DSDIFF: not a DSD form"; return false; }

    const auto formEnd = (int64_t) (12 + formSize);
    bool gotProp = false;

    while (in->getPosition() + 12 <= formEnd && ! in->isExhausted())
    {
        const auto id   = readId (*in);
        const auto size = readBE (*in, 8);
        const auto start = in->getPosition();

        if (id == "PROP")
        {
            readId (*in);                           // "SND "
            const auto propEnd = start + (int64_t) size;

            while (in->getPosition() + 12 <= propEnd)
            {
                const auto sid   = readId (*in);
                const auto ssize = readBE (*in, 8);
                const auto sstart = in->getPosition();

                if (sid == "FS  ")      info.sampleRate = (double) readBE (*in, 4);
                else if (sid == "CHNL") info.numChannels = (int) readBE (*in, 2);
                else if (sid == "CMPR")
                {
                    const auto c = readId (*in);
                    if (c != "DSD ")
                    {
                        error = "DSDIFF: compressed (" + c.trim() + ") files are not supported";
                        return false;
                    }
                }

                in->setPosition (sstart + (int64_t) ssize + (int64_t) (ssize & 1));
            }

            gotProp = true;
        }
        else if (id == "DSD ")
        {
            if (! gotProp || info.numChannels < 1)  { error = "DSDIFF: sound data before properties"; return false; }
            dataStart = start;
            bytesPerChannelTotal = size / (uint64_t) info.numChannels;
            info.samplesPerChannel = bytesPerChannelTotal * 8;
            break;
        }
        else if (id == "DST ")
        {
            error = "DSDIFF: DST-compressed files are not supported";
            return false;
        }

        in->setPosition (start + (int64_t) size + (int64_t) (size & 1));
    }

    if (dataStart == 0)                             { error = "DSDIFF: no sound data"; return false; }

    if (! isSupportedDsdRate (info.sampleRate))
    {
        error = "Unsupported DSD rate " + juce::String (info.sampleRate);
        return false;
    }

    in->setPosition (dataStart);
    return true;
}

bool Reader::fillDsfBlock()
{
    const auto n = in->read (dsfBlock.data(), (int) dsfBlock.size());
    if (n <= 0) return false;

    for (int c = 0; c < info.numChannels; ++c)
    {
        auto& p = pending[(size_t) c];
        p.assign (dsfBlock.begin() + (int64_t) c * dsfBlockSize,
                  dsfBlock.begin() + (int64_t) (c + 1) * dsfBlockSize);

        if (dsfLsbFirst)
            for (auto& b : p) b = reverseBits (b);
    }

    pendingPos = 0;
    return true;
}

int Reader::read (std::vector<std::vector<uint8_t>>& out, int maxBytes)
{
    out.resize ((size_t) info.numChannels);
    for (auto& v : out) v.clear();

    const auto remaining = bytesPerChannelTotal - bytesPerChannelDone;
    int want = (int) std::min<uint64_t> ((uint64_t) maxBytes, remaining);
    if (want <= 0) return 0;

    int got = 0;

    if (info.container == StreamInfo::Container::dsf)
    {
        while (got < want)
        {
            if (pendingPos >= pending[0].size() && ! fillDsfBlock())
                break;

            const int take = (int) std::min<size_t> ((size_t) (want - got), pending[0].size() - pendingPos);

            for (int c = 0; c < info.numChannels; ++c)
                out[(size_t) c].insert (out[(size_t) c].end(),
                                        pending[(size_t) c].begin() + (int64_t) pendingPos,
                                        pending[(size_t) c].begin() + (int64_t) (pendingPos + (size_t) take));
            pendingPos += (size_t) take;
            got += take;
        }
    }
    else
    {
        std::vector<uint8_t> interleaved ((size_t) want * (size_t) info.numChannels);
        const auto n = in->read (interleaved.data(), (int) interleaved.size());
        got = n / info.numChannels;

        for (int c = 0; c < info.numChannels; ++c)
        {
            auto& o = out[(size_t) c];
            o.resize ((size_t) got);
            for (int i = 0; i < got; ++i)
                o[(size_t) i] = interleaved[(size_t) (i * info.numChannels + c)];
        }
    }

    bytesPerChannelDone += (uint64_t) got;
    return got;
}

//==============================================================================
Decimator::Decimator (double dsdRate, double outRate)
    : inputRate (dsdRate), outputRate (outRate)
{
    decimation = juce::roundToInt (inputRate / outputRate);
    jassert (decimation >= 8 && decimation % 8 == 0);
    bytesPerOutput = decimation / 8;

    // Kaiser-windowed sinc, 48·D taps, centred at 24·D − 1 input samples so the
    // filter delay is exactly 23 output samples (compensated by the caller).
    const int numTaps = 48 * decimation;
    const double centre = 24.0 * decimation - 1.0;
    const double halfSpan = 24.0 * decimation;
    const double fc = 0.42 * outputRate / inputRate;     // −6 dB at 148 kHz for DXD out
    const double beta = 12.27;                            // ≈ 120 dB stop-band
    const double i0b = besselI0 (beta);

    taps.resize ((size_t) numTaps);
    double sum = 0;

    for (int k = 0; k < numTaps; ++k)
    {
        const double t = (double) k - centre;
        const double x = 2.0 * fc * t;
        const double sinc = (std::abs (x) < 1e-12) ? 1.0 : std::sin (juce::MathConstants<double>::pi * x) / (juce::MathConstants<double>::pi * x);
        const double r = juce::jlimit (-1.0, 1.0, t / halfSpan);
        const double w = besselI0 (beta * std::sqrt (1.0 - r * r)) / i0b;
        taps[(size_t) k] = 2.0 * fc * sinc * w;
        sum += taps[(size_t) k];
    }

    for (auto& h : taps) h /= sum;

    // Byte-group lookup tables: group g covers input samples [8g, 8g+7] back from
    // the newest; bit j (LSB = newest) of the byte is sample 8g + j.
    numGroups = numTaps / 8;
    tables.resize ((size_t) numGroups);

    for (int g = 0; g < numGroups; ++g)
        for (int b = 0; b < 256; ++b)
        {
            double acc = 0;
            for (int j = 0; j < 8; ++j)
                acc += taps[(size_t) (8 * g + j)] * (((b >> j) & 1) ? 1.0 : -1.0);
            tables[(size_t) g][(size_t) b] = acc;
        }

    history.assign ((size_t) numGroups, 0x69);
    reset();
}

void Decimator::reset()
{
    std::fill (history.begin(), history.end(), (uint8_t) 0x69);
    writeIndex = 0;
    bytesSinceOutput = 0;
}

int Decimator::process (const uint8_t* bytes, int numBytes, float* out)
{
    int written = 0;

    for (int i = 0; i < numBytes; ++i)
    {
        writeIndex = (writeIndex + 1) % numGroups;
        history[(size_t) writeIndex] = bytes[i];

        if (++bytesSinceOutput == bytesPerOutput)
        {
            bytesSinceOutput = 0;
            double acc = 0;
            int idx = writeIndex;

            for (int g = 0; g < numGroups; ++g)
            {
                acc += tables[(size_t) g][history[(size_t) idx]];
                idx = (idx == 0) ? numGroups - 1 : idx - 1;
            }

            out[written++] = (float) acc;
        }
    }

    return written;
}

int Decimator::flush (int numBytes, float* out)
{
    std::vector<uint8_t> silence ((size_t) numBytes, 0x69);
    return process (silence.data(), numBytes, out);
}

//==============================================================================
ConversionResult convertToDxd (const juce::File& source, const juce::File& destWav,
                               std::function<bool (double)> progress)
{
    ConversionResult result;
    Reader reader;

    if (! reader.open (source, result.error))
        return result;

    const auto& info = reader.getInfo();
    result.source = info;
    result.outputRate = dxdRate;

    const int numCh = info.numChannels;
    std::vector<Decimator> decimators;
    for (int c = 0; c < numCh; ++c)
        decimators.emplace_back (info.sampleRate, dxdRate);

    const int D = decimators[0].getDecimation();
    const int skip = 23;                                     // filter delay in output samples
    const int64_t totalOut = (int64_t) ((info.samplesPerChannel + (uint64_t) D - 1) / (uint64_t) D);

    destWav.deleteFile();
    std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (destWav);

    if (static_cast<juce::FileOutputStream*> (os.get())->failedToOpen())
    {
        result.error = "Cannot write " + destWav.getFullPathName();
        return result;
    }

    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}
                                              .withSampleRate (dxdRate)
                                              .withNumChannels (numCh)
                                              .withBitsPerSample (32)
                                              .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint)
                                              .withMetadata (juce::WavAudioFormat::bwavDescription,
                                                             "DXD from " + source.getFileName()));
    if (writer == nullptr)
    {
        result.error = "Cannot create WAV writer";
        return result;
    }

    const int chunkBytes = 64 * 1024;
    std::vector<std::vector<uint8_t>> bytes;
    std::vector<std::vector<float>> pcm ((size_t) numCh, std::vector<float> ((size_t) (chunkBytes * 8 / D + 8)));
    std::vector<const float*> ptrs ((size_t) numCh);

    int64_t produced = 0, written = 0;
    int toSkip = skip;

    auto emit = [&] (int n)
    {
        int offset = 0;
        if (toSkip > 0)
        {
            offset = std::min (toSkip, n);
            toSkip -= offset;
        }

        int count = n - offset;
        count = (int) std::min<int64_t> (count, totalOut - written);
        if (count <= 0) return true;

        for (int c = 0; c < numCh; ++c)
            ptrs[(size_t) c] = pcm[(size_t) c].data() + offset;

        if (! writer->writeFromFloatArrays (ptrs.data(), numCh, count))
            return false;

        written += count;
        return true;
    };

    for (;;)
    {
        const int got = reader.read (bytes, chunkBytes);
        if (got <= 0) break;

        int n = 0;
        for (int c = 0; c < numCh; ++c)
            n = decimators[(size_t) c].process (bytes[(size_t) c].data(), got, pcm[(size_t) c].data());

        produced += n;

        if (! emit (n))
        {
            result.error = "Write failed";
            return result;
        }

        if (progress && ! progress ((double) written / (double) std::max<int64_t> (1, totalOut)))
        {
            result.error = "Cancelled";
            return result;
        }
    }

    // Flush the filter tail so the last samples are complete.
    const int tailBytes = (skip + 2) * (D / 8);
    for (int c = 0; c < numCh; ++c)
        decimators[(size_t) c].flush (tailBytes, pcm[(size_t) c].data());
    emit (skip + 2);

    writer.reset();
    result.outputSamples = written;
    result.ok = (written == totalOut);

    if (! result.ok)
        result.error = "Short output: " + juce::String (written) + " of " + juce::String (totalOut);

    return result;
}
}
