#pragma once

#include "Score.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <bitset>
#include <optional>
#include <vector>

/** Hosts an instrument plugin for each of the score's parts, and plays them from the score,
    in time together. The on-screen keyboard and MIDI controllers play the active part's instrument.

    It's an audio device callback, so it can play straight through an audio device. Apart from the
    callbacks, everything here has to be called on the message thread.
*/
class InstrumentHost final : public juce::AudioIODeviceCallback,
                             public juce::MidiInputCallback,
                             private juce::AudioPlayHead
{
public:
    /** It starts with a slot for each part of a new score, without instruments. */
    InstrumentHost();

    /** The active part's instrument hears the notes played on this keyboard, and the active part's
        notes from the score show up on it.
    */
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    /** How many parts there are instruments for, which has to keep up with the score's parts. */
    int getNumParts() const noexcept { return (int) slots.size(); }

    /** Makes room for a new part's instrument, before the part at that index, or at the end.
        It doesn't have an instrument yet, and plays at 0 dB.
    */
    void insertPart (int index);

    /** Takes out a part's instrument, deleting it. The parts after it move up. */
    void removePart (int index);

    /** A part's instrument, or null if it doesn't have one yet. */
    juce::AudioPluginInstance* getInstrument (int part) const noexcept { return slots[(size_t) part]->instrument.get(); }

    /** Replaces a part's instrument, deleting the old one. */
    void setInstrument (int part, std::unique_ptr<juce::AudioPluginInstance>);

    /** Chooses the part whose instrument the keyboard and MIDI controllers play. Notes they're
        holding on the other part's instrument are let go.
    */
    void setActivePart (int part);
    int getActivePart() const noexcept { return activePart; }

    /** How loud a part's instrument is in the mix, in decibels. At minVolume or below, it's silent. */
    void setVolume (int part, float decibels);
    float getVolume (int part) const noexcept { return slots[(size_t) part]->volume; }

    static constexpr float minVolume = -60.0f, maxVolume = 6.0f;

    /** Silences a part's instrument, or lets it play again at its volume. */
    void setMuted (int part, bool);
    bool isMuted (int part) const noexcept { return slots[(size_t) part]->muted; }

    /** The sample rate and block size to create a new instrument with. */
    double getSampleRate() const;
    int getBlockSize() const;

    /** Plays some measures of the score, as they are now, every part on its own instrument. Does
        nothing unless the audio is running. When looping, they play over and over until stopped.
    */
    void play (const Score&, int firstMeasure, int lastMeasure, bool loop = false);
    void stop();

    /** Whether playback goes back to the start of the measures it's playing when it reaches the
        end of them, rather than stopping. This can change while they're playing.
    */
    void setLooping (bool);

    /** Changes the measures being played to the score as it is now, from the next time through
        when looping. Playing through once is left as it is.
    */
    void updateLoop (const Score&, int firstMeasure, int lastMeasure);

    /** How far playback has got, in beats from the start of the score, or nothing if it's stopped. */
    std::optional<double> getPlaybackPosition() const noexcept;

    void prepareToPlay (double sampleRate, int maximumBlockSize);
    void releaseResources();

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

    /** Plays a message from a MIDI controller. Its notes show on the on-screen keyboard too.
        This can be called on any thread.
    */
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

private:
    /** A note starting or stopping, or the sustain pedal going down or coming up. */
    struct NoteEvent
    {
        int64_t sample;
        int noteNumber;                 // -1 for the pedal
        bool isNoteOn;                  // for the pedal, whether it goes down
        juce::uint8 velocity = 0;       // a note-on's, from 1 to 127
        bool isPedal = false;
    };

    /** Some measures of the score, ready to play. */
    struct Passage
    {
        std::vector<std::vector<NoteEvent>> events;     // for each part
        int64_t length = 0;             // in samples
        double secondsPerBeat = 0.5;
        int beatsPerMeasure = 4;
        double firstBeat = 0.0;         // where it starts, in beats from the start of the score
    };

    static Passage createPassage (const Score&, int firstMeasure, int lastMeasure, double sampleRate);
    static std::vector<NoteEvent> createNoteEvents (const Score&, int part, int firstMeasure, int lastMeasure, double sampleRate);

    /** A part's instrument, and what it plays. */
    struct Slot
    {
        std::unique_ptr<juce::AudioPluginInstance> instrument;
        juce::AudioBuffer<float> buffer;
        juce::MidiBuffer midi;
        std::bitset<128> scoreNotesOn;      // notes from the score that are sounding
        bool scorePedalDown = false;        // whether the score has the sustain pedal down
        std::atomic<float> volume { 0.0f };                 // in decibels
        std::atomic<bool> muted { false };
        juce::SmoothedValue<float> gain { 1.0f };           // following the volume, so changes don't click
    };

    juce::Optional<PositionInfo> getPosition() const override;

    double getBeatsPlayed() const noexcept;
    void allocateInstrumentBuffer (Slot&);
    void addScoreEvents (int numSamples);
    void letGoOfKeyboardNotes (Slot&);
    void advancePlayback();

    juce::MidiKeyboardState keyboardState;
    juce::MidiMessageCollector midiInput;     // messages from MIDI controllers, waiting for the next block
    std::atomic<bool> midiInputReady { false };

    // Everything below is shared with the audio thread, which holds this lock while it's working.
    mutable juce::CriticalSection lock;
    std::vector<std::unique_ptr<Slot>> slots;      // for each part
    std::atomic<int> activePart { 0 };
    int keyboardPart = 0;               // the part the keyboard played in the last block
    double sampleRate = 0.0;
    int blockSize = 0;

    Passage passage;                    // what's playing
    Passage nextPassage;                // what the loop plays next time through, if hasNextPassage
    bool hasNextPassage = false;
    std::vector<size_t> nextNoteEvents;            // for each part
    int64_t position = 0;               // in samples since this time through started, at the start of the block being played
    int64_t nextPosition = 0;           // where the next block starts, once this one's been played
    bool looping = false;
    bool playing = false;
    bool releaseScoreNotes = false;     // whether the score's notes that are sounding need turning off

    std::atomic<double> playbackPosition { -1.0 };    // negative when stopped

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentHost)
};
