#include "ScoreDocument.h"

namespace
{
    const char* const lastScoreKey = "lastScore";
    const char* const recentScoresKey = "recentScores";
}

ScoreDocument::ScoreDocument (Score& scoreToUse, juce::PropertiesFile& settingsToUse)
    : FileBasedDocument (fileExtension, juce::String ("*") + fileExtension, "Open a score", "Save the score"),
      score (scoreToUse),
      settings (settingsToUse)
{
    recentScores.setMaxNumberOfItems (maxRecentScores);
    recentScores.restoreFromString (settings.getValue (recentScoresKey));

    // Before there was a list, only the last score chosen in a file dialog was remembered.
    if (const juce::File last (settings.getValue (lastScoreKey)); recentScores.getNumFiles() == 0 && last.hasFileExtension (fileExtension)
                                                                  && last.existsAsFile())
        recentScores.addFile (last);

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

//==============================================================================
void ScoreDocument::clearRecentScores()
{
    recentScores.clear();
    saveRecentScores();
}

void ScoreDocument::forgetRecentScore (const juce::File& file)
{
    recentScores.removeFile (file);
    saveRecentScores();
}

void ScoreDocument::rememberRecentScore (const juce::File& file)
{
    // At the top of the list, and in the Mac's own recent documents too, when it's the app
    // that's running.
    recentScores.addFile (file);

    if (juce::JUCEApplicationBase::getInstance() != nullptr)
        juce::RecentlyOpenedFilesList::registerRecentFileNatively (file);

    saveRecentScores();
}

void ScoreDocument::saveRecentScores()
{
    settings.setValue (recentScoresKey, recentScores.toString());
    settings.saveIfNeeded();

    // Listeners hear about it, to update the Open Recent menu.
    sendChangeMessage();
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

    if (const auto* volumes = json.getProperty ("volumes", {}).getArray(); result.wasOk() && volumes != nullptr && loadVolume != nullptr)
        for (int part = 0; part < Score::numParts && part < volumes->size(); ++part)
            if (const auto& volume = volumes->getReference (part); volume.isDouble() || volume.isInt() || volume.isInt64())
                loadVolume (part, (float) (double) volume);

    if (const auto* muted = json.getProperty ("muted", {}).getArray(); result.wasOk() && muted != nullptr && loadMuted != nullptr)
        for (int part = 0; part < Score::numParts && part < muted->size(); ++part)
            if (const auto& value = muted->getReference (part); value.isBool())
                loadMuted (part, (bool) value);

    if (result.wasOk())
        rememberRecentScore (file);

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

    // Each part's volume, in decibels
    if (getVolumeToSave != nullptr)
    {
        juce::Array<juce::var> volumes;

        for (int part = 0; part < Score::numParts; ++part)
            volumes.add (getVolumeToSave (part));

        json.getDynamicObject()->setProperty ("volumes", volumes);
    }

    // Whether each part is muted
    if (getMutedToSave != nullptr)
    {
        juce::Array<juce::var> muted;

        for (int part = 0; part < Score::numParts; ++part)
            muted.add (getMutedToSave (part));

        json.getDynamicObject()->setProperty ("muted", muted);
    }

    if (file.replaceWithText (juce::JSON::toString (json)))
    {
        rememberRecentScore (file);
        return juce::Result::ok();
    }

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
