#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

/** A window showing a plugin's editor, or a generic one if the plugin doesn't have its own. */
class PluginWindow final : public juce::DocumentWindow
{
public:
    /** The window leaves its owner to delete it when the close button is pressed. */
    PluginWindow (juce::AudioPluginInstance&, std::function<void()> onCloseButtonPressed);

    void closeButtonPressed() override;

private:
    std::function<void()> onCloseButtonPressed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};
