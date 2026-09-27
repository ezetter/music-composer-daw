#include "Score.h"

#include <algorithm>

void Score::addMeasure()
{
    measures.emplace_back();
    sendSynchronousChangeMessage();
}

void Score::removeLastMeasure()
{
    if (measures.size() <= 1)
        return;

    measures.pop_back();
    sendSynchronousChangeMessage();
}

bool Score::addNote (const Note& note)
{
    jassert (juce::isPositiveAndBelow (note.measure, getNumMeasures()));
    jassert (juce::isPositiveAndBelow (note.beat, beatsPerMeasure));

    auto& chord = measures[(size_t) note.measure][(size_t) note.staff][(size_t) note.beat];
    const auto insertionPoint = std::lower_bound (chord.begin(), chord.end(), note.pitch);

    if (insertionPoint != chord.end() && *insertionPoint == note.pitch)
        return false;

    chord.insert (insertionPoint, note.pitch);
    sendSynchronousChangeMessage();
    return true;
}

const std::vector<int>& Score::getChord (Staff staff, int measure, int beat) const
{
    return measures[(size_t) measure][(size_t) staff][(size_t) beat];
}
