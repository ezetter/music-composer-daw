#pragma once

#include "Score.h"
#include "StaffView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/** A strip along the top of the staves, lined up with them as they scroll sideways, with a tick
    and number for each measure and smaller ticks for its beats, and a marker for where the score
    is playing, or where it will start playing from. Clicking it, or dragging along it, chooses a
    beat to play from.
*/
class PositionRuler final : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    explicit PositionRuler (const Score&);

    /** The staves it's lined up with, and how far they've scrolled sideways, asked for whenever
        it's drawn or clicked.
    */
    std::function<const StaffView*()> getView;
    std::function<int()> getScrolledX;

    /** Shows where the score's playing, or where it will start playing from, in beats from its start. */
    void setMarker (double beats, bool playing);

    /** Called when a beat's chosen, by clicking or dragging, counted from the start of the score. */
    std::function<void (int beat)> onBeatChosen;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    static constexpr int height = 24;

private:
    /** Where a beat is across the ruler, from the start of the score. */
    float getBeatX (const StaffView&, double beats) const;
    void choose (float x);

    const Score& score;
    double markerBeats = 0.0;
    bool playing = false;
    int lastChosen = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PositionRuler)
};
