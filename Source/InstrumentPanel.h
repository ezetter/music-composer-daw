#pragma once

#include "InstrumentHost.h"
#include "PluginWindow.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>

/** Shows which instrument one of the score's parts has, with buttons to load a new one and to
    open its editor.

    The instrument and its settings are kept in the app's settings, by the part's number, and
    loaded again next time. They're saved with the score too, and loaded again when the score is
    opened.
*/
class InstrumentPanel final : public juce::Component,
                              private juce::AudioProcessorListener,
                              private juce::Timer
{
public:
    /** A panel for a part, which reloads the instrument kept in the settings for that part, unless
        it's a new part being added, which starts without one.
    */
    InstrumentPanel (InstrumentHost&, int part, juce::PropertiesFile& settings, bool reloadFromSettings = true);

    int getPart() const noexcept { return part; }

    /** Moves the panel to another part's number, e.g. when a part before it is taken out. */
    void setPart (int);

    /** Stops the instrument being kept in the settings when the panel goes, as for a part that's
        being taken out, whose number another part will have.
    */
    void forgetInstrument() noexcept { forgotten = true; }

    /** What the panel says about the instrument: its name, "Loading…" or "No instrument". */
    juce::String getStatus() const;

    /** Called when the instrument, or whether it's loading, changes. */
    std::function<void()> onStatusChanged;
    ~InstrumentPanel() override;

    void resized() override;

    //==============================================================================
    /** The instrument and its settings, to save with the score, or void if there isn't one.
        Changes to its sound from now on count as unsaved.
    */
    juce::var saveToJSON();

    /** Loads the instrument a score was saved with, with its settings. A score saved without
        one leaves the current instrument as it is.
    */
    void loadFromJSON (const juce::var&);

    /** Called when the instrument, or its sound, changes from what was last saved or loaded
        with the score.
    */
    std::function<void()> onInstrumentChanged;

    /** Looks for changes to the instrument's sound, e.g. made in its editor, since it was last
        saved or loaded with the score, and calls onInstrumentChanged if there are any.

        Only an instrument that has said its parameters or settings changed is looked at. Its
        saved state alone isn't enough to go on, since some instruments keep things in it that
        have nothing to do with the sound: opening Pigments' editor changes its state, for one.
    */
    void checkForSoundChanges();

private:
    /** Where an instrument being loaded comes from. */
    enum class Source { user, settings, score };

    void chooseInstrument();
    void loadInstrumentFrom (const juce::File& pluginFile);
    void createInstrument (const juce::PluginDescription&, const juce::MemoryBlock& state = {}, Source = Source::user);
    juce::MemoryBlock getState() const;
    void startTrackingChanges();
    void listenTo (juce::AudioPluginInstance* newInstrument);

    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override;
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override;
    void timerCallback() override;
    void reloadSavedInstrument();
    void saveInstrument();
    void showEditor();
    void updateControls();

    InstrumentHost& host;
    int part;
    juce::PropertiesFile& settings;
    bool forgotten = false;

    /** Where the instrument is kept in the settings, by the part's number */
    juce::String getInstrumentKey() const;
    juce::String getInstrumentStateKey() const;
    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File pluginFolder { "/Library/Audio/Plug-Ins/VST3" };
    std::unique_ptr<PluginWindow> editorWindow;
    juce::String nameBeingLoaded;
    int instrumentsRequested = 0;

    /** The instrument's state when it was last saved or loaded with the score. */
    juce::MemoryBlock scoreState;

    /** Set, on whichever thread the instrument calls from, when it says its sound has changed. */
    std::atomic<bool> soundTouched { false };

    juce::Label nameLabel;
    juce::TextButton loadButton, editorButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentPanel)
};
