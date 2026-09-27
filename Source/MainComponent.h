#pragma once

#include "InstrumentHost.h"
#include "InstrumentPanel.h"
#include "Score.h"
#include "StaffView.h"

#include <juce_audio_utils/juce_audio_utils.h>

/** The main window's contents: a toolbar, the scrolling grand staff, and a piano keyboard, all
    playing through an instrument plugin.
*/
class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int minimumWidth = 900;
    static constexpr int minimumHeight = 540;

private:
    void timerCallback() override;
    void togglePlayback();
    void showPlaybackPosition();
    void addMeasure();
    void removeMeasure();
    void updateButtons();

    juce::AudioDeviceManager audioDeviceManager;
    InstrumentHost instrumentHost;
    Score score;

    juce::TextButton playButton { "Play" };
    juce::TextButton addMeasureButton { "Add Measure" };
    juce::TextButton removeMeasureButton { "Remove Measure" };
    InstrumentPanel instrumentPanel { instrumentHost };

    StaffView staffView { score };
    juce::Viewport staffViewport;

    juce::MidiKeyboardComponent keyboard { instrumentHost.getKeyboardState(),
                                           juce::MidiKeyboardComponent::horizontalKeyboard };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
