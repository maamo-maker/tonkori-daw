#pragma once

#include "ClipFades.h"

/*  Take lanes and comping (Pro Tools playlists, Logic/Cubase take lanes).

    Every clip has a lane: 0 is the main (comp) lane, which is what plays; lanes 1..N
    hold takes. Take-lane clips are muted in the engine and only drawn when the track's
    lanes are shown. Each recording pass is copied to a new lane in full, and also lands
    on the main lane. Comping = choosing a time range on a lane: that part replaces the
    main lane there, with short crossfades at both comp edges. */

namespace tonkori::takes
{
    inline const juce::Identifier laneProp ("tkLane"), showLanesProp ("tkShowLanes"),
                                  fromLaneProp ("tkFromLane"), compXfadeProp ("tkCompXfadeMs");

    inline int laneOf (const te::Clip& c)              { return (int) c.state.getProperty (laneProp, 0); }
    inline bool isTake (const te::Clip& c)             { return laneOf (c) > 0; }
    inline bool lanesShown (const te::Track& t)        { return (bool) t.state.getProperty (showLanesProp, false); }
    inline void showLanes (te::Track& t, bool s)       { t.state.setProperty (showLanesProp, s, nullptr); }
    inline double compXfadeMs (const te::Edit& e)      { return (double) e.state.getProperty (compXfadeProp, 4.0); }

    inline juce::Array<te::Clip*> clipsOnLane (te::ClipTrack& t, int lane)
    {
        juce::Array<te::Clip*> r;
        for (auto* c : t.getClips())
            if (laneOf (*c) == lane) r.add (c);
        return r;
    }

    inline int numLanes (te::ClipTrack& t)
    {
        int n = 0;
        for (auto* c : t.getClips()) n = juce::jmax (n, laneOf (*c));
        return n;
    }

    inline void setLane (te::Clip& c, int lane)
    {
        auto* um = &c.edit.getUndoManager();
        c.state.setProperty (laneProp, lane, um);
        c.setMuted (lane > 0);
    }

    /** Removes main-lane material inside a range (split at the edges, delete inside). */
    inline void clearMainLane (te::ClipTrack& t, te::TimeRange r, const juce::Array<te::Clip*>& keep = {})
    {
        for (auto edge : { r.getStart(), r.getEnd() })
        {
            te::SelectableList toSplit;
            for (auto* c : clipsOnLane (t, 0))
                if (! keep.contains (c) && c->getPosition().time.contains (edge) && c->getPosition().getStart() != edge)
                    toSplit.add (c);
            if (! toSplit.isEmpty()) te::splitClips (toSplit, edge);
        }

        juce::Array<te::Clip*> doomed;
        for (auto* c : clipsOnLane (t, 0))
            if (! keep.contains (c) && r.contains (c->getPosition().time)) doomed.add (c);
        for (auto* c : doomed) c->removeFromParent();
    }

    /** Promotes [range] of a take-lane clip to the main lane, with comp crossfades. */
    inline te::Clip* promote (te::Clip& take, te::TimeRange range)
    {
        auto* track = dynamic_cast<te::ClipTrack*> (take.getTrack());
        if (track == nullptr) return nullptr;
        range = range.getIntersectionWith (take.getPosition().time);
        if (range.getLength() <= te::TimeDuration::fromSeconds (0.0005)) return nullptr;

        const auto xf = te::TimeDuration::fromSeconds (compXfadeMs (take.edit) / 1000.0);
        const auto half = xf * 0.5;

        clearMainLane (*track, range);

        auto copy = te::duplicateClip (take);
        if (copy == nullptr) return nullptr;
        setLane (*copy, 0);
        copy->state.setProperty (fromLaneProp, laneOf (take), &take.edit.getUndoManager());

        // Segment, extended by half a crossfade each side where the take allows.
        const auto tp = take.getPosition();
        const auto segStart = juce::jmax (tp.getStart(), range.getStart() - half);
        const auto segEnd   = juce::jmin (tp.getEnd(),   range.getEnd() + half);
        copy->setStart (segStart, true, false);
        copy->setEnd (segEnd, true);

        auto* seg = dynamic_cast<te::AudioClipBase*> (copy.get());
        const auto xfSpec = FadeSpec::fromString ("equalPower");

        // Neighbours on the main lane get extended into the comp edges and crossfaded.
        for (auto* c : clipsOnLane (*track, 0))
        {
            if (c == copy.get()) continue;
            auto* ac = dynamic_cast<te::AudioClipBase*> (c);
            if (ac == nullptr) continue;
            const auto p = ac->getPosition();

            if (std::abs ((p.getEnd() - range.getStart()).inSeconds()) < 1e-6)        // ends at comp start
            {
                const auto maxEnd = p.getStart() + (ac->getSourceLength() - p.getOffset());
                ac->setEnd (juce::jmin (maxEnd, range.getStart() + half), true);
                const auto ov = ac->getPosition().getEnd() - segStart;
                if (ov > te::TimeDuration() && seg != nullptr)
                {
                    fades::setSpec (*ac, false, xfSpec);  fades::setLength (*ac, false, ov);
                    fades::setSpec (*seg, true, xfSpec);  fades::setLength (*seg, true, ov);
                }
            }
            else if (std::abs ((p.getStart() - range.getEnd()).inSeconds()) < 1e-6)  // starts at comp end
            {
                const auto minStart = p.getStart() - p.getOffset();
                ac->setStart (juce::jmax (minStart, range.getEnd() - half), true, false);
                const auto ov = segEnd - ac->getPosition().getStart();
                if (ov > te::TimeDuration() && seg != nullptr)
                {
                    fades::setSpec (*ac, true, xfSpec);   fades::setLength (*ac, true, ov);
                    fades::setSpec (*seg, false, xfSpec); fades::setLength (*seg, false, ov);
                }
            }
        }

        take.edit.restartPlayback();
        return copy.get();
    }

    /** After a recording pass: each new clip is also kept, in full, on a new take lane.
        Loop recordings (one clip holding several takes) get one lane per take, and the
        last take plays. Main-lane material under the new take is replaced; anything there
        that did not come from a lane is moved to its own lane first, so nothing is lost. */
    inline void absorbRecording (te::Clip& newClip)
    {
        auto* track = dynamic_cast<te::ClipTrack*> (newClip.getTrack());
        if (track == nullptr) return;
        const auto range = newClip.getPosition().time;
        int next = numLanes (*track) + 1;

        // Preserve original (non-comp) material under the take.
        juce::Array<te::Clip*> keep { &newClip };
        bool movedOriginal = false;
        for (auto* c : clipsOnLane (*track, 0))
            if (c != &newClip && c->getPosition().time.overlaps (range) && ! c->state.hasProperty (fromLaneProp))
                if (auto d = te::duplicateClip (*c)) { setLane (*d, next); keep.add (d.get()); movedOriginal = true; }
        if (movedOriginal) ++next;

        clearMainLane (*track, range, keep);

        auto* wc = dynamic_cast<te::WaveAudioClip*> (&newClip);
        const int numTakes = wc != nullptr ? wc->getNumTakes (false) : 0;

        if (wc != nullptr && numTakes > 1)
        {
            for (int i = 0; i < numTakes; ++i)
            {
                wc->setCurrentTake (i);
                const auto f = wc->getAudioFile().getFile();
                if (auto lc = track->insertWaveClip (newClip.getName() + " take " + juce::String (i + 1), f, newClip.getPosition(), false))
                    setLane (*lc, next++);
            }
            wc->setCurrentTake (numTakes - 1);
        }
        else if (auto d = te::duplicateClip (newClip))
        {
            setLane (*d, next);
            d->setName (newClip.getName());
        }

        newClip.state.setProperty (fromLaneProp, numLanes (*track), &newClip.edit.getUndoManager());
        setLane (newClip, 0);
    }
}
