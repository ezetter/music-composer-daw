#pragma once

#include "Score.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <optional>

/** Defines the chord in one measure, with the same settings as the Chord Progression Builder's
    chord panels, plus the clef and chord type it's written with.

    Every change goes straight into the score, as it's made. Remove Chord takes the measure's
    chord out.
*/
class ChordEditor final : public juce::Component,
                          private juce::ChangeListener
{
public:
    /** Edits the chord in a measure of one part, or starts a new one there in the given style. */
    ChordEditor (Score&, int part, int measure, const ChordStyle& styleForNewChord);
    ~ChordEditor() override;

    int getPart() const noexcept { return part; }
    int getMeasure() const noexcept { return measure; }

    /** The measure's chord, or the new one it would get, with the style chosen for it. */
    const MeasureChord& getChord() const noexcept { return chord; }

    /** Adds a note to the chord, or takes it out, as clicking a piano key does. */
    void toggleNote (int midiNote);

    /** Called whenever the chord being edited changes. */
    std::function<void()> onChordChanged;

    /** Called when editing's over: Done was clicked, or the measure's gone. The owner should
        close the editor then.
    */
    std::function<void()> onFinished;

    /** The title for a window showing the editor, such as "Chord · Instrument 1 · Measure 3". */
    juce::String getTitle() const;

    int getIdealWidth() const;
    int getIdealHeight() const;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void loadChord();
    void update();
    void edit (const std::function<void (MeasureChord&)>&);
    void finish();

    Score& score;
    const int part;
    const int measure;
    MeasureChord chord;

    juce::Label title;
    juce::ToggleButton flatButton;
    juce::TextButton majorButton { "Major" }, minorButton { "Minor" }, alterButton;
    juce::ComboBox numeralBox, addedNoteBox;
    std::array<juce::TextButton, 4> positionButtons;
    std::array<juce::TextButton, 3> octaveButtons;
    juce::TextButton trebleButton { "Treble clef" }, bassButton { "Bass clef" };
    juce::ComboBox chordTypeBox;            // beside the title
    juce::Label noteLengthLabel;
    juce::ComboBox noteLengthBox;           // how long the chord's notes are
    juce::TextButton reshuffleButton { "Reshuffle" };
    juce::Label nameLabel, notesLabel, fitLabel, keyboardNotesLabel;
    juce::TextButton removeButton { "Remove Chord" }, doneButton { "Done" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordEditor)
};

//==============================================================================
/** A window for a chord editor, which floats over the main window so the staff and piano
    can still be used.
*/
class ChordWindow final : public juce::DocumentWindow
{
public:
    /** The window owns the editor, and leaves its owner to delete it when it's closed. */
    ChordWindow (std::unique_ptr<ChordEditor>, std::function<void()> onCloseButtonPressed);

    ChordEditor& getEditor() const noexcept { return editor; }

    void closeButtonPressed() override;

private:
    ChordEditor& editor;
    std::function<void()> onCloseButtonPressed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordWindow)
};
