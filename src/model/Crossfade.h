#pragma once

#include "ClipFades.h"

/*  Crossfades between two adjacent clips on one track (Pyramix / Cubase crossfade editor).

    A crossfade is defined by
      - the splice point: the edit point between the outgoing (A) and incoming (B) clip,
      - its length,
      - its alignment around the splice: 0 = ends at the splice (pre), 0.5 = centred,
        1 = starts at the splice (post),
      - one curve for each side (linked or independent).
    Moving the splice point or changing the length moves only the clip boundaries;
    both clips stay sample-locked to the timeline (their content never slides). */

namespace tonkori::xfade
{
    inline const juce::Identifier spliceProp ("tkSplice"), alignProp ("tkXAlign");

    struct Pair { te::AudioClipBase* out = nullptr; te::AudioClipBase* in = nullptr; };

    /** Finds the clip that B crossfades from: the clip on the same track (and lane) whose end
        reaches B's start most closely, overlapping or touching. */
    inline te::AudioClipBase* findOutgoing (te::AudioClipBase& b, int lane = -1)
    {
        auto* ct = dynamic_cast<te::ClipTrack*> (b.getTrack());
        if (ct == nullptr) return nullptr;
        const auto bs = b.getPosition().getStart();
        te::AudioClipBase* best = nullptr;
        for (auto* c : ct->getClips())
            if (auto* a = dynamic_cast<te::AudioClipBase*> (c); a != nullptr && a != &b)
            {
                if (lane >= 0 && (int) a->state.getProperty ("tkLane", 0) != lane) continue;
                const auto p = a->getPosition();
                if (p.getStart() < bs && p.getEnd() >= bs - te::TimeDuration::fromSeconds (1e-6) && p.getEnd() < b.getPosition().getEnd())
                    if (best == nullptr || p.getEnd() > best->getPosition().getEnd()) best = a;
            }
        return best;
    }

    inline te::AudioClipBase* findIncoming (te::AudioClipBase& a, int lane = -1)
    {
        auto* ct = dynamic_cast<te::ClipTrack*> (a.getTrack());
        if (ct == nullptr) return nullptr;
        for (auto* c : ct->getClips())
            if (auto* b = dynamic_cast<te::AudioClipBase*> (c); b != nullptr && b != &a)
                if (findOutgoing (*b, lane) == &a) return b;
        return nullptr;
    }

    struct Params
    {
        double length = 0.010;          // seconds
        double align = 0.5;             // 0 pre, 0.5 centred, 1 post
        double splice = 0.0;            // timeline seconds
        FadeSpec outSpec = FadeSpec::fromString ("equalPower");
        FadeSpec inSpec  = FadeSpec::fromString ("equalPower");
    };

    /** Current crossfade parameters, read back from the clips. */
    inline Params read (const Pair& p)
    {
        Params r;
        const auto a = p.out->getPosition(), b = p.in->getPosition();
        const double start = b.getStart().inSeconds(), end = a.getEnd().inSeconds();
        r.length = juce::jmax (0.0, end - start);
        r.align = (double) p.in->state.getProperty (alignProp, 0.5);
        r.splice = p.in->state.hasProperty (spliceProp) ? (double) p.in->state.getProperty (spliceProp)
                                                         : start + r.length * (1.0 - r.align);
        r.outSpec = fades::getSpec (*p.out, false);
        r.inSpec  = fades::getSpec (*p.in, true);
        return r;
    }

    /** Material available around the splice, given each clip's source. */
    inline te::TimeRange limits (const Pair& p)
    {
        const auto a = p.out->getPosition(), b = p.in->getPosition();
        const auto aMaxEnd = a.getStart() + (p.out->getSourceLength() - a.getOffset());
        const auto bMinStart = b.getStart() - b.getOffset();
        return { juce::jmax (bMinStart, a.getStart()), juce::jmin (aMaxEnd, b.getEnd()) };
    }

    /** Applies the crossfade. Returns the length actually used (clamped by the handles). */
    inline double apply (const Pair& p, Params prm)
    {
        if (p.out == nullptr || p.in == nullptr) return 0.0;
        const auto lim = limits (p);
        const double minT = lim.getStart().inSeconds(), maxT = lim.getEnd().inSeconds();

        prm.align = juce::jlimit (0.0, 1.0, prm.align);
        prm.splice = juce::jlimit (minT, maxT, prm.splice);

        double len = juce::jmax (0.0, prm.length);
        // Room before / after the splice for this alignment.
        const double before = prm.splice - minT, after = maxT - prm.splice;
        if (prm.align < 1.0) len = juce::jmin (len, before / (1.0 - prm.align));
        if (prm.align > 0.0) len = juce::jmin (len, after / prm.align);

        const double start = prm.splice - len * (1.0 - prm.align);
        const double end   = start + len;

        p.out->setEnd (te::TimePosition::fromSeconds (end), true);
        p.in->setStart (te::TimePosition::fromSeconds (start), true, false);

        fades::setSpec (*p.out, false, prm.outSpec);
        fades::setSpec (*p.in, true, prm.inSpec);
        fades::setLength (*p.out, false, te::TimeDuration::fromSeconds (len));
        fades::setLength (*p.in, true, te::TimeDuration::fromSeconds (len));

        auto* um = &p.in->edit.getUndoManager();
        p.in->state.setProperty (spliceProp, prm.splice, um);
        p.in->state.setProperty (alignProp, prm.align, um);
        return len;
    }

    /** Combined gain of the two curves at fraction x of the crossfade (for the editor's sum display). */
    inline double sumGainDb (const Params& prm, double x, bool coherent)
    {
        const double gIn = prm.inSpec.gainAt (x), gOut = prm.outSpec.gainAt (1.0 - x);
        const double s = coherent ? (gIn + gOut) : std::sqrt (gIn * gIn + gOut * gOut);
        return juce::Decibels::gainToDecibels (s, -100.0);
    }
}
