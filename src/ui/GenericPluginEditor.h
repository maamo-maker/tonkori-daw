#pragma once

#include <JuceHeader.h>
#include "Theme.h"

namespace te = tracktion;

/*  Editor for Tonkori's built-in processors (EQ, compressor, reverb, delay, ...), which
    have no GUI of their own: one row per parameter with a slider and a typed value,
    double-click resets, the bypass switch on top, and "Load IR..." for the convolver.
    Edits are undoable and heard live. */

namespace tonkori::ui
{
    class GenericPluginEditor : public te::Plugin::EditorComponent, private juce::Timer
    {
    public:
        explicit GenericPluginEditor (te::Plugin& p) : plugin (&p)
        {
            title.setText (p.getName(), juce::dontSendNotification);
            title.setFont (font (14.0f, true));
            title.setColour (juce::Label::textColourId, colours::text);
            addAndMakeVisible (title);

            bypass.setButtonText ("Bypass");
            bypass.setClickingTogglesState (true);
            bypass.setToggleState (! p.isEnabled(), juce::dontSendNotification);
            bypass.setColour (juce::TextButton::buttonOnColourId, colours::warn.darker (0.3f));
            bypass.onClick = [this] { if (plugin) plugin->setEnabled (! bypass.getToggleState()); };
            addAndMakeVisible (bypass);

            if (auto* ir = dynamic_cast<te::ImpulseResponsePlugin*> (&p))
            {
                loadIR.setButtonText ("Load IR...");
                loadIR.onClick = [this, ir]
                {
                    chooser = std::make_shared<juce::FileChooser> ("Impulse response", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
                    te::Plugin::Ptr keep (ir);
                    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                          [this, keep, ir] (const juce::FileChooser& fc)
                                          {
                                              if (fc.getResult().existsAsFile() && keep != nullptr)
                                              {
                                                  ir->loadImpulseResponse (fc.getResult());
                                                  irName.setText (fc.getResult().getFileName(), juce::dontSendNotification);
                                              }
                                          });
                };
                addAndMakeVisible (loadIR);
                irName.setFont (font (11.5f));
                irName.setColour (juce::Label::textColourId, colours::textDim);
                addAndMakeVisible (irName);
            }

            for (auto* param : p.getAutomatableParameters())
            {
                auto* row = rows.add (new Row (*param, p.edit.getUndoManager()));
                addAndMakeVisible (row);
            }

            setSize (420, 64 + rows.size() * 28 + (loadIR.isVisible() ? 30 : 0));
            startTimerHz (15);
        }

        bool allowWindowResizing() override                          { return false; }
        juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

        void paint (juce::Graphics& g) override { g.fillAll (colours::panel); }

        void resized() override
        {
            auto r = getLocalBounds().reduced (12, 8);
            auto top = r.removeFromTop (26);
            bypass.setBounds (top.removeFromRight (70));
            title.setBounds (top);
            r.removeFromTop (6);
            if (loadIR.isVisible())
            {
                auto row = r.removeFromTop (24);
                loadIR.setBounds (row.removeFromLeft (90));
                row.removeFromLeft (8);
                irName.setBounds (row);
                r.removeFromTop (6);
            }
            for (auto* row : rows) { row->setBounds (r.removeFromTop (24)); r.removeFromTop (4); }
        }

    private:
        struct Row : public juce::Component
        {
            Row (te::AutomatableParameter& p, juce::UndoManager& um) : param (&p), undo (um)
            {
                name.setText (p.getParameterName(), juce::dontSendNotification);
                name.setFont (font (12.0f));
                name.setColour (juce::Label::textColourId, colours::textDim);
                addAndMakeVisible (name);

                const auto range = p.getValueRange();
                slider.setSliderStyle (juce::Slider::LinearBar);
                slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                slider.setRange (range.getStart(), range.getEnd(), p.isDiscrete() ? 1.0 : 0.0);
                slider.setValue (p.getCurrentValue(), juce::dontSendNotification);
                if (auto d = p.getDefaultValue()) slider.setDoubleClickReturnValue (true, *d);
                slider.onDragStart = [this] { undo.beginNewTransaction ("Plug-in parameter"); };
                slider.onValueChange = [this] { if (param) param->setParameter ((float) slider.getValue(), juce::sendNotification); };
                addAndMakeVisible (slider);

                value.setFont (mono (11.5f));
                value.setJustificationType (juce::Justification::centredRight);
                value.setEditable (false, true, false);
                value.setColour (juce::Label::textColourId, colours::text);
                value.onTextChange = [this]
                {
                    if (param == nullptr) return;
                    undo.beginNewTransaction ("Plug-in parameter");
                    const auto r = param->getValueRange();
                    param->setParameter (juce::jlimit (r.getStart(), r.getEnd(), value.getText().getFloatValue()), juce::sendNotification);
                };
                addAndMakeVisible (value);
                refresh();
            }

            void refresh()
            {
                if (param == nullptr) return;
                if (! slider.isMouseButtonDown()) slider.setValue (param->getCurrentValue(), juce::dontSendNotification);
                if (! value.isBeingEdited()) value.setText (param->getCurrentValueAsStringWithLabel(), juce::dontSendNotification);
            }

            void resized() override
            {
                auto r = getLocalBounds();
                name.setBounds (r.removeFromLeft (130));
                value.setBounds (r.removeFromRight (90));
                slider.setBounds (r.reduced (4, 3));
            }

            te::AutomatableParameter::Ptr param;
            juce::UndoManager& undo;
            juce::Label name, value;
            juce::Slider slider;
        };

        te::Plugin::Ptr plugin;
        juce::Label title, irName;
        juce::TextButton bypass, loadIR;
        juce::OwnedArray<Row> rows;
        std::shared_ptr<juce::FileChooser> chooser;

        void timerCallback() override
        {
            for (auto* r : rows) r->refresh();
            if (plugin) bypass.setToggleState (! plugin->isEnabled(), juce::dontSendNotification);
        }
    };

    inline std::unique_ptr<te::Plugin::EditorComponent> createEditorFor (te::Plugin& p)
    {
        if (auto e = p.createEditor()) return e;
        if (dynamic_cast<te::ExternalPlugin*> (&p) != nullptr) return {};
        return std::make_unique<GenericPluginEditor> (p);
    }
}
