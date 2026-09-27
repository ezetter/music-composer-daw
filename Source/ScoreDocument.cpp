#include "ScoreDocument.h"

namespace
{
    const char* const lastScoreKey = "lastScore";
}

ScoreDocument::ScoreDocument (Score& scoreToUse, juce::PropertiesFile& settingsToUse)
    : FileBasedDocument (fileExtension, juce::String ("*") + fileExtension, "Open a score", "Save the score"),
      score (scoreToUse),
      settings (settingsToUse)
{
    score.addChangeListener (this);
}

ScoreDocument::~ScoreDocument()
{
    score.removeChangeListener (this);
}

void ScoreDocument::startNewScore()
{
    replacingScore = true;
    score.clear();
    replacingScore = false;

    setFile ({});
    setChangedFlag (false);
}

juce::String ScoreDocument::getDocumentTitle()
{
    return getFile() == juce::File() ? juce::String ("Untitled") : getFile().getFileNameWithoutExtension();
}

juce::Result ScoreDocument::loadDocument (const juce::File& file)
{
    const auto json = juce::JSON::parse (file.loadFileAsString());

    if (! json.isObject())
        return juce::Result::fail (file.getFileName() + " isn't an Anthropocene Music score.");

    replacingScore = true;
    const auto result = score.loadJSON (json);
    replacingScore = false;
    return result;
}

juce::Result ScoreDocument::saveDocument (const juce::File& file)
{
    if (file.replaceWithText (juce::JSON::toString (score.toJSON())))
        return juce::Result::ok();

    return juce::Result::fail ("Couldn't write to " + file.getFullPathName());
}

juce::File ScoreDocument::getLastDocumentOpened()
{
    return juce::File (settings.getValue (lastScoreKey));
}

void ScoreDocument::setLastDocumentOpened (const juce::File& file)
{
    settings.setValue (lastScoreKey, file.getFullPathName());
}

void ScoreDocument::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Any change to the score, other than loading one or starting afresh, needs saving.
    if (! replacingScore)
        changed();
}
