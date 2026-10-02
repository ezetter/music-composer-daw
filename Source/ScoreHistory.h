#pragma once

#include "Score.h"

#include <juce_events/juce_events.h>

#include <functional>
#include <utility>
#include <vector>

/** Remembers how the score was before each change, so changes can be undone, and undone changes
    redone.

    Everything that changes in the score while handling one event, such as a click or a menu
    command, counts as one change, so it's undone in one go.
*/
class ScoreHistory final : private juce::ChangeListener,
                           private juce::AsyncUpdater
{
public:
    explicit ScoreHistory (Score&);
    ~ScoreHistory() override;

    bool canUndo();
    bool canRedo();

    /** Puts the score back as it was before the last change, or the one before that, and so on. */
    void undo();

    /** Makes the last change undone again, unless the score's been changed since. */
    void redo();

    /** Forgets every change, e.g. when a score's opened, so the score as it is now is where undoing stops. */
    void clear();

    /** Called when what can be undone or redone changes. */
    std::function<void()> onChange;

    /** How many changes are remembered. The oldest are forgotten first. */
    static constexpr size_t maxChanges = 500;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void handleAsyncUpdate() override;
    juce::String capture() const;
    void restore (const juce::String&);
    void changed();

    Score& score;
    juce::String current;                   // the score as it was after the last change remembered
    std::vector<juce::String> undoStates, redoStates;
    bool restoring = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScoreHistory)
};
