#pragma once

#include "Score.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/** The settings for the whole score, its key and time signature, and what the alternate staff
    shows in the active part.
*/
class ScorePanel final : public juce::Component,
                         private juce::ChangeListener
{
public:
    explicit ScorePanel (Score&);
    ~ScorePanel() override;

    /** Chooses the part whose alternate staff the panel shows and sets. */
    void setPart (int);

    int getIdealHeight() const;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void update();

    Score& score;
    int part = 0;

    juce::Label keyHeading, signatureLabel, timeSignatureHeading, alternateHeading, alternateHint;
    juce::ComboBox keyBox, timeSignatureBox, alternateBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScorePanel)
};
