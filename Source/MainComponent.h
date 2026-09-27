#pragma once

#include "ChordPanel.h"
#include "InstrumentHost.h"
#include "InstrumentPanel.h"
#include "PianoKeyboard.h"
#include "Score.h"
#include "ScorePanel.h"
#include "StaffView.h"

#include <juce_audio_utils/juce_audio_utils.h>

/** The main window's contents: a toolbar, a sidebar with the score's and chords' settings, the
    scrolling grand staff, and a piano keyboard, all playing through an instrument plugin.
*/
class MainComponent final : public juce::Component,
                            private juce::Timer,
                            private juce::ChangeListener,
                            private juce::KeyListener
{
public:
    /** The settings are where the app keeps what it needs next time, such as the instrument. */
    explicit MainComponent (juce::PropertiesFile& settings);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

    static constexpr int minimumWidth = 1000;
    static constexpr int minimumHeight = 640;

private:
    using InputMode = StaffView::InputMode;

    /** Holds the sidebar's panels, with a line between them. */
    struct SidebarContent final : public juce::Component
    {
        void paint (juce::Graphics&) override;
        int dividerY = 0;
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    using juce::Component::keyPressed;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;

    void setInputMode (InputMode);
    void selectMeasure (std::optional<int>);
    void measureClicked (int measure);
    void scrollToMeasure (int measure);
    void pianoKeyClicked (int midiNote);
    void togglePlayback();
    void play (int firstMeasure, int lastMeasure);
    void showPlaybackPosition();
    void addMeasure();
    void removeMeasure();
    void showHeldNotes();
    void tempoEdited (bool finished);

    juce::AudioDeviceManager audioDeviceManager;
    InstrumentHost instrumentHost;
    Score score;

    juce::TextButton playButton { "Play" };
    juce::Label tempoLabel;
    juce::TextEditor tempoEditor;
    juce::TextButton notesButton { "Notes" };
    juce::TextButton chordsButton { "Chords" };
    InstrumentPanel instrumentPanel;

    ScorePanel scorePanel { score };
    ChordPanel chordPanel { score };
    SidebarContent sidebarContent;
    juce::Viewport sidebar;

    StaffView staffView { score };
    juce::Viewport staffViewport;

    PianoKeyboard keyboard { instrumentHost.getKeyboardState() };

    InputMode inputMode = InputMode::notes;
    std::optional<int> selectedMeasure;
    juce::Component::SafePointer<juce::Component> keyListenerTarget;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
