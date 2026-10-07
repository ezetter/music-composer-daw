#include "PositionRuler.h"

#include "Controls.h"

PositionRuler::PositionRuler (const Score& scoreToShow)
    : score (scoreToShow)
{
    setTooltip ("Click or drag to choose where the score plays from, or jump there while it plays");
}

void PositionRuler::setMarker (double beats, bool isPlaying)
{
    if (juce::exactlyEqual (beats, markerBeats) && isPlaying == playing)
        return;

    markerBeats = beats;
    playing = isPlaying;
    repaint();
}

float PositionRuler::getBeatX (const StaffView& view, double beats) const
{
    return view.getBeatX (beats) - (float) (getScrolledX != nullptr ? getScrolledX() : 0);
}

void PositionRuler::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xfff7f7f9));
    g.setColour (juce::Colours::black.withAlpha (0.15f));
    g.fillRect (bounds.withTop (bounds.getBottom() - 1.0f));

    const auto* view = getView != nullptr ? getView() : nullptr;

    if (view == nullptr)
        return;

    // A tall tick with its number on each measure's first beat, and short ticks on its other beats
    const auto beatsPerMeasure = score.getBeatsPerMeasure();
    g.setFont (juce::FontOptions (11.0f));

    for (int measure = 0; measure < score.getNumMeasures(); ++measure)
    {
        const auto first = getBeatX (*view, measure * beatsPerMeasure);

        if (first > bounds.getRight())
            break;

        if (getBeatX (*view, (measure + 1) * beatsPerMeasure) < bounds.getX())
            continue;

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float> (first - 0.5f, 3.0f, 1.0f, bounds.getBottom() - 3.0f));
        g.setColour (controls::secondaryText);
        g.drawText (juce::String (measure + 1), juce::Rectangle<float> (first + 3.0f, 2.0f, 40.0f, 12.0f), juce::Justification::centredLeft, false);

        g.setColour (juce::Colours::black.withAlpha (0.35f));

        for (int beat = 1; beat < beatsPerMeasure; ++beat)
            g.fillRect (juce::Rectangle<float> (getBeatX (*view, measure * beatsPerMeasure + beat) - 0.5f, bounds.getBottom() - 7.0f, 1.0f, 7.0f));
    }

    // The marker: filled while the score plays, an outline where it'll start from
    const auto x = getBeatX (*view, markerBeats);
    juce::Path marker;
    marker.addTriangle (x - 5.5f, bounds.getBottom() - 10.0f, x + 5.5f, bounds.getBottom() - 10.0f, x, bounds.getBottom() - 1.0f);

    if (playing)
    {
        g.setColour (controls::accent);
        g.fillPath (marker);
    }
    else
    {
        g.setColour (juce::Colours::white);
        g.fillPath (marker);
        g.setColour (controls::accent);
        g.strokePath (marker, juce::PathStrokeType (1.4f));
    }
}

void PositionRuler::choose (float x)
{
    const auto* view = getView != nullptr ? getView() : nullptr;

    if (view == nullptr)
        return;

    const auto beat = view->getBeatAt (x + (float) (getScrolledX != nullptr ? getScrolledX() : 0));

    if (beat == lastChosen)
        return;

    lastChosen = beat;

    if (onBeatChosen != nullptr)
        onBeatChosen (beat);
}

void PositionRuler::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    lastChosen = -1;
    choose (e.position.x);
}

void PositionRuler::mouseDrag (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        choose (e.position.x);
}
