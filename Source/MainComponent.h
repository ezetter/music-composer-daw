#pragma once

#include "ChordEditor.h"
#include "Controls.h"
#include "InstrumentHost.h"
#include "InstrumentPanel.h"
#include "MidiInputs.h"
#include "DynamicPicker.h"
#include "EraserButton.h"
#include "NoteLengthPicker.h"
#include "PianoKeyboard.h"
#include "Score.h"
#include "ScoreDocument.h"
#include "ScoreHistory.h"
#include "ScorePanel.h"
#include "StaffView.h"

#include <juce_audio_utils/juce_audio_utils.h>

/** The main window's contents: a toolbar, a sidebar with the score's and chords' settings, the
    scrolling grand staff, and a piano keyboard, all playing through an instrument plugin. It
    also puts the File, Edit and MIDI menus in the menu bar, for saving and opening scores,
    undoing and redoing changes to them, and choosing MIDI controllers.
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
    /** The parts' staff systems, one above the other, their measures lined up, with a + below the
        last part for adding another.
    */
    struct StaffSystems final : public juce::Component
    {
        explicit StaffSystems (Score&);
        ~StaffSystems() override;

        /** At least this tall, to fill the view it's in. */
        void setMinimumHeight (int);

        /** Each part's view, from the top, in the order of the score's parts. */
        std::vector<std::unique_ptr<StaffView>> views;

        /** Adds a view for a part at a place among the others, or takes one out. */
        StaffView& insertView (int index, int part);
        void removeView (int index);

        /** Called when the systems move or change size. */
        std::function<void()> onLayoutChanged;

        /** Called when the + below the last part is clicked. */
        std::function<void()> onAddPart;

        void layOut();

    private:
        void childBoundsChanged (juce::Component*) override;

        Score& score;
        int minimumHeight = 0;
        bool layingOut = false;

        controls::RoundButtonLookAndFeel roundButtonLookAndFeel;
        juce::TextButton addPartButton { "+" };
        juce::Label addPartLabel;
    };

    /** What each part has besides its staves: its instrument, the volume dial beside its staves,
        and a button under the dial for taking the part out.
    */
    struct Track
    {
        int partId;
        std::unique_ptr<InstrumentPanel> panel;
        std::unique_ptr<controls::ClickableDial> dial;
        std::unique_ptr<controls::BinButton> deleteButton;
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
        saveScoreAs,
        undoChange,
        redoChange
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

    /** Gives each of the score's parts a track, and takes away the tracks of parts that have
        gone, e.g. when a part's added or taken out, a score's opened, or a change is undone.
    */
    void updateTracks();
    void insertTrack (int index, bool reloadFromSettings);
    void removeTrack (int index);
    void updatePartBox();

    /** Gives each track's controls its part's number, e.g. in their tooltips. */
    void numberTracks();

    /** Adds a part after the last, makes it the active one, and scrolls down to it. */
    void addPart();

    /** Takes a part out, after asking, unless there's nothing in it to lose. */
    void deletePart (int part);

    /** Chooses the length of the notes that clicking the staff adds, in beats: 4, 3, 2, 1 or 0.5.
        Clicks add notes again, if they were marking a dynamic.
    */
    void setNoteLength (double beats);

    /** Chooses a dynamic or hairpin for clicking or dragging on the score to mark, instead of
        adding notes, or none to go back to adding notes of the length chosen.
    */
    void setMarking (std::optional<music::Marking>);

    /** Turns the eraser on, for taking out whatever the mouse passes over with its button held
        down, or off, back to adding notes of the length chosen.
    */
    void setErasing (bool);

    /** Gives another part the active part's chords, asking first if it has notes of its own. */
    void copyProgression (int toPart);

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

    /** Keeps each part's volume dial, and the button for taking it out, beside its staves, as
        they scroll up and down.
    */
    void positionVolumeDials();

    /** Keeps each part's volume and muting in the settings, by its number. */
    void saveVolumes();

    juce::PropertiesFile& settings;
    juce::AudioDeviceManager audioDeviceManager;
    InstrumentHost instrumentHost;
    MidiInputs midiInputs { audioDeviceManager, instrumentHost, settings };

    /** The MIDI inputs the MIDI menu showed, the last time it was opened */
    std::vector<MidiInputs::Device> midiMenuDevices;
    Score score;
    ScoreDocument document;
    ScoreHistory history { score };
    juce::ApplicationCommandManager commandManager;

    juce::TextButton playButton { "Play" };
    juce::TextButton loopButton { "Loop" };
    juce::Label tempoLabel;
    juce::TextEditor tempoEditor;
    juce::ComboBox partBox;             // the active part
    std::vector<Track> tracks;          // for each part, in order; only the active part's instrument panel shows

    ScorePanel scorePanel { score };
    juce::Component sidebarContent;
    juce::Viewport sidebar;

    StaffSystems staffSystems { score };
    ScrollingViewport staffViewport;

    controls::DialLookAndFeel dialLookAndFeel;
    NoteLengthPicker noteLengthPicker;  // at the top left of the score, over the staves: the length of the notes that clicks add
    DynamicPicker dynamicPicker;        // beside it: a dynamic or hairpin to mark instead
    EraserButton eraserButton;          // at the top right of the score, over the staves: the eraser

    // Over the staves at the right, halfway down, wherever they scroll to: + and − for adding a
    // measure at the end and taking the last away, and Clone for repeating them all, in every part
    controls::RoundButtonLookAndFeel roundButtonLookAndFeel;
    juce::TextButton addMeasureButton { "+" }, removeMeasureButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) };
    juce::TextButton cloneButton { "Clone" };
    juce::Component volumeColumn;       // to the left of the staves, holding each part's volume dial and delete button

    PianoKeyboard keyboard { instrumentHost.getKeyboardState() };

    int activePart = 0;
    int activePartId = 0;               // so the active part can be found again when parts are added or taken out
    ChordStyle lastChordStyle;          // what a new chord starts with: whichever was chosen last
    juce::Component::SafePointer<juce::Component> keyListenerTarget;

    // Last, so it goes before the score it edits
    std::unique_ptr<ChordWindow> chordWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
