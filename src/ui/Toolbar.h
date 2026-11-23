#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "Logo.h"
#include "ViewState.h"
#include "../model/ClipFades.h"

namespace tonkori::ui
{
    /** Command IDs the toolbar triggers (defined by MainComponent). */
    struct ToolbarCommands
    {
        juce::CommandID toStart, playStop, record, loop, punch, metronome, follow,
                        inspector, mixer, audioSettings, plugins, importAudio, exportAudio, autoFade;
    };

    //==============================================================================
    /** Two rows. Top: transport, counters, tempo/meter, panels. Bottom: tools, edit mode,
        grid, auto-fades, selection readout. Every control has a tooltip with its key. */
    class Toolbar : public juce::Component, private juce::Timer, private juce::ChangeListener
    {
    public:
        Toolbar (ViewState& v, juce::ApplicationCommandManager& cm, ToolbarCommands ids)
            : vs (v), commands (cm), cmd (ids)
        {
            auto wire = [this] (juce::Button& b, juce::CommandID id, const juce::String& tip)
            {
                b.setTooltip (tip);
                b.setWantsKeyboardFocus (false);
                b.onClick = [this, id] { commands.invokeDirectly (id, true); };
                addAndMakeVisible (b);
            };

            wire (toStart, cmd.toStart, "Insert marker to start (Home)");
            wire (play, cmd.playStop, "Play from insert marker / Stop (Space).  Shift+Space continues");
            wire (rec, cmd.record, "Record (F9)");
            wire (loop, cmd.loop, "Loop (Cmd+L).  Drag in the ruler's top strip to set the range");
            wire (punch, cmd.punch, "Punch in/out on the loop range (Cmd+Shift+P)");
            wire (click, cmd.metronome, "Metronome (O)");
            wire (follow, cmd.follow, "Follow playhead (Cmd+Shift+F)");

            for (auto* b : { &inspectorBtn, &mixerBtn, &audioBtn, &pluginsBtn, &importBtn, &exportBtn, &autoFadeBtn })
            {
                b->setWantsKeyboardFocus (false);
                addAndMakeVisible (*b);
            }
            inspectorBtn.setTooltip ("Inspector (Cmd+Opt+I)");
            mixerBtn.setTooltip ("Mixer (Cmd+Opt+M)");
            audioBtn.setTooltip ("Audio settings (Cmd+,)");
            pluginsBtn.setTooltip ("Plug-in manager: scan AU / VST3");
            importBtn.setTooltip ("Import audio or DSD (Cmd+Shift+O)");
            exportBtn.setTooltip ("Export audio (Cmd+Shift+R)");
            autoFadeBtn.setTooltip ("Session auto-fades: a minimum edge fade on every clip, live (Shift+F).  Length/shape in the Inspector");
            inspectorBtn.onClick = [this] { commands.invokeDirectly (cmd.inspector, true); };
            mixerBtn.onClick = [this] { commands.invokeDirectly (cmd.mixer, true); };
            audioBtn.onClick = [this] { commands.invokeDirectly (cmd.audioSettings, true); };
            pluginsBtn.onClick = [this] { commands.invokeDirectly (cmd.plugins, true); };
            importBtn.onClick = [this] { commands.invokeDirectly (cmd.importAudio, true); };
            exportBtn.onClick = [this] { commands.invokeDirectly (cmd.exportAudio, true); };
            autoFadeBtn.onClick = [this] { commands.invokeDirectly (cmd.autoFade, true); };

            // Counters
            counter.setFont (mono (22.0f, true));
            counter.setJustificationType (juce::Justification::centredLeft);
            counter.setColour (juce::Label::textColourId, colours::text);
            counter.setEditable (false, true, false);
            counter.setTooltip ("Click: change display  -  Double-click: type a position to locate");
            counter.onTextChange = [this]
            {
                if (auto t = vs.parse (counter.getText()))
                {
                    vs.insertMarker = *t;
                    vs.edit.getTransport().setPosition (*t);
                    vs.changed();
                }
            };
            counter.addMouseListener (this, false);
            addAndMakeVisible (counter);

            secondary.setFont (mono (12.0f));
            secondary.setColour (juce::Label::textColourId, colours::textDim);
            secondary.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (secondary);

            formatLabel.setFont (font (10.0f, true));
            formatLabel.setColour (juce::Label::textColourId, colours::textFaint);
            formatLabel.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (formatLabel);

            tempo.setFont (mono (14.0f, true));
            tempo.setEditable (false, true, false);
            tempo.setJustificationType (juce::Justification::centred);
            tempo.setTooltip ("Tempo - double-click to type");
            tempo.onTextChange = [this]
            {
                const double bpm = tempo.getText().getDoubleValue();
                if (bpm >= 20.0 && bpm <= 400.0) { vs.edit.getUndoManager().beginNewTransaction ("Tempo"); vs.edit.tempoSequence.getTempo (0)->setBpm (bpm); }
            };
            addAndMakeVisible (tempo);

            meterSig.setFont (mono (14.0f, true));
            meterSig.setJustificationType (juce::Justification::centred);
            meterSig.setTooltip ("Meter (edit in the Inspector with nothing selected)");
            addAndMakeVisible (meterSig);

            // Tools / modes (segmented)
            auto names = toolNames();
            const char* toolTips[] = { "Smart tool (1): top corners fade, edges trim, upper half selects time, lower half moves",
                                       "Range (2): select time across tracks", "Grab (3): move clips", "Trim (4): trim clip edges",
                                       "Fade (5): drag fades from either half; Opt-drag shapes the curve", "Split (6): click to split" };
            for (int i = 0; i < names.size(); ++i)
            {
                auto* b = tools.add (new juce::TextButton (names[i]));
                b->setRadioGroupId (101);
                b->setClickingTogglesState (true);
                b->setWantsKeyboardFocus (false);
                b->setTooltip (toolTips[i]);
                b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < names.size() - 1 ? juce::Button::ConnectedOnRight : 0));
                b->onClick = [this, i] { vs.tool = (Tool) i; vs.changed(); };
                addAndMakeVisible (b);
            }

            auto modes = editModeNames();
            const char* modeTips[] = { "Slip (Opt+1): free, sample-accurate", "Grid (Opt+2 / Cmd+4): snap to grid",
                                       "Shuffle (Opt+3): moves, trims and deletes close gaps", "Spot (Opt+4): type exact positions" };
            for (int i = 0; i < modes.size(); ++i)
            {
                auto* b = modeButtons.add (new juce::TextButton (modes[i]));
                b->setRadioGroupId (102);
                b->setClickingTogglesState (true);
                b->setWantsKeyboardFocus (false);
                b->setTooltip (modeTips[i]);
                b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < modes.size() - 1 ? juce::Button::ConnectedOnRight : 0));
                b->onClick = [this, i] { vs.mode = (EditMode) i; vs.changed(); };
                addAndMakeVisible (b);
            }

            for (auto& g : gridDivisions()) grid.addItem (g.name, grid.getNumItems() + 1);
            grid.setTooltip ("Grid value (Cmd+1 narrower, Cmd+2 wider)");
            grid.onChange = [this] { vs.gridIndex = grid.getSelectedItemIndex(); vs.changed(); };
            addAndMakeVisible (grid);

            triplet.setTooltip ("Triplet grid (Cmd+3)");
            triplet.setWantsKeyboardFocus (false);
            triplet.onClick = [this] { vs.triplet = ! vs.triplet; vs.changed(); };
            addAndMakeVisible (triplet);

            selection.setFont (mono (11.5f));
            selection.setColour (juce::Label::textColourId, colours::textDim);
            selection.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (selection);

            vs.addChangeListener (this);
            refresh();
            startTimerHz (20);
        }

        ~Toolbar() override { vs.removeChangeListener (this); }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (colours::panel);
            g.setColour (colours::line);
            g.drawHorizontalLine (topHeight, 0.0f, (float) getWidth());
            g.setColour (colours::lineStrong);
            g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

            drawTreeMark (g, logoArea.toFloat(), juce::Colour (0xfff5f0e8));

            // counter well
            g.setColour (colours::bg);
            g.fillRoundedRectangle (counterWell.toFloat(), 5.0f);
            g.setColour (colours::line);
            g.drawRoundedRectangle (counterWell.toFloat(), 5.0f, 1.0f);

            auto sep = [&] (juce::Component& c) { g.drawVerticalLine (c.getX() - 8, 8.0f, (float) topHeight - 8); };
            g.setColour (colours::line);
            sep (loop); sep (click);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto top = r.removeFromTop (topHeight).reduced (10, 7);
            auto bottom = r.reduced (10, 5);

            auto place = [] (juce::Rectangle<int>& row, juce::Component& c, int w, int gap = 4) { c.setBounds (row.removeFromLeft (w)); row.removeFromLeft (gap); };

            logoArea = top.removeFromLeft (26).reduced (0, 2);
            top.removeFromLeft (12);
            place (top, toStart, 34); place (top, play, 40); place (top, rec, 34, 16);
            place (top, loop, 34); place (top, punch, 34, 16);
            place (top, click, 34); place (top, follow, 34, 16);

            counterWell = top.removeFromLeft (300);
            top.removeFromLeft (12);
            auto cw = counterWell.reduced (10, 2);
            formatLabel.setBounds (cw.removeFromTop (11));
            auto cRow = cw;
            counter.setBounds (cRow.removeFromLeft (170));
            secondary.setBounds (cRow);

            tempo.setBounds (top.removeFromLeft (88)); top.removeFromLeft (4);
            meterSig.setBounds (top.removeFromLeft (44));

            pluginsBtn.setBounds (top.removeFromRight (68)); top.removeFromRight (4);
            audioBtn.setBounds (top.removeFromRight (56)); top.removeFromRight (12);
            mixerBtn.setBounds (top.removeFromRight (58)); top.removeFromRight (4);
            inspectorBtn.setBounds (top.removeFromRight (78));

            for (auto* b : tools) place (bottom, *b, 52, 0);
            bottom.removeFromLeft (14);
            for (auto* b : modeButtons) place (bottom, *b, 58, 0);
            bottom.removeFromLeft (14);
            place (bottom, grid, 78); place (bottom, triplet, 30, 14);
            place (bottom, autoFadeBtn, 116, 14);

            exportBtn.setBounds (bottom.removeFromRight (62)); bottom.removeFromRight (4);
            importBtn.setBounds (bottom.removeFromRight (62)); bottom.removeFromRight (12);
            selection.setBounds (bottom);
        }

        static constexpr int topHeight = 50, totalHeight = 86;

    private:
        ViewState& vs;
        juce::ApplicationCommandManager& commands;
        ToolbarCommands cmd;

        IconButton toStart { "start", icons::toStart }, play { "play", icons::play, colours::play },
                   rec { "rec", icons::record, colours::record }, loop { "loop", icons::loop }, punch { "punch", icons::punch, colours::record },
                   click { "click", icons::metronome }, follow { "follow", icons::follow };
        juce::TextButton inspectorBtn { "Inspector" }, mixerBtn { "Mixer" }, audioBtn { "Audio" }, pluginsBtn { "Plug-ins" },
                         importBtn { "Import" }, exportBtn { "Export" }, autoFadeBtn { "Auto-fade" }, triplet { "3" };
        juce::Label counter, secondary, formatLabel, tempo, meterSig, selection;
        juce::OwnedArray<juce::TextButton> tools, modeButtons;
        juce::ComboBox grid;
        juce::Rectangle<int> counterWell, logoArea;

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (e.eventComponent == &counter && e.getNumberOfClicks() == 1 && ! e.mouseWasDraggedSinceMouseDown())
            {
                vs.timeFormat = (TimeFormat) (((int) vs.timeFormat + 1) % 4);
                vs.secondaryFormat = vs.timeFormat == TimeFormat::barsBeats ? TimeFormat::timecode : TimeFormat::barsBeats;
                vs.changed();
            }
        }

        void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
        void timerCallback() override { refreshTransport(); }

        void refresh()
        {
            for (int i = 0; i < tools.size(); ++i) tools[i]->setToggleState ((int) vs.tool == i, juce::dontSendNotification);
            for (int i = 0; i < modeButtons.size(); ++i) modeButtons[i]->setToggleState ((int) vs.mode == i, juce::dontSendNotification);
            grid.setSelectedItemIndex (vs.gridIndex, juce::dontSendNotification);
            triplet.setToggleState (vs.triplet, juce::dontSendNotification);
            follow.setToggleState (vs.follow, juce::dontSendNotification);
            inspectorBtn.setToggleState (vs.showInspector, juce::dontSendNotification);
            mixerBtn.setToggleState (vs.showMixer, juce::dontSendNotification);

            const bool af = fades::isAutoFadeOn (vs.edit);
            autoFadeBtn.setToggleState (af, juce::dontSendNotification);
            autoFadeBtn.setButtonText ("Auto-fade " + juce::String (fades::getAutoFadeMs (vs.edit), 1) + " ms");

            static const char* names[] = { "BARS | BEATS", "TIMECODE", "MIN:SEC", "SAMPLES" };
            formatLabel.setText (juce::String (names[(int) vs.timeFormat])
                                   + (vs.timeFormat == TimeFormat::timecode ? "  " + fpsNames()[vs.fpsIndex] + " fps" : juce::String()),
                                 juce::dontSendNotification);

            if (vs.timeSelection)
                selection.setText ("Selection  " + vs.format (vs.timeSelection->getStart()) + "  >  " + vs.format (vs.timeSelection->getEnd())
                                     + "   (" + vs.formatDuration (vs.timeSelection->getLength()) + ")", juce::dontSendNotification);
            else
                selection.setText ("Insert  " + vs.format (vs.insertMarker), juce::dontSendNotification);
            refreshTransport();
        }

        void refreshTransport()
        {
            auto& t = vs.edit.getTransport();
            play.setToggleState (t.isPlaying(), juce::dontSendNotification);
            rec.setToggleState (t.isRecording(), juce::dontSendNotification);
            loop.setToggleState (t.looping.get(), juce::dontSendNotification);
            punch.setToggleState (vs.edit.recordingPunchInOut.get(), juce::dontSendNotification);
            click.setToggleState (vs.edit.clickTrackEnabled.get(), juce::dontSendNotification);

            // Live: when stopped, the position *is* the insert marker.
            const auto pos = t.isPlaying() ? t.getPosition() : vs.insertMarker;
            if (! counter.isBeingEdited()) counter.setText (vs.format (pos), juce::dontSendNotification);
            secondary.setText (vs.format (pos, vs.timeFormat == TimeFormat::barsBeats ? TimeFormat::timecode : TimeFormat::barsBeats), juce::dontSendNotification);
            if (! tempo.isBeingEdited()) tempo.setText (juce::String (vs.edit.tempoSequence.getBpmAt (pos), 2) + " bpm", juce::dontSendNotification);
            auto& ts = vs.edit.tempoSequence.getTimeSigAt (pos);
            meterSig.setText (juce::String (ts.numerator.get()) + "/" + juce::String (ts.denominator.get()), juce::dontSendNotification);
        }
    };
}
