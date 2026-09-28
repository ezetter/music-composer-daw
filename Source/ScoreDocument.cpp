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

    // Scores from before there were two parts have one instrument, for the first.
    if (result.wasOk() && loadInstrument != nullptr)
    {
        const auto* instruments = json.getProperty ("instruments", {}).getArray();

        for (int part = 0; part < Score::numParts; ++part)
            loadInstrument (part, instruments != nullptr ? instruments->operator[] (part)
                                                         : part == 0 ? json.getProperty ("instrument", {}) : juce::var());
    }

    return result;
}

juce::Result ScoreDocument::saveDocument (const juce::File& file)
{
    auto json = score.toJSON();

    // Each part's instrument, or null for a part without one
    if (getInstrumentToSave != nullptr)
    {
        juce::Array<juce::var> instruments;

        for (int part = 0; part < Score::numParts; ++part)
            instruments.add (getInstrumentToSave (part));

        json.getDynamicObject()->setProperty ("instruments", instruments);
    }

    if (file.replaceWithText (juce::JSON::toString (json)))
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
