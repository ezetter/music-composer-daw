#pragma once

#include "InstrumentHost.h"
#include "PluginWindow.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** Shows which instrument is loaded, with buttons to load a new one and to open its editor. */
class InstrumentPanel final : public juce::Component
{
public:
    explicit InstrumentPanel (InstrumentHost&);

    void resized() override;

private:
    void chooseInstrument();
    void loadInstrumentFrom (const juce::File& pluginFile);
    void createInstrument (const juce::PluginDescription&);
    void showEditor();
    void updateControls();

    InstrumentHost& host;
    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File pluginFolder { "/Library/Audio/Plug-Ins/VST3" };
    std::unique_ptr<PluginWindow> editorWindow;
    juce::String nameBeingLoaded;

    juce::Label nameLabel;
    juce::TextButton loadButton, editorButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentPanel)
};
