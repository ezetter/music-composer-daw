#pragma once

#include "Score.h"

#include <juce_gui_extra/juce_gui_extra.h>

/** Saves the score to a file and loads it back, and keeps track of whether it has unsaved changes.

    Listeners hear when the file or the unsaved state changes, e.g. to update the window's title.
*/
class ScoreDocument final : public juce::FileBasedDocument,
                            private juce::ChangeListener
{
public:
    ScoreDocument (Score&, juce::PropertiesFile& settings);
    ~ScoreDocument() override;

    /** Starts a new, empty score with no file. */
    void startNewScore();

    static constexpr auto fileExtension = ".amscore";

    juce::String getDocumentTitle() override;

protected:
    juce::Result loadDocument (const juce::File&) override;
    juce::Result saveDocument (const juce::File&) override;
    juce::File getLastDocumentOpened() override;
    void setLastDocumentOpened (const juce::File&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    Score& score;
    juce::PropertiesFile& settings;
    bool replacingScore = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScoreDocument)
};
