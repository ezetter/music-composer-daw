#include "ScorePanel.h"

#include "Controls.h"

namespace
{
    constexpr int headingHeight = 16;
    constexpr int controlHeight = 28;
    constexpr int sectionGap = 12;
}

ScorePanel::ScorePanel (Score& scoreToEdit)
    : score (scoreToEdit)
{
    controls::makeHeading (keyHeading, "Key");
    controls::makeHeading (timeSignatureHeading, "Time signature");
    controls::makeHeading (alternateHeading, "Alternate staff");
    controls::makeHeading (measuresHeading, "Measures");

    for (const auto& key : music::getMajorKeys())
        keyBox.addItem (music::getKeyName (key), keyBox.getNumItems() + 1);

    keyBox.onChange = [this] { score.setKeyIndex (keyBox.getSelectedId() - 1); };

    signatureLabel.setFont (juce::FontOptions (12.0f));
    signatureLabel.setColour (juce::Label::textColourId, controls::secondaryText);
    signatureLabel.setBorderSize ({});

    for (auto beats : { 2, 3, 4 })
        timeSignatureBox.addItem (juce::String (beats) + "/4", beats);

    timeSignatureBox.onChange = [this] { score.setBeatsPerMeasure (timeSignatureBox.getSelectedId()); };

    alternateBox.addItemList ({ "None", "Root", "Octave", "Block chord", "Rolled chord" }, 1);
    alternateBox.setTooltip ("What the other staff shows under or over every chord");
    alternateBox.onChange = [this] { score.setAlternateStaff ((music::AlternateStaff) (alternateBox.getSelectedId() - 1)); };

    alternateHint.setText ("For every chord", juce::dontSendNotification);
    alternateHint.setFont (juce::FontOptions (12.0f));
    alternateHint.setColour (juce::Label::textColourId, controls::secondaryText);
    alternateHint.setBorderSize ({});

    addMeasureButton.onClick = [this] { if (onAddMeasure != nullptr) onAddMeasure(); };
    removeMeasureButton.onClick = [this] { if (onRemoveMeasure != nullptr) onRemoveMeasure(); };

    for (auto* component : std::initializer_list<juce::Component*> { &keyHeading, &keyBox, &signatureLabel, &timeSignatureHeading,
                                                                     &timeSignatureBox, &alternateHeading, &alternateBox, &alternateHint, &measuresHeading, &addMeasureButton,
                                                                     &removeMeasureButton })
        addAndMakeVisible (component);

    addMeasureButton.setWantsKeyboardFocus (false);
    removeMeasureButton.setWantsKeyboardFocus (false);

    score.addChangeListener (this);
    update();
}

ScorePanel::~ScorePanel()
{
    score.removeChangeListener (this);
}

int ScorePanel::getIdealHeight() const
{
    return 4 * (headingHeight + 2) + 3 * controlHeight + 2 * (16 + 2) + 26 + 3 * sectionGap;
}

void ScorePanel::resized()
{
    auto bounds = getLocalBounds();

    keyHeading.setBounds (bounds.removeFromTop (headingHeight));
    bounds.removeFromTop (2);
    keyBox.setBounds (bounds.removeFromTop (controlHeight));
    bounds.removeFromTop (2);
    signatureLabel.setBounds (bounds.removeFromTop (16));
    bounds.removeFromTop (sectionGap);

    timeSignatureHeading.setBounds (bounds.removeFromTop (headingHeight));
    bounds.removeFromTop (2);
    timeSignatureBox.setBounds (bounds.removeFromTop (controlHeight).removeFromLeft (100));
    bounds.removeFromTop (sectionGap);

    alternateHeading.setBounds (bounds.removeFromTop (headingHeight));
    bounds.removeFromTop (2);
    alternateBox.setBounds (bounds.removeFromTop (controlHeight));
    bounds.removeFromTop (2);
    alternateHint.setBounds (bounds.removeFromTop (16));
    bounds.removeFromTop (sectionGap);

    measuresHeading.setBounds (bounds.removeFromTop (headingHeight));
    bounds.removeFromTop (2);
    auto buttons = bounds.removeFromTop (26);
    const auto buttonWidth = (buttons.getWidth() - 8) / 2;
    addMeasureButton.setBounds (buttons.removeFromLeft (buttonWidth));
    removeMeasureButton.setBounds (buttons.removeFromRight (buttonWidth));
}

void ScorePanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    update();
}

void ScorePanel::update()
{
    keyBox.setSelectedId (score.getKeyIndex() + 1, juce::dontSendNotification);
    signatureLabel.setText (music::getKeySignatureText (score.getKey()), juce::dontSendNotification);
    timeSignatureBox.setSelectedId (score.getBeatsPerMeasure(), juce::dontSendNotification);
    alternateBox.setSelectedId ((int) score.getAlternateStaff() + 1, juce::dontSendNotification);
    removeMeasureButton.setEnabled (score.getNumMeasures() > 1);
}
