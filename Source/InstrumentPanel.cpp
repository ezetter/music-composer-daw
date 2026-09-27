#include "InstrumentPanel.h"

namespace
{
    juce::String withEllipsis (const juce::String& text)
    {
        return text + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
    }
}

InstrumentPanel::InstrumentPanel (InstrumentHost& hostToUse)
    : host (hostToUse),
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

void InstrumentPanel::createInstrument (const juce::PluginDescription& description)
{
    nameBeingLoaded = description.name;
    updateControls();

    formatManager.createPluginInstanceAsync (description, host.getSampleRate(), host.getBlockSize(),
        [safeThis = juce::Component::SafePointer (this), name = description.name]
        (std::unique_ptr<juce::AudioPluginInstance> instrument, const juce::String& error)
        {
            if (safeThis == nullptr)
                return;

            safeThis->nameBeingLoaded = {};

            if (instrument != nullptr)
            {
                // The old instrument's editor has to go before the old instrument does.
                safeThis->editorWindow = nullptr;
                safeThis->host.setInstrument (std::move (instrument));
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "Couldn't load " + name, error);
            }

            safeThis->updateControls();
        });
}

void InstrumentPanel::showEditor()
{
    if (editorWindow != nullptr)
    {
        editorWindow->toFront (true);
        return;
    }

    if (auto* instrument = host.getInstrument())
        editorWindow = std::make_unique<PluginWindow> (*instrument, [this] { editorWindow = nullptr; });
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
