#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace tonkori::ui
{
    /** User inserts on a track = everything before the track's volume/pan and meter. */
    inline juce::Array<te::Plugin*> userPlugins (te::Track& t)
    {
        juce::Array<te::Plugin*> r;
        for (auto* p : t.pluginList)
            if (dynamic_cast<te::VolumeAndPanPlugin*> (p) == nullptr && dynamic_cast<te::LevelMeterPlugin*> (p) == nullptr)
                r.add (p);
        return r;
    }

    inline int insertIndexFor (te::Track& t)
    {
        int i = 0;
        for (auto* p : t.pluginList)
        {
            if (dynamic_cast<te::VolumeAndPanPlugin*> (p) || dynamic_cast<te::LevelMeterPlugin*> (p)) return i;
            ++i;
        }
        return i;
    }

    /** Built-in processors first, then every scanned AU/VST3 by manufacturer. */
    inline void showAddPluginMenu (te::Edit& edit, te::PluginList& list, std::function<int()> insertIndex,
                                   juce::Component* target, std::function<void()> onAdded = {})
    {
        struct BuiltIn { const char* name; const char* type; };
        static const BuiltIn builtIns[] = {
            { "EQ (4-band)",       te::EqualiserPlugin::xmlTypeName },
            { "Compressor",        te::CompressorPlugin::xmlTypeName },
            { "Reverb",            te::ReverbPlugin::xmlTypeName },
            { "Delay",             te::DelayPlugin::xmlTypeName },
            { "Chorus",            te::ChorusPlugin::xmlTypeName },
            { "Phaser",            te::PhaserPlugin::xmlTypeName },
            { "Low/High-pass",     te::LowPassPlugin::xmlTypeName },
            { "Pitch shift",       te::PitchShiftPlugin::xmlTypeName },
            { "Impulse response",  te::ImpulseResponsePlugin::xmlTypeName },
        };

        juce::PopupMenu m, builtIn;
        for (int i = 0; i < (int) std::size (builtIns); ++i)
            builtIn.addItem (1 + i, builtIns[i].name);
        m.addSubMenu ("Tonkori built-in", builtIn);

        auto& engine = edit.engine;
        auto types = engine.getPluginManager().knownPluginList.getTypes();
        if (types.isEmpty())
            m.addItem (-1, "No AU/VST3 scanned yet: File > Plug-in Manager", false, false);
        else
            juce::KnownPluginList::addToMenu (m, types, juce::KnownPluginList::sortByManufacturer);

        te::Edit* editPtr = &edit;
        te::PluginList* listPtr = &list;

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [editPtr, listPtr, insertIndex, types, onAdded] (int r)
        {
            if (r == 0) return;
            auto& edit = *editPtr;
            te::Plugin::Ptr p;

            if (r >= 1 && r <= (int) std::size (builtIns))
                p = edit.getPluginCache().createNewPlugin (builtIns[r - 1].type, {});
            else
            {
                const int idx = juce::KnownPluginList::getIndexChosenByMenu (types, r);
                if (idx >= 0) p = edit.getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, types[idx]);
            }

            if (p != nullptr)
            {
                edit.getUndoManager().beginNewTransaction ("Insert plugin");
                listPtr->insertPlugin (p, insertIndex(), nullptr);
                p->showWindowExplicitly();
                if (onAdded) onAdded();
            }
        });
    }

    inline void showAddPluginMenu (te::Track& track, juce::Component* target, std::function<void()> onAdded = {})
    {
        te::Track::Ptr t (&track);
        showAddPluginMenu (track.edit, track.pluginList, [t] { return t ? insertIndexFor (*t) : 0; }, target, std::move (onAdded));
    }

    inline void showPluginSlotMenu (te::Plugin& p, juce::Component* target, std::function<void()> onChange = {})
    {
        juce::PopupMenu m;
        m.addItem (1, "Open editor");
        m.addItem (2, "Bypass", true, ! p.isEnabled());
        m.addSeparator();
        m.addItem (3, "Remove");
        te::Plugin::Ptr ptr (&p);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [ptr, onChange] (int r)
        {
            if (ptr == nullptr) return;
            if (r == 1) ptr->showWindowExplicitly();
            if (r == 2) ptr->setEnabled (! ptr->isEnabled());
            if (r == 3) { ptr->edit.getUndoManager().beginNewTransaction ("Remove plugin"); ptr->deleteFromParent(); }
            if (r != 0 && onChange) onChange();
        });
    }
}
