#include "ChordEditor.h"

#include "Controls.h"

namespace
{
    constexpr int rowHeight = 28;
    constexpr int segmentHeight = 26;
    constexpr int headingHeight = 16;
    constexpr int gap = 6;
    constexpr int sectionGap = 12;
    constexpr int infoLineHeight = 18;
    constexpr int padding = 16;
    constexpr int buttonHeight = 28;

    /** Drops a chord's 7th or 9th, and its 3rd inversion, which only 7th chords have. */
    void clearAddedNote (music::ChordSpec& spec)
    {
        spec.addedNote = music::AddedNote::none;

        if (spec.inversion == 3)
            spec.inversion = 0;
    }
}
ChordEditor::ChordEditor (Score& scoreToEdit, int partToEdit, int measureToEdit, const ChordStyle& styleForNewChord)
    : score (scoreToEdit),
      part (partToEdit),
      measure (measureToEdit)
{
    chord.style = styleForNewChord;
    loadChord();

    controls::makeHeading (title, "Chord");
    controls::makeHeading (chordTypeHeading, "Chord type");

    // Changing anything that defines the chord replaces notes set on the piano.
    flatButton.setButtonText (controls::fromUTF8 ("\xe2\x99\xad Flat"));
    flatButton.setTooltip ("Lowers the chord's root a half step");
    flatButton.onClick = [this] { edit ([this] (MeasureChord& c) { c.spec.flat = flatButton.getToggleState(); c.keyboardNotes.reset(); }); };

    controls::makeSegmented ({ &majorButton, &minorButton }, 1001);

    for (auto* button : { &majorButton, &minorButton })
    {
        button->onClick = [this, button]
        {
            edit ([button, this] (MeasureChord& c)
            {
                c.spec.minor = button == &minorButton;
                c.spec.altered = false;
                c.keyboardNotes.reset();
                clearAddedNote (c.spec);
            });
        };
    }

    alterButton.setWantsKeyboardFocus (false);
    alterButton.setColour (juce::TextButton::buttonOnColourId, controls::accentLight);
    alterButton.setColour (juce::TextButton::textColourOnId, controls::accent);
    alterButton.onClick = [this]
    {
        edit ([] (MeasureChord& c)
        {
            c.spec.altered = ! c.spec.altered;
            c.keyboardNotes.reset();

            if (c.spec.altered)
                clearAddedNote (c.spec);
        });
    };

    numeralBox.onChange = [this]
    {
        edit ([this] (MeasureChord& c) { c.spec.degree = numeralBox.getSelectedId() - 2; c.keyboardNotes.reset(); });
    };

    addedNoteBox.onChange = [this]
    {
        edit ([this] (MeasureChord& c)
        {
            c.keyboardNotes.reset();

            if (addedNoteBox.getSelectedId() > 1)
            {
                c.spec.addedNote = (music::AddedNote) (addedNoteBox.getSelectedId() - 1);
                c.spec.altered = false;
            }
            else
            {
                clearAddedNote (c.spec);
            }
        });
    };

    const std::array<const char*, 4> positionNames { "Root", "1st", "2nd", "3rd" };
    controls::makeSegmented ({ &positionButtons[0], &positionButtons[1], &positionButtons[2], &positionButtons[3] }, 1002);

    for (size_t i = 0; i < positionButtons.size(); ++i)
    {
        positionButtons[i].setButtonText (positionNames[i]);
        positionButtons[i].setTooltip (music::getPositionName ((int) i));
        positionButtons[i].onClick = [this, i] { edit ([i] (MeasureChord& c) { c.spec.inversion = (int) i; c.keyboardNotes.reset(); }); };
    }

    const std::array<juce::String, 3> octaveNames { controls::fromUTF8 ("\xe2\x86\x93 8ve"), "0", controls::fromUTF8 ("\xe2\x86\x91 8ve") };
    const std::array<const char*, 3> octaveTooltips { "Down an octave", "Written octave", "Up an octave" };
    controls::makeSegmented ({ &octaveButtons[0], &octaveButtons[1], &octaveButtons[2] }, 1003);

    for (size_t i = 0; i < octaveButtons.size(); ++i)
    {
        octaveButtons[i].setButtonText (octaveNames[i]);
        octaveButtons[i].setTooltip (octaveTooltips[i]);
        octaveButtons[i].onClick = [this, i] { edit ([i] (MeasureChord& c) { c.spec.octave = (int) i - 1; c.keyboardNotes.reset(); }); };
    }

    controls::makeSegmented ({ &trebleButton, &bassButton }, 1004);
    trebleButton.onClick = [this] { edit ([] (MeasureChord& c) { c.style.staff = Staff::treble; }); };
    bassButton.onClick = [this] { edit ([] (MeasureChord& c) { c.style.staff = Staff::bass; }); };

    chordTypeBox.addItemList ({ "Block", "Arpeggio (asc)", "Arpeggio (desc)", "Random", "Rolled chord" }, 1);
    chordTypeBox.onChange = [this]
    {
        edit ([this] (MeasureChord& c) { c.style.type = (music::ChordType) (chordTypeBox.getSelectedId() - 1); });
    };
    reshuffleButton.setWantsKeyboardFocus (false);
    reshuffleButton.setTooltip ("Picks a new random order");
    reshuffleButton.onClick = [this] { score.reshuffle (part, measure); };

    for (auto* label : { &nameLabel, &notesLabel, &fitLabel, &keyboardNotesLabel })
    {
        label->setBorderSize ({});
        label->setFont (juce::FontOptions (13.0f));
    }

    notesLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    keyboardNotesLabel.setColour (juce::Label::textColourId, controls::accent);
    keyboardNotesLabel.setFont (juce::FontOptions (12.0f));
    keyboardNotesLabel.setJustificationType (juce::Justification::topLeft);

    removeButton.setTooltip ("Takes the chord out of the measure");
    removeButton.onClick = [this] { score.setChord (part, measure, std::nullopt); };

    // Return or Escape closes the window, as well as Done.
    doneButton.addShortcut (juce::KeyPress (juce::KeyPress::returnKey));
    doneButton.addShortcut (juce::KeyPress (juce::KeyPress::escapeKey));
    doneButton.onClick = [this] { finish(); };

    for (auto* button : { &removeButton, &doneButton })
        button->setWantsKeyboardFocus (false);

    for (auto* component : std::initializer_list<juce::Component*> {
             &title, &flatButton, &majorButton, &minorButton, &alterButton, &numeralBox, &addedNoteBox,
             &positionButtons[0], &positionButtons[1], &positionButtons[2], &positionButtons[3],
             &octaveButtons[0], &octaveButtons[1], &octaveButtons[2], &trebleButton, &bassButton,
             &chordTypeHeading, &chordTypeBox, &reshuffleButton,
             &nameLabel, &notesLabel, &fitLabel, &keyboardNotesLabel, &removeButton, &doneButton })
        addAndMakeVisible (component);

    score.addChangeListener (this);
    setSize (getIdealWidth(), getIdealHeight());
    update();
}

ChordEditor::~ChordEditor()
{
    score.removeChangeListener (this);
}

juce::String ChordEditor::getTitle() const
{
    return "Chord" + controls::fromUTF8 (" \xc2\xb7 Part ") + juce::String (part + 1)
         + controls::fromUTF8 (" \xc2\xb7 Measure ") + juce::String (measure + 1);
}

void ChordEditor::loadChord()
{
    // A measure without a chord keeps the style chosen for its new one.
    if (const auto* existing = score.getChord (part, measure))
        chord = *existing;
    else
        chord = MeasureChord { {}, {}, chord.style, {}, {} };
}

void ChordEditor::toggleNote (int midiNote)
{
    edit ([this, midiNote] (MeasureChord& c) { score.toggleChordNote (c, midiNote); });
}

void ChordEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (measure >= score.getNumMeasures())
    {
        finish();
        return;
    }

    // Whatever changed the chord, the editor here or a click on the staff, it shows it as it is.
    // The key changes what the numerals and notes are called, too.
    loadChord();
    update();

    if (onChordChanged != nullptr)
        onChordChanged();
}

void ChordEditor::edit (const std::function<void (MeasureChord&)>& change)
{
    auto newChord = chord;
    change (newChord);

    if (newChord == chord)
        return;

    // A different chord, or a chord on the other staff, gives the alternate staff its own notes again.
    if (newChord.spec != chord.spec || newChord.keyboardNotes != chord.keyboardNotes || newChord.style.staff != chord.style.staff)
        newChord.alternateNotes.reset();

    // Every change goes straight into the score, which sends it back here to show.
    score.setChord (part, measure, newChord);
}

void ChordEditor::finish()
{
    score.removeChangeListener (this);

    if (onFinished != nullptr)
        onFinished();
}

void ChordEditor::update()
{
    const auto& spec = chord.spec;
    const auto& style = chord.style;
    const auto key = score.getKey();

    title.setText ("PART " + juce::String (part + 1) + controls::fromUTF8 (" \xc2\xb7 ") + "MEASURE " + juce::String (measure + 1),
                   juce::dontSendNotification);

    flatButton.setToggleState (spec.flat, juce::dontSendNotification);
    majorButton.setToggleState (! spec.minor, juce::dontSendNotification);
    minorButton.setToggleState (spec.minor, juce::dontSendNotification);

    // The + / ° button makes a major chord augmented or a minor chord diminished.
    alterButton.setButtonText (spec.minor ? controls::fromUTF8 ("\xc2\xb0") : juce::String ("+"));
    alterButton.setTooltip (spec.minor ? "Diminished" : "Augmented");
    alterButton.setToggleState (spec.altered, juce::dontSendNotification);

    // Each numeral names its chord in the current key, e.g. "vi (Am)".
    numeralBox.clear (juce::dontSendNotification);
    numeralBox.addItem (controls::fromUTF8 ("\xe2\x80\x94"), 1);

    for (int degree = 0; degree < 7; ++degree)
    {
        auto option = spec;
        option.degree = degree;
        option.inversion = 0;
        option.octave = 0;

        const auto named = music::createChord (key, option);
        numeralBox.addItem (named.label + controls::fromUTF8 ("\xe2\x80\x82(") + named.symbol + ")", degree + 2);
    }

    numeralBox.setSelectedId (spec.degree + 2, juce::dontSendNotification);

    addedNoteBox.clear (juce::dontSendNotification);
    addedNoteBox.addItem ("No added note", 1);

    for (auto choice : music::getAddedNoteChoices (spec.minor))
        addedNoteBox.addItem (juce::String (music::getInfo (choice).name) + " (" + music::getInfo (choice).numeral + ")", (int) choice + 1);

    addedNoteBox.setSelectedId ((int) spec.addedNote + 1, juce::dontSendNotification);

    for (size_t i = 0; i < positionButtons.size(); ++i)
    {
        positionButtons[i].setToggleState (spec.inversion == (int) i, juce::dontSendNotification);
        positionButtons[i].setEnabled (! spec.isEmpty() && (i < 3 || spec.addedNote != music::AddedNote::none));
    }

    for (size_t i = 0; i < octaveButtons.size(); ++i)
    {
        octaveButtons[i].setToggleState (spec.octave == (int) i - 1, juce::dontSendNotification);
        octaveButtons[i].setEnabled (! spec.isEmpty());
    }

    trebleButton.setToggleState (style.staff == Staff::treble, juce::dontSendNotification);
    bassButton.setToggleState (style.staff == Staff::bass, juce::dontSendNotification);
    chordTypeBox.setSelectedId ((int) style.type + 1, juce::dontSendNotification);
    reshuffleButton.setVisible (style.type == music::ChordType::random);
    reshuffleButton.setEnabled (chord.hasNotes());

    // The chord's tones, as the builder's chord tones table lists them
    const auto notes = score.getChordNotes (chord);

    if (notes.has_value())
    {
        juce::StringArray names, outside;

        for (const auto& tone : notes->tones)
        {
            names.add (tone.getNameWithOctave());

            if (! tone.inKey)
                outside.add (tone.getName());
        }

        nameLabel.setText (notes->name + (notes->inversion.has_value() ? controls::fromUTF8 (" \xc2\xb7 ") + music::getPositionName (*notes->inversion)
                                                                       : juce::String()),
                           juce::dontSendNotification);
        notesLabel.setText (names.joinIntoString ("  "), juce::dontSendNotification);
        fitLabel.setText (outside.isEmpty() ? "In key" : outside.joinIntoString (", ") + " outside key", juce::dontSendNotification);
        fitLabel.setColour (juce::Label::textColourId, outside.isEmpty() ? controls::secondaryText : controls::accent);
    }
    else
    {
        nameLabel.setText ("Choose a chord, or play its notes on the piano", juce::dontSendNotification);
        notesLabel.setText ({}, juce::dontSendNotification);
        fitLabel.setText ({}, juce::dontSendNotification);
    }

    const auto fromKeyboard = chord.keyboardNotes.has_value() && notes.has_value();
    keyboardNotesLabel.setVisible (fromKeyboard);

    if (fromKeyboard)
    {
        juce::StringArray names;

        for (const auto& tone : notes->tones)
            names.add (tone.getNameWithOctave());

        keyboardNotesLabel.setText ("Set on the keyboard: " + names.joinIntoString (" "), juce::dontSendNotification);
    }

    removeButton.setEnabled (score.getChord (part, measure) != nullptr);
    resized();
}

int ChordEditor::getIdealWidth() const
{
    return 340;
}

int ChordEditor::getIdealHeight() const
{
    // Enough for the keyboard notes too, which only show some of the time.
    return padding + 24 + gap
         + 3 * (rowHeight + gap) + 3 * (segmentHeight + gap)
         + sectionGap - gap + headingHeight + 2 + rowHeight
         + sectionGap + 3 * infoLineHeight + 34
         + sectionGap + buttonHeight + padding;
}

void ChordEditor::resized()
{
    auto bounds = getLocalBounds().reduced (padding);

    // Remove Chord on the left, and Done on the right, along the bottom
    auto bottomRow = bounds.removeFromBottom (buttonHeight);
    doneButton.setBounds (bottomRow.removeFromRight (90));
    removeButton.setBounds (bottomRow.removeFromLeft (116));

    auto titleRow = bounds.removeFromTop (24);
    flatButton.setBounds (titleRow.removeFromRight (74));
    title.setBounds (titleRow);

    bounds.removeFromTop (gap);

    auto qualityRow = bounds.removeFromTop (rowHeight);
    alterButton.setBounds (qualityRow.removeFromRight (40));
    qualityRow.removeFromRight (gap);
    majorButton.setBounds (qualityRow.removeFromLeft (qualityRow.getWidth() / 2));
    minorButton.setBounds (qualityRow);
    bounds.removeFromTop (gap);

    numeralBox.setBounds (bounds.removeFromTop (rowHeight));
    bounds.removeFromTop (gap);
    addedNoteBox.setBounds (bounds.removeFromTop (rowHeight));
    bounds.removeFromTop (gap);

    const auto layOutSegments = [&] (auto& buttons)
    {
        auto row = bounds.removeFromTop (segmentHeight);
        const auto width = row.getWidth() / (int) buttons.size();

        for (size_t i = 0; i < buttons.size(); ++i)
            (*buttons[i]).setBounds (i + 1 < buttons.size() ? row.removeFromLeft (width) : row);

        bounds.removeFromTop (gap);
    };

    std::array<juce::TextButton*, 4> positions { &positionButtons[0], &positionButtons[1], &positionButtons[2], &positionButtons[3] };
    std::array<juce::TextButton*, 3> octaves { &octaveButtons[0], &octaveButtons[1], &octaveButtons[2] };
    std::array<juce::TextButton*, 2> clefs { &trebleButton, &bassButton };
    layOutSegments (positions);
    layOutSegments (octaves);
    layOutSegments (clefs);

    bounds.removeFromTop (sectionGap - gap);
    chordTypeHeading.setBounds (bounds.removeFromTop (headingHeight));
    bounds.removeFromTop (2);
    auto typeRow = bounds.removeFromTop (rowHeight);

    if (reshuffleButton.isVisible())
    {
        reshuffleButton.setBounds (typeRow.removeFromRight (86));
        typeRow.removeFromRight (gap);
    }

    chordTypeBox.setBounds (typeRow);
    bounds.removeFromTop (sectionGap);

    nameLabel.setBounds (bounds.removeFromTop (infoLineHeight));
    notesLabel.setBounds (bounds.removeFromTop (infoLineHeight));
    fitLabel.setBounds (bounds.removeFromTop (infoLineHeight));

    if (keyboardNotesLabel.isVisible())
        keyboardNotesLabel.setBounds (bounds.removeFromTop (34));
}

//==============================================================================
ChordWindow::ChordWindow (std::unique_ptr<ChordEditor> editorToShow, std::function<void()> onClose)
    : DocumentWindow (editorToShow->getTitle(),
                      juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
                      DocumentWindow::closeButton),
      editor (*editorToShow),
      onCloseButtonPressed (std::move (onClose))
{
    setUsingNativeTitleBar (true);
    setContentOwned (editorToShow.release(), true);
    setResizable (false, false);

    // It stays in front of the main window, whose staff and piano can still be used with it open.
    setAlwaysOnTop (true);
}

void ChordWindow::closeButtonPressed()
{
    // Call a copy, as the callback is likely to delete this window, and the original with it.
    const auto callback = onCloseButtonPressed;
    callback();
}
