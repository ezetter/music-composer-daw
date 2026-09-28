#include "MainComponent.h"

#include "Controls.h"

#include <algorithm>

namespace
{
    constexpr int toolbarHeight = 44;
    constexpr int sidebarWidth = 280;
    constexpr int sidebarPadding = 14;
    constexpr int keyboardHeight = 140;

    // A full 88-key piano, A0 to C8
    constexpr int lowestKey = 21;
    constexpr int highestKey = 108;
    constexpr int numWhiteKeys = 52;

    // Where Loop is kept in the settings
    const char* const loopKey = "loop";

    // The MIDI menu's items are numbered from here, in the order of the inputs they're for.
    constexpr int firstMidiInputItem = 1000;

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
{
    for (int part = 0; part < Score::numParts; ++part)
    {
        views[(size_t) part] = std::make_unique<StaffView> (scoreToShow, part);
        addAndMakeVisible (*views[(size_t) part]);
    }

    layOut();
}

void MainComponent::StaffSystems::setMinimumHeight (int height)
{
    minimumHeight = height;
    layOut();
}

void MainComponent::StaffSystems::childBoundsChanged (juce::Component*)
{
    // A view grows or shrinks with its score.
    layOut();
}

void MainComponent::StaffSystems::layOut()
{
    if (layingOut)
        return;

    const juce::ScopedValueSetter<bool> guard (layingOut, true);

    auto width = 0;

    for (auto& view : views)
        width = juce::jmax (width, view->getContentWidth());

    // Each system is as tall as its music needs, stacked from the top, with any spare room below.
    auto y = 0;

    for (auto& view : views)
    {
        const auto height = view->getContentHeight();
        view->setBounds (0, y, width, height);
        y += height;
    }

    setSize (width, juce::jmax (y, minimumHeight));
}

//==============================================================================
MainComponent::MainComponent (juce::PropertiesFile& settingsToUse)
    : settings (settingsToUse),
      document (score, settings)
{
    // Each part has its own instrument, which is saved with the score, sound and all.
    for (int part = 0; part < Score::numParts; ++part)
    {
        auto& panel = instrumentPanels[(size_t) part];
        panel = std::make_unique<InstrumentPanel> (instrumentHost, part, settings);
        panel->onInstrumentChanged = [this] { document.changed(); };
        panel->onStatusChanged = [this] { showPartTitles(); };
        addChildComponent (*panel);
    }

    document.getInstrumentToSave = [this] (int part) { return instrumentPanels[(size_t) part]->saveToJSON(); };
    document.loadInstrument = [this] (int part, const juce::var& json) { instrumentPanels[(size_t) part]->loadFromJSON (json); };

    playButton.onClick = [this] { togglePlayback(); };
    playButton.addShortcut (juce::KeyPress (juce::KeyPress::spaceKey));

    // Loop is remembered from one run of the app to the next.
    playButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    loopButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
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


    // The active part, which the keyboard plays
    controls::makeSegmented ({ &partButtons[0], &partButtons[1] }, 1);

    for (int part = 0; part < Score::numParts; ++part)
    {
        auto& button = partButtons[(size_t) part];
        button.setButtonText ("Part " + juce::String (part + 1));
        button.setTooltip ("Play part " + juce::String (part + 1) + "'s instrument on the keyboard, and set its alternate staff");
        button.onClick = [this, part] { setActivePart (part); };
    }

    // Clicking a button shouldn't take the keyboard focus away from the piano, which the
    // computer keyboard can play too.
    playButton.setWantsKeyboardFocus (false);
    loopButton.setWantsKeyboardFocus (false);

    for (auto* component : std::initializer_list<juce::Component*> { &playButton, &loopButton, &tempoLabel, &tempoEditor,
                                                                     &partButtons[0], &partButtons[1] })
        addAndMakeVisible (component);

    scorePanel.onAddMeasure = [this] { addMeasure(); };
    scorePanel.onRemoveMeasure = [this] { removeMeasure(); };
    sidebarContent.addAndMakeVisible (scorePanel);
    sidebar.setViewedComponent (&sidebarContent, false);
    sidebar.setScrollBarsShown (true, false);
    addAndMakeVisible (sidebar);

    // Clicking a part's staff, or its chord buttons, makes it the active part.
    for (auto& view : staffSystems.views)
    {
        const auto part = view->getPart();
        view->onClicked = [this, part] { setActivePart (part); };
        view->onChordButtonClicked = [this, part] (int measure) { editChord (part, measure); };
    }

    staffViewport.setViewedComponent (&staffSystems, false);
    staffViewport.setScrollBarsShown (true, true);
    addAndMakeVisible (staffViewport);

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

    // The File menu, with its keyboard shortcuts
    commandManager.registerAllCommandsForTarget (this);
    commandManager.setFirstCommandTarget (this);
    setApplicationCommandManagerToWatch (&commandManager);
    juce::MenuBarModel::setMacMainMenu (this);

    setActivePart (0);
    showPartTitles();
    setSize (1280, 920);

    juce::AudioDeviceManager::AudioDeviceSetup preferredSetup;
    preferredSetup.bufferSize = 256;    // small enough for the keyboard to feel immediate

    if (const auto error = audioDeviceManager.initialise (0, 2, nullptr, true, {}, &preferredSetup); error.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't open the audio output", error);

    audioDeviceManager.addAudioCallback (&instrumentHost);
}

MainComponent::~MainComponent()
{
    juce::MenuBarModel::setMacMainMenu (nullptr);
    commandManager.setFirstCommandTarget (nullptr);

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

    // The staff view doesn't cover the space for the scroll bar, so fill that in to match.
    g.setColour (StaffView::paperColour);
    g.fillRect (staffViewport.getBounds());

    g.setColour (juce::Colours::black.withAlpha (0.15f));
    g.fillRect (0, toolbarHeight - 1, getWidth(), 1);
    g.fillRect (sidebar.getRight(), toolbarHeight, 1, getHeight() - toolbarHeight);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    auto toolbar = bounds.removeFromTop (toolbarHeight).reduced (12, 8);
    playButton.setBounds (toolbar.removeFromLeft (80));
    loopButton.setBounds (toolbar.removeFromLeft (64));
    toolbar.removeFromLeft (16);
    tempoLabel.setBounds (toolbar.removeFromLeft (34));
    toolbar.removeFromLeft (4);
    tempoEditor.setBounds (toolbar.removeFromLeft (56));
    toolbar.removeFromLeft (20);
    partButtons[0].setBounds (toolbar.removeFromLeft (70));
    partButtons[1].setBounds (toolbar.removeFromLeft (70));
    toolbar.removeFromLeft (20);

    for (auto& panel : instrumentPanels)
        panel->setBounds (toolbar);

    sidebar.setBounds (bounds.removeFromLeft (sidebarWidth));
    bounds.removeFromLeft (1);

    const auto contentWidth = sidebar.getMaximumVisibleWidth();
    const auto panelWidth = contentWidth - 2 * sidebarPadding;
    scorePanel.setBounds (sidebarPadding, sidebarPadding, panelWidth, scorePanel.getIdealHeight());
    sidebarContent.setSize (contentWidth, scorePanel.getBottom() + sidebarPadding);

    keyboard.setBounds (bounds.removeFromBottom (keyboardHeight));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / (float) numWhiteKeys);

    staffViewport.setBounds (bounds);
    staffSystems.setMinimumHeight (staffViewport.getHeight() - staffViewport.getScrollBarThickness());
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

bool MainComponent::keyPressed (const juce::KeyPress& key, juce::Component*)
{
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
        showDocumentTitle();
        return;
    }

    // The tempo can change by loading a score, so show it unless it's being typed.
    if (! tempoEditor.hasKeyboardFocus (false))
        tempoEditor.setText (formatNumber (score.getBeatsPerMinute()), false);

    // A loop plays changes to the score from the next time through.
    if (instrumentHost.getPlaybackPosition().has_value())
        instrumentHost.updateLoop (score, 0, score.getNumMeasures() - 1);

    showHeldNotes();
}

//==============================================================================
void MainComponent::setActivePart (int part)
{
    activePart = part;
    instrumentHost.setActivePart (part);
    scorePanel.setPart (part);

    for (int p = 0; p < Score::numParts; ++p)
    {
        partButtons[(size_t) p].setToggleState (p == part, juce::dontSendNotification);
        instrumentPanels[(size_t) p]->setVisible (p == part);
        staffSystems.views[(size_t) p]->setActive (p == part);
    }

    // The chord window is for the active part's chords.
    if (const auto* editor = getChordEditor(); editor != nullptr && editor->getPart() != part)
        closeChordEditor();
}

void MainComponent::showPartTitles()
{
    for (int part = 0; part < Score::numParts; ++part)
        staffSystems.views[(size_t) part]->setTitle ("Part " + juce::String (part + 1) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "))
                                                     + instrumentPanels[(size_t) part]->getStatus());
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
    showPlaybackPosition();
}

void MainComponent::togglePlayback()
{
    if (instrumentHost.getPlaybackPosition().has_value())
    {
        instrumentHost.stop();
        showPlaybackPosition();
    }
    else
    {
        instrumentHost.play (score, 0, score.getNumMeasures() - 1, loopButton.getToggleState());
        startTimerHz (30);
        showPlaybackPosition();
    }
}

void MainComponent::showPlaybackPosition()
{
    const auto position = instrumentHost.getPlaybackPosition();

    playButton.setButtonText (position.has_value() ? "Stop" : "Play");
    for (auto& view : staffSystems.views)
        view->setPlaybackPosition (position);

    if (! position.has_value())
    {
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

void MainComponent::addMeasure()
{
    score.addMeasure();

    // Scroll to the end, so the new measure is in view.
    staffViewport.setViewPosition (staffSystems.getWidth(), staffViewport.getViewPositionY());
}

void MainComponent::removeMeasure()
{
    score.removeLastMeasure();
}

//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "MIDI" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int menuIndex, const juce::String&)
{
    juce::PopupMenu menu;

    if (menuIndex == 1)
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
    menu.addSeparator();
    menu.addCommandItem (&commandManager, saveScore);
    menu.addCommandItem (&commandManager, saveScoreAs);
    return menu;
}

void MainComponent::menuItemSelected (int menuItemID, int topLevelMenuIndex)
{
    const auto index = (size_t) (menuItemID - firstMidiInputItem);

    if (topLevelMenuIndex == 1 && menuItemID >= firstMidiInputItem && index < midiMenuDevices.size())
        midiInputs.setEnabled (midiMenuDevices[index].info.identifier, ! midiMenuDevices[index].enabled);
}

juce::ApplicationCommandTarget* MainComponent::getNextCommandTarget()
{
    return nullptr;
}

void MainComponent::getAllCommands (juce::Array<juce::CommandID>& commands)
{
    commands.addArray ({ newScore, openScore, saveScore, saveScoreAs });
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

        default:
            return false;
    }
}

void MainComponent::saveChangesThen (std::function<void()> action)
{
    // An instrument's sound may have been changed in its editor, which might still be open.
    for (auto& panel : instrumentPanels)
        panel->checkForSoundChanges();

    document.saveIfNeededAndUserAgreesAsync ([safeThis = juce::Component::SafePointer (this), action] (juce::FileBasedDocument::SaveResult result)
    {
        if (safeThis != nullptr && result == juce::FileBasedDocument::savedOk)
            action();
    });
}

void MainComponent::scoreReplaced()
{
    // A new score starts from the beginning, with nothing playing or selected.
    instrumentHost.stop();
    showPlaybackPosition();
    closeChordEditor();
    staffViewport.setViewPosition (0, 0);
}

void MainComponent::showDocumentTitle()
{
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        window->setName (document.getDocumentTitle() + (document.hasChangedSinceSaved() ? juce::String (juce::CharPointer_UTF8 (" \xe2\x80\x94 Edited")) : juce::String()));
}
