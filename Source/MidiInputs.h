#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>

#include <array>
#include <functional>
#include <vector>

/** Connects MIDI controllers, so they play the instrument like the on-screen keyboard.

    Every MIDI input is used, including ones plugged in while the app is running, apart from
    any that have been turned off. Those are remembered in the settings.
*/
class MidiInputs final : private juce::MidiInputCallback,
                         private juce::AsyncUpdater
{
public:
    /** The player hears every message from the MIDI inputs that are on, on the MIDI thread. */
    MidiInputs (juce::AudioDeviceManager&, juce::MidiInputCallback& player, juce::PropertiesFile& settings);
    ~MidiInputs() override;

    struct Device
    {
        juce::MidiDeviceInfo info;
        bool enabled;
    };

    /** The MIDI inputs there are now, in the order the system lists them. */
    std::vector<Device> getDevices() const;

    /** Turns a MIDI input on or off. It stays that way the next time the app starts. */
    void setEnabled (const juce::String& identifier, bool);

    /** Called on the message thread for each key pressed on a MIDI controller. */
    std::function<void (int midiNote)> onNoteOn;

    /** Called on the message thread when a MIDI input is plugged in, unplugged, or turned on or off. */
    std::function<void()> onDevicesChanged;

private:
    void updateDevices();
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void handleAsyncUpdate() override;

    juce::AudioDeviceManager& deviceManager;
    juce::MidiInputCallback& player;
    juce::PropertiesFile& settings;
    juce::StringArray turnedOff;    // the identifiers of inputs that have been turned off
    juce::StringArray opened;       // the identifiers of inputs that are open

    // Keys pressed, waiting to be passed on to the message thread
    juce::AbstractFifo pressedKeys { 256 };
    std::array<int, 256> pressedKeyNotes {};

    juce::MidiDeviceListConnection deviceListConnection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiInputs)
};
