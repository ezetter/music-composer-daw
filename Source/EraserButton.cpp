#include "EraserButton.h"

#include "Controls.h"

EraserButton::EraserButton()
    : juce::Button ("Eraser")
{
    setTooltip ("Eraser: hold the mouse button down and move over notes, ties, dynamics and hairpins to take them out");
    setWantsKeyboardFocus (false);
}

void EraserButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto ink = controls::drawSymbolButtonTile (g, *this, highlighted, down);
    const auto [outline, tip] = getEraserShape (getLocalBounds().toFloat().withSizeKeepingCentre (20.0f, 20.0f));

    g.setColour (ink);
    g.fillPath (tip);
    g.strokePath (outline, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved));
}

juce::MouseCursor EraserButton::createCursor()
{
    // Drawn at twice the size, for sharp edges on a Retina screen
    constexpr int size = 24, scale = 2;
    juce::Image image (juce::Image::ARGB, size * scale, size * scale, true);

    {
        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale ((float) scale));
        const auto [outline, tip] = getEraserShape ({ 1.0f, 1.0f, (float) size - 2.0f, (float) size - 2.0f });

        // White, edged in black, so it shows on the staff lines and the paper alike
        g.setColour (juce::Colours::white);
        g.fillPath (outline);
        g.setColour (juce::Colour (0xffd8413a));
        g.fillPath (tip);
        g.setColour (juce::Colours::black);
        g.strokePath (outline, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved));
    }

    // Rubbing with the end of the tip, at the bottom left
    return { juce::ScaledImage (image, (double) scale), { 4, size - 4 } };
}

std::pair<juce::Path, juce::Path> EraserButton::getEraserShape (juce::Rectangle<float> area)
{
    // A block a little under half as wide as it's long, turned up at 45 degrees, tip first
    const auto length = area.getWidth() * 1.05f, width = area.getWidth() * 0.42f;
    const auto block = juce::Rectangle<float> (length, width).withCentre (area.getCentre());
    const auto turn = juce::AffineTransform::rotation (-juce::MathConstants<float>::pi / 4.0f, area.getCentreX(), area.getCentreY());

    juce::Path outline;
    outline.addRoundedRectangle (block, width * 0.18f);
    outline.applyTransform (turn);

    juce::Path tip;
    const auto tipArea = block.withWidth (length * 0.36f);
    tip.addRoundedRectangle (tipArea.getX(), tipArea.getY(), tipArea.getWidth(), tipArea.getHeight(),
                             width * 0.18f, width * 0.18f, true, false, true, false);
    tip.applyTransform (turn);

    return { outline, tip };
}
