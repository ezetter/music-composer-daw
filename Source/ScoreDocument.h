#pragma once

#include "Score.h"

#include <juce_gui_extra/juce_gui_extra.h>

/** Saves the score to a file, with the instrument it's played on, and loads it back, and keeps
    track of whether it has unsaved changes.

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

    /** Supplies a part's instrument and its settings to save with the score, as JSON. */
    std::function<juce::var (int part)> getInstrumentToSave;

    /** Loads the instrument a part of an opened score was saved with, given the JSON
        getInstrumentToSave gave, or void if it was saved without one.
    */
    std::function<void (int part, const juce::var&)> loadInstrument;

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
