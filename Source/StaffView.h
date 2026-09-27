#pragma once

#include "MusicGlyphs.h"
#include "Score.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>
#include <vector>

/** Draws a score on a single grand staff that extends to the right as measures are added, and
    adds a quarter note wherever the staff is clicked.
*/
class StaffView final : public juce::Component,
                        private juce::ChangeListener
{
public:
    explicit StaffView (Score&);
    ~StaffView() override;

    /** The size needed to show the whole score. If the view is taller, the staff is centred. */
    int getContentWidth() const;
    static int getContentHeight();

    /** The note that clicking at this point would add, if any. */
    std::optional<Note> getNoteAt (juce::Point<float>) const;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    inline static const juce::Colour paperColour { 0xffffffff };

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    float getStaffTop (Staff) const;
    float getY (Staff, int staffPosition) const;

    void drawSystemStart (juce::Graphics&) const;
    void drawMeasure (juce::Graphics&, int measure) const;
    void drawRests (juce::Graphics&, Staff, int measure) const;
    void drawChord (juce::Graphics&, Staff, float centreX, const std::vector<int>& pitches) const;
    void drawLedgerLines (juce::Graphics&, Staff, const std::vector<int>& staffPositions,
                          const std::vector<float>& noteheadLefts) const;
    void drawHoverNote (juce::Graphics&) const;
    void drawCentred (juce::Graphics&, juce::juce_wchar glyph, float centreX, float y) const;

    void setHoverNote (std::optional<Note>);
    juce::Rectangle<int> getBeatArea (const Note&) const;

    Score& score;
    MusicGlyphs glyphs;
    std::optional<Note> hoverNote;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StaffView)
};
