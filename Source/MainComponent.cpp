#include "MainComponent.h"

#include "Controls.h"
#include "MeasureContent.h"

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

    /** A number without trailing zeros, e.g. "120" or "92.5". */
    juce::String formatNumber (double number)
    {
        auto text = juce::String (number, 2);

        while (text.endsWithChar ('0'))
            text = text.dropLastCharacters (1);

        return text.trimCharactersAtEnd (".");
    }

    bool hasNotes (const Score& score, int measure)
    {
        const auto content = getMeasureContent (score, measure);

        return std::any_of (content.staves.begin(), content.staves.end(), [] (const StaffContent& staff)
        {
            return std::any_of (staff.events.begin(), staff.events.end(), [] (const StaffEvent& e) { return ! e.isRest(); });
        });
    }
}

void MainComponent::SidebarContent::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::black.withAlpha (0.1f));
    g.fillRect (sidebarPadding, dividerY, getWidth() - 2 * sidebarPadding, 1);
}

MainComponent::MainComponent()
{
    playButton.onClick = [this] { togglePlayback(); };
    playButton.addShortcut (juce::KeyPress (juce::KeyPress::spaceKey));

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

    controls::makeSegmented ({ &notesButton, &chordsButton }, 1);
    notesButton.setTooltip ("Click the staff to add quarter notes");
    chordsButton.setTooltip ("Click a measure to add or change its chord");
    notesButton.onClick = [this] { setInputMode (InputMode::notes); };
    chordsButton.onClick = [this] { setInputMode (InputMode::chords); };

    // Clicking a button shouldn't take the keyboard focus away from the piano, which the
    // computer keyboard can play too.
    playButton.setWantsKeyboardFocus (false);

    for (auto* component : std::initializer_list<juce::Component*> { &playButton, &tempoLabel, &tempoEditor,
                                                                     &notesButton, &chordsButton, &instrumentPanel })
        addAndMakeVisible (component);

    scorePanel.onAddMeasure = [this] { addMeasure(); };
    scorePanel.onRemoveMeasure = [this] { removeMeasure(); };
    sidebarContent.addAndMakeVisible (scorePanel);
    sidebarContent.addAndMakeVisible (chordPanel);
    sidebar.setViewedComponent (&sidebarContent, false);
    sidebar.setScrollBarsShown (true, false);
    addAndMakeVisible (sidebar);

    staffView.onMeasureClicked = [this] (int measure) { measureClicked (measure); };
    staffViewport.setViewedComponent (&staffView, false);
    staffViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (staffViewport);

    keyboard.setAvailableRange (lowestKey, highestKey);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setKeyPressBaseOctave (5);     // the computer keyboard's A key plays middle C
    keyboard.setScrollButtonsVisible (false);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (0xff4a8fe0));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour (0x264a8fe0));
    keyboard.onKeyClicked = [this] (int midiNote) { pianoKeyClicked (midiNote); };
    addAndMakeVisible (keyboard);

    score.addChangeListener (this);
    setInputMode (InputMode::notes);
    setSize (1280, 820);

    juce::AudioDeviceManager::AudioDeviceSetup preferredSetup;
    preferredSetup.bufferSize = 256;    // small enough for the keyboard to feel immediate

    if (const auto error = audioDeviceManager.initialise (0, 2, nullptr, true, {}, &preferredSetup); error.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't open the audio output", error);

    audioDeviceManager.addAudioCallback (&instrumentHost);
}

MainComponent::~MainComponent()
{
    audioDeviceManager.removeAudioCallback (&instrumentHost);
    score.removeChangeListener (this);

    if (keyListenerTarget != nullptr)
        keyListenerTarget->removeKeyListener (this);
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
    toolbar.removeFromLeft (16);
    tempoLabel.setBounds (toolbar.removeFromLeft (34));
    toolbar.removeFromLeft (4);
    tempoEditor.setBounds (toolbar.removeFromLeft (56));
    toolbar.removeFromLeft (20);
    notesButton.setBounds (toolbar.removeFromLeft (76));
    chordsButton.setBounds (toolbar.removeFromLeft (76));
    toolbar.removeFromLeft (20);
    instrumentPanel.setBounds (toolbar);

    sidebar.setBounds (bounds.removeFromLeft (sidebarWidth));
    bounds.removeFromLeft (1);

    const auto contentWidth = sidebar.getMaximumVisibleWidth();
    const auto panelWidth = contentWidth - 2 * sidebarPadding;
    scorePanel.setBounds (sidebarPadding, sidebarPadding, panelWidth, scorePanel.getIdealHeight());
    sidebarContent.dividerY = scorePanel.getBottom() + 14;
    chordPanel.setBounds (sidebarPadding, sidebarContent.dividerY + 14, panelWidth, chordPanel.getIdealHeight());
    sidebarContent.setSize (contentWidth, chordPanel.getBottom() + sidebarPadding);

    keyboard.setBounds (bounds.removeFromBottom (keyboardHeight));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / (float) numWhiteKeys);

    staffViewport.setBounds (bounds);
    staffView.setSize (staffView.getContentWidth(),
                       juce::jmax (StaffView::getContentHeight(),
                                   staffViewport.getHeight() - staffViewport.getScrollBarThickness()));
}

void MainComponent::parentHierarchyChanged()
{
    // Esc is listened for on the whole window, whichever control has the keyboard focus.
    auto* top = getTopLevelComponent();

    if (top == keyListenerTarget.getComponent())
        return;

    if (keyListenerTarget != nullptr)
        keyListenerTarget->removeKeyListener (this);

    keyListenerTarget = top;
    top->addKeyListener (this);
}

bool MainComponent::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (key == juce::KeyPress::escapeKey && selectedMeasure.has_value())
    {
        selectMeasure ({});
        return true;
    }

    return false;
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (selectedMeasure.has_value() && *selectedMeasure >= score.getNumMeasures())
        selectMeasure ({});

    showHeldNotes();
}

//==============================================================================
void MainComponent::setInputMode (InputMode mode)
{
    const auto switchingToChords = mode == InputMode::chords && inputMode != InputMode::chords;

    inputMode = mode;
    notesButton.setToggleState (mode == InputMode::notes, juce::dontSendNotification);
    chordsButton.setToggleState (mode == InputMode::chords, juce::dontSendNotification);
    staffView.setInputMode (mode);

    if (mode == InputMode::notes)
    {
        selectMeasure ({});
    }
    else if (switchingToChords)
    {
        // Start on the first measure without any notes, or on the first measure if they all have some.
        auto measure = 0;

        for (int m = 0; m < score.getNumMeasures(); ++m)
        {
            if (! hasNotes (score, m))
            {
                measure = m;
                break;
            }
        }

        selectMeasure (measure);
        scrollToMeasure (measure);
    }

    chordPanel.setHint (mode == InputMode::notes ? "Switch to Chords above, then click a measure to give it a chord."
                                                 : "Click a measure to add a chord to it, or to change its chord.");
    showHeldNotes();
}

void MainComponent::selectMeasure (std::optional<int> measure)
{
    selectedMeasure = measure;
    staffView.setSelectedMeasure (measure);
    chordPanel.setMeasure (measure);
    showHeldNotes();
}

void MainComponent::measureClicked (int measure)
{
    // Clicking a measure selects it, and plays it; clicking it again lets it go.
    if (selectedMeasure == measure)
    {
        selectMeasure ({});
        return;
    }

    selectMeasure (measure);

    if (hasNotes (score, measure))
        play (measure, measure);
}

void MainComponent::scrollToMeasure (int measure)
{
    // A measure out of view is brought to a quarter of the way across.
    const auto area = staffView.getMeasureArea (measure);
    const auto viewArea = staffViewport.getViewArea();

    if (area.getX() < viewArea.getX() || area.getRight() > viewArea.getRight())
        staffViewport.setViewPosition (area.getX() - viewArea.getWidth() / 4, 0);
}

void MainComponent::pianoKeyClicked (int midiNote)
{
    // With a measure selected, a key adds its note to the measure's chord, or takes it out.
    if (inputMode == InputMode::chords && selectedMeasure.has_value())
        score.toggleChordNote (*selectedMeasure, midiNote, chordPanel.getStyleForNewChord());
}

void MainComponent::showHeldNotes()
{
    std::map<int, juce::String> heldNotes;

    if (inputMode == InputMode::chords && selectedMeasure.has_value())
        if (const auto chord = score.getChordNotes (*selectedMeasure))
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
        play (0, score.getNumMeasures() - 1);
    }
}

void MainComponent::play (int firstMeasure, int lastMeasure)
{
    instrumentHost.play (score, firstMeasure, lastMeasure);
    startTimerHz (30);
    showPlaybackPosition();
}

void MainComponent::showPlaybackPosition()
{
    const auto position = instrumentHost.getPlaybackPosition();

    playButton.setButtonText (position.has_value() ? "Stop" : "Play");
    staffView.setPlaybackPosition (position);

    if (! position.has_value())
    {
        stopTimer();
        return;
    }

    // When the beat that's playing goes out of view, scroll it back to a quarter of the way across.
    const auto playingArea = staffView.getPlaybackArea();
    const auto viewArea = staffViewport.getViewArea();

    if (playingArea.getX() < viewArea.getX() || playingArea.getRight() > viewArea.getRight())
        staffViewport.setViewPosition (playingArea.getX() - viewArea.getWidth() / 4, 0);
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
    staffViewport.setViewPosition (staffView.getWidth(), 0);
}

void MainComponent::removeMeasure()
{
    score.removeLastMeasure();
}
