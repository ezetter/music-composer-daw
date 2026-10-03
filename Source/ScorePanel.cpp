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
    controls::makeHeading (progressionHeading, "Progression");

    // The key is its tonic's letter, with Major or Minor beside it.
    keyBox.setTooltip ("The key's tonic");
    keyBox.onChange = [this] { score.setKeyIndex (keyBox.getSelectedId() - 1); };
    controls::makeSegmented ({ &majorButton, &minorButton }, 2001);
    majorButton.setTooltip ("A major key");
    minorButton.setTooltip ("A minor key, with the key signature of the major key a minor third above, and the natural minor scale");
    majorButton.onClick = [this] { score.setMinor (false); };
    minorButton.onClick = [this] { score.setMinor (true); };

    scaleButton.setWantsKeyboardFocus (false);
    scaleButton.setTooltip ("Add notes from the key's scale, in order or at random, where you click on a staff");
    scaleButton.onClick = [this] { if (onScale != nullptr) onScale (scaleButton); };

    signatureLabel.setFont (juce::FontOptions (12.0f));
    signatureLabel.setColour (juce::Label::textColourId, controls::secondaryText);
    signatureLabel.setBorderSize ({});

    for (auto beats : { 2, 3, 4 })
        timeSignatureBox.addItem (juce::String (beats) + "/4", beats);

    timeSignatureBox.onChange = [this] { score.setBeatsPerMeasure (timeSignatureBox.getSelectedId()); };

    alternateBox.addItemList ({ "None", "Root", "Octave", "Block chord", "Rolled chord" }, 1);
    alternateBox.setTooltip ("What the other staff shows under or over every chord");
    alternateBox.onChange = [this] { score.setAlternateStaff (part, (music::AlternateStaff) (alternateBox.getSelectedId() - 1)); };

    for (auto* hint : { &alternateHint, &progressionHint })
    {
        hint->setFont (juce::FontOptions (12.0f));
        hint->setColour (juce::Label::textColourId, controls::secondaryText);
        hint->setBorderSize ({});
        hint->setJustificationType (juce::Justification::topLeft);
        hint->setMinimumHorizontalScale (1.0f);     // wrapped onto a second line, rather than squeezed
    }

    copyProgressionButton.setWantsKeyboardFocus (false);
    copyProgressionButton.onClick = [this]
    {
        // With two parts it goes to the other one, and with more, to the one chosen.
        if (score.getNumParts() == 2)
        {
            if (onCopyProgression != nullptr)
                onCopyProgression (1 - part);

            return;
        }

        getCopyTargetsMenu().showMenuAsync (juce::PopupMenu::Options().withTargetComponent (copyProgressionButton),
                                            [safeThis = juce::Component::SafePointer (this)] (int result)
                                            {
                                                if (safeThis != nullptr && result > 0 && safeThis->onCopyProgression != nullptr)
                                                    safeThis->onCopyProgression (result - 1);
                                            });
    };

    for (auto* component : std::initializer_list<juce::Component*> { &keyHeading, &keyBox, &majorButton, &minorButton, &signatureLabel, &timeSignatureHeading,
                                                                     &timeSignatureBox, &alternateHeading, &alternateBox, &alternateHint,
                                                                     &progressionHeading, &copyProgressionButton, &progressionHint, &scaleButton })
        addAndMakeVisible (component);

    score.addChangeListener (this);
    update();
}

void ScorePanel::setPart (int newPart)
{
    part = newPart;
    update();
}

ScorePanel::~ScorePanel()
{
    score.removeChangeListener (this);
}

int ScorePanel::getIdealHeight() const
{
    return 4 * (headingHeight + 2) + 5 * controlHeight + 2 * (16 + 2) + 2 + 32 + 4 + 3 * sectionGap;
}

void ScorePanel::resized()
{
    auto bounds = getLocalBounds();

    keyHeading.setBounds (bounds.removeFromTop (headingHeight));
    bounds.removeFromTop (2);
    auto keyRow = bounds.removeFromTop (controlHeight);
    keyBox.setBounds (keyRow.removeFromLeft (90));
    keyRow.removeFromLeft (8);
    majorButton.setBounds (keyRow.removeFromLeft (keyRow.getWidth() / 2));
    minorButton.setBounds (keyRow);
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

    progressionHeading.setBounds (bounds.removeFromTop (headingHeight));
    bounds.removeFromTop (2);
    copyProgressionButton.setBounds (bounds.removeFromTop (controlHeight));
    bounds.removeFromTop (2);
    progressionHint.setBounds (bounds.removeFromTop (32));
    bounds.removeFromTop (4);
    scaleButton.setBounds (bounds.removeFromTop (controlHeight));
}

void ScorePanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    update();
}

juce::PopupMenu ScorePanel::getCopyTargetsMenu() const
{
    // Each part's item is its number, counting from 1.
    juce::PopupMenu menu;

    for (int target = 0; target < score.getNumParts(); ++target)
        if (target != part)
            menu.addItem (target + 1, "Instrument " + juce::String (target + 1));

    return menu;
}

void ScorePanel::update()
{
    // The active part may have just been taken out, until another's chosen.
    part = juce::jlimit (0, score.getNumParts() - 1, part);

    // The keys' tonics, for the mode the key's in
    keyBox.clear (juce::dontSendNotification);

    for (const auto& key : music::getMajorKeys())
        keyBox.addItem ((score.isMinor() ? music::getRelativeMinor (key) : key).getName(), keyBox.getNumItems() + 1);

    keyBox.setSelectedId (score.getKeyIndex() + 1, juce::dontSendNotification);
    majorButton.setToggleState (! score.isMinor(), juce::dontSendNotification);
    minorButton.setToggleState (score.isMinor(), juce::dontSendNotification);
    signatureLabel.setText (music::getKeySignatureText (score.getKey()), juce::dontSendNotification);
    timeSignatureBox.setSelectedId (score.getBeatsPerMeasure(), juce::dontSendNotification);
    alternateBox.setSelectedId ((int) score.getAlternateStaff (part) + 1, juce::dontSendNotification);
    alternateHint.setText ("For every chord of instrument " + juce::String (part + 1), juce::dontSendNotification);

    // The progression goes from this part to the other, or to one chosen from a menu.
    const auto from = juce::String (part + 1);

    if (score.getNumParts() == 1)
    {
        copyProgressionButton.setButtonText ("Copy Progression");
        copyProgressionButton.setEnabled (false);
        copyProgressionButton.setTooltip ({});
        progressionHint.setText ("Add another instrument to copy instrument " + from + "'s chords to", juce::dontSendNotification);
    }
    else if (score.getNumParts() == 2)
    {
        const auto to = juce::String (2 - part);
        copyProgressionButton.setButtonText ("Copy Progression to Instrument " + to);
        copyProgressionButton.setEnabled (score.hasChords (part));
        copyProgressionButton.setTooltip ("Give instrument " + to + " instrument " + from + "'s chords, in place of its own notes and chords");
        progressionHint.setText ("Replaces instrument " + to + "'s notes and chords with instrument " + from + "'s chords",
                                 juce::dontSendNotification);
    }
    else
    {
        copyProgressionButton.setButtonText ("Copy Progression To" + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6")));
        copyProgressionButton.setEnabled (score.hasChords (part));
        copyProgressionButton.setTooltip ("Give an instrument you choose instrument " + from + "'s chords, in place of its own notes and chords");
        progressionHint.setText ("Replaces the chosen instrument's notes and chords with instrument " + from + "'s chords",
                                 juce::dontSendNotification);
    }
}
