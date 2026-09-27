#include "PluginWindow.h"

namespace
{
    juce::AudioProcessorEditor* createEditor (juce::AudioPluginInstance& plugin)
    {
        if (plugin.hasEditor())
            if (auto* editor = plugin.createEditorAndMakeActive())
                return editor;

        return new juce::GenericAudioProcessorEditor (plugin);
    }
}

PluginWindow::PluginWindow (juce::AudioPluginInstance& plugin, std::function<void()> onClose)
    : DocumentWindow (plugin.getName(),
                      juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
                      DocumentWindow::closeButton),
      onCloseButtonPressed (std::move (onClose))
{
    setUsingNativeTitleBar (true);

    auto* editor = createEditor (plugin);
    setContentOwned (editor, true);
    setResizable (editor->isResizable(), false);

    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

void PluginWindow::closeButtonPressed()
{
    // Call a copy, as the callback is likely to delete this window, and the original with it.
    const auto callback = onCloseButtonPressed;
    callback();
}
