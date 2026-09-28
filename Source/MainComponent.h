#pragma once

#include "ChordEditor.h"
#include "InstrumentHost.h"
#include "InstrumentPanel.h"
#include "MidiInputs.h"
#include "PianoKeyboard.h"
#include "Score.h"
#include "ScoreDocument.h"
#include "ScorePanel.h"
#include "StaffView.h"

#include <juce_audio_utils/juce_audio_utils.h>

/** The main window's contents: a toolbar, a sidebar with the score's and chords' settings, the
    scrolling grand staff, and a piano keyboard, all playing through an instrument plugin. It
    also puts the File menu in the menu bar, for saving and opening scores.
*/
class MainComponent final : public juce::Component,
                            public juce::ApplicationCommandTarget,
                            private juce::MenuBarModel,
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

    /** Offers to save unsaved changes, then does the action, unless the user cancels. */
    void saveChangesThen (std::function<void()> action);

    ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands (juce::Array<juce::CommandID>&) override;
    void getCommandInfo (juce::CommandID, juce::ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;

    static constexpr int minimumWidth = 1000;
    static constexpr int minimumHeight = 640;

private:
    enum Commands
    {
        newScore = 1,
        openScore,
        saveScore,
        saveScoreAs
    };

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int menuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    using juce::Component::keyPressed;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;


    /** Opens the chord window for a measure, to add a chord or edit the one it has. */
    void editChord (int measure);

    /** The chord being edited, if the chord window is open. */
    ChordEditor* getChordEditor() const;

    /** Closes the chord window, if it's open, without changing the chord. */
    void closeChordEditor();
    void scrollToMeasure (int measure);
    void pianoKeyClicked (int midiNote);
    void togglePlayback();
    void showPlaybackPosition();
    void addMeasure();
    void removeMeasure();
    void showHeldNotes();
    void tempoEdited (bool finished);
    void scoreReplaced();
    void showDocumentTitle();

    juce::PropertiesFile& settings;
    juce::AudioDeviceManager audioDeviceManager;
    InstrumentHost instrumentHost;
    MidiInputs midiInputs { audioDeviceManager, instrumentHost, settings };

    /** The MIDI inputs the MIDI menu showed, the last time it was opened */
    std::vector<MidiInputs::Device> midiMenuDevices;
    Score score;
    ScoreDocument document;
    juce::ApplicationCommandManager commandManager;

    juce::TextButton playButton { "Play" };
    juce::TextButton loopButton { "Loop" };
    juce::Label tempoLabel;
    juce::TextEditor tempoEditor;
    InstrumentPanel instrumentPanel;

    ScorePanel scorePanel { score };
    juce::Component sidebarContent;
    juce::Viewport sidebar;

    StaffView staffView { score };
    juce::Viewport staffViewport;

    PianoKeyboard keyboard { instrumentHost.getKeyboardState() };

    ChordStyle lastChordStyle;          // what a new chord starts with: whichever was chosen last
    juce::Component::SafePointer<juce::Component> keyListenerTarget;

    // Last, so it goes before the score it edits
    std::unique_ptr<ChordWindow> chordWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
