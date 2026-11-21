#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <vector>

/*  DSD → DXD conversion, the way Pyramix treats DSD: decode the 1-bit stream
    to 352.8 kHz floating-point PCM for editing and mixing.

    Supports .dsf (Sony DSD Stream File) and .dff (Philips DSDIFF, uncompressed)
    at DSD64 / DSD128 / DSD256. DST-compressed DSDIFF is rejected.

    Level convention: a fully-modulated (100%) DSD stream maps to ±1.0 float.
    The SACD "0 dB" reference (50% modulation) therefore lands at −6.02 dBFS,
    which leaves the +6 dB of legal SACD headroom intact and never clips. */

namespace tonkori::dsd
{
    struct StreamInfo
    {
        enum class Container { dsf, dff };

        Container container = Container::dsf;
        int numChannels = 0;
        double sampleRate = 0;            // DSD bit rate, e.g. 2'822'400
        uint64_t samplesPerChannel = 0;   // in 1-bit samples
        juce::String describe() const;
    };

    //==============================================================================
    /** Reads raw DSD as channel-planar bytes, MSB = earliest sample. */
    class Reader
    {
    public:
        bool open (const juce::File&, juce::String& error);
        const StreamInfo& getInfo() const noexcept      { return info; }

        /** Fills perChannel[c] with up to maxBytes bytes. Returns bytes per channel read (0 at end). */
        int read (std::vector<std::vector<uint8_t>>& perChannel, int maxBytes);

    private:
        bool openDsf (juce::String& error);
        bool openDff (juce::String& error);

        std::unique_ptr<juce::FileInputStream> in;
        StreamInfo info;
        int64_t dataStart = 0;
        uint64_t bytesPerChannelTotal = 0, bytesPerChannelDone = 0;

        // DSF: block-interleaved, LSB-first within bytes when bitsPerSample == 1
        uint32_t dsfBlockSize = 4096;
        bool dsfLsbFirst = true;
        std::vector<uint8_t> dsfBlock;          // one block group (all channels)
        std::vector<std::vector<uint8_t>> pending; // decoded block not yet handed out
        size_t pendingPos = 0;
        bool fillDsfBlock();
    };

    //==============================================================================
    /** Lookup-table polyphase FIR decimator: 1-bit DSD → float PCM.
        For decimation D (8, 16, 32) the filter has 48·D taps, grouped per byte so
        each output sample costs 6·D table lookups. Tables and accumulation are double. */
    class Decimator
    {
    public:
        Decimator (double dsdRate, double outputRate);

        int getDecimation() const noexcept              { return decimation; }
        int getNumTaps() const noexcept                 { return (int) taps.size(); }
        double getGroupDelaySeconds() const noexcept    { return ((double) taps.size() - 1.0) * 0.5 / inputRate; }

        void reset();

        /** Consumes MSB-first DSD bytes; writes floor(total/(D/8)) samples to out. Returns samples written. */
        int process (const uint8_t* bytes, int numBytes, float* out);

        /** Pushes 'numBytes' bytes of DSD silence pattern (0x69) and returns samples, for flushing the tail. */
        int flush (int numBytes, float* out);

    private:
        double inputRate, outputRate;
        int decimation = 8, bytesPerOutput = 1, numGroups = 0;
        std::vector<double> taps;
        std::vector<std::array<double, 256>> tables;
        std::vector<uint8_t> history;   // circular, size numGroups
        int writeIndex = 0, bytesSinceOutput = 0;
    };

    //==============================================================================
    struct ConversionResult
    {
        bool ok = false;
        juce::String error;
        StreamInfo source;
        double outputRate = 0;
        int64_t outputSamples = 0;
    };

    /** Decodes a .dsf/.dff into a 32-bit float WAV at 352.8 kHz (DXD).
        The FIR's group delay is compensated so the PCM is time-aligned with the DSD. */
    ConversionResult convertToDxd (const juce::File& source, const juce::File& destWav,
                                   std::function<bool (double progress)> progressCallback = {});

    inline bool isDsdFile (const juce::File& f)
    {
        return f.hasFileExtension ("dsf;dff");
    }

    constexpr double dxdRate = 352800.0;
}
