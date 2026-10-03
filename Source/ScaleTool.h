#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/** How notes from the key's scale are to be added: how many, how long, which way, and whether in
    order, from the note clicked, or at random.
*/
struct ScaleSettings
{
    static constexpr int minNotes = 3, maxNotes = 14;

    int numNotes = 8;
    int length = 8;             // in 32nds: a quarter note
    bool ascending = true;
    bool random = false;
};

/** The Scale button's settings: the number of notes, their length, ascending or descending, and
    Sequence and Random buttons, which each say to go and add the notes, in order or at random.
*/
class ScalePanel final : public juce::Component
{
public:
    explicit ScalePanel (const ScaleSettings&);

    /** Called when Sequence or Random is clicked, with the settings, random or not. */
    std::function<void (ScaleSettings)> onPlace;

    void resized() override;

private:
    ScaleSettings getSettings (bool random) const;

    juce::Label notesLabel, lengthLabel, directionLabel;
    juce::Slider notesSlider;
    juce::ComboBox lengthBox, directionBox;
    juce::TextButton sequenceButton { "Sequence" }, randomButton { "Random" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScalePanel)
};
