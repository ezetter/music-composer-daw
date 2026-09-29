#pragma once

#include "ChordEditor.h"
#include "Controls.h"
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
    /** The parts' staff systems, one above the other, their measures lined up, with + and −
        buttons after the last measure for adding and removing measures, and Clone for repeating them all.
    */
    struct StaffSystems final : public juce::Component
    {
        explicit StaffSystems (Score&);
        ~StaffSystems() override;

        /** At least this tall, to fill the view it's in. */
        void setMinimumHeight (int);

        std::array<std::unique_ptr<StaffView>, Score::numParts> views;

        /** Called when the systems move or change size. */
        std::function<void()> onLayoutChanged;

        /** Called when +, − or Clone is clicked. */
        std::function<void()> onAddMeasure, onRemoveMeasure, onCloneMeasures;

    private:
        void childBoundsChanged (juce::Component*) override;
        void layOut();

        Score& score;
        int minimumHeight = 0;
        bool layingOut = false;

        controls::RoundButtonLookAndFeel roundButtonLookAndFeel;
        juce::TextButton addMeasureButton { "+" }, removeMeasureButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) };
        juce::TextButton cloneButton { "Clone" };
    };

    /** A viewport that says when it scrolls. */
    struct ScrollingViewport final : public juce::Viewport
    {
        std::function<void()> onScroll;

        void visibleAreaChanged (const juce::Rectangle<int>&) override
        {
            if (onScroll != nullptr)
                onScroll();
        }
    };

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

    /** The File menu's Open Recent menu, listing the recent scores. */
    juce::PopupMenu getRecentScoresMenu() const;

    /** Opens a recent score, after asking to save any changes to this one. */
    void openRecentScore (const juce::File&);

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    using juce::Component::keyPressed;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;


    /** Makes a part the active one: the keyboard plays its instrument, and the toolbar and the
        sidebar's alternate staff show and set its.
    */
    void setActivePart (int part);

    /** Chooses the length of the notes that clicking the staff adds, in beats: 4, 2 or 1. */
    void setNoteLength (int beats);

    /** Gives the other part the active part's chords, asking first if it has notes of its own. */
    void copyProgression();

    /** Opens the chord window for a measure of a part, to add a chord or edit the one it has.
        The part becomes the active one.
    */
    void editChord (int part, int measure);

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
    void cloneMeasures();
    void showHeldNotes();
    void tempoEdited (bool finished);
    void scoreReplaced();
    void showDocumentTitle();
    void showPartTitles();

    /** Changes a part's volume, and shows it on its dial. A change made on the dial counts as a
        change to the score.
    */
    void setVolume (int part, float decibels, bool changedOnDial);

    /** Mutes or unmutes a part, and shows it on its dial. A change made on the dial counts as a
        change to the score.
    */
    void setMuted (int part, bool muted, bool changedOnDial);

    /** Keeps each part's volume dial beside its staves, as they scroll up and down. */
    void positionVolumeDials();

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
    std::array<juce::TextButton, Score::numParts> partButtons;
    std::array<std::unique_ptr<InstrumentPanel>, Score::numParts> instrumentPanels;     // only the active part's shows

    juce::Label noteLengthHeading;      // at the top of the sidebar: the length of the notes that clicks add
    std::array<juce::TextButton, 3> noteLengthButtons;      // Whole, Half and Quarter
    ScorePanel scorePanel { score };
    juce::Component sidebarContent;
    juce::Viewport sidebar;

    StaffSystems staffSystems { score };
    ScrollingViewport staffViewport;

    controls::DialLookAndFeel dialLookAndFeel;
    juce::Component volumeColumn;       // to the left of the staves, holding a volume dial for each part
    std::array<controls::ClickableDial, Score::numParts> volumeDials;

    PianoKeyboard keyboard { instrumentHost.getKeyboardState() };

    int activePart = 0;
    ChordStyle lastChordStyle;          // what a new chord starts with: whichever was chosen last
    juce::Component::SafePointer<juce::Component> keyListenerTarget;

    // Last, so it goes before the score it edits
    std::unique_ptr<ChordWindow> chordWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
