#pragma once

#include "Score.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/** The settings for the whole score, its key and time signature, and for the active part, what
    its alternate staff shows and a button to copy its chord progression to another part: the
    other one, when there are two, or one chosen from a menu, when there are more.
*/
class ScorePanel final : public juce::Component,
                         private juce::ChangeListener
{
public:
    explicit ScorePanel (Score&);
    ~ScorePanel() override;

    /** Chooses the part whose alternate staff the panel shows and sets, and whose progression it copies. */
    void setPart (int);

    /** Called when Copy Progression is clicked, to copy the part's chords to another part. */
    std::function<void (int toPart)> onCopyProgression;

    /** The parts Copy Progression offers to copy to, in its menu when there's more than one. */
    juce::PopupMenu getCopyTargetsMenu() const;

    int getIdealHeight() const;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void update();

    Score& score;
    int part = 0;

    juce::Label keyHeading, signatureLabel, timeSignatureHeading, alternateHeading, alternateHint, progressionHeading, progressionHint;
    juce::ComboBox keyBox, timeSignatureBox, alternateBox;
    juce::TextButton copyProgressionButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScorePanel)
};
