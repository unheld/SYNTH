#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

class MidiRollComponent : public juce::Component,
                          private juce::Timer
{
public:
    MidiRollComponent();
    ~MidiRollComponent() override = default;

    struct Note
    {
        int    midiNote    = 60;
        double startBeat   = 0.0;
        double lengthBeats = 1.0;
    };

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Note management
    const std::vector<Note>& getNotes() const noexcept { return notes; }
    void clearNotes();

    // Playback control
    void startPlayback();
    void stopPlayback();
    void togglePlayback();
    bool isCurrentlyPlaying() const noexcept { return isPlaying.load(); }

    void renderNextMidiBlock (juce::MidiBuffer& buffer, int numSamples, double sampleRate);

    double getBpm() const noexcept { return bpm; }

private:
    // Piano roll configuration
    static constexpr int    kMinNote          = 36;        // C2
    static constexpr int    kMaxNote          = 84;        // C6
    static constexpr int    kNoteHeight       = 18;
    static constexpr double kTotalLengthBeats = 32.0;      // 8 bars @ 4/4
    static constexpr float  kPixelsPerBeat    = 60.0f;
    static constexpr int    kTopMargin        = 4;
    static constexpr int    kLeftMargin       = 40;

    std::vector<Note> notes;
    mutable juce::SpinLock noteMutex;

    // View state
    double scrollX = 0.0;

    // Playback
    std::atomic<bool>   isPlaying { false };
    std::atomic<double> playheadBeat { 0.0 };
    double bpm = 120.0;
    double secondsPerBeat = 0.5;

    std::atomic<bool> flushActiveNotes { false };
    std::vector<int> activeNotes;

    // Drag/edit state
    int     draggingNoteIndex = -1;
    bool    resizingNote      = false;
    double  dragStartBeat     = 0.0;
    double  dragOffsetBeat    = 0.0;

    // Helpers
    int    pitchToY (int midiNote) const;
    int    yToPitch (int y) const;
    double xToBeat (int x) const;
    int    beatToX (double beat) const;
    int    hitTestNote (int x, int y) const;
    int    hitTestNoteUnlocked (int x, int y) const;

    void timerCallback() override;

    // Input
    void mouseDown  (const juce::MouseEvent& e) override;
    void mouseDrag  (const juce::MouseEvent& e) override;
    void mouseUp    (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e,
                         const juce::MouseWheelDetails& wheel) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiRollComponent)
};
