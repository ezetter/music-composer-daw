#pragma once

#include "InstrumentHost.h"
#include "PluginWindow.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** Shows which instrument is loaded, with buttons to load a new one and to open its editor.

    The instrument and its settings are kept in the app's settings, and loaded again next time.
*/
class InstrumentPanel final : public juce::Component
{
public:
    InstrumentPanel (InstrumentHost&, juce::PropertiesFile& settings);
    ~InstrumentPanel() override;

    void resized() override;

private:
    void chooseInstrument();
    void loadInstrumentFrom (const juce::File& pluginFile);
    void createInstrument (const juce::PluginDescription&, const juce::MemoryBlock& state = {}, bool reloading = false);
    void reloadSavedInstrument();
    void saveInstrument();
    void showEditor();
    void updateControls();

    InstrumentHost& host;
    juce::PropertiesFile& settings;
    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File pluginFolder { "/Library/Audio/Plug-Ins/VST3" };
    std::unique_ptr<PluginWindow> editorWindow;
    juce::String nameBeingLoaded;

    juce::Label nameLabel;
    juce::TextButton loadButton, editorButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentPanel)
};
