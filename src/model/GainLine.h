#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

/*  Clip gain line (Pro Tools clip gain / Cubase event envelope).
    Points live in the clip's state as TKGAIN/PT children, in clip-content seconds
    (time since the clip's source start), so they stay locked to the audio when the
    clip is moved, trimmed or split. Engine patch 0002 applies them after the fades,
    interpolated linearly in dB. A clip without points renders bit-identically. */

namespace tonkori::gainline
{
    inline const juce::Identifier tree ("TKGAIN"), point ("PT"), timeProp ("t"), dbProp ("db"), showProp ("tkShowGain");

    struct Point { double t; double db; };

    inline std::vector<Point> getPoints (const te::Clip& c)
    {
        std::vector<Point> r;
        auto g = c.state.getChildWithName (tree);
        for (auto p : g)
            if (p.hasType (point))
                r.push_back ({ (double) p.getProperty (timeProp), (double) p.getProperty (dbProp) });
        std::sort (r.begin(), r.end(), [] (auto& a, auto& b) { return a.t < b.t; });
        return r;
    }

    inline bool hasLine (const te::Clip& c)       { return c.state.getChildWithName (tree).getNumChildren() > 0; }

    inline void setPoints (te::Clip& c, std::vector<Point> pts)
    {
        auto* um = &c.edit.getUndoManager();
        std::sort (pts.begin(), pts.end(), [] (auto& a, auto& b) { return a.t < b.t; });
        auto g = c.state.getChildWithName (tree);
        if (! g.isValid())
        {
            g = juce::ValueTree (tree);
            c.state.appendChild (g, um);
        }
        g.removeAllChildren (um);
        for (auto& p : pts)
            g.appendChild (juce::ValueTree (point, { { timeProp, p.t }, { dbProp, juce::jlimit (-144.0, 24.0, p.db) } }), um);
        if (pts.empty())
            c.state.removeChild (g, um);
        c.edit.restartPlayback();
    }

    inline void clear (te::Clip& c)               { setPoints (c, {}); }

    /** Content time of a timeline position for this clip. */
    inline double contentTime (const te::Clip& c, te::TimePosition t)
    {
        const auto pos = c.getPosition();
        return (t - pos.getStart()).inSeconds() + pos.getOffset().inSeconds();
    }

    inline te::TimePosition timelineTime (const te::Clip& c, double content)
    {
        const auto pos = c.getPosition();
        return pos.getStart() + te::TimeDuration::fromSeconds (content - pos.getOffset().inSeconds());
    }

    /** dB at a content time (flat outside the points; 0 dB with no points). */
    inline double dbAt (const std::vector<Point>& pts, double t)
    {
        if (pts.empty())          return 0.0;
        if (t <= pts.front().t)   return pts.front().db;
        if (t >= pts.back().t)    return pts.back().db;
        for (size_t i = 0; i + 1 < pts.size(); ++i)
            if (t >= pts[i].t && t <= pts[i + 1].t)
            {
                const double span = pts[i + 1].t - pts[i].t;
                const double f = span > 0 ? (t - pts[i].t) / span : 0.0;
                return pts[i].db + (pts[i + 1].db - pts[i].db) * f;
            }
        return pts.back().db;
    }

    /** Engine envelope, rebuilt only when the points change. */
    inline std::shared_ptr<const te::ClipGainEnvelope> envelopeFor (const te::AudioClipBase& c)
    {
        auto g = c.state.getChildWithName (tree);
        if (! g.isValid() || g.getNumChildren() == 0) return {};

        static std::mutex lock;
        static std::map<juce::String, std::weak_ptr<const te::ClipGainEnvelope>> cache;

        auto pts = getPoints (c);
        juce::String key;
        for (auto& p : pts) key << p.t << ':' << p.db << ';';

        std::lock_guard<std::mutex> sl (lock);
        if (auto it = cache.find (key); it != cache.end())
            if (auto sp = it->second.lock()) return sp;

        auto env = std::make_shared<te::ClipGainEnvelope>();
        for (auto& p : pts)
            env->points.push_back ({ p.t, p.db <= -144.0 ? 0.0f : (float) juce::Decibels::decibelsToGain (p.db, -1000.0) });
        cache[key] = env;
        return env;
    }

    inline bool isShown (const te::Edit& e)       { return (bool) e.state.getProperty (showProp, true); }
    inline void setShown (te::Edit& e, bool s)    { e.state.setProperty (showProp, s, nullptr); }
}
