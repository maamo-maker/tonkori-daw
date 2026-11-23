#pragma once

#include <JuceHeader.h>

/*  Video for post-production work (Pro Tools / Pyramix / Nuendo video track).

    Decoding and encoding use FFmpeg (static, LGPL build; see scripts/build-ffmpeg.sh).
    Import accepts what those DAWs accept and more: QuickTime .mov, MP4/M4V, MXF (OP1a and
    OP-Atom), AVI, MKV/WebM, MPEG-2 PS/TS (.mpg .m2ts .mts .ts), DV, with ProRes (all
    flavours), DNxHD/DNxHR, H.264, HEVC, MPEG-2/XDCAM, DV, MJPEG, VP8/9, AV1 and more.
    Export: ProRes 422 Proxy/LT/422/HQ and 4444, DNxHR LB/SQ/HQ/HQX/444, H.264 and HEVC
    (Apple VideoToolbox hardware encoders), or the source stream copied untouched. */

namespace tonkori::video
{
    struct AudioStreamInfo { int index; int channels; int sampleRate; juce::String codec, language; };

    struct Info
    {
        int width = 0, height = 0;
        double fps = 0, durationSec = 0;
        juce::int64 numFrames = 0;
        juce::String codec, container, pixelFormat, timecode;
        bool hasVideo = false;
        std::vector<AudioStreamInfo> audio;
        juce::String describe() const;
    };

    juce::StringArray fileExtensions();          // "*.mov;*.mp4;..."
    bool isVideoFile (const juce::File&);

    /** One decoder. Not thread-safe: use one instance per thread. */
    class VideoFile
    {
    public:
        static std::unique_ptr<VideoFile> open (const juce::File&, juce::String& error);
        ~VideoFile();

        const Info& getInfo() const       { return info; }
        const juce::File& getFile() const { return file; }

        /** Frame shown at time t (seconds from the start of the file), scaled to fit w x h
            (aspect kept). Sequential calls decode forward; jumps seek to the previous keyframe. */
        juce::Image frameAt (double seconds, int maxWidth, int maxHeight);
        juce::int64 frameIndexAt (double seconds) const;

        /** Decodes one audio stream to a 32-bit float WAV at 'targetRate' (0 = native). */
        bool extractAudio (int streamIndex, const juce::File& wavOut, double targetRate,
                           std::function<bool (double)> progress, juce::String& error);

        struct Impl;
        Impl& getImpl() { return *impl; }
    private:
        VideoFile() = default;
        juce::File file;
        Info info;
        std::unique_ptr<Impl> impl;
    };

    //==============================================================================
    enum class Codec
    {
        sameAsSource,
        proresProxy, proresLT, prores422, proresHQ, prores4444,
        dnxhrLB, dnxhrSQ, dnxhrHQ, dnxhrHQX, dnxhr444,
        h264, hevc, hevc10
    };

    juce::StringArray codecNames();               // UI order = enum order
    juce::String containerFor (Codec, bool preferMxf);   // ".mov", ".mxf" or ".mp4"
    bool isEncoderAvailable (Codec);

    enum class AudioFormat { pcm24, pcm32f, aac320 };

    struct ExportParams
    {
        juce::File source;            // the session's video
        double videoStart = 0;        // timeline seconds where the video's first frame sits
        double rangeStart = 0, rangeEnd = 0;   // timeline seconds to export
        juce::File audioWav;          // rendered mix for exactly [rangeStart, rangeEnd]
        Codec codec = Codec::proresHQ;
        AudioFormat audio = AudioFormat::pcm24;
        int width = 0, height = 0;    // 0 = source size
        juce::File output;
        juce::String timecode;        // start timecode written to the file, e.g. "01:00:00:00"
    };

    /** Encodes picture + audio. 'progress' returns false to cancel. */
    bool exportVideo (const ExportParams&, std::function<bool (double)> progress, juce::String& error);

    //==============================================================================
    /** Session state: one picture track per session, stored in the edit. */
    struct SessionVideo
    {
        juce::File file;
        double start = 0;             // timeline seconds of frame 0
        double syncOffsetMs = 0;      // picture delay vs audio (positive = picture later)

        static SessionVideo get (const tracktion::Edit&);
        static void set (tracktion::Edit&, const SessionVideo&);
        static void clear (tracktion::Edit&);
        bool isValid() const { return file.existsAsFile(); }
    };
}
