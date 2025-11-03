#pragma once

#include <JuceHeader.h>
#include <vector>

class MidiRollComponent : public juce::Component
{
public:
    struct Note
    {
        int midiNote = 60;
        int startStep = 0;
        int lengthSteps = 4;
        float velocity = 0.9f;
    };

    MidiRollComponent();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    void setLoopBeats(double beats);
    double getLoopBeats() const noexcept { return loopBeats; }
    int getTotalSteps() const noexcept { return totalSteps; }

    void setRange(int lowest, int highest);
    int getLowestNote() const noexcept { return lowestNote; }
    int getHighestNote() const noexcept { return highestNote; }

    void setPlayheadBeat(double beat);
    double getPlayheadBeat() const noexcept { return playheadBeat; }

    const std::vector<Note>& getNotes() const noexcept { return notes; }
    void setNotes(const std::vector<Note>& newNotes);
    void clear();

    juce::MidiMessageSequence createMidiSequence(int midiChannel, double bpm) const;

    std::function<void()> onNotesChanged;

private:
    enum class DragMode
    {
        None,
        Create,
        Move,
        Stretch
    };

    juce::Rectangle<float> getContentArea() const noexcept;
    int stepFromX(float x) const noexcept;
    int noteFromY(float y) const noexcept;
    juce::Rectangle<float> getNoteBounds(const Note& note) const noexcept;
    int findNoteAt(int midiNote, int step) const noexcept;
    void deleteNoteAt(int midiNote, int step);
    void commitNotesChange(bool notify);
    void quantizeNote(Note& note) const noexcept;
    void startDrag(const juce::MouseEvent& event, int step, int midiNote);
    void updateDragPosition(const juce::MouseEvent& event);
    void endDrag();

    std::vector<Note> notes;
    double loopBeats = 16.0;
    int totalSteps = 64;
    int lowestNote = 36;
    int highestNote = 84;
    double playheadBeat = 0.0;

    int draggingIndex = -1;
    DragMode dragMode = DragMode::None;
    int dragAnchorStep = 0;
    int dragOffsetSteps = 0;
};

