#include "ChordPanel.h"

#include "Controls.h"

namespace
{
    constexpr int rowHeight = 28;
    constexpr int segmentHeight = 26;
    constexpr int headingHeight = 16;
    constexpr int gap = 6;
    constexpr int sectionGap = 12;
    constexpr int infoLineHeight = 18;

    /** Drops a chord's 7th or 9th, and its 3rd inversion, which only 7th chords have. */
    void clearAddedNote (music::ChordSpec& spec)
    {
        spec.addedNote = music::AddedNote::none;

        if (spec.inversion == 3)
            spec.inversion = 0;
    }
}

ChordPanel::ChordPanel (Score& scoreToEdit)
    : score (scoreToEdit)
{
    controls::makeHeading (title, "Chord");
    controls::makeHeading (chordTypeHeading, "Chord type");

    hintLabel.setFont (juce::FontOptions (12.5f));
    hintLabel.setColour (juce::Label::textColourId, controls::secondaryText);
    hintLabel.setBorderSize ({});
    hintLabel.setJustificationType (juce::Justification::topLeft);

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
    reshuffleButton.onClick = [this] { if (measure.has_value()) score.reshuffle (*measure); };

    for (auto* label : { &nameLabel, &notesLabel, &fitLabel, &keyboardNotesLabel })
    {
        label->setBorderSize ({});
        label->setFont (juce::FontOptions (13.0f));
    }

    notesLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    keyboardNotesLabel.setColour (juce::Label::textColourId, controls::accent);
    keyboardNotesLabel.setFont (juce::FontOptions (12.0f));
    keyboardNotesLabel.setJustificationType (juce::Justification::topLeft);

    clearButton.setWantsKeyboardFocus (false);
    clearButton.onClick = [this] { if (measure.has_value()) score.setChord (*measure, std::nullopt); };

    for (auto* component : std::initializer_list<juce::Component*> {
             &title, &hintLabel, &flatButton, &majorButton, &minorButton, &alterButton, &numeralBox, &addedNoteBox,
             &positionButtons[0], &positionButtons[1], &positionButtons[2], &positionButtons[3],
             &octaveButtons[0], &octaveButtons[1], &octaveButtons[2], &trebleButton, &bassButton,
             &chordTypeHeading, &chordTypeBox, &reshuffleButton,
             &nameLabel, &notesLabel, &fitLabel, &keyboardNotesLabel, &clearButton })
        addAndMakeVisible (component);

    score.addChangeListener (this);
    update();
}

ChordPanel::~ChordPanel()
{
    score.removeChangeListener (this);
}

void ChordPanel::setMeasure (std::optional<int> newMeasure)
{
    measure = newMeasure;
    update();
}

void ChordPanel::setHint (const juce::String& hint)
{
    hintLabel.setText (hint, juce::dontSendNotification);
}

void ChordPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (measure.has_value() && *measure >= score.getNumMeasures())
        measure.reset();

    update();
}

void ChordPanel::edit (const std::function<void (MeasureChord&)>& change)
{
    if (! measure.has_value())
        return;

    const auto* existing = score.getChord (*measure);
    auto chord = existing != nullptr ? *existing : MeasureChord { {}, {}, lastStyle, {}, {} };

    const auto before = chord;
    change (chord);
    lastStyle = chord.style;

    // A different chord, or a chord on the other staff, gives the alternate staff its own notes again.
    if (chord.spec != before.spec || chord.keyboardNotes != before.keyboardNotes || chord.style.staff != before.style.staff)
        chord.alternateNotes.reset();

    score.setChord (*measure, chord);
}

void ChordPanel::update()
{
    const auto* chord = measure.has_value() ? score.getChord (*measure) : nullptr;
    const auto spec = chord != nullptr ? chord->spec : music::ChordSpec {};
    const auto style = chord != nullptr ? chord->style : lastStyle;
    const auto editable = measure.has_value();
    const auto key = score.getKey();

    title.setText (measure.has_value() ? "CHORD " + controls::fromUTF8 ("\xc2\xb7") + " MEASURE " + juce::String (*measure + 1)
                                       : juce::String ("CHORD"),
                   juce::dontSendNotification);
    hintLabel.setVisible (! editable);

    for (auto* component : std::initializer_list<juce::Component*> {
             &flatButton, &majorButton, &minorButton, &alterButton, &numeralBox, &addedNoteBox,
             &trebleButton, &bassButton, &chordTypeBox })
        component->setEnabled (editable);

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
        positionButtons[i].setEnabled (editable && ! spec.isEmpty() && (i < 3 || spec.addedNote != music::AddedNote::none));
    }

    for (size_t i = 0; i < octaveButtons.size(); ++i)
    {
        octaveButtons[i].setToggleState (spec.octave == (int) i - 1, juce::dontSendNotification);
        octaveButtons[i].setEnabled (editable && ! spec.isEmpty());
    }

    trebleButton.setToggleState (style.staff == Staff::treble, juce::dontSendNotification);
    bassButton.setToggleState (style.staff == Staff::bass, juce::dontSendNotification);
    chordTypeBox.setSelectedId ((int) style.type + 1, juce::dontSendNotification);
    reshuffleButton.setVisible (style.type == music::ChordType::random);
    reshuffleButton.setEnabled (editable && chord != nullptr && chord->hasNotes());

    // The chord's tones, as the builder's chord tones table lists them
    const auto notes = measure.has_value() ? score.getChordNotes (*measure) : std::nullopt;

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
        nameLabel.setText (editable ? "No chord in this measure" : "", juce::dontSendNotification);
        notesLabel.setText ({}, juce::dontSendNotification);
        fitLabel.setText ({}, juce::dontSendNotification);
    }

    const auto fromKeyboard = chord != nullptr && chord->keyboardNotes.has_value() && notes.has_value();
    keyboardNotesLabel.setVisible (fromKeyboard);

    if (fromKeyboard)
    {
        juce::StringArray names;

        for (const auto& tone : notes->tones)
            names.add (tone.getNameWithOctave());

        keyboardNotesLabel.setText ("Set on the keyboard: " + names.joinIntoString (" "), juce::dontSendNotification);
    }

    clearButton.setEnabled (editable && chord != nullptr);
    resized();
}

int ChordPanel::getIdealHeight() const
{
    // Enough for the hint and the keyboard notes too, which only show some of the time.
    return 24 + 38 + gap
         + 3 * (rowHeight + gap) + 3 * (segmentHeight + gap)
         + sectionGap - gap + headingHeight + 2 + rowHeight
         + sectionGap + 3 * infoLineHeight + 34
         + 8 + 26;
}

void ChordPanel::resized()
{
    auto bounds = getLocalBounds();

    auto titleRow = bounds.removeFromTop (24);
    flatButton.setBounds (titleRow.removeFromRight (74));
    title.setBounds (titleRow);

    if (hintLabel.isVisible())
        hintLabel.setBounds (bounds.removeFromTop (38));

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

    bounds.removeFromTop (8);
    clearButton.setBounds (bounds.removeFromTop (26).removeFromLeft (110));
}
