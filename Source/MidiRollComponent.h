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
        int id = 0;
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

    void importFromFile();
    void exportToFile();

    std::vector<Note> getNotes() const { return notes; }

private:
    class GridComponent;
    friend class GridComponent;

    void timerCallback() override;

    Note& createNote(int midiNote, double startBeat, double lengthBeats, float velocity);
    void removeNoteById(int noteId);
    Note* findNoteById(int noteId);
    Note* findNoteAtPosition(int midiNote, double beat);
    void notifyNotesChanged();
    void sortNotesByPosition();
    void enforceLoopBounds(Note& note);
    double quantiseBeat(double beat) const;

    int rowToMidiNote(int row) const noexcept;
    int midiNoteToRow(int midiNote) const noexcept;
    int getTotalNoteRows() const noexcept;

    double beatToPixel(double beat) const noexcept;

    void handlePlaybackAdvance(double previousAbsoluteBeat, double newAbsoluteBeat);
    void startNotePlayback(const Note& note, double absoluteEndBeat);
    void stopNotePlayback(int noteId);
    void sendAllActiveNotesOff();
    void updateContentSize();
    void ensurePlayheadVisible();
    void refreshTransportButtons();
    void resetPlaybackState();

    std::function<void(int, float)> noteOnCallback;
    std::function<void(int)> noteOffCallback;

    std::unique_ptr<GridComponent> gridComponent;
    juce::Viewport viewport;

    juce::Slider bpmSlider;
    juce::Slider loopLengthSlider;
    juce::Label bpmLabel;
    juce::Label loopLabel;
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::TextButton restartButton;
    juce::TextButton importButton;
    juce::TextButton exportButton;
    juce::ToggleButton followButton;

    std::vector<Note> notes;
    int nextNoteId = 1;

    struct ActiveNote
    {
        int id = 0;
        double scheduledEndBeat = 0.0;
    };

    std::vector<ActiveNote> activeNotes;

    double bpm = 120.0;
    int loopLengthBeats = 16;

    double absoluteBeat = 0.0;
    double previousAbsoluteBeat = 0.0;
    double playheadBeat = 0.0;

    bool playing = false;
    bool followEnabled = true;

    const double pixelsPerBeat = 96.0;
    const double minimumNoteLength = 0.125;
    const int noteHeight = 22;
    const int keyboardWidth = 76;
    const int lowestMidiNote = 24;
    const int highestMidiNote = 108;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiRollComponent)
};
