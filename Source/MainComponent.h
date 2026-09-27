#pragma once

#include "Score.h"
#include "StaffView.h"

#include <juce_audio_utils/juce_audio_utils.h>

/** The main window's contents: a toolbar, the scrolling grand staff, and a piano keyboard. */
class MainComponent final : public juce::Component
{
public:
    MainComponent();

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int minimumWidth = 640;
    static constexpr int minimumHeight = 540;

private:
    void addMeasure();
    void removeMeasure();
    void updateButtons();

    Score score;

    juce::TextButton addMeasureButton { "Add Measure" };
    juce::TextButton removeMeasureButton { "Remove Measure" };

    StaffView staffView { score };
    juce::Viewport staffViewport;

    // The keyboard doesn't make any sound yet. Its state is where MIDI input and plugins will connect.
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
