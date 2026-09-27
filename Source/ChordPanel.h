#pragma once

#include "Score.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <optional>

/** Defines the chord in one measure, with the same settings as the Chord Progression Builder's
    chord panels, plus the clef, chord type and alternate staff it's written with.
*/
class ChordPanel final : public juce::Component,
                         private juce::ChangeListener
{
public:
    explicit ChordPanel (Score&);
    ~ChordPanel() override;

    /** The measure whose chord the panel shows and edits, if any. */
    void setMeasure (std::optional<int>);

    /** What the panel says when there's no measure to edit. */
    void setHint (const juce::String&);

    /** The style a measure's first chord is given: whichever was chosen last. */
    const ChordStyle& getStyleForNewChord() const noexcept { return lastStyle; }

    /** The height the panel needs at its fullest. */
    int getIdealHeight() const;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void update();
    void edit (const std::function<void (MeasureChord&)>&);

    Score& score;
    std::optional<int> measure;
    ChordStyle lastStyle;

    juce::Label title, hintLabel;
    juce::ToggleButton flatButton;
    juce::TextButton majorButton { "Major" }, minorButton { "Minor" }, alterButton;
    juce::ComboBox numeralBox, addedNoteBox;
    std::array<juce::TextButton, 4> positionButtons;
    std::array<juce::TextButton, 3> octaveButtons;
    juce::TextButton trebleButton { "Treble clef" }, bassButton { "Bass clef" };
    juce::Label chordTypeHeading, alternateHeading;
    juce::ComboBox chordTypeBox, alternateBox;
    juce::TextButton reshuffleButton { "Reshuffle" };
    juce::Label nameLabel, notesLabel, fitLabel, keyboardNotesLabel;
    juce::TextButton clearButton { "Clear Chord" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordPanel)
};
