#include "ScoreHistory.h"

ScoreHistory::ScoreHistory (Score& scoreToUse)
    : score (scoreToUse),
      current (capture())
{
    score.addChangeListener (this);
}

ScoreHistory::~ScoreHistory()
{
    score.removeChangeListener (this);
}

bool ScoreHistory::canUndo()
{
    handleUpdateNowIfNeeded();
    return gestures == 0 && ! undoStates.empty();
}

bool ScoreHistory::canRedo()
{
    handleUpdateNowIfNeeded();
    return gestures == 0 && ! redoStates.empty();
}

void ScoreHistory::undo()
{
    // A change still waiting to be remembered is the one undone.
    handleUpdateNowIfNeeded();

    if (gestures > 0 || undoStates.empty())
        return;

    redoStates.push_back (current);
    current = undoStates.back();
    undoStates.pop_back();
    restore (current);
    changed();
}

void ScoreHistory::redo()
{
    handleUpdateNowIfNeeded();

    if (gestures > 0 || redoStates.empty())
        return;

    undoStates.push_back (current);
    current = redoStates.back();
    redoStates.pop_back();
    restore (current);
    changed();
}

void ScoreHistory::clear()
{
    cancelPendingUpdate();
    undoStates.clear();
    redoStates.clear();
    current = capture();
    changed();
}

void ScoreHistory::beginGesture()
{
    // What changed before it is a change of its own.
    handleUpdateNowIfNeeded();
    ++gestures;
}

void ScoreHistory::endGesture()
{
    jassert (gestures > 0);

    if (gestures > 0 && --gestures == 0)
    {
        cancelPendingUpdate();
        handleAsyncUpdate();
    }
}

void ScoreHistory::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // The changes made while handling this event are remembered together, once it's done.
    if (! restoring)
        triggerAsyncUpdate();
}

void ScoreHistory::handleAsyncUpdate()
{
    // A gesture's changes are remembered together when it ends.
    if (gestures > 0)
        return;

    auto now = capture();

    if (now == current)
        return;

    undoStates.push_back (std::exchange (current, std::move (now)));

    if (undoStates.size() > maxChanges)
        undoStates.erase (undoStates.begin());

    // A new change starts a new history from here, so what was undone can't be redone.
    redoStates.clear();
    changed();
}

juce::String ScoreHistory::capture() const
{
    return juce::JSON::toString (score.toJSON(), true);
}

void ScoreHistory::restore (const juce::String& state)
{
    const juce::ScopedValueSetter<bool> setter (restoring, true);
    score.loadJSON (juce::JSON::parse (state));
}

void ScoreHistory::changed()
{
    if (onChange != nullptr)
        onChange();
}
