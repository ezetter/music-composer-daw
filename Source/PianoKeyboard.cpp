#include "PianoKeyboard.h"

#include "Controls.h"

PianoKeyboard::PianoKeyboard (juce::MidiKeyboardState& state)
    : MidiKeyboardComponent (state, horizontalKeyboard)
{
}

void PianoKeyboard::setHeldNotes (std::map<int, juce::String> notes)
{
    if (notes == heldNotes)
        return;

    heldNotes = std::move (notes);
    repaint();
}

bool PianoKeyboard::mouseDownOnKey (int midiNoteNumber, const juce::MouseEvent&)
{
    if (onKeyClicked != nullptr)
        onKeyClicked (midiNoteNumber);

    return true;
}

juce::String PianoKeyboard::getWhiteNoteText (int midiNoteNumber)
{
    if (const auto held = heldNotes.find (midiNoteNumber); held != heldNotes.end())
        return held->second;

    return MidiKeyboardComponent::getWhiteNoteText (midiNoteNumber);
}

void PianoKeyboard::drawWhiteNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                                   bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour)
{
    const auto held = heldNotes.count (midiNoteNumber) > 0;

    if (held)
    {
        g.setColour (controls::accentLight.darker (0.08f));
        g.fillRect (area);
    }

    MidiKeyboardComponent::drawWhiteNote (midiNoteNumber, g, area, isDown, isOver, lineColour,
                                          held && ! isDown ? controls::accent : textColour);
}

void PianoKeyboard::drawBlackNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                                   bool isDown, bool isOver, juce::Colour noteFillColour)
{
    const auto held = heldNotes.find (midiNoteNumber);

    MidiKeyboardComponent::drawBlackNote (midiNoteNumber, g, area, isDown, isOver,
                                          held != heldNotes.end() ? controls::accent : noteFillColour);

    if (held != heldNotes.end())
    {
        // Black keys are narrow, so the name is squeezed to fit.
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (11.0f));
        g.drawFittedText (held->second, area.withTrimmedBottom (3.0f).toNearestInt(), juce::Justification::centredBottom, 1, 0.5f);
    }
}
