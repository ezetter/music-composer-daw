#pragma once

#include "MeasureContent.h"
#include "MusicGlyphs.h"
#include "Score.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

/** Draws a score on a single grand staff that extends to the right as measures are added.

    In notes mode, clicking the staff adds a quarter note, and clicking a quarter note takes it
    out again. In chords mode, clicking a measure selects it, so its chord can be edited.
*/
class StaffView final : public juce::Component,
                        private juce::ChangeListener
{
public:
    enum class InputMode { notes, chords };

    explicit StaffView (Score&);
    ~StaffView() override;

    /** The size needed to show the whole score. If the view is taller, the staff is centred. */
    int getContentWidth() const;
    static int getContentHeight();

    void setInputMode (InputMode);

    /** Outlines a measure as selected, or none. */
    void setSelectedMeasure (std::optional<int>);

    /** Called when a measure is clicked in chords mode. */
    std::function<void (int measure)> onMeasureClicked;

    /** The note that clicking at this point would add in notes mode, if any. */
    std::optional<Note> getNoteAt (juce::Point<float>) const;

    /** The measure at this point, if any. */
    std::optional<int> getMeasureAt (juce::Point<float>) const;

    /** Highlights the beat that's playing, given in beats from the start of the score. */
    void setPlaybackPosition (std::optional<double> beats);

    /** The area of the beat that's highlighted as playing, or an empty rectangle if none is. */
    juce::Rectangle<int> getPlaybackArea() const;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

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

    float getStaffTop (Staff) const;
    float getY (Staff, int staffPosition) const;
    float getOnsetX (int measure, double onset) const;
    float getEventX (int measure, const StaffEvent&) const;
    int findMeasure (float x) const;
    juce::Rectangle<int> getBeatArea (int measure, int beat) const;
    juce::Rectangle<float> getMeasureBox (int measure) const;
    float getSymbolBaseline() const;
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
    void drawCentred (juce::Graphics&, juce::juce_wchar glyph, float centreX, float y) const;

    void setHoverNote (std::optional<Note>);
    void setHoverMeasure (std::optional<int>);

    Score& score;
    MusicGlyphs glyphs;
    InputMode inputMode = InputMode::notes;
    std::optional<Note> hoverNote;
    bool hoverHintHidden = false;       // after a click, until the pointer moves to another spot
    std::optional<int> hoverMeasure;
    std::optional<int> selectedMeasure;
    std::optional<int> playingBeat;     // counted from the start of the score

    // Worked out whenever the score changes
    float timeSignatureLeft = 0.0f;
    float firstMeasureLeft = 0.0f;
    std::vector<MeasureLayout> measureLayouts;
    float reachAbove = 0.0f;            // how far the music goes above the treble staff, in staff spaces
    float reachBelow = 0.0f;            // and below the bass staff

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StaffView)
};
