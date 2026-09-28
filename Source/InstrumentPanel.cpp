#include "InstrumentPanel.h"

namespace
{
    // Where a part's instrument's description and state are kept in the settings: part 1's as
    // they were when there was only one part, and part 2's after it
    juce::String getInstrumentKey (int part)
    {
        return part == 0 ? "instrument" : "instrument" + juce::String (part + 1);
    }

    juce::String withEllipsis (const juce::String& text)
    {
        return text + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
    }
}

InstrumentPanel::InstrumentPanel (InstrumentHost& hostToUse, int partToUse, juce::PropertiesFile& settingsToUse)
    : host (hostToUse),
      part (partToUse),
      settings (settingsToUse),
      instrumentKey (getInstrumentKey (part)),
      instrumentStateKey (getInstrumentKey (part) + "State"),
      loadButton (withEllipsis ("Load Instrument")),
      editorButton ("Show Editor")
{
    juce::addDefaultFormatsToManager (formatManager);

    nameLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (nameLabel);

    loadButton.onClick = [this] { chooseInstrument(); };
    editorButton.onClick = [this] { showEditor(); };

    for (auto* button : { &loadButton, &editorButton })
    {
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (button);
    }

    updateControls();
    reloadSavedInstrument();

    // Changes the instrument reports are looked into a couple of times a second, rather than
    // at every turn of a knob.
    startTimer (500);
}

InstrumentPanel::~InstrumentPanel()
{
    // Keep any changes made to the instrument's sound since it was last saved.
    saveInstrument();
    listenTo (nullptr);
}

void InstrumentPanel::resized()
{
    auto bounds = getLocalBounds();

    editorButton.setBounds (bounds.removeFromRight (110));
    bounds.removeFromRight (8);
    loadButton.setBounds (bounds.removeFromRight (150));
    bounds.removeFromRight (8);
    nameLabel.setBounds (bounds);
}

void InstrumentPanel::chooseInstrument()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Choose a VST3 instrument", pluginFolder, "*.vst3");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (file == juce::File())
                                      return;

                                  pluginFolder = file.getParentDirectory();
                                  loadInstrumentFrom (file);
                              });
}

void InstrumentPanel::loadInstrumentFrom (const juce::File& pluginFile)
{
    juce::OwnedArray<juce::PluginDescription> descriptions;

    juce::MouseCursor::showWaitCursor();

    for (auto* format : formatManager.getFormats())
        if (format->fileMightContainThisPluginType (pluginFile.getFullPathName()))
            format->findAllTypesForFile (descriptions, pluginFile.getFullPathName());

    juce::MouseCursor::hideWaitCursor();

    juce::Array<juce::PluginDescription> instruments;

    for (const auto* description : descriptions)
        if (description->isInstrument)
            instruments.add (*description);

    if (instruments.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Couldn't load an instrument",
                                                pluginFile.getFileNameWithoutExtension() + " isn't a VST3 instrument.");
        return;
    }

    if (instruments.size() == 1)
    {
        createInstrument (instruments.getReference (0));
        return;
    }

    // Some plugins contain several instruments, so let the user pick one.
    juce::PopupMenu menu;

    for (int i = 0; i < instruments.size(); ++i)
        menu.addItem (i + 1, instruments.getReference (i).name);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (loadButton),
                        [safeThis = juce::Component::SafePointer (this), instruments] (int chosenItem)
                        {
                            if (safeThis != nullptr && chosenItem > 0)
                                safeThis->createInstrument (instruments.getReference (chosenItem - 1));
                        });
}

void InstrumentPanel::createInstrument (const juce::PluginDescription& description, const juce::MemoryBlock& state, Source source)
{
    nameBeingLoaded = description.name;
    updateControls();

    // Only the instrument asked for last is kept, if another is asked for while one is loading.
    const auto request = ++instrumentsRequested;

    formatManager.createPluginInstanceAsync (description, host.getSampleRate(), host.getBlockSize(),
        [safeThis = juce::Component::SafePointer (this), name = description.name, state, source, request]
        (std::unique_ptr<juce::AudioPluginInstance> instrument, const juce::String& error)
        {
            if (safeThis == nullptr || request != safeThis->instrumentsRequested)
                return;

            safeThis->nameBeingLoaded = {};

            if (instrument != nullptr)
            {
                if (! state.isEmpty())
                    instrument->setStateInformation (state.getData(), (int) state.getSize());

                // The old instrument's editor has to go before the old instrument does.
                safeThis->editorWindow = nullptr;
                safeThis->listenTo (instrument.get());
                safeThis->host.setInstrument (safeThis->part, std::move (instrument));
                safeThis->saveInstrument();

                // A new instrument chosen by hand is a change to the score. One reloaded from the
                // settings when the app starts isn't, since a new score doesn't have one yet.
                safeThis->startTrackingChanges();

                if (source == Source::user && safeThis->onInstrumentChanged != nullptr)
                    safeThis->onInstrumentChanged();
            }
            else if (source == Source::settings)
            {
                // Forget an instrument that can't be loaded any more, rather than failing every time.
                safeThis->settings.removeValue (safeThis->instrumentKey);
                safeThis->settings.removeValue (safeThis->instrumentStateKey);
                safeThis->settings.saveIfNeeded();

                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't reload " + name,
                                                        error + (error.isEmpty() ? "" : "\n\n")
                                                            + withEllipsis ("You can choose an instrument with Load Instrument"));
            }
            else if (source == Source::score)
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load " + name,
                                                        "The score was saved with " + name + ", which couldn't be loaded. "
                                                            + (error.isEmpty() ? "" : error + "\n\n")
                                                            + "The instrument you had stays loaded.");
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load " + name, error);
            }

            safeThis->updateControls();
        });
}

void InstrumentPanel::reloadSavedInstrument()
{
    const auto xml = settings.getXmlValue (instrumentKey);
    juce::PluginDescription description;

    if (xml == nullptr || ! description.loadFromXml (*xml))
        return;

    juce::MemoryBlock state;
    state.fromBase64Encoding (settings.getValue (instrumentStateKey));
    createInstrument (description, state, Source::settings);
}

void InstrumentPanel::saveInstrument()
{
    auto* instrument = host.getInstrument (part);

    if (instrument == nullptr)
        return;

    settings.setValue (instrumentKey, instrument->getPluginDescription().createXml().get());
    settings.setValue (instrumentStateKey, getState().toBase64Encoding());
    settings.saveIfNeeded();
}

juce::MemoryBlock InstrumentPanel::getState() const
{
    juce::MemoryBlock state;

    if (auto* instrument = host.getInstrument (part))
        instrument->getStateInformation (state);

    return state;
}

//==============================================================================
juce::var InstrumentPanel::saveToJSON()
{
    auto* instrument = host.getInstrument (part);

    if (instrument == nullptr)
        return {};

    startTrackingChanges();

    auto* json = new juce::DynamicObject();
    json->setProperty ("name", instrument->getName());
    json->setProperty ("plugin", instrument->getPluginDescription().createXml()->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader()));
    json->setProperty ("state", scoreState.toBase64Encoding());
    return json;
}

void InstrumentPanel::loadFromJSON (const juce::var& json)
{
    juce::PluginDescription description;
    const auto xml = juce::parseXML (json.getProperty ("plugin", {}).toString());

    if (xml == nullptr || ! description.loadFromXml (*xml))
    {
        // A score saved without an instrument keeps the one that's loaded, as it is.
        startTrackingChanges();
        return;
    }

    juce::MemoryBlock state;
    state.fromBase64Encoding (json.getProperty ("state", {}).toString());

    // The same instrument just takes the score's settings, without loading it again.
    if (auto* instrument = host.getInstrument (part); instrument != nullptr && instrument->getPluginDescription().isDuplicateOf (description))
    {
        ++instrumentsRequested;
        nameBeingLoaded = {};

        if (! state.isEmpty())
            instrument->setStateInformation (state.getData(), (int) state.getSize());

        saveInstrument();
        startTrackingChanges();
        updateControls();
        return;
    }

    createInstrument (description, state, Source::score);
}

void InstrumentPanel::checkForSoundChanges()
{
    if (host.getInstrument (part) == nullptr || nameBeingLoaded.isNotEmpty() || ! soundTouched.exchange (false))
        return;

    // The instrument may have been put back as it was, e.g. by undoing a change in its editor.
    if (auto state = getState(); state != scoreState)
    {
        scoreState = std::move (state);

        if (onInstrumentChanged != nullptr)
            onInstrumentChanged();
    }
}

void InstrumentPanel::startTrackingChanges()
{
    soundTouched = false;
    scoreState = getState();
}

void InstrumentPanel::listenTo (juce::AudioPluginInstance* newInstrument)
{
    if (auto* instrument = host.getInstrument (part))
        instrument->removeListener (this);

    if (newInstrument != nullptr)
        newInstrument->addListener (this);
}

void InstrumentPanel::audioProcessorParameterChanged (juce::AudioProcessor*, int, float)
{
    soundTouched = true;
}

void InstrumentPanel::audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details)
{
    // Latency and parameter names don't change the sound.
    if (details.programChanged || details.nonParameterStateChanged)
        soundTouched = true;
}

void InstrumentPanel::timerCallback()
{
    checkForSoundChanges();
}

void InstrumentPanel::showEditor()
{
    if (editorWindow != nullptr)
    {
        editorWindow->toFront (true);
        return;
    }

    // Closing the editor saves the instrument, with any sound chosen in it.
    if (auto* instrument = host.getInstrument (part))
        editorWindow = std::make_unique<PluginWindow> (*instrument, [this]
        {
            saveInstrument();
            checkForSoundChanges();
            editorWindow = nullptr;
        });

    // Both parts could have the same instrument, so the window says which part's it is.
    if (editorWindow != nullptr)
        editorWindow->setName (editorWindow->getName() + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 Part ")) + juce::String (part + 1));
}

void InstrumentPanel::updateControls()
{
    const auto* instrument = host.getInstrument (part);
    const auto isLoading = nameBeingLoaded.isNotEmpty();

    if (isLoading)
        nameLabel.setText (withEllipsis ("Loading " + nameBeingLoaded), juce::dontSendNotification);
    else
        nameLabel.setText (instrument != nullptr ? instrument->getName() : "No instrument", juce::dontSendNotification);

    nameLabel.setColour (juce::Label::textColourId,
                         instrument != nullptr || isLoading ? juce::Colours::black : juce::Colours::grey);

    loadButton.setEnabled (! isLoading);
    editorButton.setEnabled (instrument != nullptr && ! isLoading);

    if (onStatusChanged != nullptr)
        onStatusChanged();
}

juce::String InstrumentPanel::getStatus() const
{
    return nameLabel.getText();
}
