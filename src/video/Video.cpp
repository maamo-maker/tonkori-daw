#include "Video.h"

extern "C"
{
   #include <libavformat/avformat.h>
   #include <libavcodec/avcodec.h>
   #include <libavutil/imgutils.h>
   #include <libavutil/opt.h>
   #include <libavutil/channel_layout.h>
   #include <libswscale/swscale.h>
   #include <libswresample/swresample.h>
}

namespace tonkori::video
{
    namespace
    {
        juce::String avErr (int e)
        {
            char b[256] = {};
            av_strerror (e, b, sizeof (b));
            return b;
        }

        struct FrameDeleter  { void operator() (AVFrame* f) const  { av_frame_free (&f); } };
        struct PacketDeleter { void operator() (AVPacket* p) const { av_packet_free (&p); } };
        using FramePtr  = std::unique_ptr<AVFrame, FrameDeleter>;
        using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
    }

    juce::StringArray fileExtensions()
    {
        return { "*.mov", "*.mp4", "*.m4v", "*.mxf", "*.avi", "*.mkv", "*.webm", "*.mpg", "*.mpeg", "*.m2v",
                 "*.ts", "*.mts", "*.m2ts", "*.dv", "*.flv", "*.wmv", "*.asf", "*.3gp" };
    }

    bool isVideoFile (const juce::File& f)
    {
        for (auto& w : fileExtensions())
            if (f.hasFileExtension (w.fromFirstOccurrenceOf (".", false, false))) return true;
        return false;
    }

    juce::String Info::describe() const
    {
        if (! hasVideo) return "No picture";
        juce::String s;
        s << width << "x" << height << "  " << juce::String (fps, 3).trimCharactersAtEnd ("0").trimCharactersAtEnd (".")
          << " fps  " << codec << " (" << pixelFormat << ")  " << container;
        if (timecode.isNotEmpty()) s << "  TC " << timecode;
        s << "  " << juce::String (durationSec, 2) << " s";
        if (! audio.empty()) s << "  " << (int) audio.size() << " audio stream" << (audio.size() > 1 ? "s" : "");
        return s;
    }

    //==============================================================================
    struct VideoFile::Impl
    {
        AVFormatContext* fmt = nullptr;
        AVCodecContext* dec = nullptr;
        int vIndex = -1;
        AVRational tb { 1, 1 }, frameRate { 25, 1 };
        juce::int64 startPts = 0;
        FramePtr frame { av_frame_alloc() };
        FramePtr current { av_frame_alloc() };
        bool hasCurrent = false, eof = false;
        PacketPtr pkt { av_packet_alloc() };
        SwsContext* sws = nullptr;

        ~Impl()
        {
            sws_freeContext (sws);
            avcodec_free_context (&dec);
            avformat_close_input (&fmt);
        }

        juce::int64 ptsFor (double seconds) const
        {
            // floor, not round: with coarse time bases (MXF 1/25) a time inside a frame must map to that frame
            return startPts + (juce::int64) std::floor (seconds * tb.den / (double) tb.num + 1.0e-7);
        }

        juce::int64 frameDuration() const
        {
            return juce::jmax ((juce::int64) 1, (juce::int64) std::llround ((double) tb.den * frameRate.den / ((double) tb.num * frameRate.num)));
        }

        juce::int64 ptsOf (const AVFrame* f) const
        {
            return f->best_effort_timestamp != AV_NOPTS_VALUE ? f->best_effort_timestamp : f->pts;
        }

        /** Decodes the next frame into 'frame'. Returns false at the end. */
        bool decodeNext()
        {
            for (;;)
            {
                const int r = avcodec_receive_frame (dec, frame.get());
                if (r == 0) return true;
                if (r == AVERROR_EOF) return false;
                if (r != AVERROR (EAGAIN)) return false;

                if (eof) return false;
                const int rr = av_read_frame (fmt, pkt.get());
                if (rr < 0)
                {
                    eof = true;
                    avcodec_send_packet (dec, nullptr);
                    continue;
                }
                if (pkt->stream_index == vIndex)
                    avcodec_send_packet (dec, pkt.get());
                av_packet_unref (pkt.get());
            }
        }

        void keep()
        {
            av_frame_unref (current.get());
            av_frame_move_ref (current.get(), frame.get());
            hasCurrent = true;
        }

        /** The frame on screen at 'seconds'; nullptr if nothing decodable. */
        AVFrame* decodeAt (double seconds)
        {
            const auto target = ptsFor (seconds);
            const auto dur = frameDuration();

            if (hasCurrent)
            {
                const auto cp = ptsOf (current.get());
                if (target >= cp && target < cp + dur) return current.get();
                const bool forward = target > cp && (target - cp) < (juce::int64) (2.0 * tb.den / tb.num);
                if (forward)
                {
                    while (decodeNext())
                    {
                        keep();
                        if (ptsOf (current.get()) + dur > target) return current.get();
                    }
                    return current.get();        // end of file: hold the last frame
                }
            }

            av_seek_frame (fmt, vIndex, target, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers (dec);
            eof = false;
            bool any = false;
            while (decodeNext())
            {
                keep();
                any = true;
                if (ptsOf (current.get()) + dur > target) break;
            }
            return any || hasCurrent ? current.get() : nullptr;
        }
    };

    std::unique_ptr<VideoFile> VideoFile::open (const juce::File& f, juce::String& error)
    {
        std::unique_ptr<VideoFile> v (new VideoFile());
        v->file = f;
        v->impl = std::make_unique<Impl>();
        auto& im = *v->impl;

        int r = avformat_open_input (&im.fmt, f.getFullPathName().toRawUTF8(), nullptr, nullptr);
        if (r < 0) { error = "Can't open " + f.getFileName() + ": " + avErr (r); return {}; }
        if ((r = avformat_find_stream_info (im.fmt, nullptr)) < 0) { error = "No stream info: " + avErr (r); return {}; }

        auto& info = v->info;
        info.container = im.fmt->iformat->long_name != nullptr ? im.fmt->iformat->long_name : im.fmt->iformat->name;
        info.durationSec = im.fmt->duration > 0 ? im.fmt->duration / (double) AV_TIME_BASE : 0.0;
        if (auto* tc = av_dict_get (im.fmt->metadata, "timecode", nullptr, 0)) info.timecode = tc->value;

        for (unsigned i = 0; i < im.fmt->nb_streams; ++i)
        {
            auto* s = im.fmt->streams[i];
            if (s->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                AudioStreamInfo a;
                a.index = (int) i;
                a.channels = s->codecpar->ch_layout.nb_channels;
                a.sampleRate = s->codecpar->sample_rate;
                a.codec = avcodec_get_name (s->codecpar->codec_id);
                if (auto* l = av_dict_get (s->metadata, "language", nullptr, 0)) a.language = l->value;
                info.audio.push_back (a);
            }
            if (info.timecode.isEmpty())
                if (auto* tc = av_dict_get (s->metadata, "timecode", nullptr, 0)) info.timecode = tc->value;
        }

        const AVCodec* codec = nullptr;
        im.vIndex = av_find_best_stream (im.fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        if (im.vIndex < 0 || codec == nullptr)
            return v;                                   // audio-only file: still usable for audio extraction

        auto* vs = im.fmt->streams[im.vIndex];
        im.dec = avcodec_alloc_context3 (codec);
        avcodec_parameters_to_context (im.dec, vs->codecpar);
        im.dec->thread_count = 0;
        im.dec->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        if ((r = avcodec_open2 (im.dec, codec, nullptr)) < 0) { error = "Can't decode " + juce::String (codec->name) + ": " + avErr (r); return {}; }

        im.tb = vs->time_base;
        im.frameRate = vs->avg_frame_rate.num > 0 ? vs->avg_frame_rate : (vs->r_frame_rate.num > 0 ? vs->r_frame_rate : AVRational { 25, 1 });
        im.startPts = vs->start_time != AV_NOPTS_VALUE ? vs->start_time : 0;

        info.hasVideo = true;
        info.width = vs->codecpar->width;
        info.height = vs->codecpar->height;
        info.fps = av_q2d (im.frameRate);
        info.codec = juce::String (codec->long_name != nullptr ? codec->long_name : codec->name).replace (" (iCodec Pro)", "");
        if (auto* d = av_get_pix_fmt_name ((AVPixelFormat) vs->codecpar->format)) info.pixelFormat = d;
        if (vs->duration > 0) info.durationSec = vs->duration * av_q2d (vs->time_base);
        info.numFrames = vs->nb_frames > 0 ? vs->nb_frames : (juce::int64) std::llround (info.durationSec * info.fps);
        return v;
    }

    VideoFile::~VideoFile() = default;

    juce::int64 VideoFile::frameIndexAt (double seconds) const
    {
        return (juce::int64) std::floor (seconds * info.fps + 1.0e-6);
    }

    juce::Image VideoFile::frameAt (double seconds, int maxW, int maxH)
    {
        if (! info.hasVideo || impl == nullptr) return {};
        seconds = juce::jlimit (0.0, juce::jmax (0.0, info.durationSec - 0.5 / juce::jmax (1.0, info.fps)), seconds);
        auto* f = impl->decodeAt (seconds);
        if (f == nullptr || f->width <= 0) return {};

        const double scale = juce::jmin ((double) maxW / f->width, (double) maxH / f->height, 1.0);
        const int w = juce::jmax (2, (int) (f->width * scale)), h = juce::jmax (2, (int) (f->height * scale));

        impl->sws = sws_getCachedContext (impl->sws, f->width, f->height, (AVPixelFormat) f->format,
                                          w, h, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (impl->sws == nullptr) return {};

        juce::Image img (juce::Image::ARGB, w, h, false);
        {
            juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
            uint8_t* dst[4] = { bd.data, nullptr, nullptr, nullptr };
            int dstStride[4] = { bd.lineStride, 0, 0, 0 };
            sws_scale (impl->sws, f->data, f->linesize, 0, f->height, dst, dstStride);
        }
        return img;
    }

    bool VideoFile::extractAudio (int streamIndex, const juce::File& wavOut, double targetRate,
                                  std::function<bool (double)> progress, juce::String& error)
    {
        AVFormatContext* fmt = nullptr;
        int r = avformat_open_input (&fmt, file.getFullPathName().toRawUTF8(), nullptr, nullptr);
        if (r < 0) { error = avErr (r); return false; }
        std::unique_ptr<AVFormatContext, void (*) (AVFormatContext*)> fmtGuard (fmt, [] (AVFormatContext* c) { avformat_close_input (&c); });
        avformat_find_stream_info (fmt, nullptr);
        if (streamIndex < 0 || streamIndex >= (int) fmt->nb_streams) { error = "No such audio stream"; return false; }

        auto* st = fmt->streams[streamIndex];
        const AVCodec* codec = avcodec_find_decoder (st->codecpar->codec_id);
        if (codec == nullptr) { error = "No decoder for " + juce::String (avcodec_get_name (st->codecpar->codec_id)); return false; }

        AVCodecContext* dec = avcodec_alloc_context3 (codec);
        std::unique_ptr<AVCodecContext, void (*) (AVCodecContext*)> decGuard (dec, [] (AVCodecContext* c) { avcodec_free_context (&c); });
        avcodec_parameters_to_context (dec, st->codecpar);
        if ((r = avcodec_open2 (dec, codec, nullptr)) < 0) { error = avErr (r); return false; }

        const int nch = juce::jmax (1, dec->ch_layout.nb_channels);
        const int inRate = dec->sample_rate;
        const int outRate = targetRate > 0 ? (int) targetRate : inRate;

        SwrContext* swr = nullptr;
        AVChannelLayout outLayout;
        av_channel_layout_default (&outLayout, nch);
        swr_alloc_set_opts2 (&swr, &outLayout, AV_SAMPLE_FMT_FLTP, outRate, &dec->ch_layout, dec->sample_fmt, inRate, 0, nullptr);
        av_opt_set_int (swr, "filter_size", 128, 0);
        av_opt_set_int (swr, "phase_shift", 12, 0);
        av_opt_set_double (swr, "cutoff", 0.98, 0);
        if (swr == nullptr || swr_init (swr) < 0) { swr_free (&swr); error = "Resampler setup failed"; return false; }
        std::unique_ptr<SwrContext, void (*) (SwrContext*)> swrGuard (swr, [] (SwrContext* s) { swr_free (&s); });

        wavOut.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (wavOut.createOutputStream());
        if (os == nullptr) { error = "Can't write " + wavOut.getFullPathName(); return false; }
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (os.get(), outRate, (unsigned int) nch, 32, {}, 0));
        if (writer == nullptr) { error = "WAV writer failed"; return false; }
        os.release();

        // Keep picture sync: audio that starts after the picture gets leading silence.
        double offset = 0;
        if (st->start_time != AV_NOPTS_VALUE)
        {
            offset = st->start_time * av_q2d (st->time_base);
            if (impl != nullptr && impl->vIndex >= 0)
                offset -= impl->startPts * av_q2d (impl->tb);
        }
        juce::int64 dropSamples = offset < 0 ? (juce::int64) std::llround (-offset * outRate) : 0;
        if (offset > 0)
        {
            juce::AudioBuffer<float> silence (nch, (int) std::llround (offset * outRate));
            silence.clear();
            writer->writeFromAudioSampleBuffer (silence, 0, silence.getNumSamples());
        }

        PacketPtr pkt (av_packet_alloc());
        FramePtr frame (av_frame_alloc());
        juce::AudioBuffer<float> buf (nch, 1 << 16);
        const double total = juce::jmax (0.001, fmt->duration > 0 ? fmt->duration / (double) AV_TIME_BASE : 1.0);
        bool flushed = false;

        auto writeConverted = [&] (const uint8_t** in, int inSamples)
        {
            const int cap = swr_get_out_samples (swr, inSamples);
            if (cap > buf.getNumSamples()) buf.setSize (nch, cap, false, false, true);
            uint8_t* outPtrs[64] = {};
            for (int ch = 0; ch < nch && ch < 64; ++ch) outPtrs[ch] = (uint8_t*) buf.getWritePointer (ch);
            int got = swr_convert (swr, outPtrs, buf.getNumSamples(), in, inSamples);
            if (got <= 0) return;
            int startAt = 0;
            if (dropSamples > 0) { const int d = (int) juce::jmin ((juce::int64) got, dropSamples); dropSamples -= d; startAt = d; }
            if (got > startAt) writer->writeFromAudioSampleBuffer (buf, startAt, got - startAt);
        };

        while (! flushed)
        {
            r = av_read_frame (fmt, pkt.get());
            if (r < 0) { avcodec_send_packet (dec, nullptr); flushed = true; }
            else if (pkt->stream_index == streamIndex)
            {
                avcodec_send_packet (dec, pkt.get());
                if (progress && pkt->pts != AV_NOPTS_VALUE && ! progress (juce::jlimit (0.0, 1.0, pkt->pts * av_q2d (st->time_base) / total)))
                { av_packet_unref (pkt.get()); error = "Cancelled"; return false; }
            }
            if (r >= 0) av_packet_unref (pkt.get());

            while (avcodec_receive_frame (dec, frame.get()) == 0)
            {
                writeConverted ((const uint8_t**) frame->extended_data, frame->nb_samples);
                av_frame_unref (frame.get());
            }
        }
        writeConverted (nullptr, 0);        // drain the resampler
        writer.reset();
        return true;
    }

    //==============================================================================
    juce::StringArray codecNames()
    {
        return { "Same as source (copy picture, new audio)",
                 "Apple ProRes 422 Proxy", "Apple ProRes 422 LT", "Apple ProRes 422", "Apple ProRes 422 HQ", "Apple ProRes 4444",
                 "Avid DNxHR LB", "Avid DNxHR SQ", "Avid DNxHR HQ", "Avid DNxHR HQX (10-bit)", "Avid DNxHR 444 (10-bit)",
                 "H.264 (VideoToolbox)", "HEVC / H.265 (VideoToolbox)", "HEVC / H.265 10-bit (VideoToolbox)" };
    }

    juce::String containerFor (Codec c, bool preferMxf)
    {
        switch (c)
        {
            case Codec::dnxhrLB: case Codec::dnxhrSQ: case Codec::dnxhrHQ: case Codec::dnxhrHQX: case Codec::dnxhr444:
                return preferMxf ? ".mxf" : ".mov";
            case Codec::h264: case Codec::hevc: case Codec::hevc10:
                return ".mp4";
            default:
                return ".mov";
        }
    }

    namespace
    {
        struct EncoderChoice { const char* name; AVPixelFormat pix; const char* option; const char* value; };

        EncoderChoice encoderFor (Codec c)
        {
            switch (c)
            {
                case Codec::proresProxy: return { "prores_ks", AV_PIX_FMT_YUV422P10LE, "profile", "0" };
                case Codec::proresLT:    return { "prores_ks", AV_PIX_FMT_YUV422P10LE, "profile", "1" };
                case Codec::prores422:   return { "prores_ks", AV_PIX_FMT_YUV422P10LE, "profile", "2" };
                case Codec::proresHQ:    return { "prores_ks", AV_PIX_FMT_YUV422P10LE, "profile", "3" };
                case Codec::prores4444:  return { "prores_ks", AV_PIX_FMT_YUVA444P10LE, "profile", "4" };
                case Codec::dnxhrLB:     return { "dnxhd", AV_PIX_FMT_YUV422P, "profile", "dnxhr_lb" };
                case Codec::dnxhrSQ:     return { "dnxhd", AV_PIX_FMT_YUV422P, "profile", "dnxhr_sq" };
                case Codec::dnxhrHQ:     return { "dnxhd", AV_PIX_FMT_YUV422P, "profile", "dnxhr_hq" };
                case Codec::dnxhrHQX:    return { "dnxhd", AV_PIX_FMT_YUV422P10LE, "profile", "dnxhr_hqx" };
                case Codec::dnxhr444:    return { "dnxhd", AV_PIX_FMT_YUV444P10LE, "profile", "dnxhr_444" };
                case Codec::h264:        return { "h264_videotoolbox", AV_PIX_FMT_NV12, "profile", "high" };
                case Codec::hevc:        return { "hevc_videotoolbox", AV_PIX_FMT_NV12, "profile", "main" };
                case Codec::hevc10:      return { "hevc_videotoolbox", AV_PIX_FMT_P010LE, "profile", "main10" };
                case Codec::sameAsSource: break;
            }
            return { nullptr, AV_PIX_FMT_NONE, nullptr, nullptr };
        }
    }

    bool isEncoderAvailable (Codec c)
    {
        if (c == Codec::sameAsSource) return true;
        auto e = encoderFor (c);
        return e.name != nullptr && avcodec_find_encoder_by_name (e.name) != nullptr;
    }

    //==============================================================================
    bool exportVideo (const ExportParams& p, std::function<bool (double)> progress, juce::String& error)
    {
        auto src = VideoFile::open (p.source, error);
        if (src == nullptr) return false;
        const auto& si = src->getInfo();
        if (! si.hasVideo) { error = "The source has no picture"; return false; }
        auto& sim = src->getImpl();

        // Audio source
        juce::WavAudioFormat wavFmt;
        std::unique_ptr<juce::AudioFormatReader> wav;
        if (auto in = p.audioWav.createInputStream()) wav.reset (wavFmt.createReaderFor (in.release(), true));
        if (wav == nullptr) { error = "Rendered mix missing"; return false; }

        const juce::String ext = p.output.getFileExtension().toLowerCase();
        const bool isMxf = ext == ".mxf", isMp4 = ext == ".mp4" || ext == ".m4v";

        AVFormatContext* oc = nullptr;
        int r = avformat_alloc_output_context2 (&oc, nullptr, nullptr, p.output.getFullPathName().toRawUTF8());
        if (r < 0 || oc == nullptr) { error = "Can't create " + p.output.getFileName() + ": " + avErr (r); return false; }
        std::unique_ptr<AVFormatContext, void (*) (AVFormatContext*)> ocGuard (oc, [] (AVFormatContext* c)
        {
            if (c->pb != nullptr && ! (c->oformat->flags & AVFMT_NOFILE)) avio_closep (&c->pb);
            avformat_free_context (c);
        });

        const bool copyPicture = p.codec == Codec::sameAsSource;
        auto* srcStream = sim.fmt->streams[sim.vIndex];

        // --- picture stream
        AVStream* vst = avformat_new_stream (oc, nullptr);
        AVCodecContext* venc = nullptr;
        std::unique_ptr<AVCodecContext, void (*) (AVCodecContext*)> vencGuard (nullptr, [] (AVCodecContext* c) { avcodec_free_context (&c); });
        const AVRational fr = sim.frameRate;
        int outW = p.width > 0 ? p.width : si.width, outH = p.height > 0 ? p.height : si.height;
        outW &= ~1; outH &= ~1;
        AVPixelFormat encPix = AV_PIX_FMT_NONE;

        if (copyPicture)
        {
            avcodec_parameters_copy (vst->codecpar, srcStream->codecpar);
            vst->codecpar->codec_tag = 0;
            vst->time_base = srcStream->time_base;
            vst->avg_frame_rate = fr;
        }
        else
        {
            auto choice = encoderFor (p.codec);
            const AVCodec* enc = choice.name != nullptr ? avcodec_find_encoder_by_name (choice.name) : nullptr;
            if (enc == nullptr) { error = juce::String (choice.name != nullptr ? choice.name : "?") + " encoder not available on this system"; return false; }

            venc = avcodec_alloc_context3 (enc);
            vencGuard.reset (venc);
            venc->width = outW; venc->height = outH;
            venc->time_base = av_inv_q (fr);
            venc->framerate = fr;
            venc->pix_fmt = encPix = choice.pix;
            venc->sample_aspect_ratio = srcStream->codecpar->sample_aspect_ratio;
            venc->color_range = AVCOL_RANGE_MPEG;
            venc->colorspace = outH >= 720 ? AVCOL_SPC_BT709 : AVCOL_SPC_SMPTE170M;
            venc->color_primaries = outH >= 720 ? AVCOL_PRI_BT709 : AVCOL_PRI_SMPTE170M;
            venc->color_trc = outH >= 720 ? AVCOL_TRC_BT709 : AVCOL_TRC_SMPTE170M;
            venc->thread_count = 0;
            if (oc->oformat->flags & AVFMT_GLOBALHEADER) venc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            AVDictionary* opts = nullptr;
            av_dict_set (&opts, choice.option, choice.value, 0);
            if (p.codec == Codec::h264 || p.codec == Codec::hevc || p.codec == Codec::hevc10)
            {
                const double pixelsPerSec = (double) outW * outH * av_q2d (fr);
                const double bitsPerPixel = p.codec == Codec::h264 ? 0.20 : 0.12;    // ≈ 25 Mb/s at 1080p25 H.264
                venc->bit_rate = (int64_t) (pixelsPerSec * bitsPerPixel);
                venc->gop_size = (int) std::ceil (av_q2d (fr));                      // 1 s GOP: scrubbable
                if (p.codec != Codec::h264) vst->codecpar->codec_tag = MKTAG ('h', 'v', 'c', '1');
            }
            if (p.codec >= Codec::dnxhrLB && p.codec <= Codec::dnxhr444) venc->bit_rate = 0;

            r = avcodec_open2 (venc, enc, &opts);
            av_dict_free (&opts);
            if (r < 0) { error = "Encoder setup failed (" + juce::String (enc->name) + "): " + avErr (r); return false; }
            avcodec_parameters_from_context (vst->codecpar, venc);
            if (p.codec == Codec::hevc || p.codec == Codec::hevc10) vst->codecpar->codec_tag = MKTAG ('h', 'v', 'c', '1');
            vst->time_base = venc->time_base;
            vst->avg_frame_rate = fr;
        }

        if (p.timecode.isNotEmpty())
        {
            av_dict_set (&vst->metadata, "timecode", p.timecode.toRawUTF8(), 0);
            av_dict_set (&oc->metadata, "timecode", p.timecode.toRawUTF8(), 0);
        }

        // --- audio stream
        const int nch = (int) wav->numChannels;
        const int inRate = (int) wav->sampleRate;
        const int outRate = isMxf ? 48000 : inRate;
        const char* aName = isMp4 ? "aac" : (p.audio == AudioFormat::pcm32f && ! isMxf ? "pcm_f32le" : "pcm_s24le");
        const AVCodec* aCodec = avcodec_find_encoder_by_name (aName);
        if (aCodec == nullptr) { error = juce::String (aName) + " encoder missing"; return false; }

        AVStream* ast = avformat_new_stream (oc, nullptr);
        AVCodecContext* aenc = avcodec_alloc_context3 (aCodec);
        std::unique_ptr<AVCodecContext, void (*) (AVCodecContext*)> aencGuard (aenc, [] (AVCodecContext* c) { avcodec_free_context (&c); });
        av_channel_layout_default (&aenc->ch_layout, nch);
        aenc->sample_rate = outRate;
        aenc->sample_fmt = isMp4 ? AV_SAMPLE_FMT_FLTP : (std::strcmp (aName, "pcm_f32le") == 0 ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S32);
        aenc->time_base = { 1, outRate };
        if (isMp4) aenc->bit_rate = 160000 * juce::jmax (1, nch);   // 320 kb/s stereo
        if (std::strcmp (aName, "pcm_s24le") == 0) aenc->bits_per_raw_sample = 24;
        if (oc->oformat->flags & AVFMT_GLOBALHEADER) aenc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        if ((r = avcodec_open2 (aenc, aCodec, nullptr)) < 0) { error = "Audio encoder: " + avErr (r); return false; }
        avcodec_parameters_from_context (ast->codecpar, aenc);
        ast->time_base = aenc->time_base;

        SwrContext* swr = nullptr;
        AVChannelLayout inLayout;
        av_channel_layout_default (&inLayout, nch);
        swr_alloc_set_opts2 (&swr, &aenc->ch_layout, aenc->sample_fmt, outRate, &inLayout, AV_SAMPLE_FMT_FLTP, inRate, 0, nullptr);
        if (swr == nullptr || swr_init (swr) < 0) { swr_free (&swr); error = "Audio converter setup failed"; return false; }
        std::unique_ptr<SwrContext, void (*) (SwrContext*)> swrGuard (swr, [] (SwrContext* s) { swr_free (&s); });

        if (! (oc->oformat->flags & AVFMT_NOFILE))
            if ((r = avio_open (&oc->pb, p.output.getFullPathName().toRawUTF8(), AVIO_FLAG_WRITE)) < 0)
            { error = "Can't write " + p.output.getFullPathName() + ": " + avErr (r); return false; }

        if ((r = avformat_write_header (oc, nullptr)) < 0) { error = "Container header: " + avErr (r); return false; }

        PacketPtr pkt (av_packet_alloc());
        FramePtr vframe (av_frame_alloc()), aframe (av_frame_alloc());

        auto drain = [&] (AVCodecContext* enc, AVStream* st) -> bool
        {
            for (;;)
            {
                const int rr = avcodec_receive_packet (enc, pkt.get());
                if (rr == AVERROR (EAGAIN) || rr == AVERROR_EOF) return true;
                if (rr < 0) { error = "Encoding: " + avErr (rr); return false; }
                av_packet_rescale_ts (pkt.get(), enc->time_base, st->time_base);
                pkt->stream_index = st->index;
                if ((r = av_interleaved_write_frame (oc, pkt.get())) < 0) { error = "Write: " + avErr (r); return false; }
            }
        };

        // Audio: feed the encoder up to a given output time.
        const int aFrameSize = aenc->frame_size > 0 ? aenc->frame_size : 1024;
        juce::AudioBuffer<float> inBuf (nch, 4096);
        juce::int64 wavPos = 0, audioOutSamples = 0;
        const juce::int64 wavLen = wav->lengthInSamples;

        auto pushAudioUntil = [&] (double seconds, bool final) -> bool
        {
            const juce::int64 wantIn = final ? wavLen : juce::jmin (wavLen, (juce::int64) std::llround (seconds * inRate));
            while (wavPos < wantIn || final)
            {
                int n = (int) juce::jmin ((juce::int64) inBuf.getNumSamples(), wantIn - wavPos);
                if (n > 0)
                {
                    wav->read (&inBuf, 0, n, wavPos, true, true);
                    wavPos += n;
                    const uint8_t* in[64] = {};
                    for (int ch = 0; ch < nch && ch < 64; ++ch) in[ch] = (const uint8_t*) inBuf.getReadPointer (ch);
                    if (swr_convert (swr, nullptr, 0, in, n) < 0) { error = "Audio conversion failed"; return false; }
                }
                else if (! final) break;

                const bool flushNow = final && n <= 0;
                while (swr_get_out_samples (swr, 0) >= aFrameSize || (flushNow && swr_get_out_samples (swr, 0) > 0))
                {
                    av_frame_unref (aframe.get());
                    aframe->format = aenc->sample_fmt;
                    av_channel_layout_copy (&aframe->ch_layout, &aenc->ch_layout);
                    aframe->sample_rate = outRate;
                    aframe->nb_samples = aFrameSize;
                    av_frame_get_buffer (aframe.get(), 0);
                    const int got = swr_convert (swr, aframe->data, aFrameSize, nullptr, 0);
                    if (got <= 0) break;
                    aframe->nb_samples = got;
                    aframe->pts = audioOutSamples;
                    audioOutSamples += got;
                    if (avcodec_send_frame (aenc, aframe.get()) < 0) { error = "Audio encode failed"; return false; }
                    if (! drain (aenc, ast)) return false;
                }
                if (flushNow) break;
            }
            return true;
        };

        const double rangeLen = juce::jmax (0.0, p.rangeEnd - p.rangeStart);
        bool ok = true;

        if (copyPicture)
        {
            // Copy every picture packet of the source (the export covers the whole video).
            AVFormatContext* in = nullptr;
            if ((r = avformat_open_input (&in, p.source.getFullPathName().toRawUTF8(), nullptr, nullptr)) < 0) { error = avErr (r); return false; }
            std::unique_ptr<AVFormatContext, void (*) (AVFormatContext*)> inGuard (in, [] (AVFormatContext* c) { avformat_close_input (&c); });
            avformat_find_stream_info (in, nullptr);
            const int idx = sim.vIndex;
            const auto start = in->streams[idx]->start_time != AV_NOPTS_VALUE ? in->streams[idx]->start_time : 0;
            PacketPtr inPkt (av_packet_alloc());          // separate from 'pkt', which the audio encoder drains into
            while (ok && av_read_frame (in, inPkt.get()) >= 0)
            {
                if (inPkt->stream_index == idx)
                {
                    const auto pts = inPkt->pts != AV_NOPTS_VALUE ? inPkt->pts : inPkt->dts;
                    const double t = (pts - start) * av_q2d (in->streams[idx]->time_base);
                    if (! pushAudioUntil (t, false)) { ok = false; break; }
                    if (inPkt->pts != AV_NOPTS_VALUE) inPkt->pts -= start;
                    if (inPkt->dts != AV_NOPTS_VALUE) inPkt->dts -= start;
                    av_packet_rescale_ts (inPkt.get(), in->streams[idx]->time_base, vst->time_base);
                    inPkt->stream_index = vst->index;
                    inPkt->pos = -1;
                    if ((r = av_interleaved_write_frame (oc, inPkt.get())) < 0) { error = "Write: " + avErr (r); ok = false; }
                    if (progress && ! progress (si.durationSec > 0 ? t / si.durationSec : 0)) { error = "Cancelled"; ok = false; }
                }
                av_packet_unref (inPkt.get());
            }
        }
        else
        {
            const juce::int64 numFrames = (juce::int64) std::llround (rangeLen * av_q2d (fr));
            SwsContext* sws = nullptr;
            std::unique_ptr<SwsContext, void (*) (SwsContext*)> swsGuard (nullptr, [] (SwsContext* s) { sws_freeContext (s); });

            // Black frame for time outside the picture.
            FramePtr black (av_frame_alloc());
            black->format = AV_PIX_FMT_BGRA; black->width = outW; black->height = outH;
            av_frame_get_buffer (black.get(), 0);
            for (int y = 0; y < outH; ++y)
            {
                auto* row = black->data[0] + y * black->linesize[0];
                for (int x = 0; x < outW; ++x) { row[4 * x] = row[4 * x + 1] = row[4 * x + 2] = 0; row[4 * x + 3] = 255; }
            }

            for (juce::int64 i = 0; i < numFrames && ok; ++i)
            {
                const double t = i / av_q2d (fr);
                const double srcTime = p.rangeStart + t - p.videoStart;
                AVFrame* sf = nullptr;
                if (srcTime >= 0 && srcTime < si.durationSec) sf = sim.decodeAt (srcTime);
                if (sf == nullptr) sf = black.get();

                sws = sws_getCachedContext (sws, sf->width, sf->height, (AVPixelFormat) sf->format, outW, outH, encPix,
                                            SWS_LANCZOS | SWS_ACCURATE_RND | SWS_FULL_CHR_H_INT, nullptr, nullptr, nullptr);
                swsGuard.release(); swsGuard.reset (sws);
                if (sws == nullptr) { error = "Scaler setup failed"; ok = false; break; }

                av_frame_unref (vframe.get());
                vframe->format = encPix; vframe->width = outW; vframe->height = outH;
                av_frame_get_buffer (vframe.get(), 0);
                sws_scale (sws, sf->data, sf->linesize, 0, sf->height, vframe->data, vframe->linesize);
                if (encPix == AV_PIX_FMT_YUVA444P10LE && sf->format != AV_PIX_FMT_YUVA444P10LE)
                    for (int y = 0; y < outH; ++y)
                    {
                        auto* a = (uint16_t*) (vframe->data[3] + y * vframe->linesize[3]);
                        for (int x = 0; x < outW; ++x) a[x] = 1023;
                    }
                vframe->pts = i;

                if (avcodec_send_frame (venc, vframe.get()) < 0) { error = "Picture encode failed"; ok = false; break; }
                if (! drain (venc, vst)) { ok = false; break; }
                if (! pushAudioUntil (t + 1.0 / av_q2d (fr), false)) { ok = false; break; }
                if (progress && ! progress ((double) (i + 1) / juce::jmax ((juce::int64) 1, numFrames))) { error = "Cancelled"; ok = false; }
            }

            if (ok)
            {
                avcodec_send_frame (venc, nullptr);
                ok = drain (venc, vst);
            }
        }

        if (ok) ok = pushAudioUntil (rangeLen, true);
        if (ok)
        {
            avcodec_send_frame (aenc, nullptr);
            ok = drain (aenc, ast);
        }
        if (ok && (r = av_write_trailer (oc)) < 0) { error = "Finalising: " + avErr (r); ok = false; }
        if (! ok) { ocGuard.reset(); p.output.deleteFile(); }
        return ok;
    }

    //==============================================================================
    static const juce::Identifier videoTree ("TKVIDEO");

    SessionVideo SessionVideo::get (const tracktion::Edit& e)
    {
        SessionVideo v;
        auto t = e.state.getChildWithName (videoTree);
        if (! t.isValid()) return v;
        v.file = juce::File (t.getProperty ("file").toString());
        v.start = (double) t.getProperty ("start", 0.0);
        v.syncOffsetMs = (double) t.getProperty ("offsetMs", 0.0);
        return v;
    }

    void SessionVideo::set (tracktion::Edit& e, const SessionVideo& v)
    {
        auto* um = &e.getUndoManager();
        auto t = e.state.getOrCreateChildWithName (videoTree, um);
        t.setProperty ("file", v.file.getFullPathName(), um);
        t.setProperty ("start", v.start, um);
        t.setProperty ("offsetMs", v.syncOffsetMs, um);
    }

    void SessionVideo::clear (tracktion::Edit& e)
    {
        auto t = e.state.getChildWithName (videoTree);
        if (t.isValid()) e.state.removeChild (t, &e.getUndoManager());
    }
}
