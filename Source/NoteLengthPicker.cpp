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
    // Whole, dotted half, half, quarter and eighth, longest first
    for (auto [index, beats, name] : { std::tuple { 0, 4.0, "Whole" }, { 1, 3.0, "Dotted half" }, { 2, 2.0, "Half" },
                                       { 3, 1.0, "Quarter" }, { 4, 0.5, "Eighth" } })
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

    setLength (1.0);
    setSize (getIdealBounds().getWidth(), getIdealBounds().getHeight());
}

void NoteLengthPicker::setLength (double beats)
{
    length = beats;
    setChoiceShown (true);
}

void NoteLengthPicker::setChoiceShown (bool shown)
{
    for (auto& button : buttons)
        button->setToggleState (shown && juce::exactlyEqual (button->beats, length), juce::dontSendNotification);
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
NoteLengthPicker::NoteButton::NoteButton (const MusicGlyphs& glyphsToUse, double beatsToShow)
    : juce::Button ({}),
      glyphs (glyphsToUse),
      beats (beatsToShow)
{
}

void NoteLengthPicker::NoteButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto ink = controls::drawSymbolButtonTile (g, *this, highlighted, down);

    // The note: an open notehead for a whole or half note, a filled one for a quarter or eighth,
    // a stem up for all but the whole note, a flag on the eighth's stem and a dot after a dotted half.
    const auto glyph = beats >= 4.0 ? Smufl::noteheadWhole : beats >= 2.0 ? Smufl::noteheadHalf : Smufl::noteheadBlack;
    const auto dotted = juce::exactlyEqual (beats, 3.0);
    const auto flagged = beats < 1.0;
    const auto headBounds = glyphs.getPath (glyph).getBounds();
    const auto hasStem = beats < 4.0;
    const auto stemLength = 2.8f * glyphStaffSpace;
    const auto noteHeight = headBounds.getHeight() + (hasStem ? stemLength - headBounds.getHeight() / 2.0f : 0.0f);

    // A dotted or flagged note is moved left a little, to leave room for its dot or flag.
    const auto centre = bounds.getCentre().translated (dotted || flagged ? -0.35f * glyphStaffSpace : 0.0f, 0.0f);
    const auto origin = juce::Point<float> (centre.x - headBounds.getCentreX(),
                                            centre.y + noteHeight / 2.0f - headBounds.getBottom());

    g.setColour (ink);
    glyphs.draw (g, glyph, origin);

    if (dotted)
        glyphs.draw (g, Smufl::augmentationDot, { origin.x + headBounds.getRight() + 0.3f * glyphStaffSpace, origin.y });

    if (hasStem)
    {
        const auto stemThickness = 0.13f * glyphStaffSpace * 1.6f;
        const auto right = origin.x + headBounds.getRight();
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (right - stemThickness, origin.y - stemLength,
                                                                right, origin.y - 0.168f * glyphStaffSpace));

        // The flag's anchor meets the top of the stem, from Bravura's metadata.
        if (flagged)
            glyphs.draw (g, Smufl::flag8thUp, { right - stemThickness, origin.y - stemLength - 0.04f * glyphStaffSpace });
    }
}
