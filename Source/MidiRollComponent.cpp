#include "MidiRollComponent.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float cornerRadius = 4.0f;
    constexpr float dragHandleWidth = 8.0f;
}

MidiRollComponent::MidiRollComponent()
{
    setInterceptsMouseClicks(true, true);
}

void MidiRollComponent::paint(juce::Graphics& g)
{
    auto area = getContentArea();

    g.setColour(juce::Colour::fromRGB(18, 18, 20));
    g.fillRect(area);

    const int noteCount = juce::jmax(1, highestNote - lowestNote + 1);
    const float rowHeight = area.getHeight() / (float)noteCount;
    const float stepWidth = area.getWidth() / (float)juce::jmax(1, totalSteps);

    // Alternate row shading for quick orientation
    for (int note = lowestNote; note <= highestNote; ++note)
    {
        const int rowIndex = highestNote - note;
        if (juce::MidiMessage::isMidiNoteBlack(note))
        {
            auto row = area.withHeight(rowHeight).withY(area.getY() + rowIndex * rowHeight);
            g.setColour(juce::Colour::fromRGBA(255, 255, 255, 12));
            g.fillRect(row);
        }
    }

    // Vertical grid: 16th, beats, bars
    for (int step = 0; step <= totalSteps; ++step)
    {
        const float x = area.getX() + step * stepWidth;
        const bool isBar = (step % 16) == 0;
        const bool isBeat = (step % 4) == 0;
        const float thickness = isBar ? 1.4f : (isBeat ? 1.0f : 0.6f);
        juce::Colour colour = isBar ? juce::Colour::fromRGB(58, 58, 64)
                                    : (isBeat ? juce::Colour::fromRGB(40, 40, 44)
                                              : juce::Colour::fromRGBA(255, 255, 255, 38));
        g.setColour(colour.withAlpha(isBar ? 0.9f : (isBeat ? 0.6f : 0.25f)));
        g.drawLine(x, area.getY(), x, area.getBottom(), thickness);
    }

    // Horizontal grid lines
    for (int i = 0; i <= noteCount; ++i)
    {
        const float y = area.getY() + i * rowHeight;
        const bool octaveLine = ((highestNote - i + 1) % 12) == 0;
        g.setColour(octaveLine ? juce::Colour::fromRGB(52, 52, 58)
                               : juce::Colour::fromRGBA(255, 255, 255, 32));
        g.drawLine(area.getX(), y, area.getRight(), y, octaveLine ? 1.0f : 0.5f);
    }

    // Draw notes
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto& note = notes[i];
        auto bounds = getNoteBounds(note);
        if (bounds.getWidth() <= 0.0f || bounds.getHeight() <= 0.0f)
            continue;

        juce::Colour fill = juce::Colour::fromRGB(255, 140, 66);
        if ((int)i == draggingIndex)
            fill = fill.brighter(0.2f);

        g.setColour(fill.withAlpha(0.9f));
        g.fillRoundedRectangle(bounds, cornerRadius);

        g.setColour(fill.brighter(0.4f));
        g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);

        auto handle = bounds.withWidth(std::min(bounds.getWidth(), dragHandleWidth))
                             .withX(bounds.getRight() - std::min(bounds.getWidth(), dragHandleWidth));
        g.setColour(fill.brighter(0.6f).withAlpha(0.7f));
        g.fillRoundedRectangle(handle, cornerRadius);
    }

    if (loopBeats > 0.0)
    {
        double wrappedBeat = playheadBeat;
        if (wrappedBeat < 0.0)
        {
            wrappedBeat = std::fmod(std::abs(wrappedBeat), loopBeats);
            wrappedBeat = loopBeats - wrappedBeat;
        }
        wrappedBeat = std::fmod(wrappedBeat, loopBeats);

        const float x = area.getX() + (float)((wrappedBeat / loopBeats) * area.getWidth());
        g.setColour(juce::Colour::fromRGB(255, 125, 30));
        g.drawLine(x, area.getY(), x, area.getBottom(), 2.0f);
    }
}

void MidiRollComponent::resized()
{
}

void MidiRollComponent::mouseDown(const juce::MouseEvent& event)
{
    if (!getContentArea().contains(event.position))
        return;

    const int step = juce::jlimit(0, totalSteps - 1, stepFromX(event.position.x));
    const int midi = juce::jlimit(lowestNote, highestNote, noteFromY(event.position.y));

    if (event.mods.isRightButtonDown())
    {
        deleteNoteAt(midi, step);
        return;
    }

    startDrag(event, step, midi);
    repaint();
}

void MidiRollComponent::mouseDrag(const juce::MouseEvent& event)
{
    updateDragPosition(event);
}

void MidiRollComponent::mouseUp(const juce::MouseEvent&)
{
    endDrag();
}

void MidiRollComponent::setLoopBeats(double beats)
{
    loopBeats = beats > 0.0 ? beats : 4.0;
    totalSteps = juce::jmax(4, (int)std::round(loopBeats * 4.0));
    for (auto& note : notes)
        quantizeNote(note);
    repaint();
    if (onNotesChanged)
        onNotesChanged();
}

void MidiRollComponent::setRange(int lowest, int highest)
{
    if (lowest >= highest)
        return;

    lowestNote = lowest;
    highestNote = highest;
    for (auto& note : notes)
        note.midiNote = juce::jlimit(lowestNote, highestNote, note.midiNote);
    repaint();
    if (onNotesChanged)
        onNotesChanged();
}

void MidiRollComponent::setPlayheadBeat(double beat)
{
    if (loopBeats <= 0.0)
        return;

    double wrapped = std::fmod(beat, loopBeats);
    if (wrapped < 0.0)
        wrapped += loopBeats;

    if (std::abs(wrapped - playheadBeat) > 1.0e-3)
    {
        playheadBeat = wrapped;
        repaint();
    }
}

void MidiRollComponent::setNotes(const std::vector<Note>& newNotes)
{
    notes = newNotes;
    for (auto& note : notes)
        quantizeNote(note);
    repaint();
    if (onNotesChanged)
        onNotesChanged();
}

void MidiRollComponent::clear()
{
    notes.clear();
    repaint();
    if (onNotesChanged)
        onNotesChanged();
}

juce::MidiMessageSequence MidiRollComponent::createMidiSequence(int midiChannel, double bpm) const
{
    juce::MidiMessageSequence sequence;
    if (bpm <= 0.0)
        return sequence;

    const double secondsPerBeat = 60.0 / bpm;

    for (const auto& note : notes)
    {
        const double startBeat = note.startStep * 0.25;
        const double endBeat = (note.startStep + note.lengthSteps) * 0.25;
        const double startSeconds = startBeat * secondsPerBeat;
        const double endSeconds = endBeat * secondsPerBeat;

        auto on = juce::MidiMessage::noteOn(midiChannel, note.midiNote, note.velocity);
        on.setTimeStamp(startSeconds);
        sequence.addEvent(on);

        auto off = juce::MidiMessage::noteOff(midiChannel, note.midiNote);
        off.setTimeStamp(endSeconds);
        sequence.addEvent(off);
    }

    sequence.sort();
    sequence.updateMatchedPairs();
    return sequence;
}

juce::Rectangle<float> MidiRollComponent::getContentArea() const noexcept
{
    return getLocalBounds().toFloat().reduced(8.0f, 6.0f);
}

int MidiRollComponent::stepFromX(float x) const noexcept
{
    auto area = getContentArea();
    const float relative = juce::jlimit(0.0f, 1.0f, (x - area.getX()) / juce::jmax(1.0f, area.getWidth()));
    return (int)std::floor(relative * (float)totalSteps + 0.0001f);
}

int MidiRollComponent::noteFromY(float y) const noexcept
{
    auto area = getContentArea();
    const float relative = juce::jlimit(0.0f, 1.0f, (y - area.getY()) / juce::jmax(1.0f, area.getHeight()));
    const int noteCount = juce::jmax(1, highestNote - lowestNote + 1);
    const int index = juce::jlimit(0, noteCount - 1, (int)std::floor(relative * (float)noteCount));
    return highestNote - index;
}

juce::Rectangle<float> MidiRollComponent::getNoteBounds(const Note& note) const noexcept
{
    auto area = getContentArea();
    const int noteCount = juce::jmax(1, highestNote - lowestNote + 1);
    const float rowHeight = area.getHeight() / (float)noteCount;
    const float stepWidth = area.getWidth() / (float)juce::jmax(1, totalSteps);

    const int rowIndex = highestNote - juce::jlimit(lowestNote, highestNote, note.midiNote);
    const float x = area.getX() + (float)note.startStep * stepWidth;
    const float y = area.getY() + rowIndex * rowHeight + 1.0f;
    const float w = (float)note.lengthSteps * stepWidth;
    const float h = rowHeight - 2.0f;

    return { x + 1.0f, y, std::max(1.0f, w - 2.0f), std::max(4.0f, h - 2.0f) };
}

int MidiRollComponent::findNoteAt(int midiNote, int step) const noexcept
{
    for (int i = (int)notes.size() - 1; i >= 0; --i)
    {
        const auto& note = notes[(size_t)i];
        if (note.midiNote != midiNote)
            continue;
        if (step >= note.startStep && step < note.startStep + note.lengthSteps)
            return i;
    }
    return -1;
}

void MidiRollComponent::deleteNoteAt(int midiNote, int step)
{
    const int index = findNoteAt(midiNote, step);
    if (index < 0)
        return;

    notes.erase(notes.begin() + index);
    repaint();
    if (onNotesChanged)
        onNotesChanged();
}

void MidiRollComponent::commitNotesChange(bool notify)
{
    for (auto& note : notes)
        quantizeNote(note);

    if (notify && onNotesChanged)
        onNotesChanged();

    repaint();
}

void MidiRollComponent::quantizeNote(Note& note) const noexcept
{
    note.midiNote = juce::jlimit(lowestNote, highestNote, note.midiNote);
    note.startStep = juce::jlimit(0, totalSteps - 1, note.startStep);
    note.lengthSteps = juce::jlimit(1, totalSteps - note.startStep, note.lengthSteps);
}

void MidiRollComponent::startDrag(const juce::MouseEvent& event, int step, int midiNote)
{
    draggingIndex = findNoteAt(midiNote, step);
    dragMode = DragMode::None;

    if (draggingIndex >= 0)
    {
        auto bounds = getNoteBounds(notes[(size_t)draggingIndex]);
        const float resizeThreshold = std::min(bounds.getWidth() * 0.35f, dragHandleWidth + 4.0f);
        if (event.position.x >= bounds.getRight() - resizeThreshold)
        {
            dragMode = DragMode::Stretch;
            dragAnchorStep = notes[(size_t)draggingIndex].startStep;
        }
        else
        {
            dragMode = DragMode::Move;
            dragOffsetSteps = step - notes[(size_t)draggingIndex].startStep;
        }
    }
    else
    {
        Note note;
        note.midiNote = midiNote;
        note.startStep = step;
        note.lengthSteps = juce::jlimit(1, totalSteps - step, 1);
        note.velocity = 0.9f;
        notes.push_back(note);
        draggingIndex = (int)notes.size() - 1;
        dragMode = DragMode::Create;
        dragAnchorStep = step;
    }
}

void MidiRollComponent::updateDragPosition(const juce::MouseEvent& event)
{
    if (draggingIndex < 0 || dragMode == DragMode::None)
        return;

    const int step = juce::jlimit(0, totalSteps - 1, stepFromX(event.position.x));
    auto& note = notes[(size_t)draggingIndex];

    if (dragMode == DragMode::Move)
    {
        const int newStart = juce::jlimit(0, totalSteps - note.lengthSteps, step - dragOffsetSteps);
        note.startStep = newStart;
    }
    else
    {
        const int anchor = dragMode == DragMode::Stretch ? dragAnchorStep : dragAnchorStep;
        const int start = juce::jlimit(0, totalSteps - 1, std::min(anchor, step));
        const int end = juce::jlimit(start + 1, totalSteps, std::max(anchor, step) + 1);
        note.startStep = start;
        note.lengthSteps = juce::jlimit(1, totalSteps - note.startStep, end - note.startStep);
    }

    quantizeNote(note);
    repaint();
}

void MidiRollComponent::endDrag()
{
    if (draggingIndex >= 0)
        commitNotesChange(true);

    draggingIndex = -1;
    dragMode = DragMode::None;
}

