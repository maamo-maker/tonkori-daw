#pragma once

#include <JuceHeader.h>
#include <map>
#include <mutex>
#include "FadeSpec.h"

namespace te = tracktion;

/*  Fade state for clips and the session, and the engine-side resolution of it.

    Per clip  : fade-in/out length (Tracktion's own fadeIn/fadeOut), numeric curve spec
                (tkFadeIn / tkFadeOut), and an on/off switch (tkFadesOn) that bypasses the
                fades without forgetting them.
    Per edit  : auto-fades (tkAutoFade, tkAutoFadeMs, tkAutoFadeSpec): a minimum edge fade
                on every clip, Pyramix/Cubase-style de-clicking, switchable live.

    Every setter restarts the playback graph, so changes are heard immediately while
    playing or recording. */
namespace tonkori::fades
{
    inline FadeSpec getSpec (const te::AudioClipBase& c, bool isIn)
    {
        const auto s = c.state.getProperty (isIn ? ids::fadeInSpec : ids::fadeOutSpec).toString();
        if (s.isNotEmpty()) return FadeSpec::fromString (s);

        // No Tonkori spec yet: describe Tracktion's own curve type.
        FadeSpec f;
        switch (isIn ? c.getFadeInType() : c.getFadeOutType())
        {
            case te::AudioFadeCurve::linear:  f.shape = FadeSpec::Shape::linear; break;
            case te::AudioFadeCurve::sCurve:  f.shape = FadeSpec::Shape::sCurve; f.param = 0.25; break;
            default:                          f.shape = FadeSpec::Shape::equalPower; break;
        }
        return f;
    }

    inline bool areFadesOn (const te::AudioClipBase& c)        { return (bool) c.state.getProperty (ids::fadesOn, true); }
    inline bool isAutoFadeOn (const te::Edit& e)               { return (bool) e.state.getProperty (ids::autoFade, false); }
    inline double getAutoFadeMs (const te::Edit& e)            { return (double) e.state.getProperty (ids::autoFadeMs, 3.0); }
    inline FadeSpec getAutoFadeSpec (const te::Edit& e)
    {
        const auto s = e.state.getProperty (ids::autoFadeSpec).toString();
        return s.isNotEmpty() ? FadeSpec::fromString (s) : FadeSpec::fromString ("sCurve 0.25");
    }

    //==============================================================================
    inline void restart (te::Edit& e)       { e.restartPlayback(); }

    inline void setSpec (te::AudioClipBase& c, bool isIn, const FadeSpec& f)
    {
        c.state.setProperty (isIn ? ids::fadeInSpec : ids::fadeOutSpec, f.toString(), &c.edit.getUndoManager());
        restart (c.edit);
    }

    inline void setLength (te::AudioClipBase& c, bool isIn, te::TimeDuration len)
    {
        const auto maxLen = c.getPosition().getLength();
        len = te::TimeDuration::fromSeconds (juce::jlimit (0.0, maxLen.inSeconds(), len.inSeconds()));
        if (isIn) c.setFadeIn (len); else c.setFadeOut (len);
        restart (c.edit);
    }

    inline void setFadesOn (te::AudioClipBase& c, bool on)
    {
        c.state.setProperty (ids::fadesOn, on, &c.edit.getUndoManager());
        restart (c.edit);
    }

    inline void setAutoFade (te::Edit& e, bool on, double ms, const FadeSpec* spec = nullptr)
    {
        auto* um = &e.getUndoManager();
        e.state.setProperty (ids::autoFade, on, um);
        e.state.setProperty (ids::autoFadeMs, juce::jlimit (0.0, 500.0, ms), um);
        if (spec != nullptr) e.state.setProperty (ids::autoFadeSpec, spec->toString(), um);
        restart (e);
    }

    //==============================================================================
    /** Curve tables are shared between clips with the same spec. */
    inline std::shared_ptr<const std::vector<float>> tableFor (const FadeSpec& f)
    {
        static std::mutex lock;
        static std::map<juce::String, std::shared_ptr<const std::vector<float>>> cache;

        const auto key = f.toString();
        std::scoped_lock sl (lock);

        if (auto it = cache.find (key); it != cache.end())
            return it->second;

        auto t = std::make_shared<const std::vector<float>> (f.makeTable());
        if (cache.size() > 512) cache.clear();
        cache[key] = t;
        return t;
    }

    /** Resolves what the engine should play for one side of a clip. */
    inline te::FadeOverride resolve (const te::AudioClipBase& c, bool isIn)
    {
        const bool hasSpec  = c.state.hasProperty (isIn ? ids::fadeInSpec : ids::fadeOutSpec);
        const bool fadesOn  = areFadesOn (c);
        const bool autoOn   = isAutoFadeOn (c.edit);

        if (! hasSpec && fadesOn && ! autoOn)
            return {};                                   // untouched: engine default (bit-identical path)

        const auto clipLen = c.getPosition().getLength();
        auto len = fadesOn ? (isIn ? c.getFadeIn() : c.getFadeOut()) : te::TimeDuration();
        auto spec = getSpec (c, isIn);

        if (autoOn)
        {
            const auto autoLen = te::TimeDuration::fromSeconds (getAutoFadeMs (c.edit) / 1000.0);
            if (len < autoLen)
            {
                len = autoLen;
                spec = getAutoFadeSpec (c.edit);
            }
        }

        // The two fades together can never exceed the clip.
        len = te::TimeDuration::fromSeconds (juce::jmin (len.inSeconds(), clipLen.inSeconds() * (autoOn ? 0.5 : 1.0)));

        te::FadeOverride o;
        o.valid = true;
        o.length = len;
        o.curve = tableFor (spec);
        return o;
    }
}
