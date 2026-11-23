#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

/*  Buses, sends and track outputs (Pro Tools aux tracks / Pyramix buses / Cubase FX channels).

    A bus is an audio track whose first processor is an Aux Return on bus number N.
    A send is an Aux Send on N placed before the track fader (pre) or after it (post).
    Any track — audio or bus — can output to the master, to a bus track as a subgroup,
    or straight to hardware outputs (e.g. MADI channels). Plug-in delay compensation
    covers every path, including sends into buses. */

namespace tonkori::routing
{
    inline const juce::Identifier busProp ("tkBus");

    inline te::AuxReturnPlugin* returnOf (te::Track& t)
    {
        for (auto* p : t.pluginList)
            if (auto* r = dynamic_cast<te::AuxReturnPlugin*> (p)) return r;
        return nullptr;
    }

    inline bool isBus (te::Track& t)       { return returnOf (t) != nullptr; }
    inline int busNumberOf (te::Track& t)  { if (auto* r = returnOf (t)) return r->busNumber.get(); return -1; }

    inline juce::Array<te::AudioTrack*> buses (te::Edit& e)
    {
        juce::Array<te::AudioTrack*> r;
        for (auto* t : te::getAudioTracks (e)) if (isBus (*t)) r.add (t);
        return r;
    }

    inline int nextFreeBus (te::Edit& e)
    {
        for (int n = 0; n < 32; ++n)
        {
            bool used = false;
            for (auto* b : buses (e)) used = used || busNumberOf (*b) == n;
            if (! used) return n;
        }
        return -1;
    }

    inline int faderIndex (te::Track& t)
    {
        int i = 0;
        for (auto* p : t.pluginList)
        {
            if (dynamic_cast<te::VolumeAndPanPlugin*> (p)) return i;
            ++i;
        }
        return i;
    }

    inline te::AudioTrack* createBus (te::Edit& e, juce::String name = {})
    {
        const int n = nextFreeBus (e);
        if (n < 0) return nullptr;
        if (name.isEmpty()) name = "Bus " + juce::String (n + 1);

        auto* last = te::getAudioTracks (e).getLast();
        auto t = e.insertNewAudioTrack (te::TrackInsertPoint (nullptr, last), nullptr);
        if (t == nullptr) return nullptr;
        t->setName (name);
        t->state.setProperty (busProp, n, nullptr);
        e.setAuxBusName (n, name);

        if (auto p = e.getPluginCache().createNewPlugin (te::AuxReturnPlugin::xmlTypeName, {}))
        {
            if (auto* r = dynamic_cast<te::AuxReturnPlugin*> (p.get())) r->busNumber = n;
            t->pluginList.insertPlugin (p, 0, nullptr);
        }
        return t.get();
    }

    struct Send { te::AuxSendPlugin* plugin; bool preFader; };

    inline std::vector<Send> sendsOf (te::Track& t)
    {
        std::vector<Send> r;
        const int fi = faderIndex (t);
        int i = 0;
        for (auto* p : t.pluginList)
        {
            if (auto* s = dynamic_cast<te::AuxSendPlugin*> (p)) r.push_back ({ s, i < fi });
            ++i;
        }
        return r;
    }

    inline te::AuxSendPlugin* findSend (te::Track& t, int bus)
    {
        for (auto& s : sendsOf (t)) if (s.plugin->busNumber.get() == bus) return s.plugin;
        return nullptr;
    }

    inline te::AuxSendPlugin* addSend (te::Track& t, int bus, bool preFader, float gainDb = 0.0f)
    {
        if (auto* existing = findSend (t, bus)) return existing;
        auto p = t.edit.getPluginCache().createNewPlugin (te::AuxSendPlugin::xmlTypeName, {});
        auto* s = dynamic_cast<te::AuxSendPlugin*> (p.get());
        if (s == nullptr) return nullptr;
        s->busNumber = bus;
        const int fi = faderIndex (t);
        t.pluginList.insertPlugin (p, preFader ? fi : fi + 1, nullptr);
        s->setGainDb (gainDb);
        return s;
    }

    inline void setPreFader (te::Track& t, te::AuxSendPlugin& s, bool pre)
    {
        te::Plugin::Ptr keep (&s);
        const float g = s.getGainDb();
        const int bus = s.busNumber.get();
        s.removeFromParent();
        if (auto* n = addSend (t, bus, pre, g)) juce::ignoreUnused (n);
        juce::ignoreUnused (keep);
    }

    //==============================================================================
    /** Output destinations: "Master", every bus track, and every hardware output. */
    struct Destination { juce::String label; te::AudioTrack* track = nullptr; juce::String device; bool master = false; };

    inline std::vector<Destination> destinationsFor (te::AudioTrack& t)
    {
        std::vector<Destination> r;
        r.push_back ({ "Master", nullptr, {}, true });
        for (auto* b : buses (t.edit))
            if (b != &t && ! b->getOutput().feedsInto (&t))
                r.push_back ({ b->getName(), b, {}, false });

        auto& dm = t.edit.engine.getDeviceManager();
        for (int i = 0; i < dm.getNumWaveOutDevices(); ++i)
            if (auto* d = dm.getWaveOutDevice (i); d != nullptr && d->isEnabled())
                r.push_back ({ "Out: " + d->getName(), nullptr, d->getDeviceID(), false });
        return r;
    }

    inline juce::String currentDestination (te::AudioTrack& t)
    {
        auto& o = t.getOutput();
        if (auto* d = o.getDestinationTrack()) return d->getName();
        if (o.usesDefaultAudioOut()) return "Master";
        return "Out: " + o.getOutputName();
    }

    inline void setDestination (te::AudioTrack& t, const Destination& d)
    {
        auto& o = t.getOutput();
        if (d.master)            o.setOutputToDefaultDevice (false);
        else if (d.track)        o.setOutputToTrack (d.track);
        else                     o.setOutputToDeviceID (d.device);
    }
}
