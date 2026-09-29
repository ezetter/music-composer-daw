#include "NoteLengthPicker.h"

#include "Controls.h"

namespace
{
    constexpr float glyphStaffSpace = 6.5f;     // the notes are drawn as if on a staff this size
    constexpr int gap = 3;
}

NoteLengthPicker::NoteLengthPicker()
    : glyphs (glyphStaffSpace)
{
    // Whole, dotted half, half and quarter, longest first
    for (auto [index, beats, name] : { std::tuple { 0, 4, "Whole" }, { 1, 3, "Dotted half" }, { 2, 2, "Half" }, { 3, 1, "Quarter" } })
    {
        auto& button = buttons[(size_t) index];
        button = std::make_unique<NoteButton> (glyphs, beats);
        button->setTooltip (juce::String (name) + " notes: click the staff to add " + juce::String (name).toLowerCase() + " notes");
        button->setClickingTogglesState (true);
        button->setRadioGroupId (1);
        button->setWantsKeyboardFocus (false);
        button->onClick = [this, chosen = beats]
        {
            length = chosen;

            if (onChange != nullptr)
                onChange (chosen);
        };
        addAndMakeVisible (*button);
    }

    setLength (1);
    setSize (getIdealBounds().getWidth(), getIdealBounds().getHeight());
}

void NoteLengthPicker::setLength (int beats)
{
    length = beats;

    for (auto& button : buttons)
        button->setToggleState (button->beats == beats, juce::dontSendNotification);
}

juce::Rectangle<int> NoteLengthPicker::getIdealBounds() const
{
    return { (int) buttons.size() * buttonSize + ((int) buttons.size() - 1) * gap, buttonSize };
}

void NoteLengthPicker::resized()
{
    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds ((int) i * (buttonSize + gap), 0, buttonSize, buttonSize);
}

//==============================================================================
NoteLengthPicker::NoteButton::NoteButton (const MusicGlyphs& glyphsToUse, int beatsToShow)
    : juce::Button ({}),
      glyphs (glyphsToUse),
      beats (beatsToShow)
{
}

void NoteLengthPicker::NoteButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto chosen = getToggleState();

    g.setColour (chosen ? controls::accentLight : down ? juce::Colour (0xffe8e8ec) : highlighted ? juce::Colour (0xfff2f2f5) : juce::Colours::white);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (chosen ? controls::accent.withAlpha (0.6f) : juce::Colours::black.withAlpha (0.18f));
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

    // The note: an open notehead for a whole or half note, a filled one for a quarter, a stem up
    // for all but the whole note, and a dot after a dotted half.
    const auto glyph = beats >= 4 ? Smufl::noteheadWhole : beats >= 2 ? Smufl::noteheadHalf : Smufl::noteheadBlack;
    const auto dotted = beats == 3;
    const auto headBounds = glyphs.getPath (glyph).getBounds();
    const auto hasStem = beats < 4;
    const auto stemLength = 2.8f * glyphStaffSpace;
    const auto noteHeight = headBounds.getHeight() + (hasStem ? stemLength - headBounds.getHeight() / 2.0f : 0.0f);

    // A dotted note is moved left a little, to leave room for its dot.
    const auto centre = bounds.getCentre().translated (dotted ? -0.35f * glyphStaffSpace : 0.0f, 0.0f);
    const auto origin = juce::Point<float> (centre.x - headBounds.getCentreX(),
                                            centre.y + noteHeight / 2.0f - headBounds.getBottom());

    g.setColour (chosen ? controls::accent : juce::Colour (0xff1b1b1b));
    glyphs.draw (g, glyph, origin);

    if (dotted)
        glyphs.draw (g, Smufl::augmentationDot, { origin.x + headBounds.getRight() + 0.3f * glyphStaffSpace, origin.y });

    if (hasStem)
    {
        const auto stemThickness = 0.13f * glyphStaffSpace * 1.6f;
        const auto right = origin.x + headBounds.getRight();
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (right - stemThickness, origin.y - stemLength,
                                                                right, origin.y - 0.168f * glyphStaffSpace));
    }
}
