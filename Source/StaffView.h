#pragma once

#include "MeasureContent.h"
#include "MusicGlyphs.h"
#include "Score.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

/** Draws a score on a single grand staff that extends to the right as measures are added.

    It shows one of the score's parts. Its measures are as wide as any part needs, so the parts'
    staves line up when they're stacked.

    Clicking the staff adds a note and clicking a note takes it out again: a note of the length
    chosen, or on a staff with a chord's notes, a note of the chord. Both happen when the mouse is
    let go without moving. Dragging from a note to the next note of the same pitch ties them, or
    unties them; dragging anywhere else does nothing. Above each measure is a button to add a
    chord to it, or edit the one it has.
*/
class StaffView final : public juce::Component,
                        private juce::ChangeListener
{
public:
    StaffView (Score&, int part);

    int getPart() const noexcept { return part; }

    /** How long the notes that clicks add are, in beats: 1 for quarter notes, 2 for half notes,
        3 for dotted half notes and 4 for whole notes.
    */
    void setNoteLength (int beats);

    /** Marks the view as showing the active part, whose instrument the keyboard plays. */
    void setActive (bool);

    /** The name written at the top left, such as the part's number and instrument. */
    void setTitle (const juce::String&);

    /** Called when the view is clicked, to make its part the active one. */
    std::function<void()> onClicked;
    ~StaffView() override;

    /** The size needed to show the whole score. If the view is taller, the staff is centred. */
    int getContentWidth() const;
    int getContentHeight() const;

    /** Outlines a measure as selected, e.g. while its chord is being edited, or none. */
    void setSelectedMeasure (std::optional<int>);

    /** Called when a measure's Add Chord or Edit Chord button is clicked. */
    std::function<void (int measure)> onChordButtonClicked;

    /** The note that clicking at this point would add, if any. */
    std::optional<Note> getNoteAt (juce::Point<float>) const;

    /** The measure at this point, if any. */
    std::optional<int> getMeasureAt (juce::Point<float>) const;

    /** The span of the view from the top of the treble staff to the bottom of the bass staff. */
    juce::Range<int> getStavesRange() const;

    /** The area a measure takes up, the full height of the view. */
    juce::Rectangle<int> getMeasureArea (int measure) const;

    /** Highlights the beat that's playing, given in beats from the start of the score. */
    void setPlaybackPosition (std::optional<double> beats);

    /** The area of the beat that's highlighted as playing, or an empty rectangle if none is. */
    juce::Rectangle<int> getPlaybackArea() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    inline static const juce::Colour paperColour { 0xffffffff };

private:
    struct MeasureLayout
    {
        float x = 0.0f;
        float width = 0.0f;
        float padding = 0.0f;           // from the barline to the centre of the first beat
        float beatWidth = 0.0f;
        MeasureContent content;
    };

    struct NoteLayout;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void updateLayout();
    void updateChordButtons();

    float getStaffTop (Staff) const;
    float getY (Staff, int staffPosition) const;
    float getOnsetX (int measure, double onset) const;
    float getEventX (int measure, const StaffEvent&) const;
    int findMeasure (float x) const;
    juce::Rectangle<int> getBeatArea (int measure, int beat) const;
    juce::Rectangle<float> getMeasureBox (int measure) const;
    float getSymbolBaseline() const;
    float getSymbolOffset() const;          // these offsets are in staff spaces, from the outer staff lines
    float getChordButtonOffset() const;
    float getNumeralOffset() const;
    float getMarginAbove() const;
    float getMarginBelow() const;
    float getNumeralBaseline() const;

    void drawHeader (juce::Graphics&) const;
    void drawMeasure (juce::Graphics&, int measure) const;
    void drawStaff (juce::Graphics&, int measure, Staff) const;
    void drawNote (juce::Graphics&, Staff, const NoteLayout&, bool rolled, std::optional<int> highlightedPosition) const;
    void drawStem (juce::Graphics&, Staff, const NoteLayout&) const;
    void drawBeams (juce::Graphics&, Staff, std::vector<NoteLayout*>&) const;
    void drawLedgerLines (juce::Graphics&, Staff, const std::vector<int>& staffPositions,
                          const std::vector<float>& noteheadLefts, float noteheadWidth) const;
    void drawRollSign (juce::Graphics&, float right, float top, float bottom) const;
    void drawLabels (juce::Graphics&, int measure) const;
    void drawHoverNote (juce::Graphics&) const;
    void drawTies (juce::Graphics&, int firstMeasure, int lastMeasure) const;
    void drawTieDrag (juce::Graphics&) const;
    void drawTie (juce::Graphics&, juce::Point<float> start, juce::Point<float> end, bool upwards) const;

    /** Where a note's head is, and which way a tie from it curves: away from its stem, or for a
        chord, up from its upper notes and down from its lower ones.
    */
    struct TiePoint { juce::Point<float> centre; float halfWidth; bool upwards; };
    std::optional<TiePoint> getTiePoint (int measure, Staff, int beat, music::Pitch) const;

    /** What a click without dragging does: adds or takes out a note. */
    void clickNote (const Note&);

    /** The note dragged from, and so a tie can be drawn to wherever the mouse is, if it's one
        clicked into the staff, rather than a chord's.
    */
    std::optional<Note> getTieableNoteAt (juce::Point<float>) const;
    void drawCentred (juce::Graphics&, juce::juce_wchar glyph, float centreX, float y) const;

    /** The note already written where this note would go, if any. */
    std::optional<music::Tone> findNoteUnder (const Note&) const;

    void setHoverNote (std::optional<Note>);

    Score& score;
    const int part;
    MusicGlyphs glyphs;
    bool active = false;
    int noteLength = 1;
    juce::String title;
    std::optional<Note> hoverNote;
    std::optional<Note> pressedNote;                // where the mouse went down, and the note there, if any
    std::optional<Note> tieFrom;                    // the note a tie's being dragged from
    juce::Point<float> dragPoint;
    bool hoverHintHidden = false;       // after a click, until the pointer moves to another spot
    std::optional<int> selectedMeasure;
    juce::OwnedArray<juce::TextButton> chordButtons;    // one above each measure
    std::optional<int> playingBeat;     // counted from the start of the score

    // Worked out whenever the score changes
    float timeSignatureLeft = 0.0f;
    float firstMeasureLeft = 0.0f;
    std::vector<MeasureLayout> measureLayouts;
    float reachAbove = 0.0f;            // how far the music goes above the treble staff, in staff spaces
    float reachBelow = 0.0f;            // and below the bass staff

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StaffView)
};
