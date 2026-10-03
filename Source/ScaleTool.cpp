#include "ScaleTool.h"

#include <array>

namespace
{
    constexpr int rowHeight = 26, gap = 6, labelWidth = 84, padding = 12;

    /** The note lengths the notes can have, in 32nds, longest first, and their names */
    const std::array<std::pair<int, const char*>, 6> noteLengths { { { 32, "Whole notes" }, { 16, "Half notes" }, { 8, "Quarter notes" },
                                                                     { 4, "Eighth notes" }, { 2, "16th notes" }, { 1, "32nd notes" } } };
}

ScalePanel::ScalePanel (const ScaleSettings& settings)
{
    for (auto [label, text] : { std::pair { &notesLabel, "Notes" }, { &lengthLabel, "Note length" }, { &directionLabel, "Direction" } })
    {
        label->setText (text, juce::dontSendNotification);
        label->setFont (juce::FontOptions (13.0f));
        label->setBorderSize ({});
        addAndMakeVisible (label);
    }

    notesSlider.setSliderStyle (juce::Slider::IncDecButtons);
    notesSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, rowHeight);
    notesSlider.setRange (ScaleSettings::minNotes, ScaleSettings::maxNotes, 1);
    notesSlider.setValue (settings.numNotes, juce::dontSendNotification);
    notesSlider.setTooltip ("How many notes to add, from " + juce::String (ScaleSettings::minNotes) + " to " + juce::String (ScaleSettings::maxNotes));
    addAndMakeVisible (notesSlider);

    for (size_t i = 0; i < noteLengths.size(); ++i)
        lengthBox.addItem (noteLengths[i].second, (int) i + 1);

    for (size_t i = 0; i < noteLengths.size(); ++i)
        if (noteLengths[i].first == settings.length)
            lengthBox.setSelectedId ((int) i + 1, juce::dontSendNotification);

    addAndMakeVisible (lengthBox);

    directionBox.addItemList ({ "Ascending", "Descending" }, 1);
    directionBox.setSelectedId (settings.ascending ? 1 : 2, juce::dontSendNotification);
    directionBox.setTooltip ("Which way the notes go through the scale, for Sequence");
    addAndMakeVisible (directionBox);

    sequenceButton.setTooltip ("Then click on a staff: the notes go up or down the scale from the note clicked, one after another");
    randomButton.setTooltip ("Then click on a staff: the notes are picked from the scale at random, one after another, from where you click");
    sequenceButton.onClick = [this] { if (onPlace != nullptr) onPlace (getSettings (false)); };
    randomButton.onClick = [this] { if (onPlace != nullptr) onPlace (getSettings (true)); };

    for (auto* button : { &sequenceButton, &randomButton })
    {
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (button);
    }

    setSize (300, padding * 2 + 4 * rowHeight + 3 * gap + gap);
}

ScaleSettings ScalePanel::getSettings (bool random) const
{
    ScaleSettings settings;
    settings.numNotes = juce::roundToInt (notesSlider.getValue());
    settings.length = noteLengths[(size_t) juce::jlimit (0, (int) noteLengths.size() - 1, lengthBox.getSelectedId() - 1)].first;
    settings.ascending = directionBox.getSelectedId() != 2;
    settings.random = random;
    return settings;
}

void ScalePanel::resized()
{
    auto bounds = getLocalBounds().reduced (padding);

    for (auto [label, control] : { std::pair<juce::Label*, juce::Component*> { &notesLabel, &notesSlider }, { &lengthLabel, &lengthBox },
                                   { &directionLabel, &directionBox } })
    {
        auto row = bounds.removeFromTop (rowHeight);
        label->setBounds (row.removeFromLeft (labelWidth));
        control->setBounds (control == &notesSlider ? row.removeFromLeft (110) : row);
        bounds.removeFromTop (gap);
    }

    bounds.removeFromTop (gap);
    auto buttons = bounds.removeFromTop (rowHeight);
    sequenceButton.setBounds (buttons.removeFromLeft ((buttons.getWidth() - gap) / 2));
    buttons.removeFromLeft (gap);
    randomButton.setBounds (buttons);
}
