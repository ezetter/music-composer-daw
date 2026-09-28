#pragma once

#include "Score.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/** The settings for the whole score: its key, time signature, what the alternate staff shows,
    and the number of measures.
*/
class ScorePanel final : public juce::Component,
                         private juce::ChangeListener
{
public:
    explicit ScorePanel (Score&);
    ~ScorePanel() override;

    std::function<void()> onAddMeasure, onRemoveMeasure;

    int getIdealHeight() const;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void update();

    Score& score;

    juce::Label keyHeading, signatureLabel, timeSignatureHeading, alternateHeading, alternateHint, measuresHeading;
    juce::ComboBox keyBox, timeSignatureBox, alternateBox;
    juce::TextButton addMeasureButton { "Add Measure" };
    juce::TextButton removeMeasureButton { "Remove Measure" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScorePanel)
};
