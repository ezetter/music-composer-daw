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
    playButton.onClick = [this] { togglePlayback(); };
    playButton.addShortcut (juce::KeyPress (juce::KeyPress::spaceKey));
    addMeasureButton.onClick = [this] { addMeasure(); };
    removeMeasureButton.onClick = [this] { removeMeasure(); };

    for (auto* button : { &playButton, &addMeasureButton, &removeMeasureButton })
    {
        // Clicking a button shouldn't take the keyboard focus away from the piano, which the
        // computer keyboard can play too.
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (button);
    }

    addAndMakeVisible (instrumentPanel);

    staffViewport.setViewedComponent (&staffView, false);
    staffViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (staffViewport);

    keyboard.setAvailableRange (lowestKey, highestKey);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setKeyPressBaseOctave (5);     // the computer keyboard's A key plays middle C
    keyboard.setScrollButtonsVisible (false);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (0xff4a8fe0));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour (0x264a8fe0));
    addAndMakeVisible (keyboard);

    updateButtons();
    setSize (1280, 680);

    juce::AudioDeviceManager::AudioDeviceSetup preferredSetup;
    preferredSetup.bufferSize = 256;    // small enough for the keyboard to feel immediate

    if (const auto error = audioDeviceManager.initialise (0, 2, nullptr, true, {}, &preferredSetup); error.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't open the audio output", error);

    audioDeviceManager.addAudioCallback (&instrumentHost);
}

MainComponent::~MainComponent()
{
    audioDeviceManager.removeAudioCallback (&instrumentHost);
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
    playButton.setBounds (toolbar.removeFromLeft (80));
    toolbar.removeFromLeft (16);
    addMeasureButton.setBounds (toolbar.removeFromLeft (120));
    toolbar.removeFromLeft (8);
    removeMeasureButton.setBounds (toolbar.removeFromLeft (140));
    toolbar.removeFromLeft (16);
    instrumentPanel.setBounds (toolbar);

    keyboard.setBounds (bounds.removeFromBottom (keyboardHeight));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / (float) numWhiteKeys);

    staffViewport.setBounds (bounds);
    staffView.setSize (staffView.getContentWidth(),
                       juce::jmax (StaffView::getContentHeight(),
                                   staffViewport.getHeight() - staffViewport.getScrollBarThickness()));
}

void MainComponent::timerCallback()
{
    showPlaybackPosition();
}

void MainComponent::togglePlayback()
{
    if (instrumentHost.getPlaybackPosition().has_value())
    {
        instrumentHost.stop();
    }
    else
    {
        instrumentHost.play (score);
        startTimerHz (30);
    }

    showPlaybackPosition();
}

void MainComponent::showPlaybackPosition()
{
    const auto position = instrumentHost.getPlaybackPosition();

    playButton.setButtonText (position.has_value() ? "Stop" : "Play");
    staffView.setPlaybackPosition (position);

    if (! position.has_value())
    {
        stopTimer();
        return;
    }

    // When the beat that's playing goes out of view, scroll it back to a quarter of the way across.
    const auto playingArea = staffView.getPlaybackArea();
    const auto viewArea = staffViewport.getViewArea();

    if (playingArea.getX() < viewArea.getX() || playingArea.getRight() > viewArea.getRight())
        staffViewport.setViewPosition (playingArea.getX() - viewArea.getWidth() / 4, 0);
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
