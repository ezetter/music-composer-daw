#include "MainComponent.h"

namespace
{
    constexpr int toolbarHeight = 44;
    constexpr int keyboardHeight = 140;

    // A full 88-key piano, A0 to C8
    constexpr int lowestKey = 21;
    constexpr int highestKey = 108;
    constexpr int numWhiteKeys = 52;
}

MainComponent::MainComponent()
{
    addMeasureButton.onClick = [this] { addMeasure(); };
    removeMeasureButton.onClick = [this] { removeMeasure(); };
    addAndMakeVisible (addMeasureButton);
    addAndMakeVisible (removeMeasureButton);

    staffViewport.setViewedComponent (&staffView, false);
    staffViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (staffViewport);

    keyboard.setAvailableRange (lowestKey, highestKey);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (0xff4a8fe0));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour (0x264a8fe0));
    addAndMakeVisible (keyboard);

    updateButtons();
    setSize (1280, 680);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // The staff view doesn't cover the space for the scroll bar, so fill that in to match.
    g.setColour (StaffView::paperColour);
    g.fillRect (staffViewport.getBounds());

    g.setColour (juce::Colours::black.withAlpha (0.15f));
    g.fillRect (0, staffViewport.getY() - 1, getWidth(), 1);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    auto toolbar = bounds.removeFromTop (toolbarHeight).reduced (12, 8);
    addMeasureButton.setBounds (toolbar.removeFromLeft (120));
    toolbar.removeFromLeft (8);
    removeMeasureButton.setBounds (toolbar.removeFromLeft (140));

    keyboard.setBounds (bounds.removeFromBottom (keyboardHeight));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / (float) numWhiteKeys);

    staffViewport.setBounds (bounds);
    staffView.setSize (staffView.getContentWidth(),
                       juce::jmax (StaffView::getContentHeight(),
                                   staffViewport.getHeight() - staffViewport.getScrollBarThickness()));
}

void MainComponent::addMeasure()
{
    score.addMeasure();
    updateButtons();

    // Scroll to the end, so the new measure is in view.
    staffViewport.setViewPosition (staffView.getWidth(), 0);
}

void MainComponent::removeMeasure()
{
    score.removeLastMeasure();
    updateButtons();
}

void MainComponent::updateButtons()
{
    removeMeasureButton.setEnabled (score.getNumMeasures() > 1);
}
