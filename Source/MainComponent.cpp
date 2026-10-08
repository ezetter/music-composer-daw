#include "MainComponent.h"

#include "Controls.h"

#include <algorithm>
#include <limits>

namespace
{
    constexpr int toolbarHeight = 44;
    constexpr int sidebarWidth = 280;
    constexpr int sidebarPadding = 14;
    constexpr int keyboardHeight = 140;
    constexpr int volumeColumnWidth = 64;
    constexpr int volumeDialSize = 44;
    constexpr int volumeTextHeight = 16;
    constexpr int deleteButtonSize = 22;        // to the right of the staves, in line with the measure buttons
    constexpr int addPartButtonSize = 28;       // in the space under the last part's staves, at the left
    constexpr int addPartBottom = 8;            // up from the bottom of the last part
    constexpr int measureButtonSize = 28;
    // The note length, dynamic and eraser buttons, over the top of the staves
    constexpr int toolButtonsTop = 8, toolButtonsHeight = 28;

    // Above the first system, so even when its chord buttons are as near its top as they go,
    // half a staff space below it, there's a gap between them and the buttons above
    constexpr int stavesTopPadding = toolButtonsTop + toolButtonsHeight + 10 - 6;
    constexpr int measureButtonsWidth = 84;     // the measure buttons, after the final barline
    constexpr int measureButtonsGap = 14;       // between the final barline and them
    constexpr int eraserMargin = 10;            // from the eraser to the scroll bar
    constexpr int editButtonWidth = 44, editButtonGap = 6;     // the Edit button, to the eraser's left
    constexpr int cloneButtonWidth = 56, cloneButtonHeight = 26;
    constexpr int rightMargin = measureButtonsGap + measureButtonsWidth + 12;      // after the final barline, for the measure buttons

    // A full 88-key piano, A0 to C8
    constexpr int lowestKey = 21;
    constexpr int highestKey = 108;
    constexpr int numWhiteKeys = 52;

    // Where Loop is kept in the settings
    const char* const loopKey = "loop";

    // And the metronome
    const char* const metronomeKey = "metronome";

    // The MIDI menu's items are numbered from here, in the order of the inputs they're for.
    constexpr int firstMidiInputItem = 1000;

    // The File menu's Open Recent items, numbered from here in the order of the recent scores
    constexpr int firstRecentScoreItem = 2000;
    constexpr int noRecentScoresItem = firstRecentScoreItem + 90;
    constexpr int clearRecentScoresItem = firstRecentScoreItem + 91;

    // Where a part's volume is kept in the settings, such as "volume1"
    juce::String getVolumeKey (int part)
    {
        return "volume" + juce::String (part + 1);
    }

    // And whether it's muted, such as "muted1"
    juce::String getMutedKey (int part)
    {
        return "muted" + juce::String (part + 1);
    }

    /** A number without trailing zeros, e.g. "120" or "92.5". */
    juce::String formatNumber (double number)
    {
        auto text = juce::String (number, 2);

        while (text.endsWithChar ('0'))
            text = text.dropLastCharacters (1);

        return text.trimCharactersAtEnd (".");
    }
}

//==============================================================================
MainComponent::StaffSystems::StaffSystems (Score& scoreToShow)
    : score (scoreToShow)
{
    // The + below the last part adds another part after it.
    addPartButton.setTooltip ("Add an instrument, with its own staves, below the last");
    addPartButton.onClick = [this] { if (onAddPart != nullptr) onAddPart(); };

    addPartButton.setLookAndFeel (&roundButtonLookAndFeel);
    addPartButton.setColour (juce::TextButton::textColourOffId, controls::accent);
    addPartButton.setWantsKeyboardFocus (false);
    addAndMakeVisible (addPartButton);

    layOut();
}

MainComponent::StaffSystems::~StaffSystems()
{
    addPartButton.setLookAndFeel (nullptr);
}

StaffView& MainComponent::StaffSystems::insertView (int index, int part)
{
    auto& view = **views.insert (views.begin() + index, std::make_unique<StaffView> (score, part));
    addAndMakeVisible (view);
    layOut();
    return view;
}

void MainComponent::StaffSystems::removeView (int index)
{
    views.erase (views.begin() + index);
    layOut();
}

void MainComponent::StaffSystems::setMinimumSize (int width, int height)
{
    minimumWidth = width;
    minimumHeight = height;
    layOut();
}

std::optional<int> MainComponent::StaffSystems::getPartAt (int y) const
{
    if (views.empty())
        return {};

    for (const auto& view : views)
        if (y < view->getBottom())
            return view->getPart();

    return views.back()->getPart();
}

void MainComponent::StaffSystems::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || onPartClicked == nullptr)
        return;

    if (const auto part = getPartAt (e.getPosition().y); part.has_value() && *part >= 0)
        onPartClicked (*part);
}

void MainComponent::StaffSystems::childBoundsChanged (juce::Component*)
{
    // A view grows or shrinks with its score.
    layOut();
}

void MainComponent::StaffSystems::layOut()
{
    if (layingOut || views.empty())
        return;

    const juce::ScopedValueSetter<bool> guard (layingOut, true);

    auto width = 0;

    for (auto& view : views)
        width = juce::jmax (width, view->getContentWidth());

    // Each system is as tall as its music needs, stacked from the top, under the note length
    // buttons, with any spare room below.
    auto y = stavesTopPadding;

    for (auto& view : views)
    {
        const auto height = view->getContentHeight();
        view->setBounds (0, y, width, height);
        y += height;
    }

    // The + for adding a part, below the last one's staves, at the left, in the room left under
    // them for low notes, so it takes no more height
    addPartButton.setBounds (12, y - addPartBottom - addPartButtonSize, addPartButtonSize, addPartButtonSize);
    addPartButton.toFront (false);
    y += 4;

    setSize (juce::jmax (width + rightMargin, minimumWidth), juce::jmax (y, minimumHeight));

    if (onLayoutChanged != nullptr)
        onLayoutChanged();
}

//==============================================================================
MainComponent::MainComponent (juce::PropertiesFile& settingsToUse)
    : settings (settingsToUse),
      document (score, settings)
{
    // Each part has its own instrument, which is saved with the score, sound and all.
    document.getInstrumentToSave = [this] (int part) { return tracks[(size_t) part].panel->saveToJSON(); };
    document.loadInstrument = [this] (int part, const juce::var& json) { tracks[(size_t) part].panel->loadFromJSON (json); };

    playButton.setButtonText ("Play");
    playButton.setTooltip ("Play the score, or stop it (space)");
    playButton.onClick = [this] { togglePlayback(); };
    playButton.addShortcut (juce::KeyPress (juce::KeyPress::spaceKey));

    // Record plays the score too, and writes what's played on the keyboard into the active part.
    recordButton.setButtonText ("Record");
    recordButton.setTooltip ("Play the score, and add what's played on the keyboard to the active instrument's staves");
    recordButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xfffbe2e0));
    recordButton.onClick = [this] { toggleRecording(); };

    // Loop is remembered from one run of the app to the next.
    playButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    recordButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    loopButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    loopButton.setClickingTogglesState (true);
    loopButton.setToggleState (settings.getBoolValue (loopKey), juce::dontSendNotification);
    loopButton.setColour (juce::TextButton::buttonOnColourId, controls::accentLight);
    loopButton.setColour (juce::TextButton::textColourOnId, controls::accent);
    loopButton.setTooltip ("Play the score over and over, rather than stopping at the end");
    loopButton.onClick = [this]
    {
        settings.setValue (loopKey, loopButton.getToggleState());
        instrumentHost.setLooping (loopButton.getToggleState());
    };

    // The metronome ticks each beat as the score plays or records, and is remembered too.
    metronomeButton.setButtonText ("Metronome");
    metronomeButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    metronomeButton.setClickingTogglesState (true);
    metronomeButton.setToggleState (settings.getBoolValue (metronomeKey), juce::dontSendNotification);
    metronomeButton.setColour (juce::TextButton::buttonOnColourId, controls::accentLight);
    metronomeButton.setTooltip ("Metronome: a clang on the first beat of each measure, and a click on the others, as the score plays or records");
    metronomeButton.onClick = [this]
    {
        settings.setValue (metronomeKey, metronomeButton.getToggleState());
        instrumentHost.setMetronome (metronomeButton.getToggleState());
    };
    instrumentHost.setMetronome (metronomeButton.getToggleState());

    tempoLabel.setText ("BPM", juce::dontSendNotification);
    tempoLabel.setFont (juce::FontOptions (12.5f));
    tempoLabel.setColour (juce::Label::textColourId, controls::secondaryText);
    tempoLabel.setJustificationType (juce::Justification::centredRight);

    tempoEditor.setInputRestrictions (6, "0123456789.");
    tempoEditor.setJustification (juce::Justification::centred);
    tempoEditor.setText (formatNumber (score.getBeatsPerMinute()), false);
    tempoEditor.setTooltip ("The tempo, in quarter notes per minute, from 20 to 300");
    tempoEditor.onTextChange = [this] { tempoEdited (false); };
    tempoEditor.onReturnKey = [this] { tempoEdited (true); tempoEditor.giveAwayKeyboardFocus(); };
    tempoEditor.onFocusLost = [this] { tempoEdited (true); };

    // The arrows beside it step it up and down, as the up and down arrow keys do while typing in
    // it. Holding an arrow down goes on stepping, and is undone in one go.
    tempoEditor.addKeyListener (this);
    tempoUpButton.setTooltip ("Faster");
    tempoDownButton.setTooltip ("Slower");

    for (auto [button, direction] : { std::pair { &tempoUpButton, 1 }, { &tempoDownButton, -1 } })
    {
        button->onClick = [this, step = direction] { stepTempo (step); };
        button->onPress = [this] { history.beginGesture(); };
        button->onRelease = [this] { history.endGesture(); };
        addAndMakeVisible (*button);
    }

    updateTempoArrows();


    // The active part, which the keyboard plays
    partBox.setTooltip ("The instrument the keyboard plays, and whose alternate staff and progression the sidebar sets");
    partBox.setWantsKeyboardFocus (false);
    partBox.onChange = [this] { if (partBox.getSelectedId() > 0) setActivePart (partBox.getSelectedId() - 1); };

    // Clicking a button shouldn't take the keyboard focus away from the piano, which the
    // computer keyboard can play too.
    playButton.setWantsKeyboardFocus (false);
    recordButton.setWantsKeyboardFocus (false);
    loopButton.setWantsKeyboardFocus (false);
    metronomeButton.setWantsKeyboardFocus (false);

    for (auto* component : std::initializer_list<juce::Component*> { &playButton, &recordButton, &loopButton, &metronomeButton, &tempoLabel, &tempoEditor, &partBox })
        addAndMakeVisible (component);

    staffSystems.onAddPart = [this] { addPart(); };

    // Clicking beside an instrument's staves, past the end of the score or to the left of them,
    // by its volume dial, makes it the active instrument, as clicking its staves does.
    staffSystems.onPartClicked = [this] (int part) { setActivePart (part); };
    volumeColumn.onClicked = [this] (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
            return;

        if (const auto part = staffSystems.getPartAt (staffSystems.getLocalPoint (&volumeColumn, e.getPosition()).y); part.has_value() && *part >= 0)
            setActivePart (*part);
    };

    // + adds a measure at the end and − takes the last one away, and Clone repeats every measure
    // after the last, in every part.
    addMeasureButton.setTooltip ("Add a measure at the end");
    cloneButton.setTooltip ("Repeat all the measures after the last one, with everything in them");
    addMeasureButton.onClick = [this] { addMeasure(); };
    removeMeasureButton.onClick = [this] { removeMeasure(); };
    cloneButton.onClick = [this] { cloneMeasures(); };
    cloneButton.setColour (juce::TextButton::buttonColourId, juce::Colours::white);
    cloneButton.setColour (juce::TextButton::textColourOffId, controls::accent);
    cloneButton.setWantsKeyboardFocus (false);

    for (auto* button : { &addMeasureButton, &removeMeasureButton })
    {
        button->setLookAndFeel (&roundButtonLookAndFeel);
        button->setColour (juce::TextButton::textColourOffId, controls::accent);
        button->setWantsKeyboardFocus (false);
    }
    // Note lengths for clicking into the staff, quarter notes to start with, or dynamics and hairpins to mark
    noteLengthPicker.onChange = [this] (double beats) { setNoteLength (beats); };
    dynamicPicker.onChange = [this] (std::optional<music::Marking> marking) { setMarking (marking); };
    eraserButton.onClick = [this] { setErasing (! eraserButton.getToggleState()); };
    editButton.setTooltip ("Edit notes: drag a note to move it, or click one to change its length or delete it");
    editButton.onClick = [this] { setEditing (! editButton.getToggleState()); };
    setNoteLength (1.0);

    scorePanel.onCopyProgression = [this] (int toPart) { copyProgression (toPart); };
    scorePanel.onScale = [this] (juce::Component& button) { showScalePanel (button); };
    sidebarContent.addAndMakeVisible (scorePanel);
    sidebar.setViewedComponent (&sidebarContent, false);
    sidebar.setScrollBarsShown (true, false);
    addAndMakeVisible (sidebar);

    staffViewport.setViewedComponent (&staffSystems, false);
    staffViewport.setScrollBarsShown (true, true);
    addAndMakeVisible (staffViewport);

    addAndMakeVisible (volumeColumn);
    addAndMakeVisible (noteLengthPicker);
    addAndMakeVisible (dynamicPicker);
    addAndMakeVisible (eraserButton);
    addAndMakeVisible (editButton);

    for (auto* component : std::initializer_list<juce::Component*> { &addMeasureButton, &removeMeasureButton, &cloneButton })
        staffSystems.addAndMakeVisible (component);

    updateRemoveMeasureButton();
    staffSystems.onLayoutChanged = [this] { positionVolumeDials(); positionMeasureButtons(); positionRuler.repaint(); };
    staffViewport.onScroll = [this] { positionVolumeDials(); positionMeasureButtons(); positionRuler.repaint(); };

    // The ruler follows the first part's staves, as they all line up, and where they've scrolled to.
    positionRuler.getView = [this] { return staffSystems.views.empty() ? nullptr : staffSystems.views[0].get(); };
    positionRuler.getScrolledX = [this] { return staffViewport.getViewPositionX(); };
    positionRuler.onBeatChosen = [this] (int beat) { choosePlaybackStart (beat); };
    addAndMakeVisible (positionRuler);

    // A track for each part of the new score, with the instruments, volumes and muting it had last time
    for (int part = 0; part < score.getNumParts(); ++part)
        insertTrack (part, true);

    numberTracks();
    updatePartBox();

    document.getVolumeToSave = [this] (int part) { return instrumentHost.getVolume (part); };
    document.loadVolume = [this] (int part, float decibels) { setVolume (part, decibels, false); };
    document.getMutedToSave = [this] (int part) { return instrumentHost.isMuted (part); };
    document.loadMuted = [this] (int part, bool muted) { setMuted (part, muted, false); };

    keyboard.setAvailableRange (lowestKey, highestKey);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setKeyPressBaseOctave (5);     // the computer keyboard's A key plays middle C
    keyboard.setScrollButtonsVisible (false);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (0xff4a8fe0));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour (0x264a8fe0));
    keyboard.onKeyClicked = [this] (int midiNote) { pianoKeyClicked (midiNote); };

    // A MIDI controller plays like the on-screen piano, including adding notes to a chord.
    midiInputs.onNoteOn = [this] (int midiNote) { pianoKeyClicked (midiNote); };
    midiInputs.onDevicesChanged = [this] { menuItemsChanged(); };
    addAndMakeVisible (keyboard);

    score.addChangeListener (this);
    document.addChangeListener (this);

    // The File and Edit menus, with their keyboard shortcuts. Undo and Redo are greyed out when
    // there's nothing to undo or redo.
    history.onChange = [this] { commandManager.commandStatusChanged(); };
    commandManager.registerAllCommandsForTarget (this);
    commandManager.setFirstCommandTarget (this);
    setApplicationCommandManagerToWatch (&commandManager);
    juce::MenuBarModel::setMacMainMenu (this);

    setActivePart (0);
    showPartTitles();
    setSize (1676, 944);    // wide enough for four measures of most music, and the buttons after them, and tall enough for two instruments

    juce::AudioDeviceManager::AudioDeviceSetup preferredSetup;
    preferredSetup.bufferSize = 256;    // small enough for the keyboard to feel immediate

    if (const auto error = audioDeviceManager.initialise (0, 2, nullptr, true, {}, &preferredSetup); error.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't open the audio output", error);

    audioDeviceManager.addAudioCallback (&instrumentHost);
}

MainComponent::~MainComponent()
{
    // The menu bar stops watching the command manager before the manager goes, which is before
    // the menu bar is.
    juce::MenuBarModel::setMacMainMenu (nullptr);
    setApplicationCommandManagerToWatch (nullptr);
    commandManager.setFirstCommandTarget (nullptr);

    // The dials' and buttons' look goes before they do.
    for (auto& track : tracks)
        track.dial->setLookAndFeel (nullptr);

    for (auto* button : { &addMeasureButton, &removeMeasureButton })
        button->setLookAndFeel (nullptr);

    tempoEditor.removeKeyListener (this);
    audioDeviceManager.removeAudioCallback (&instrumentHost);
    score.removeChangeListener (this);
    document.removeChangeListener (this);

    if (keyListenerTarget != nullptr)
    {
        keyListenerTarget->removeKeyListener (this);
        keyListenerTarget->removeKeyListener (commandManager.getKeyMappings());
    }
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (controls::sidebarBackground);
    g.fillRect (sidebar.getBounds());

    // The staff view doesn't cover the space for the scroll bar, so fill that in to match, and the
    // volume dials sit on the same paper.
    g.setColour (StaffView::paperColour);
    g.fillRect (staffViewport.getBounds());
    g.fillRect (volumeColumn.getBounds());
    g.fillRect (positionRuler.getBounds().withLeft (volumeColumn.getX()));

    g.setColour (juce::Colours::black.withAlpha (0.15f));
    g.fillRect (0, toolbarHeight - 1, getWidth(), 1);
    g.fillRect (sidebar.getRight(), toolbarHeight, 1, getHeight() - toolbarHeight);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    auto toolbar = bounds.removeFromTop (toolbarHeight).reduced (12, 8);
    playButton.setBounds (toolbar.removeFromLeft (46));
    recordButton.setBounds (toolbar.removeFromLeft (46));
    loopButton.setBounds (toolbar.removeFromLeft (44));
    metronomeButton.setBounds (toolbar.removeFromLeft (44));
    toolbar.removeFromLeft (16);
    tempoLabel.setBounds (toolbar.removeFromLeft (34));
    toolbar.removeFromLeft (4);
    tempoEditor.setBounds (toolbar.removeFromLeft (56));
    toolbar.removeFromLeft (2);
    auto arrows = toolbar.removeFromLeft (18);
    tempoUpButton.setBounds (arrows.removeFromTop (arrows.getHeight() / 2));
    tempoDownButton.setBounds (arrows);
    toolbar.removeFromLeft (20);
    partBox.setBounds (toolbar.removeFromLeft (150));
    toolbar.removeFromLeft (20);

    for (auto& track : tracks)
        track.panel->setBounds (toolbar);

    sidebar.setBounds (bounds.removeFromLeft (sidebarWidth));
    bounds.removeFromLeft (1);

    const auto contentWidth = sidebar.getMaximumVisibleWidth();
    const auto panelWidth = contentWidth - 2 * sidebarPadding;
    scorePanel.setBounds (sidebarPadding, sidebarPadding, panelWidth, scorePanel.getIdealHeight());
    sidebarContent.setSize (contentWidth, scorePanel.getBottom() + sidebarPadding);

    keyboard.setBounds (bounds.removeFromBottom (keyboardHeight));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / (float) numWhiteKeys);

    // The ruler goes along the top of the staves, the volume dials and staves under it.
    auto rulerRow = bounds.removeFromTop (PositionRuler::height);
    volumeColumn.setBounds (bounds.removeFromLeft (volumeColumnWidth));
    staffViewport.setBounds (bounds);
    positionRuler.setBounds (rulerRow.withLeft (staffViewport.getX()));

    // The note length and dynamic buttons stay in the top left corner of the score, over the
    // staves as they scroll, and the eraser in the top right corner, clear of the scroll bar.
    noteLengthPicker.setTopLeftPosition (volumeColumn.getX() + 10, staffViewport.getY() + toolButtonsTop);
    dynamicPicker.setTopLeftPosition (noteLengthPicker.getRight() + 14, noteLengthPicker.getY());
    eraserButton.setBounds (staffViewport.getRight() - staffViewport.getScrollBarThickness() - eraserMargin - EraserButton::buttonWidth,
                            noteLengthPicker.getY(), EraserButton::buttonWidth, EraserButton::buttonHeight);
    editButton.setBounds (eraserButton.getBounds().withWidth (editButtonWidth).translated (-editButtonWidth - editButtonGap, 0));

    staffSystems.setMinimumSize (staffViewport.getWidth() - staffViewport.getScrollBarThickness(),
                                 staffViewport.getHeight() - staffViewport.getScrollBarThickness());
    positionVolumeDials();
    positionMeasureButtons();
}

void MainComponent::parentHierarchyChanged()
{
    // Esc is listened for on the whole window, whichever control has the keyboard focus.
    auto* top = getTopLevelComponent();

    if (top == keyListenerTarget.getComponent())
        return;

    if (keyListenerTarget != nullptr)
    {
        keyListenerTarget->removeKeyListener (this);
        keyListenerTarget->removeKeyListener (commandManager.getKeyMappings());
    }

    keyListenerTarget = top;
    top->addKeyListener (this);
    top->addKeyListener (commandManager.getKeyMappings());   // the menu's shortcuts
    showDocumentTitle();
}

bool MainComponent::keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent)
{
    // Typing the tempo, the up and down arrow keys step it.
    if (originatingComponent == &tempoEditor && (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey))
    {
        stepTempo (key == juce::KeyPress::upKey ? 1 : -1);
        return true;
    }

    if (key == juce::KeyPress::escapeKey && placingScale)
    {
        stopPlacingScale();
        return true;
    }

    if (key == juce::KeyPress::escapeKey && getChordEditor() != nullptr)
    {
        closeChordEditor();
        return true;
    }

    return false;
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &document)
    {
        // The file, whether there are unsaved changes, or the recent scores have changed.
        showDocumentTitle();
        menuItemsChanged();
        return;
    }

    // Parts may have been added or taken out, or a whole new score loaded.
    updateTracks();
    updateRemoveMeasureButton();

    // The tempo can change by loading a score, so show it unless it's being typed.
    if (! tempoEditor.hasKeyboardFocus (false))
        tempoEditor.setText (formatNumber (score.getBeatsPerMinute()), false);

    updateTempoArrows();

    // Where the score plays from stays within it, as measures come and go and the time signature changes.
    playbackStart = juce::jlimit (0.0, (double) (score.getNumMeasures() * score.getBeatsPerMeasure() - 1), playbackStart);
    positionRuler.repaint();

    // Changes to the score are heard as soon as playback reaches them. If the repeats have
    // changed what's played when, it carries on from the same place in the same time through.
    if (const auto played = instrumentHost.getPlaybackPosition())
    {
        const auto previousOrder = performanceOrder;
        const auto beatsPerMeasure = score.getBeatsPerMeasure();
        const auto index = juce::jlimit (0, juce::jmax (0, (int) previousOrder.size() - 1), (int) std::floor (*played / beatsPerMeasure));
        updatePerformance();

        if (performanceOrder != previousOrder && ! previousOrder.empty())
        {
            // The same time through the measure it's in, or the last time it's played now
            const auto measure = previousOrder[(size_t) index];
            const auto timesBefore = (size_t) std::count (previousOrder.begin(), previousOrder.begin() + index, measure);
            std::vector<size_t> times;

            for (size_t i = 0; i < performanceOrder.size(); ++i)
                if (performanceOrder[i] == measure)
                    times.push_back (i);

            const auto found = times.empty() ? (size_t) 0 : times[juce::jmin (timesBefore, times.size() - 1)];
            const auto beat = (double) found * beatsPerMeasure + (*played - index * beatsPerMeasure);
            instrumentHost.updatePlaying (performance, 0, performance.getNumMeasures() - 1, beat);
        }
        else
        {
            instrumentHost.updatePlaying (performance, 0, performance.getNumMeasures() - 1);
        }
    }

    showPlaybackPosition();

    showHeldNotes();
}

//==============================================================================
void MainComponent::setActivePart (int part)
{
    activePart = part;
    activePartId = score.getPartId (part);
    instrumentHost.setActivePart (part);
    scorePanel.setPart (part);
    partBox.setSelectedId (part + 1, juce::dontSendNotification);

    for (size_t p = 0; p < tracks.size(); ++p)
    {
        tracks[p].panel->setVisible ((int) p == part);
        staffSystems.views[p]->setActive ((int) p == part);
    }

    // The chord window is for the active part's chords.
    if (const auto* editor = getChordEditor(); editor != nullptr && editor->getPart() != part)
        closeChordEditor();
}

void MainComponent::updatePartBox()
{
    partBox.clear (juce::dontSendNotification);

    for (int part = 0; part < score.getNumParts(); ++part)
        partBox.addItem ("Instrument " + juce::String (part + 1), part + 1);

    partBox.setSelectedId (activePart + 1, juce::dontSendNotification);
}

void MainComponent::insertTrack (int index, bool reloadFromSettings)
{
    // The part's found by its id in what the track's controls do, as its number can change.
    const auto id = score.getPartId (index);
    Track track { id, std::make_unique<InstrumentPanel> (instrumentHost, index, settings, reloadFromSettings),
                  std::make_unique<controls::ClickableDial>(), std::make_unique<controls::BinButton>() };

    auto& panel = *track.panel;
    panel.onInstrumentChanged = [this] { document.changed(); };
    panel.onStatusChanged = [this] { showPartTitles(); };
    addChildComponent (panel);

    // A volume dial beside the part's staves, from off up to +6 dB. Clicking it without turning it
    // mutes the part, or unmutes it. Both are remembered from one run of the app to the next, and
    // saved with the score.
    auto& dial = *track.dial;

    // The text comes first, so the dial shows it from the start.
    dial.textFromValueFunction = [this, id] (double decibels)
    {
        if (const auto part = score.findPart (id); part >= 0 && part < instrumentHost.getNumParts() && instrumentHost.isMuted (part))
            return juce::String ("Muted");

        if (decibels <= InstrumentHost::minVolume)
            return juce::String ("Off");

        return (decibels > 0.05 ? "+" : "") + juce::String (std::abs (decibels) < 0.05 ? 0.0 : decibels, 1) + " dB";
    };
    dial.valueFromTextFunction = [] (const juce::String& text)
    {
        return text.trim().equalsIgnoreCase ("off") ? (double) InstrumentHost::minVolume : text.getDoubleValue();
    };

    dial.setLookAndFeel (&dialLookAndFeel);
    dial.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    dial.setTextBoxStyle (juce::Slider::TextBoxBelow, true, volumeColumnWidth, volumeTextHeight);
    dial.setRange (InstrumentHost::minVolume, InstrumentHost::maxVolume, 0.1);
    dial.setSkewFactorFromMidPoint (-12.0);
    dial.setWantsKeyboardFocus (false);
    dial.setColour (juce::Slider::textBoxTextColourId, controls::secondaryText);
    dial.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    dial.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

    // Turning a muted dial unmutes it, rather than changing a volume that can't be heard.
    dial.onValueChange = [this, id, &dial]
    {
        const auto part = score.findPart (id);

        if (part < 0)
            return;

        if (instrumentHost.isMuted (part))
            setMuted (part, false, true);

        setVolume (part, (float) dial.getValue(), true);
    };
    dial.onClick = [this, id]
    {
        if (const auto part = score.findPart (id); part >= 0)
            setMuted (part, ! instrumentHost.isMuted (part), true);
    };
    volumeColumn.addAndMakeVisible (dial);

    // And under it, a button for taking the part out
    auto& deleteButton = *track.deleteButton;
    deleteButton.onClick = [this, id]
    {
        if (const auto part = score.findPart (id); part >= 0)
            deletePart (part);
    };
    staffSystems.addAndMakeVisible (deleteButton);

    tracks.insert (tracks.begin() + index, std::move (track));

    // Clicking the part's staff, or its chord buttons, makes it the active part.
    auto& view = staffSystems.insertView (index, index);
    view.onClicked = [this, &view] { setActivePart (view.getPart()); };
    view.onChordButtonClicked = [this, &view] (int measure) { editChord (view.getPart(), measure); };

    // An eraser stroke is undone in one go, however much it takes out.
    view.onEraseStarted = [this] { history.beginGesture(); };
    view.onEraseFinished = [this] { history.endGesture(); };

    // Adding from the scale is done once the notes have gone in.
    view.onScalePlaced = [this] { stopPlacingScale(); };

    // It does what clicks on the other parts do.
    view.setNoteLength (noteLengthPicker.getLength());

    if (eraserButton.getToggleState())
        view.setErasing (true);
    else if (const auto marking = dynamicPicker.getChoice())
        view.setMarking (marking);

    view.setEditing (editButton.getToggleState());

    // A part being added starts at 0 dB, rather than with whatever its number had before.
    setVolume (index, reloadFromSettings ? (float) settings.getDoubleValue (getVolumeKey (index), 0.0) : 0.0f, false);
    setMuted (index, reloadFromSettings && settings.getBoolValue (getMutedKey (index), false), false);
}

void MainComponent::removeTrack (int index)
{
    // Its instrument isn't kept in the settings, as another part will have its number.
    tracks[(size_t) index].panel->forgetInstrument();
    tracks.erase (tracks.begin() + index);
    staffSystems.removeView (index);
}

void MainComponent::updateTracks()
{
    // Tracks are matched to parts by their ids. One whose part has gone, or come before it, goes;
    // a part without one gets a new one, without an instrument. The instruments follow along.
    auto firstChange = std::numeric_limits<int>::max();

    for (int index = 0; index < juce::jmax (score.getNumParts(), (int) tracks.size());)
    {
        if (index < (int) tracks.size() && index < score.getNumParts() && tracks[(size_t) index].partId == score.getPartId (index))
        {
            ++index;
            continue;
        }

        firstChange = juce::jmin (firstChange, index);

        if (index < (int) tracks.size() && score.findPart (tracks[(size_t) index].partId) < index)
        {
            removeTrack (index);
            instrumentHost.removePart (index);
            continue;
        }

        instrumentHost.insertPart (index);
        insertTrack (index, false);
        ++index;
    }

    if (firstChange == std::numeric_limits<int>::max())
        return;

    // The tracks after the change have new numbers.
    numberTracks();

    // A chord being edited in a part that's moved, or gone, can't be any more.
    if (const auto* editor = getChordEditor(); editor != nullptr && editor->getPart() >= firstChange)
        closeChordEditor();

    // The active part stays active, wherever it is now, or the one after it takes over.
    const auto active = score.findPart (activePartId);
    activePart = active >= 0 ? active : juce::jmin (activePart, score.getNumParts() - 1);
    updatePartBox();
    setActivePart (activePart);

    showPartTitles();
    saveVolumes();
    resized();
}

void MainComponent::numberTracks()
{
    for (size_t index = 0; index < tracks.size(); ++index)
    {
        tracks[index].panel->setPart ((int) index);
        tracks[index].dial->setTooltip ("Instrument " + juce::String (index + 1) + "'s volume. Drag to turn it, or click to mute or unmute.");
        tracks[index].deleteButton->setTooltip ("Delete instrument " + juce::String (index + 1) + ", with its staves");
        tracks[index].deleteButton->setEnabled (tracks.size() > 1);
        staffSystems.views[index]->followPart();
    }
}

void MainComponent::addPart()
{
    const auto part = score.addPart();
    setActivePart (part);

    // Scrolled down far enough to see it, and the + under it
    const auto area = staffSystems.views[(size_t) part]->getBounds();
    const auto viewArea = staffViewport.getViewArea();

    if (area.getBottom() > viewArea.getBottom())
        staffViewport.setViewPosition (viewArea.getX(), area.getBottom() + 4 - viewArea.getHeight());
}

void MainComponent::deletePart (int part)
{
    if (score.getNumParts() <= 1)
        return;

    // An empty part, without an instrument, goes straight away. Otherwise, it's asked first.
    const auto* instrument = instrumentHost.getInstrument (part);

    if (score.isPartEmpty (part) && instrument == nullptr)
    {
        score.removePart (part);
        return;
    }

    const auto name = "instrument " + juce::String (part + 1);
    auto message = juce::String ("Its notes, chords and dynamics will be taken out of the score");

    if (instrument != nullptr)
        message << ", and its " << instrument->getName() << " unloaded. Undo puts back the music, but the plugin will need loading again.";
    else
        message << ". Undo puts them back.";

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Delete " + name + "?")
                                      .withMessage (message)
                                      .withButton ("Delete")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [safeThis = juce::Component::SafePointer (this), id = score.getPartId (part)] (int result)
                                  {
                                      // Delete is 1, and Cancel 0.
                                      if (safeThis != nullptr && result == 1)
                                          safeThis->score.removePart (safeThis->score.findPart (id));
                                  });
}

void MainComponent::copyProgression (int to)
{
    const auto from = activePart;

    if (to == from || ! juce::isPositiveAndBelow (to, score.getNumParts()) || ! score.hasChords (from))
        return;

    // A part with nothing in it can take the progression straight away.
    if (! score.hasNotes (to))
    {
        score.copyChords (from, to);
        return;
    }

    const auto fromName = "instrument " + juce::String (from + 1), toName = "instrument " + juce::String (to + 1);

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Replace " + toName + "'s notes?")
                                      .withMessage ("Copying " + fromName + "'s chord progression to " + toName + " replaces the notes and chords "
                                                    + toName + " has now.")
                                      .withButton ("Replace")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [safeThis = juce::Component::SafePointer (this), from, to] (int result)
                                  {
                                      // Replace is 1, and Cancel 0.
                                      if (safeThis != nullptr && result == 1)
                                          safeThis->score.copyChords (from, to);
                                  });
}

void MainComponent::showScalePanel (juce::Component& button)
{
    auto panel = std::make_unique<ScalePanel> (lastScale);
    auto* panelPointer = panel.get();

    // Sequence or Random closes the box, and the next click on a staff adds the notes.
    panel->onPlace = [this, panelPointer] (ScaleSettings chosen)
    {
        lastScale = chosen;

        if (auto* box = panelPointer->findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();

        startPlacingScale (chosen);
    };

    juce::CallOutBox::launchAsynchronously (std::move (panel), getLocalArea (&button, button.getLocalBounds()), this);
}

void MainComponent::startPlacingScale (const ScaleSettings& scale)
{
    // Whatever tool was chosen, it's back to notes afterwards.
    setNoteLength (noteLengthPicker.getLength());
    placingScale = true;

    for (auto& view : staffSystems.views)
        view->setScalePlacement (scale);
}

void MainComponent::stopPlacingScale()
{
    placingScale = false;

    for (auto& view : staffSystems.views)
        view->setScalePlacement ({});
}

void MainComponent::setNoteLength (double beats)
{
    if (placingScale)
        stopPlacingScale();

    noteLengthPicker.setLength (beats);
    dynamicPicker.setChoice ({});
    eraserButton.setToggleState (false, juce::dontSendNotification);

    for (auto& view : staffSystems.views)
    {
        view->setNoteLength (beats);
        view->setMarking ({});
    }
}

void MainComponent::setMarking (std::optional<music::Marking> marking)
{
    if (placingScale)
        stopPlacingScale();

    if (! marking.has_value())
    {
        setNoteLength (noteLengthPicker.getLength());
        return;
    }

    dynamicPicker.setChoice (marking);
    noteLengthPicker.setChoiceShown (false);
    eraserButton.setToggleState (false, juce::dontSendNotification);

    for (auto& view : staffSystems.views)
        view->setMarking (marking);
}

void MainComponent::setErasing (bool shouldErase)
{
    if (placingScale)
        stopPlacingScale();

    if (! shouldErase)
    {
        setNoteLength (noteLengthPicker.getLength());
        return;
    }

    setEditing (false);
    eraserButton.setToggleState (true, juce::dontSendNotification);
    dynamicPicker.setChoice ({});
    noteLengthPicker.setChoiceShown (false);

    for (auto& view : staffSystems.views)
        view->setErasing (true);
}

void MainComponent::setEditing (bool shouldEdit)
{
    // Editing goes with adding notes or marking, as before, but not with erasing.
    if (shouldEdit && eraserButton.getToggleState())
        setErasing (false);

    editButton.setToggleState (shouldEdit, juce::dontSendNotification);

    for (auto& view : staffSystems.views)
        view->setEditing (shouldEdit);
}

void MainComponent::showPartTitles()
{
    for (size_t part = 0; part < tracks.size(); ++part)
        staffSystems.views[part]->setTitle ("Instrument " + juce::String (part + 1) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "))
                                            + tracks[part].panel->getStatus());
}

void MainComponent::setVolume (int part, float decibels, bool changedOnDial)
{
    // To a tenth of a decibel, as the dial goes, without the dial's rounding errors
    decibels = juce::jlimit (InstrumentHost::minVolume, InstrumentHost::maxVolume, std::round (decibels * 10.0f) / 10.0f);
    instrumentHost.setVolume (part, decibels);
    settings.setValue (getVolumeKey (part), decibels);
    tracks[(size_t) part].dial->setValue (decibels, juce::dontSendNotification);

    if (changedOnDial)
        document.changed();
}

void MainComponent::setMuted (int part, bool muted, bool changedOnDial)
{
    instrumentHost.setMuted (part, muted);
    settings.setValue (getMutedKey (part), muted);

    auto& dial = *tracks[(size_t) part].dial;
    dial.getProperties().set ("muted", muted);
    dial.updateText();
    dial.repaint();

    if (changedOnDial)
        document.changed();
}

void MainComponent::positionVolumeDials()
{
    // Each dial is centred on its part's staves, wherever they've scrolled to.
    const auto scrolled = staffViewport.getViewPositionY();

    for (size_t part = 0; part < tracks.size() && part < staffSystems.views.size(); ++part)
    {
        const auto& view = *staffSystems.views[part];
        const auto staves = view.getStavesRange();
        const auto centreY = view.getBounds().getY() + staves.getStart() + staves.getLength() / 2 - scrolled;
        const auto dialHeight = volumeDialSize + volumeTextHeight;

        tracks[part].dial->setBounds (0, centreY - dialHeight / 2, volumeColumnWidth, dialHeight);
    }
}

void MainComponent::positionMeasureButtons()
{
    if (staffSystems.views.empty())
        return;

    // Just after the final barline, so they scroll sideways with the staves, but halfway down
    // what's in view, however far the staves have scrolled up or down: + and − side by side,
    // over Clone
    constexpr int gap = 6;
    const auto viewArea = staffViewport.getViewArea();
    const auto columnX = staffSystems.views[0]->getFinalBarlineX() + measureButtonsGap + measureButtonsWidth / 2;
    auto stack = juce::Rectangle<int> (measureButtonsWidth, measureButtonSize + 2 * gap + cloneButtonHeight)
                     .withCentre ({ columnX, viewArea.getCentreY() });

    // Each part's delete button, in line with them, level with the middle of its staves. They
    // scroll with the staves, and the measure buttons move up or down, as little as they need
    // to, to stay clear of them.
    for (size_t part = 0; part < tracks.size() && part < staffSystems.views.size(); ++part)
    {
        const auto& view = *staffSystems.views[part];
        const auto staves = view.getStavesRange();
        const auto bin = juce::Rectangle<int> (deleteButtonSize, deleteButtonSize)
                             .withCentre ({ columnX, view.getBounds().getY() + staves.getStart() + staves.getLength() / 2 });
        tracks[part].deleteButton->setBounds (bin);
        tracks[part].deleteButton->toFront (false);

        if (const auto clear = bin.expanded (0, 2 * gap); stack.intersects (clear))
        {
            const auto up = stack.getBottom() - clear.getY();
            const auto down = clear.getBottom() - stack.getY();
            stack.translate (0, up <= down ? -up : down);
        }
    }

    auto pair = stack.removeFromTop (measureButtonSize).withSizeKeepingCentre (measureButtonSize * 2 + gap, measureButtonSize);
    addMeasureButton.setBounds (pair.removeFromLeft (measureButtonSize));
    removeMeasureButton.setBounds (pair.removeFromRight (measureButtonSize));
    stack.removeFromTop (2 * gap);
    cloneButton.setBounds (stack.removeFromTop (cloneButtonHeight).withSizeKeepingCentre (cloneButtonWidth, cloneButtonHeight));
}

void MainComponent::saveVolumes()
{
    for (int part = 0; part < instrumentHost.getNumParts(); ++part)
    {
        settings.setValue (getVolumeKey (part), instrumentHost.getVolume (part));
        settings.setValue (getMutedKey (part), instrumentHost.isMuted (part));
    }
}

void MainComponent::editChord (int part, int measure)
{
    // One chord is edited at a time. The window stays where it was, if it's open already.
    std::optional<juce::Point<int>> position;

    if (chordWindow != nullptr && chordWindow->isVisible())
        position = chordWindow->getPosition();

    closeChordEditor();
    setActivePart (part);

    auto editor = std::make_unique<ChordEditor> (score, part, measure, lastChordStyle);
    auto* editorPointer = editor.get();

    editor->onChordChanged = [this, editorPointer]
    {
        lastChordStyle = editorPointer->getChord().style;
        showHeldNotes();
    };

    editor->onFinished = [this, editorPointer]
    {
        // The editor has finished, but it's still busy, so its window goes once it's done.
        if (chordWindow == nullptr || &chordWindow->getEditor() != editorPointer)
            return;

        chordWindow->setVisible (false);

        for (auto& view : staffSystems.views)
            view->setSelectedMeasure ({});

        showHeldNotes();

        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer (this), editorPointer]
        {
            if (safeThis != nullptr && safeThis->chordWindow != nullptr && &safeThis->chordWindow->getEditor() == editorPointer)
                safeThis->chordWindow = nullptr;
        });
    };

    chordWindow = std::make_unique<ChordWindow> (std::move (editor), [this] { closeChordEditor(); });
    chordWindow->addKeyListener (commandManager.getKeyMappings());     // so Undo works from the chord window too

    // Over the sidebar, to start with, leaving the staff and the piano clear.
    const auto sidebarArea = localAreaToGlobal (sidebar.getBounds());
    chordWindow->setTopLeftPosition (position.value_or (sidebarArea.getTopLeft() + juce::Point<int> (8, 8)));
    chordWindow->setVisible (true);

    staffSystems.views[(size_t) part]->setSelectedMeasure (measure);
    scrollToMeasure (measure);
    showHeldNotes();
}

ChordEditor* MainComponent::getChordEditor() const
{
    return chordWindow != nullptr && chordWindow->isVisible() ? &chordWindow->getEditor() : nullptr;
}

void MainComponent::closeChordEditor()
{
    chordWindow = nullptr;

    for (auto& view : staffSystems.views)
        view->setSelectedMeasure ({});

    showHeldNotes();
}

void MainComponent::scrollToMeasure (int measure)
{
    // A measure out of view is brought to a quarter of the way across.
    const auto area = staffSystems.views[0]->getMeasureArea (measure);
    const auto viewArea = staffViewport.getViewArea();

    if (area.getX() < viewArea.getX() || area.getRight() > viewArea.getRight())
        staffViewport.setViewPosition (area.getX() - viewArea.getWidth() / 4, viewArea.getY());
}

void MainComponent::pianoKeyClicked (int midiNote)
{
    // While a chord's being edited, a key adds its note to the chord, or takes it out.
    if (auto* editor = getChordEditor())
        editor->toggleNote (midiNote);
}

void MainComponent::showHeldNotes()
{
    std::map<int, juce::String> heldNotes;

    if (const auto* editor = getChordEditor())
        if (const auto chord = score.getChordNotes (editor->getChord()))
            for (const auto& tone : chord->tones)
                heldNotes[tone.midi] = tone.getName();

    keyboard.setHeldNotes (heldNotes);
}

//==============================================================================
void MainComponent::timerCallback()
{
    recordPlayedKeys();
    showPlaybackPosition();
}

void MainComponent::togglePlayback()
{
    if (const auto position = instrumentHost.getPlaybackPosition())
    {
        if (instrumentHost.isRecording())
            finishRecording (position);

        instrumentHost.stop();
        showPlaybackPosition();
    }
    else
    {
        updatePerformance();
        instrumentHost.play (performance, 0, performance.getNumMeasures() - 1, loopButton.getToggleState(), toPlayedBeats (playbackStart));
        startTimerHz (30);
        showPlaybackPosition();
    }
}

void MainComponent::updatePerformance()
{
    score.writePerformance (performance);
    performanceOrder = score.getPerformanceOrder();
}

double MainComponent::toWrittenBeats (double playedBeats) const
{
    if (performanceOrder.empty())
        return playedBeats;

    const auto beatsPerMeasure = score.getBeatsPerMeasure();
    const auto index = juce::jlimit (0, (int) performanceOrder.size() - 1, (int) std::floor (playedBeats / beatsPerMeasure));
    return performanceOrder[(size_t) index] * beatsPerMeasure + (playedBeats - index * beatsPerMeasure);
}

double MainComponent::toPlayedBeats (double writtenBeats) const
{
    const auto beatsPerMeasure = score.getBeatsPerMeasure();
    const auto measure = (int) std::floor (writtenBeats / beatsPerMeasure);
    const auto first = std::find (performanceOrder.begin(), performanceOrder.end(), measure);

    if (first == performanceOrder.end())
        return writtenBeats;

    return (double) std::distance (performanceOrder.begin(), first) * beatsPerMeasure + (writtenBeats - measure * beatsPerMeasure);
}

void MainComponent::choosePlaybackStart (int beat)
{
    playbackStart = beat;

    if (const auto position = instrumentHost.getPlaybackPosition())
    {
        // Keys held while recording end where it jumps from.
        if (instrumentHost.isRecording())
        {
            recordPlayedKeys();
            recorder.finish (position);
        }

        instrumentHost.jumpTo (toPlayedBeats (beat));
    }

    showPlaybackPosition();
}

void MainComponent::toggleRecording()
{
    // Recording again stops recording, and the score plays on.
    if (instrumentHost.isRecording())
    {
        finishRecording (instrumentHost.getPlaybackPosition());
        return;
    }

    if (! instrumentHost.getPlaybackPosition().has_value())
    {
        updatePerformance();
        instrumentHost.play (performance, 0, performance.getNumMeasures() - 1, loopButton.getToggleState(), toPlayedBeats (playbackStart));
        startTimerHz (30);
    }

    // Without the audio running, nothing plays, so there's nothing to record along with.
    if (instrumentHost.getPlaybackPosition().has_value())
    {
        // Everything recorded is undone in one go.
        history.beginGesture();
        instrumentHost.takeRecordedKeys();
        instrumentHost.setRecording (true);
        recordButton.setToggleState (true, juce::dontSendNotification);
    }

    showPlaybackPosition();
}

void MainComponent::recordPlayedKeys()
{
    for (const auto& key : instrumentHost.takeRecordedKeys())
    {
        if (key.isDown)
            recorder.keyDown (key.noteNumber, key.beat, activePartId);
        else
            recorder.keyUp (key.noteNumber, key.beat);
    }
}

void MainComponent::finishRecording (std::optional<double> beat)
{
    instrumentHost.setRecording (false);
    recordPlayedKeys();
    recorder.finish (beat);
    recordButton.setToggleState (false, juce::dontSendNotification);
    history.endGesture();
}

void MainComponent::showPlaybackPosition()
{
    // Shown where it is in the score as it's written
    const auto played = instrumentHost.getPlaybackPosition();
    const auto position = played.has_value() ? std::optional (toWrittenBeats (*played)) : std::nullopt;

    playButton.setButtonText (position.has_value() ? "Stop" : "Play");
    playButton.setSymbol (position.has_value() ? controls::TransportButton::Symbol::stop : controls::TransportButton::Symbol::play);

    for (auto& view : staffSystems.views)
        view->setPlaybackPosition (position);

    // The ruler's marker is where it's playing, or where it'll start from.
    positionRuler.setMarker (position.value_or (playbackStart), position.has_value());

    if (! position.has_value())
    {
        // Recording stops when the score stops, at the end, the notes still held lasting to there.
        if (instrumentHost.isRecording())
            finishRecording ({});

        stopTimer();
        return;
    }

    // When the beat that's playing goes out of view, scroll it back to a quarter of the way across.
    const auto playingArea = staffSystems.views[0]->getPlaybackArea();
    const auto viewArea = staffViewport.getViewArea();

    if (playingArea.getX() < viewArea.getX() || playingArea.getRight() > viewArea.getRight())
        staffViewport.setViewPosition (playingArea.getX() - viewArea.getWidth() / 4, viewArea.getY());
}

void MainComponent::tempoEdited (bool finished)
{
    const auto text = tempoEditor.getText().trim();
    const auto beatsPerMinute = text.getDoubleValue();
    const auto valid = text.containsOnly ("0123456789.") && text.containsAnyOf ("0123456789")
                    && text.indexOfChar ('.') == text.lastIndexOfChar ('.')
                    && beatsPerMinute >= Score::minBeatsPerMinute && beatsPerMinute <= Score::maxBeatsPerMinute;

    if (valid)
        score.setBeatsPerMinute (beatsPerMinute);
    else if (finished)
        tempoEditor.setText (formatNumber (score.getBeatsPerMinute()), false);

    // A value that won't do is outlined in red until it's fixed, or put back when editing stops.
    for (auto colourId : { juce::TextEditor::outlineColourId, juce::TextEditor::focusedOutlineColourId })
    {
        if (! valid && ! finished)
            tempoEditor.setColour (colourId, juce::Colours::red);
        else
            tempoEditor.removeColour (colourId);
    }

    tempoEditor.repaint();
}

void MainComponent::stepTempo (int direction)
{
    // From a fraction, to the whole number either side of it
    const auto beatsPerMinute = score.getBeatsPerMinute();
    const auto stepped = direction > 0 ? std::floor (beatsPerMinute) + 1.0 : std::ceil (beatsPerMinute) - 1.0;
    score.setBeatsPerMinute (juce::jlimit (Score::minBeatsPerMinute, Score::maxBeatsPerMinute, stepped));

    // Shown straight away, even while it's being typed, which it replaces
    tempoEditor.setText (formatNumber (score.getBeatsPerMinute()), false);

    for (auto colourId : { juce::TextEditor::outlineColourId, juce::TextEditor::focusedOutlineColourId })
        tempoEditor.removeColour (colourId);

    tempoEditor.repaint();
    updateTempoArrows();
}

void MainComponent::updateTempoArrows()
{
    tempoUpButton.setEnabled (score.getBeatsPerMinute() < Score::maxBeatsPerMinute);
    tempoDownButton.setEnabled (score.getBeatsPerMinute() > Score::minBeatsPerMinute);
}

void MainComponent::addMeasure()
{
    score.addMeasure();

    // Scroll to the end, so the new measure is in view.
    staffViewport.setViewPosition (staffSystems.getWidth(), staffViewport.getViewPositionY());
}

void MainComponent::cloneMeasures()
{
    score.cloneMeasures();

    // Scroll to the end, so the new measures are in view.
    staffViewport.setViewPosition (staffSystems.getWidth(), staffViewport.getViewPositionY());
}

void MainComponent::removeMeasure()
{
    if (removeMeasureButton.isEnabled())
        score.removeLastMeasure();
}

void MainComponent::updateRemoveMeasureButton()
{
    // Only an empty last measure can be taken away, and not the only one.
    const auto last = score.getNumMeasures() - 1;
    const auto canRemove = last > 0 && score.isMeasureEmpty (last);
    removeMeasureButton.setEnabled (canRemove);
    removeMeasureButton.setTooltip (canRemove ? "Remove the last measure"
                                              : last == 0 ? "The only measure can't be removed"
                                                          : "Only an empty last measure, with nothing but rests in every instrument, can be removed");
}

//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "MIDI" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int menuIndex, const juce::String&)
{
    juce::PopupMenu menu;

    if (menuIndex == 1)
    {
        menu.addCommandItem (&commandManager, undoChange);
        menu.addCommandItem (&commandManager, redoChange);
        return menu;
    }

    if (menuIndex == 2)
    {
        // Each MIDI input, ticked when it's on.
        midiMenuDevices = midiInputs.getDevices();

        if (midiMenuDevices.empty())
            menu.addItem (firstMidiInputItem - 1, "No MIDI Inputs", false);

        for (size_t i = 0; i < midiMenuDevices.size(); ++i)
            menu.addItem (firstMidiInputItem + (int) i, midiMenuDevices[i].info.name, true, midiMenuDevices[i].enabled);

        return menu;
    }

    menu.addCommandItem (&commandManager, newScore);
    menu.addCommandItem (&commandManager, openScore);
    menu.addSubMenu ("Open Recent", getRecentScoresMenu());
    menu.addSeparator();
    menu.addCommandItem (&commandManager, saveScore);
    menu.addCommandItem (&commandManager, saveScoreAs);
    return menu;
}

juce::PopupMenu MainComponent::getRecentScoresMenu() const
{
    // The scores that are still there, newest first, by name. Two with the same name say which folder they're in.
    const auto& recent = document.getRecentScores();
    juce::StringArray names;

    for (int i = 0; i < recent.getNumFiles(); ++i)
        names.add (recent.getFile (i).getFileNameWithoutExtension());

    juce::PopupMenu menu;

    for (int i = 0; i < recent.getNumFiles(); ++i)
    {
        const auto file = recent.getFile (i);

        if (! file.existsAsFile())
            continue;

        auto name = names[i];

        if (std::count (names.begin(), names.end(), name) > 1)
            name << juce::String (juce::CharPointer_UTF8 (" \xe2\x80\x94 ")) << file.getParentDirectory().getFileName();

        menu.addItem (firstRecentScoreItem + i, name);
    }

    if (menu.getNumItems() == 0)
        menu.addItem (noRecentScoresItem, "No Recent Scores", false);

    menu.addSeparator();
    menu.addItem (clearRecentScoresItem, "Clear Menu", recent.getNumFiles() > 0);
    return menu;
}

void MainComponent::openRecentScore (const juce::File& file)
{
    saveChangesThen ([this, file]
    {
        // One that's been moved, deleted or damaged is taken off the list.
        if (document.loadFrom (file, true).wasOk())
            scoreReplaced();
        else
            document.forgetRecentScore (file);
    });
}

void MainComponent::menuItemSelected (int menuItemID, int topLevelMenuIndex)
{
    if (topLevelMenuIndex == 0)
    {
        const auto& recent = document.getRecentScores();

        if (menuItemID == clearRecentScoresItem)
            document.clearRecentScores();
        else if (juce::isPositiveAndBelow (menuItemID - firstRecentScoreItem, recent.getNumFiles()))
            openRecentScore (recent.getFile (menuItemID - firstRecentScoreItem));

        return;
    }

    const auto index = (size_t) (menuItemID - firstMidiInputItem);

    if (topLevelMenuIndex == 2 && menuItemID >= firstMidiInputItem && index < midiMenuDevices.size())
        midiInputs.setEnabled (midiMenuDevices[index].info.identifier, ! midiMenuDevices[index].enabled);
}

juce::ApplicationCommandTarget* MainComponent::getNextCommandTarget()
{
    return nullptr;
}

void MainComponent::getAllCommands (juce::Array<juce::CommandID>& commands)
{
    commands.addArray ({ newScore, openScore, saveScore, saveScoreAs, undoChange, redoChange });
}

void MainComponent::getCommandInfo (juce::CommandID command, juce::ApplicationCommandInfo& info)
{
    const auto cmd = juce::ModifierKeys::commandModifier;

    switch (command)
    {
        case newScore:     info.setInfo ("New", "Starts a new, empty score", "File", 0);
                           info.addDefaultKeypress ('n', cmd); break;
        case openScore:    info.setInfo (juce::String (juce::CharPointer_UTF8 ("Open\xe2\x80\xa6")), "Opens a saved score", "File", 0);
                           info.addDefaultKeypress ('o', cmd); break;
        case saveScore:    info.setInfo ("Save", "Saves the score", "File", 0);
                           info.addDefaultKeypress ('s', cmd); break;
        case saveScoreAs:  info.setInfo (juce::String (juce::CharPointer_UTF8 ("Save As\xe2\x80\xa6")), "Saves the score in a new file", "File", 0);
                           info.addDefaultKeypress ('s', cmd | juce::ModifierKeys::shiftModifier); break;
        case undoChange:   info.setInfo ("Undo", "Undoes the last change to the score", "Edit", 0);
                           info.setActive (history.canUndo());
                           info.addDefaultKeypress ('z', cmd); break;
        case redoChange:   info.setInfo ("Redo", "Makes the last change undone again", "Edit", 0);
                           info.setActive (history.canRedo());
                           info.addDefaultKeypress ('z', cmd | juce::ModifierKeys::shiftModifier); break;
        default:           break;
    }
}

bool MainComponent::perform (const InvocationInfo& invocation)
{
    switch (invocation.commandID)
    {
        case newScore:
            saveChangesThen ([this] { document.startNewScore(); scoreReplaced(); });
            return true;

        case openScore:
            saveChangesThen ([this]
            {
                document.loadFromUserSpecifiedFileAsync (true, [safeThis = juce::Component::SafePointer (this)] (juce::Result result)
                {
                    if (safeThis != nullptr && result.wasOk())
                        safeThis->scoreReplaced();
                });
            });
            return true;

        case saveScore:
            document.saveAsync (true, true, nullptr);
            return true;

        case saveScoreAs:
            document.saveAsInteractiveAsync (true, nullptr);
            return true;

        case undoChange:
            history.undo();
            return true;

        case redoChange:
            history.redo();
            return true;

        default:
            return false;
    }
}

void MainComponent::saveChangesThen (std::function<void()> action)
{
    // An instrument's sound may have been changed in its editor, which might still be open.
    for (auto& track : tracks)
        track.panel->checkForSoundChanges();

    document.saveIfNeededAndUserAgreesAsync ([safeThis = juce::Component::SafePointer (this), action] (juce::FileBasedDocument::SaveResult result)
    {
        if (safeThis != nullptr && result == juce::FileBasedDocument::savedOk)
            action();
    });
}

void MainComponent::scoreReplaced()
{
    // A new score starts from the beginning, with nothing playing, recording or selected, and
    // nothing to undo. Keys held while recording the old one aren't written into it.
    if (instrumentHost.isRecording())
    {
        instrumentHost.setRecording (false);
        recorder.clear();
        recordButton.setToggleState (false, juce::dontSendNotification);
        history.endGesture();
    }

    history.clear();
    instrumentHost.stop();
    playbackStart = 0.0;
    showPlaybackPosition();
    closeChordEditor();
    staffViewport.setViewPosition (0, 0);
}

void MainComponent::showDocumentTitle()
{
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        window->setName (document.getDocumentTitle() + (document.hasChangedSinceSaved() ? juce::String (juce::CharPointer_UTF8 (" \xe2\x80\x94 Edited")) : juce::String()));
}
