#include "MidiInputs.h"

#include <algorithm>

namespace
{
    // Where the inputs that have been turned off are kept in the settings
    const char* const turnedOffKey = "midiInputsOff";
}

MidiInputs::MidiInputs (juce::AudioDeviceManager& deviceManagerToUse, juce::MidiInputCallback& playerToUse,
                        juce::PropertiesFile& settingsToUse)
    : deviceManager (deviceManagerToUse),
      player (playerToUse),
      settings (settingsToUse)
{
    turnedOff.addTokens (settings.getValue (turnedOffKey), "\n", {});
    turnedOff.removeEmptyStrings();

    // An empty identifier means every input that's on.
    deviceManager.addMidiInputDeviceCallback ({}, &player);
    deviceManager.addMidiInputDeviceCallback ({}, this);

    updateDevices();
    deviceListConnection = juce::MidiDeviceListConnection::make ([this] { updateDevices(); });
}

MidiInputs::~MidiInputs()
{
    // Once these are gone, no more messages arrive.
    deviceManager.removeMidiInputDeviceCallback ({}, this);
    deviceManager.removeMidiInputDeviceCallback ({}, &player);
    cancelPendingUpdate();
}

std::vector<MidiInputs::Device> MidiInputs::getDevices() const
{
    std::vector<Device> devices;

    for (const auto& info : juce::MidiInput::getAvailableDevices())
        devices.push_back ({ info, deviceManager.isMidiInputDeviceEnabled (info.identifier) });

    return devices;
}

void MidiInputs::setEnabled (const juce::String& identifier, bool enabled)
{
    if (enabled)
        turnedOff.removeString (identifier);
    else
        turnedOff.addIfNotAlreadyThere (identifier);

    settings.setValue (turnedOffKey, turnedOff.joinIntoString ("\n"));
    settings.saveIfNeeded();
    updateDevices();
}

void MidiInputs::updateDevices()
{
    const auto available = juce::MidiInput::getAvailableDevices();

    // An input that's been unplugged is closed, so it opens afresh if it's plugged in again.
    for (const auto& identifier : juce::StringArray (opened))
    {
        if (std::none_of (available.begin(), available.end(), [&] (const auto& info) { return info.identifier == identifier; }))
        {
            deviceManager.setMidiInputDeviceEnabled (identifier, false);
            opened.removeString (identifier);
        }
    }

    for (const auto& info : available)
    {
        const auto enabled = ! turnedOff.contains (info.identifier);

        if (deviceManager.isMidiInputDeviceEnabled (info.identifier) != enabled)
            deviceManager.setMidiInputDeviceEnabled (info.identifier, enabled);

        if (deviceManager.isMidiInputDeviceEnabled (info.identifier))
            opened.addIfNotAlreadyThere (info.identifier);
        else
            opened.removeString (info.identifier);
    }

    if (onDevicesChanged != nullptr)
        onDevicesChanged();
}

void MidiInputs::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    if (! message.isNoteOn())
        return;

    // A key pressed while the queue is full, which would take some doing, is left out.
    const auto scope = pressedKeys.write (1);

    if (scope.blockSize1 > 0)
        pressedKeyNotes[(size_t) scope.startIndex1] = message.getNoteNumber();

    triggerAsyncUpdate();
}

void MidiInputs::handleAsyncUpdate()
{
    for (;;)
    {
        int note = -1;

        {
            const auto scope = pressedKeys.read (1);

            if (scope.blockSize1 > 0)
                note = pressedKeyNotes[(size_t) scope.startIndex1];
        }

        if (note < 0)
            return;

        if (onNoteOn != nullptr)
            onNoteOn (note);
    }
}
