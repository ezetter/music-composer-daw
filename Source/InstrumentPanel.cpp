#include "InstrumentPanel.h"

namespace
{
    // Where the instrument's description and state are kept in the settings
    const char* const instrumentKey = "instrument";
    const char* const instrumentStateKey = "instrumentState";

    juce::String withEllipsis (const juce::String& text)
    {
        return text + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
    }
}

InstrumentPanel::InstrumentPanel (InstrumentHost& hostToUse, juce::PropertiesFile& settingsToUse)
    : host (hostToUse),
      settings (settingsToUse),
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
}

InstrumentPanel::~InstrumentPanel()
{
    // Keep any changes made to the instrument's sound since it was last saved.
    saveInstrument();
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
                safeThis->host.setInstrument (std::move (instrument));
                safeThis->saveInstrument();

                // A new instrument chosen by hand is a change to the score. One reloaded from the
                // settings when the app starts isn't, since a new score doesn't have one yet.
                safeThis->scoreState = safeThis->getState();

                if (source == Source::user && safeThis->onInstrumentChanged != nullptr)
                    safeThis->onInstrumentChanged();
            }
            else if (source == Source::settings)
            {
                // Forget an instrument that can't be loaded any more, rather than failing every time.
                safeThis->settings.removeValue (instrumentKey);
                safeThis->settings.removeValue (instrumentStateKey);
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
    auto* instrument = host.getInstrument();

    if (instrument == nullptr)
        return;

    settings.setValue (instrumentKey, instrument->getPluginDescription().createXml().get());
    settings.setValue (instrumentStateKey, getState().toBase64Encoding());
    settings.saveIfNeeded();
}

juce::MemoryBlock InstrumentPanel::getState() const
{
    juce::MemoryBlock state;

    if (auto* instrument = host.getInstrument())
        instrument->getStateInformation (state);

    return state;
}

//==============================================================================
juce::var InstrumentPanel::saveToJSON()
{
    auto* instrument = host.getInstrument();

    if (instrument == nullptr)
        return {};

    scoreState = getState();

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
        scoreState = getState();
        return;
    }

    juce::MemoryBlock state;
    state.fromBase64Encoding (json.getProperty ("state", {}).toString());

    // The same instrument just takes the score's settings, without loading it again.
    if (auto* instrument = host.getInstrument(); instrument != nullptr && instrument->getPluginDescription().isDuplicateOf (description))
    {
        ++instrumentsRequested;
        nameBeingLoaded = {};

        if (! state.isEmpty())
            instrument->setStateInformation (state.getData(), (int) state.getSize());

        saveInstrument();
        scoreState = getState();
        updateControls();
        return;
    }

    createInstrument (description, state, Source::score);
}

void InstrumentPanel::checkForSoundChanges()
{
    if (host.getInstrument() == nullptr || nameBeingLoaded.isNotEmpty())
        return;

    if (auto state = getState(); state != scoreState)
    {
        scoreState = std::move (state);

        if (onInstrumentChanged != nullptr)
            onInstrumentChanged();
    }
}

void InstrumentPanel::showEditor()
{
    if (editorWindow != nullptr)
    {
        editorWindow->toFront (true);
        return;
    }

    // Closing the editor saves the instrument, with any sound chosen in it.
    if (auto* instrument = host.getInstrument())
        editorWindow = std::make_unique<PluginWindow> (*instrument, [this]
        {
            saveInstrument();
            checkForSoundChanges();
            editorWindow = nullptr;
        });
}

void InstrumentPanel::updateControls()
{
    const auto* instrument = host.getInstrument();
    const auto isLoading = nameBeingLoaded.isNotEmpty();

    if (isLoading)
        nameLabel.setText (withEllipsis ("Loading " + nameBeingLoaded), juce::dontSendNotification);
    else
        nameLabel.setText (instrument != nullptr ? instrument->getName() : "No instrument", juce::dontSendNotification);

    nameLabel.setColour (juce::Label::textColourId,
                         instrument != nullptr || isLoading ? juce::Colours::black : juce::Colours::grey);

    loadButton.setEnabled (! isLoading);
    editorButton.setEnabled (instrument != nullptr && ! isLoading);
}
