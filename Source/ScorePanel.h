#pragma once

#include "Score.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/** The settings for the whole score: its key, time signature and number of measures, and what
    the alternate staff shows in the active part.
*/
class ScorePanel final : public juce::Component,
                         private juce::ChangeListener
{
public:
    explicit ScorePanel (Score&);
    ~ScorePanel() override;

    std::function<void()> onAddMeasure, onRemoveMeasure;

    /** Chooses the part whose alternate staff the panel shows and sets. */
    void setPart (int);

    int getIdealHeight() const;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void update();

    Score& score;
    int part = 0;

    juce::Label keyHeading, signatureLabel, timeSignatureHeading, alternateHeading, alternateHint, measuresHeading;
    juce::ComboBox keyBox, timeSignatureBox, alternateBox;
    juce::TextButton addMeasureButton { "Add Measure" };
    juce::TextButton removeMeasureButton { "Remove Measure" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScorePanel)
};
