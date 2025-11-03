#pragma once
#include <JuceHeader.h>
#include <functional>
#include <vector>

class MidiRollComponent : public juce::Component,
                          private juce::Timer
{
public:
    struct Note
    {
        int midiNote = 60;
        double startBeat = 0.0;
        double lengthBeats = 1.0;
        float velocity = 0.8f;
    };

    MidiRollComponent();
    ~MidiRollComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setNoteOnCallback(std::function<void(int, float)> cb);
    void setNoteOffCallback(std::function<void(int)> cb);

    double getBpm() const noexcept { return bpm; }
    void setBpm(double newBpm);

    int getLoopLengthInBeats() const noexcept { return loopLengthBeats; }
    void setLoopLengthInBeats(int newLength);

    void play();
    void stop();
    void restart();
    void seekToBeat(double beatPosition);

    void importFromFile();
    void exportToFile() const;

    const std::vector<Note>& getNotes() const noexcept { return notes; }

private:
    class Editor;

    void timerCallback() override;

    void ensureTimerRunning();
    void sendAllNotesOff();
    void updateContentSize();
    void sortNotes();

    double snapBeat(double beat) const noexcept;
    double beatFromX(float x) const noexcept;
    float xFromBeat(double beat) const noexcept;
    int midiNoteFromY(float y) const noexcept;
    int rowFromMidiNote(int midiNote) const noexcept;
    float yFromMidiNote(int midiNote) const noexcept;

    void addNote(const Note& note);
    void removeNote(int noteIndex);

    struct ActiveNote
    {
        int index = -1;
        double endBeatAbsolute = 0.0;
    };

    friend class MidiRollComponent::Editor;

    // ===== Layout constants =====
    static constexpr int lowestMidiNote = 36;   // C2
    static constexpr int highestMidiNote = 84;  // C6
    static constexpr int rowHeight = 26;
    static constexpr int timelineHeight = 24;
    static constexpr int labelWidth = 72;
    static constexpr double beatPixelWidth = 90.0;
    static constexpr double gridSubdivision = 0.25; // quarter beat

    std::vector<Note> notes;
    std::vector<ActiveNote> activeNotes;

    std::unique_ptr<Editor> editor;
    juce::Viewport viewport;

    juce::Slider bpmSlider;
    juce::Slider loopLengthSlider;
    juce::TextButton playButton{ "Play" };
    juce::TextButton stopButton{ "Stop" };
    juce::TextButton restartButton{ "Restart" };
    juce::TextButton importButton{ "Import MIDI" };
    juce::TextButton exportButton{ "Export MIDI" };
    juce::ToggleButton followButton{ "Follow" };
    juce::Label bpmLabel{ "BPM", "BPM" };
    juce::Label loopLabel{ "Loop", "Loop" };

    std::function<void(int, float)> noteOnCallback;
    std::function<void(int)> noteOffCallback;

    double bpm = 120.0;
    int loopLengthBeats = 16;
    double absoluteBeatPosition = 0.0;
    double lastAbsoluteBeatPosition = 0.0;
    double playheadBeat = 0.0;
    bool playing = false;
    bool followEnabled = true;
    double lastTimerSeconds = 0.0;
};

