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

    With a dynamic chosen, clicks mark it instead, between the staves, at the beat clicked. With
    a crescendo or decrescendo chosen, dragging along the staves marks one as long as the drag,
    and dragging the end of one stretches it.
*/
class StaffView final : public juce::Component,
                        private juce::ChangeListener
{
public:
    StaffView (Score&, int part);

    int getPart() const noexcept { return part; }

    /** How long the notes that clicks add are, in beats: 0.5 for eighth notes, 1 for quarter
        notes, 2 for half notes, 3 for dotted half notes and 4 for whole notes. Clicks add eighths
        halfway through beats as well as on them; the others go on beats.
    */
    void setNoteLength (double beats);

    /** What clicks mark, rather than adding notes, or none to go back to adding notes.

        A dynamic is marked where the staff's clicked; clicking the same dynamic takes it out, and
        a different one is replaced. A hairpin is dragged out from where the mouse goes down to
        where it's let go, or is a beat long if the mouse doesn't move. Dragging a hairpin's end
        stretches it, and clicking a hairpin takes it out.
    */
    void setMarking (std::optional<music::Marking>);

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

    /** Where clicking at this point would mark a dynamic, as a measure and beat, if anywhere:
        the nearest beat, or the nearest half beat if a note starts there.
    */
    std::optional<std::pair<int, double>> getDynamicPointAt (juce::Point<float>) const;

    /** Where a hairpin dragged to this x would end, in beats from the start of the score: the
        nearest beat or barline, or the nearest half beat if a note starts there.
    */
    double getHairpinEndAt (float x) const;

    /** The hairpin drawn at this point, if any, and whether the point's on its end, for stretching it. */
    std::optional<std::pair<HairpinMark, bool>> findHairpinAt (juce::Point<float>) const;

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

    /** The eighth note of a measure nearest to x, if a note could start there, or else the nearest beat's. */
    int getSlotAt (int measure, float x, const std::function<bool (double beat)>& canStartBetweenBeats) const;

    /** Whether the point's in reach of the staves, where clicks add notes or mark dynamics. */
    bool isInClickRange (juce::Point<float>) const;
    juce::Rectangle<int> getBeatArea (int measure, double beat) const;
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

    /** Which of a staff's notes, given in order, share a beam. In a run of eighths, each starting
        as the one before ends, every four in a row share a beam; the rest of the run, and
        sixteenths, are beamed a beat at a time. A note on its own in a group keeps its flag.
    */
    static std::vector<std::vector<size_t>> groupBeams (const std::vector<const StaffEvent*>& notes);
    void drawNote (juce::Graphics&, Staff, const NoteLayout&, bool rolled, std::optional<int> highlightedPosition) const;
    void drawStem (juce::Graphics&, Staff, const NoteLayout&) const;
    void drawBeams (juce::Graphics&, Staff, std::vector<NoteLayout*>&) const;
    void drawLedgerLines (juce::Graphics&, Staff, const std::vector<int>& staffPositions,
                          const std::vector<float>& noteheadLefts, float noteheadWidth) const;
    void drawRollSign (juce::Graphics&, float right, float top, float bottom) const;
    void drawLabels (juce::Graphics&, int measure) const;
    void drawHoverNote (juce::Graphics&) const;
    void drawDynamics (juce::Graphics&, int measure) const;
    void drawHoverDynamic (juce::Graphics&) const;
    float getDynamicBaseline() const;
    float getMarkingCentreY() const;        // halfway between the staves, where dynamics and hairpins go

    /** Whether a note starts at this point of a measure, on either staff, as written. */
    bool hasNoteStartingAt (int measure, double beat) const;

    /** Where a hairpin from a point to an end, in beats from the start of the score, is drawn
        from and to: from its first note, or just after a dynamic there, to just before the note
        it leads to, or a dynamic there.
    */
    juce::Range<float> getHairpinSpan (int measure, double beat, double end) const;
    void drawHairpins (juce::Graphics&) const;
    void drawHairpin (juce::Graphics&, music::Hairpin, juce::Range<float> span) const;

    double toScoreBeats (int measure, double beat) const;               // in beats from the start of the score
    std::pair<int, double> fromScoreBeats (double beats) const;
    void drawTies (juce::Graphics&, int firstMeasure, int lastMeasure) const;
    void drawTieDrag (juce::Graphics&) const;
    void drawTie (juce::Graphics&, juce::Point<float> start, juce::Point<float> end, bool upwards) const;

    /** Where a note's head is, and which way a tie from it curves: away from its stem, or for a
        chord, up from its upper notes and down from its lower ones.
    */
    struct TiePoint { juce::Point<float> centre; float halfWidth; bool upwards; };
    std::optional<TiePoint> getTiePoint (int measure, Staff, double beat, music::Pitch) const;

    /** What a click without dragging does: adds or takes out a note. */
    void clickNote (const Note&);

    /** What a click does with a dynamic chosen: marks it, or takes it out if it's there already. */
    void clickDynamic (std::pair<int, double> point);
    void setHoverMarkPoint (std::optional<std::pair<int, double>>);

    std::optional<music::Dynamic> getChosenDynamic() const;
    std::optional<music::Hairpin> getChosenHairpin() const;

    /** A hairpin being dragged out, or stretched: where it starts and which it is, where the
        drag's got to, and the hairpin it's stretching, if it is.
    */
    struct HairpinDrag
    {
        HairpinMark start;
        double end;                         // in beats from the start of the score
        std::optional<HairpinMark> stretching;
    };

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
    double noteLength = 1.0;
    std::optional<music::Marking> marking;          // marked by clicks and drags, instead of adding notes
    std::optional<std::pair<int, double>> hoverMarkPoint, pressedDynamic;
    std::optional<HairpinMark> hoverHairpin, pressedHairpin;     // the hairpin under the mouse, and one a click would take out
    std::optional<HairpinDrag> hairpinDrag;
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
