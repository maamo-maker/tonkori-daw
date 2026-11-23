#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "../video/Video.h"

namespace te = tracktion;

namespace tonkori::ui
{
    /** Decodes frames on its own thread for one display; the newest request wins. */
    class FrameSource : private juce::Thread
    {
    public:
        explicit FrameSource (const juce::File& f) : juce::Thread ("Tonkori video"), file (f) { startThread (juce::Thread::Priority::high); }
        ~FrameSource() override { stopThread (2000); }

        void request (double seconds, int w, int h)
        {
            wanted.store (seconds);
            wantW = w; wantH = h;
            notify();
        }

        juce::Image latest()        { const juce::ScopedLock sl (lock); return image; }
        std::function<void()> onFrame;
        tonkori::video::Info info() { const juce::ScopedLock sl (lock); return infoCopy; }

    private:
        juce::File file;
        std::atomic<double> wanted { -1.0 };
        std::atomic<int> wantW { 960 }, wantH { 540 };
        juce::CriticalSection lock;
        juce::Image image;
        tonkori::video::Info infoCopy;

        void run() override
        {
            juce::String err;
            auto vf = tonkori::video::VideoFile::open (file, err);
            if (vf == nullptr) return;
            { const juce::ScopedLock sl (lock); infoCopy = vf->getInfo(); }
            juce::int64 lastFrame = -2;
            int lastW = 0;

            while (! threadShouldExit())
            {
                const double t = wanted.load();
                const auto frame = t >= 0 ? vf->frameIndexAt (t) : -1;
                if (t >= 0 && (frame != lastFrame || wantW != lastW))
                {
                    auto img = vf->frameAt (t, wantW, wantH);
                    lastFrame = frame; lastW = wantW;
                    { const juce::ScopedLock sl (lock); image = img; }
                    if (onFrame) juce::MessageManager::callAsync ([cb = onFrame] { cb(); });
                }
                wait (8);
            }
        }
    };

    /** Small frames for the picture track, decoded in the background and cached. */
    class ThumbnailCache : private juce::Thread
    {
    public:
        explicit ThumbnailCache (const juce::File& f) : juce::Thread ("Tonkori thumbnails"), file (f) { startThread (juce::Thread::Priority::low); }
        ~ThumbnailCache() override { stopThread (2000); }

        std::function<void()> onReady;

        juce::Image get (double seconds)
        {
            const auto key = (juce::int64) std::floor (seconds * 100.0);       // 10 ms buckets
            const juce::ScopedLock sl (lock);
            if (auto it = cache.find (key); it != cache.end()) return it->second;
            if (std::find (pending.begin(), pending.end(), key) == pending.end())
            {
                pending.push_back (key);
                if (pending.size() > 64) pending.erase (pending.begin());
                notify();
            }
            return {};
        }

    private:
        juce::File file;
        juce::CriticalSection lock;
        std::map<juce::int64, juce::Image> cache;
        std::vector<juce::int64> pending;

        void run() override
        {
            juce::String err;
            auto vf = tonkori::video::VideoFile::open (file, err);
            if (vf == nullptr) return;
            while (! threadShouldExit())
            {
                juce::int64 key = -1;
                {
                    const juce::ScopedLock sl (lock);
                    if (! pending.empty()) { key = pending.back(); pending.pop_back(); }
                }
                if (key < 0) { wait (100); continue; }
                auto img = vf->frameAt (key / 100.0 + 0.001, 160, 90);
                {
                    const juce::ScopedLock sl (lock);
                    if (cache.size() > 600) cache.clear();
                    cache[key] = img;
                }
                if (onReady) juce::MessageManager::callAsync ([cb = onReady] { cb(); });
            }
        }
    };

    //==============================================================================
    /** Floating picture window, locked to what you hear: the frame shown is the one at the
        playhead minus the output latency, plus the session's video sync offset. */
    class VideoPanel : public juce::Component, private juce::Timer
    {
    public:
        VideoPanel (te::Edit& e, const juce::File& f, std::function<double()> videoStart, std::function<double()> offsetMs,
                    std::function<te::TimePosition()> stoppedPosition)
            : edit (e), source (f), start (std::move (videoStart)), offset (std::move (offsetMs)), stopped (std::move (stoppedPosition))
        {
            source.onFrame = [safe = juce::Component::SafePointer<VideoPanel> (this)] { if (safe) safe->repaint(); };
            setSize (960, 540 + 24);
            startTimerHz (60);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::black);
            auto r = getLocalBounds();
            auto bar = r.removeFromBottom (24);
            auto img = source.latest();
            if (img.isValid())
                g.drawImage (img, r.toFloat(), juce::RectanglePlacement::centred);
            else
            {
                g.setColour (colours::textFaint);
                g.drawText (inRange ? "Decoding..." : "No picture at this position", r, juce::Justification::centred);
            }
            g.setColour (colours::panel);
            g.fillRect (bar);
            g.setColour (colours::text);
            g.setFont (mono (12.0f));
            g.drawText (status, bar.reduced (8, 0), juce::Justification::centredLeft);
        }

    private:
        te::Edit& edit;
        FrameSource source;
        std::function<double()> start, offset;
        std::function<te::TimePosition()> stopped;
        juce::String status;
        bool inRange = false;

        void timerCallback() override
        {
            auto& t = edit.getTransport();
            double pos = t.isPlaying() ? t.getPosition().inSeconds() : stopped().inSeconds();
            if (t.isPlaying())
                pos -= edit.engine.getDeviceManager().getOutputLatencySeconds();
            pos -= offset() / 1000.0;
            const double src = pos - start();
            const auto info = source.info();
            inRange = src >= 0 && src < info.durationSec;
            if (inRange) source.request (src, juce::jmax (64, getWidth()), juce::jmax (36, getHeight() - 24));

            const double fps = info.fps > 0 ? info.fps : 25.0;
            const auto frame = (juce::int64) std::floor (juce::jmax (0.0, src) * fps + 1e-6);
            const int nominal = juce::roundToInt (fps);
            const auto ff = frame % juce::jmax (1, nominal), secs = frame / juce::jmax (1, nominal);
            const juce::String tc = juce::String::formatted ("%02d:%02d:%02d:%02d", (int) (secs / 3600), (int) ((secs / 60) % 60), (int) (secs % 60), (int) ff);
            const juce::String s = (inRange ? "src " + tc + "   frame " + juce::String (frame) : juce::String ("--"))
                                   + "   " + juce::String (fps, 3).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " fps   "
                                   + info.codec + "  " + juce::String (info.width) + "x" + juce::String (info.height);
            if (s != status) { status = s; repaint (getLocalBounds().removeFromBottom (24)); }
        }
    };
}
