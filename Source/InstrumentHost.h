#pragma once

#include "Score.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <bitset>
#include <optional>
#include <vector>

/** Hosts an instrument plugin, and plays it from the on-screen keyboard and from the score.

    It's an audio device callback, so it can play straight through an audio device. Apart from the
    callbacks, everything here has to be called on the message thread.
*/
class InstrumentHost final : public juce::AudioIODeviceCallback,
                             private juce::AudioPlayHead
{
public:
    InstrumentHost() = default;

    /** The instrument hears the notes played on this keyboard, and the score's notes show up on it. */
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    /** The instrument, or null if there isn't one yet. */
    juce::AudioPluginInstance* getInstrument() const noexcept { return instrument.get(); }

    /** Replaces the instrument, deleting the old one. */
    void setInstrument (std::unique_ptr<juce::AudioPluginInstance>);

    /** The sample rate and block size to create a new instrument with. */
    double getSampleRate() const;
    int getBlockSize() const;

    /** Plays some measures of the score, as they are now. Does nothing unless the audio is running. */
    void play (const Score&, int firstMeasure, int lastMeasure);
    void stop();

    /** How far playback has got, in beats from the start of the score, or nothing if it's stopped. */
    std::optional<double> getPlaybackPosition() const noexcept;

    void prepareToPlay (double sampleRate, int maximumBlockSize);
    void releaseResources();

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

private:
    struct NoteEvent
    {
        int64_t sample;
        int noteNumber;
        bool isNoteOn;
    };

    static std::vector<NoteEvent> createNoteEvents (const Score&, int firstMeasure, int lastMeasure, double sampleRate);

    juce::Optional<PositionInfo> getPosition() const override;

    double getBeatsPlayed() const noexcept;
    void allocateInstrumentBuffer();
    void addScoreEvents (juce::MidiBuffer&, int numSamples);
    void advancePlayback (int numSamples);

    juce::MidiKeyboardState keyboardState;

    // Everything below is shared with the audio thread, which holds this lock while it's working.
    mutable juce::CriticalSection lock;
    std::unique_ptr<juce::AudioPluginInstance> instrument;
    juce::AudioBuffer<float> instrumentBuffer;
    juce::MidiBuffer midiBuffer;
    double sampleRate = 0.0;
    int blockSize = 0;

    std::vector<NoteEvent> noteEvents;
    size_t nextNoteEvent = 0;
    int64_t position = 0;               // in samples since playback started, at the start of the block being played
    int64_t endPosition = 0;
    double secondsPerBeat = 0.5;
    int beatsPerMeasure = 4;
    double firstBeat = 0.0;             // where playback started, in beats from the start of the score
    bool playing = false;
    std::bitset<128> scoreNotesOn;      // notes from the score that are sounding
    bool releaseScoreNotes = false;     // whether they need turning off

    std::atomic<double> playbackPosition { -1.0 };    // negative when stopped

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentHost)
};
