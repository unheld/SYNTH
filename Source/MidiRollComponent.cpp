#include "MidiRollComponent.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double minNoteLength = MidiRollComponent::gridSubdivision;
    constexpr double ticksPerQuarterNote = 960.0;
    constexpr int timerFrequencyHz = 120;
}

//==============================================================================
class MidiRollComponent::Editor : public juce::Component
{
public:
    explicit Editor(MidiRollComponent& ownerRef)
        : owner(ownerRef)
    {
        setInterceptsMouseClicks(true, true);
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds();
        auto timeline = bounds.removeFromTop(timelineHeight);

        g.setColour(juce::Colour(0xff101010));
        g.fillRect(timeline);

        g.setColour(juce::Colour(0xff1c1c1c));
        g.fillRect(juce::Rectangle<int>(0, timeline.getBottom(), labelWidth, bounds.getHeight()));

        const double loopLength = std::max(1, owner.loopLengthBeats);
        const int totalBeats = static_cast<int>(std::ceil(loopLength));

        // Draw beat grid and numbers
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        for (int beat = 0; beat <= totalBeats; ++beat)
        {
            const float x = owner.xFromBeat(static_cast<double>(beat));
            g.drawLine(x, (float)timeline.getBottom(), x, (float)getHeight(), beat % 4 == 0 ? 1.6f : 0.8f);

            if (beat < totalBeats)
            {
                juce::String label = juce::String(beat + 1);
                auto labelX = (int)(x + 4.0f);
                g.drawText(label, labelX, timeline.getY(), (int)owner.beatPixelWidth - 8, timeline.getHeight(), juce::Justification::centredLeft, false);
            }
        }

        const int numRows = MidiRollComponent::highestMidiNote - MidiRollComponent::lowestMidiNote + 1;
        for (int row = 0; row < numRows; ++row)
        {
            const int midiNote = MidiRollComponent::highestMidiNote - row;
            const bool isBlack = juce::MidiMessage::isMidiNoteBlack(midiNote);
            auto rowY = timeline.getBottom() + row * rowHeight;
            auto rowRect = juce::Rectangle<int>(0, rowY, getWidth(), rowHeight);

            juce::Colour background = isBlack ? juce::Colour(0xff181818) : juce::Colour(0xff202020);
            g.setColour(background);
            g.fillRect(rowRect);

            g.setColour(juce::Colour(0xff242424));
            g.drawLine(0.0f, (float)rowRect.getBottom(), (float)getWidth(), (float)rowRect.getBottom(), 0.6f);

            // Note labels on the left
            juce::String noteName = juce::MidiMessage::getMidiNoteName(midiNote, true, true, 4);
            g.setColour(juce::Colours::white.withAlpha(0.75f));
            g.drawText(noteName, 0, rowY, labelWidth - 6, rowHeight, juce::Justification::centredRight, false);
        }

        // Draw subdivision grid lines
        const double subdivisionsPerBeat = 1.0 / MidiRollComponent::gridSubdivision;
        const double totalSubdivisions = loopLength * subdivisionsPerBeat;
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        for (int i = 0; i <= static_cast<int>(totalSubdivisions); ++i)
        {
            const double beat = static_cast<double>(i) * MidiRollComponent::gridSubdivision;
            const float x = owner.xFromBeat(beat);
            g.drawLine(x, (float)timeline.getBottom(), x, (float)getHeight(), 0.4f);
        }

        // Draw notes
        const auto& noteList = owner.notes;
        for (size_t i = 0; i < noteList.size(); ++i)
        {
            const auto& note = noteList[i];
            const int midiNote = juce::jlimit(MidiRollComponent::lowestMidiNote, MidiRollComponent::highestMidiNote, note.midiNote);
            const float noteX = owner.xFromBeat(note.startBeat);
            const float noteW = (float)juce::jmax(minNoteLength * owner.beatPixelWidth, note.lengthBeats * owner.beatPixelWidth);
            const float noteY = (float)(timeline.getBottom() + owner.rowFromMidiNote(midiNote) * rowHeight) + 1.0f;
            const float noteH = (float)rowHeight - 2.0f;

            juce::Rectangle<float> rect(noteX + 1.0f, noteY, noteW - 2.0f, noteH);
            const bool selected = (static_cast<int>(i) == selectedNote);
            g.setColour(selected ? juce::Colour(0xfff39c12) : juce::Colour(0xfff2784b));
            g.fillRoundedRectangle(rect, 3.0f);
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.drawRoundedRectangle(rect, 3.0f, 1.2f);
        }

        // Draw playhead
        g.setColour(juce::Colour(0xff81d4fa));
        const float playheadX = owner.xFromBeat(owner.playheadBeat);
        g.drawLine(playheadX, (float)timeline.getY(), playheadX, (float)getHeight(), 1.6f);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
        {
            const int index = findNoteAt(event.position);
            if (index >= 0)
            {
                owner.removeNote(index);
                selectedNote = -1;
                repaint();
            }
            return;
        }

        if (event.y < timelineHeight)
        {
            const double beat = owner.snapBeat(owner.beatFromX((float)event.x));
            owner.seekToBeat(beat);
            repaint();
            return;
        }

        const int index = findNoteAt(event.position);
        if (index >= 0)
        {
            selectedNote = index;
            const auto& note = owner.notes[(size_t)index];
            dragMode = DragMode::Move;
            dragIndex = index;
            dragOriginalNote = note;
            dragGrabOffset = owner.snapBeat(owner.beatFromX((float)event.x)) - note.startBeat;

            const double beatAtPos = owner.snapBeat(owner.beatFromX((float)event.x));
            const double endBeat = note.startBeat + note.lengthBeats;
            const double distanceToEnd = std::abs(endBeat - beatAtPos) * owner.beatPixelWidth;
            if (distanceToEnd <= 8.0 || event.mods.isShiftDown())
            {
                dragMode = DragMode::Resize;
                dragGrabOffset = note.lengthBeats;
            }
        }
        else
        {
            MidiRollComponent::Note newNote;
            newNote.midiNote = owner.midiNoteFromY((float)event.y);
            newNote.startBeat = owner.snapBeat(owner.beatFromX((float)event.x));
            newNote.startBeat = juce::jlimit(0.0, (double)owner.loopLengthBeats - minNoteLength, newNote.startBeat);
            newNote.lengthBeats = juce::jmax(minNoteLength, owner.gridSubdivision);
            const double availableLength = std::max(minNoteLength, (double)owner.loopLengthBeats - newNote.startBeat);
            newNote.lengthBeats = juce::jlimit(minNoteLength, availableLength, newNote.lengthBeats);
            newNote.velocity = 0.85f;

            owner.notes.push_back(newNote);
            dragIndex = static_cast<int>(owner.notes.size()) - 1;
            selectedNote = dragIndex;
            dragMode = DragMode::Resize;
            dragOriginalNote = newNote;
            dragGrabOffset = newNote.lengthBeats;
            owner.editor->repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (dragIndex < 0 || dragIndex >= static_cast<int>(owner.notes.size()))
            return;

        auto& note = owner.notes[(size_t)dragIndex];

        if (dragMode == DragMode::Move)
        {
            double beat = owner.snapBeat(owner.beatFromX((float)event.x) - dragGrabOffset);
            beat = juce::jlimit(0.0, (double)owner.loopLengthBeats - minNoteLength, beat);
            note.startBeat = beat;
            note.midiNote = owner.midiNoteFromY((float)event.y);
        }
        else if (dragMode == DragMode::Resize)
        {
            double beat = owner.snapBeat(owner.beatFromX((float)event.x));
            const double availableLength = std::max(minNoteLength, (double)owner.loopLengthBeats - note.startBeat);
            double newLength = juce::jlimit(minNoteLength, availableLength, beat - note.startBeat);
            newLength = juce::jmax(minNoteLength, newLength);
            note.lengthBeats = newLength;
        }

        owner.editor->repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragIndex >= 0)
        {
            owner.sortNotes();
            dragIndex = -1;
            dragMode = DragMode::Move;
            selectedNote = -1;
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& event) override
    {
        const int index = findNoteAt(event.position);
        if (index >= 0)
        {
            owner.removeNote(index);
            selectedNote = -1;
            repaint();
        }
    }

private:
    enum class DragMode { Move, Resize };

    int findNoteAt(juce::Point<float> pos) const
    {
        if (pos.y < timelineHeight)
            return -1;

        for (int i = static_cast<int>(owner.notes.size()) - 1; i >= 0; --i)
        {
            const auto& note = owner.notes[(size_t)i];
            const float noteX = owner.xFromBeat(note.startBeat);
            const float noteW = (float)(note.lengthBeats * owner.beatPixelWidth);
            const float noteY = (float)(timelineHeight + owner.rowFromMidiNote(note.midiNote) * rowHeight);
            juce::Rectangle<float> rect(noteX, noteY, noteW, (float)rowHeight);
            if (rect.contains(pos))
                return i;
        }
        return -1;
    }

    MidiRollComponent& owner;
    DragMode dragMode { DragMode::Move };
    int dragIndex = -1;
    double dragGrabOffset = 0.0;
    MidiRollComponent::Note dragOriginalNote;
    int selectedNote = -1;
};

//==============================================================================
MidiRollComponent::MidiRollComponent()
{
    editor = std::make_unique<Editor>(*this);
    viewport.setViewedComponent(editor.get(), false);
    viewport.setScrollBarsShown(true, true);
    addAndMakeVisible(viewport);

    bpmSlider.setSliderStyle(juce::Slider::LinearBar);
    bpmSlider.setRange(30.0, 300.0, 0.1);
    bpmSlider.setValue(bpm);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 20);
    bpmSlider.onValueChange = [this]
    {
        setBpm(bpmSlider.getValue());
    };
    addAndMakeVisible(bpmSlider);

    loopLengthSlider.setSliderStyle(juce::Slider::LinearBar);
    loopLengthSlider.setRange(1, 64, 1);
    loopLengthSlider.setValue(loopLengthBeats);
    loopLengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 20);
    loopLengthSlider.onValueChange = [this]
    {
        setLoopLengthInBeats((int)std::round(loopLengthSlider.getValue()));
    };
    addAndMakeVisible(loopLengthSlider);

    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
    bpmLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bpmLabel);

    loopLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
    loopLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(loopLabel);

    playButton.onClick = [this] { play(); };
    stopButton.onClick = [this] { stop(); };
    restartButton.onClick = [this] { restart(); };
    importButton.onClick = [this] { importFromFile(); };
    exportButton.onClick = [this] { exportToFile(); };

    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(restartButton);
    addAndMakeVisible(importButton);
    addAndMakeVisible(exportButton);

    followButton.setToggleState(followEnabled, juce::dontSendNotification);
    followButton.onClick = [this]
    {
        followEnabled = followButton.getToggleState();
    };
    addAndMakeVisible(followButton);

    updateContentSize();
}

MidiRollComponent::~MidiRollComponent()
{
    stop();
}

void MidiRollComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0f0f0f));
    g.setColour(juce::Colour(0xff1d1d1d));
    g.drawRect(getLocalBounds());
}

void MidiRollComponent::resized()
{
    auto bounds = getLocalBounds().reduced(8, 6);

    auto controlRow = bounds.removeFromTop(32);

    auto bpmArea = controlRow.removeFromLeft(180);
    bpmLabel.setBounds(bpmArea.removeFromLeft(44));
    bpmSlider.setBounds(bpmArea);

    controlRow.removeFromLeft(8);
    auto loopArea = controlRow.removeFromLeft(200);
    loopLabel.setBounds(loopArea.removeFromLeft(48));
    loopLengthSlider.setBounds(loopArea);

    controlRow.removeFromLeft(8);
    auto buttonWidth = 76;
    playButton.setBounds(controlRow.removeFromLeft(buttonWidth));
    controlRow.removeFromLeft(4);
    stopButton.setBounds(controlRow.removeFromLeft(buttonWidth));
    controlRow.removeFromLeft(4);
    restartButton.setBounds(controlRow.removeFromLeft(buttonWidth));
    controlRow.removeFromLeft(6);
    followButton.setBounds(controlRow.removeFromLeft(96));
    controlRow.removeFromLeft(8);
    importButton.setBounds(controlRow.removeFromLeft(120));
    controlRow.removeFromLeft(4);
    exportButton.setBounds(controlRow.removeFromLeft(120));

    bounds.removeFromTop(4);
    viewport.setBounds(bounds);
    updateContentSize();
}

void MidiRollComponent::setNoteOnCallback(std::function<void(int, float)> cb)
{
    noteOnCallback = std::move(cb);
}

void MidiRollComponent::setNoteOffCallback(std::function<void(int)> cb)
{
    noteOffCallback = std::move(cb);
}

void MidiRollComponent::setBpm(double newBpm)
{
    bpm = juce::jlimit(20.0, 400.0, newBpm);
    bpmSlider.setValue(bpm, juce::dontSendNotification);
}

void MidiRollComponent::setLoopLengthInBeats(int newLength)
{
    loopLengthBeats = juce::jlimit(1, 128, newLength);
    loopLengthSlider.setValue(loopLengthBeats, juce::dontSendNotification);

    const double maxBeat = (double)loopLengthBeats;
    for (auto& note : notes)
    {
        if (note.startBeat >= maxBeat)
            note.startBeat = std::fmod(note.startBeat, maxBeat);

        if (note.startBeat + note.lengthBeats > maxBeat)
            note.lengthBeats = juce::jmax(minNoteLength, maxBeat - note.startBeat);
    }

    sortNotes();
    if (loopLengthBeats > 0)
    {
        const double loop = (double)loopLengthBeats;
        absoluteBeatPosition = std::fmod(absoluteBeatPosition, loop);
        if (absoluteBeatPosition < 0.0)
            absoluteBeatPosition += loop;
        playheadBeat = std::fmod(playheadBeat, loop);
        if (playheadBeat < 0.0)
            playheadBeat += loop;
        lastAbsoluteBeatPosition = absoluteBeatPosition;
    }
    else
    {
        absoluteBeatPosition = 0.0;
        playheadBeat = 0.0;
        lastAbsoluteBeatPosition = 0.0;
    }
    updateContentSize();
}

void MidiRollComponent::play()
{
    if (!playing)
    {
        playing = true;
        lastTimerSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        ensureTimerRunning();
    }
}

void MidiRollComponent::stop()
{
    if (playing)
    {
        playing = false;
    }
    stopTimer();
    sendAllNotesOff();
    lastTimerSeconds = 0.0;
    editor->repaint();
}

void MidiRollComponent::restart()
{
    sendAllNotesOff();
    absoluteBeatPosition = 0.0;
    lastAbsoluteBeatPosition = 0.0;
    playheadBeat = 0.0;
    lastTimerSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (playing)
        ensureTimerRunning();
    editor->repaint();
}

void MidiRollComponent::seekToBeat(double beatPosition)
{
    sendAllNotesOff();

    const double loop = (double)juce::jmax(1, loopLengthBeats);
    beatPosition = juce::jlimit(0.0, loop, beatPosition);

    const double loopCount = loop > 0.0 ? std::floor(absoluteBeatPosition / loop) : 0.0;
    absoluteBeatPosition = loopCount * loop + beatPosition;
    lastAbsoluteBeatPosition = absoluteBeatPosition;
    playheadBeat = beatPosition;
    editor->repaint();
}

void MidiRollComponent::importFromFile()
{
    juce::FileChooser chooser("Import MIDI", juce::File(), "*.mid;*.midi");
    if (!chooser.browseForFileToOpen())
        return;

    juce::File file = chooser.getResult();
    juce::FileInputStream stream(file);
    if (!stream.openedOk())
        return;

    juce::MidiFile midi;
    if (!midi.readFrom(stream))
        return;

    const double ticksPerBeat = midi.getTimeFormat() > 0 ? (double)midi.getTimeFormat() : ticksPerQuarterNote;

    // Attempt to grab tempo from first track if present
    for (int trackIndex = 0; trackIndex < midi.getNumTracks(); ++trackIndex)
    {
        const auto* track = midi.getTrack(trackIndex);
        if (track == nullptr)
            continue;

        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& msg = track->getEventPointer(i)->message;
            if (msg.isTempoMetaEvent())
            {
                const double secondsPerQuarter = msg.getTempoSecondsPerQuarterNote();
                if (secondsPerQuarter > 0.0)
                {
                    setBpm(60.0 / secondsPerQuarter);
                }
                break;
            }
        }
    }

    sendAllNotesOff();
    notes.clear();

    const auto processTrack = [this, ticksPerBeat](const juce::MidiMessageSequence& track)
    {
        for (int i = 0; i < track.getNumEvents(); ++i)
        {
            const auto* holder = track.getEventPointer(i);
            if (holder == nullptr)
                continue;

            const auto& msg = holder->message;
            if (msg.isNoteOn())
            {
                const double startTick = msg.getTimeStamp();
                const double endTick = track.getTimeOfMatchingNoteOff(i);
                if (endTick <= startTick)
                    continue;

                Note note;
                note.midiNote = msg.getNoteNumber();
                note.startBeat = (startTick / ticksPerBeat);
                note.lengthBeats = juce::jmax(minNoteLength, (endTick - startTick) / ticksPerBeat);
                note.velocity = msg.getFloatVelocity();

                notes.push_back(note);
            }
        }
    };

    for (int t = 0; t < midi.getNumTracks(); ++t)
    {
        if (const auto* track = midi.getTrack(t))
            processTrack(*track);
    }

    if (!notes.empty())
    {
        double maxBeat = 0.0;
        for (const auto& note : notes)
            maxBeat = std::max(maxBeat, note.startBeat + note.lengthBeats);

        setLoopLengthInBeats((int)std::ceil(maxBeat + gridSubdivision));
    }
    else
    {
        setLoopLengthInBeats(loopLengthBeats);
    }

    sortNotes();
    updateContentSize();
    restart();
    editor->repaint();
}

void MidiRollComponent::exportToFile() const
{
    juce::FileChooser chooser("Export MIDI", juce::File(), "*.mid;*.midi");
    if (!chooser.browseForFileToSave(true))
        return;

    juce::File file = chooser.getResult();
    if (!file.hasFileExtension("mid") && !file.hasFileExtension("midi"))
        file = file.withFileExtension("mid");
    juce::MidiFile midi;
    midi.setTicksPerQuarterNote((int)ticksPerQuarterNote);

    juce::MidiMessageSequence sequence;
    const double secondsPerQuarter = 60.0 / bpm;
    sequence.addEvent(juce::MidiMessage::tempoMetaEvent(secondsPerQuarter), 0.0);

    for (const auto& note : notes)
    {
        const double startTick = note.startBeat * ticksPerQuarterNote;
        const double endTick = (note.startBeat + note.lengthBeats) * ticksPerQuarterNote;

        auto on = juce::MidiMessage::noteOn(1, note.midiNote, note.velocity);
        auto off = juce::MidiMessage::noteOff(1, note.midiNote);
        on.setTimeStamp(startTick);
        off.setTimeStamp(endTick);

        sequence.addEvent(on);
        sequence.addEvent(off);
    }

    sequence.updateMatchedPairs();
    sequence.sort();
    midi.addTrack(sequence);

    juce::FileOutputStream out(file);
    if (out.openedOk())
        midi.writeTo(out);
}

void MidiRollComponent::timerCallback()
{
    if (!playing)
        return;

    const double nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (lastTimerSeconds <= 0.0)
        lastTimerSeconds = nowSeconds;

    const double deltaSeconds = nowSeconds - lastTimerSeconds;
    lastTimerSeconds = nowSeconds;

    const double deltaBeats = (bpm / 60.0) * deltaSeconds;
    lastAbsoluteBeatPosition = absoluteBeatPosition;
    absoluteBeatPosition += deltaBeats;

    const double loop = (double)juce::jmax(1, loopLengthBeats);
    playheadBeat = std::fmod(absoluteBeatPosition, loop);
    if (playheadBeat < 0.0)
        playheadBeat += loop;

    const double prev = lastAbsoluteBeatPosition;
    const double current = absoluteBeatPosition;

    // Handle note offs
    for (int i = static_cast<int>(activeNotes.size()) - 1; i >= 0; --i)
    {
        auto active = activeNotes[(size_t)i];
        if (active.endBeatAbsolute <= current)
        {
            if (noteOffCallback && active.index >= 0 && active.index < (int)notes.size())
                noteOffCallback(notes[(size_t)active.index].midiNote);
            activeNotes.erase(activeNotes.begin() + i);
        }
    }

    // Handle note ons
    for (int i = 0; i < (int)notes.size(); ++i)
    {
        const auto& note = notes[(size_t)i];
        const double startBeat = note.startBeat;

        double firstOccurrence = startBeat;
        if (loop > 0.0 && prev > startBeat)
        {
            const double steps = std::floor((prev - startBeat) / loop);
            firstOccurrence = startBeat + (steps + 1.0) * loop;
        }

        for (double occurrence = firstOccurrence; occurrence <= current + 1.0e-9; occurrence += loop)
        {
            if (occurrence < prev - 1.0e-9)
                continue;

            if (noteOnCallback)
                noteOnCallback(note.midiNote, note.velocity);

            ActiveNote active;
            active.index = i;
            active.endBeatAbsolute = occurrence + juce::jmax(minNoteLength, note.lengthBeats);
            activeNotes.push_back(active);

            if (loop <= 0.0)
                break;
        }
    }

    if (followEnabled)
    {
        const int viewWidth = viewport.getViewWidth();
        const int viewHeight = viewport.getViewHeight();
        if (viewWidth > 0 && viewHeight > 0)
        {
            const int totalWidth = editor->getWidth();
            const double desiredX = xFromBeat(playheadBeat) - viewWidth * 0.25;
            const int clampedX = juce::jlimit(0, std::max(0, totalWidth - viewWidth), (int)std::round(desiredX));
            viewport.setViewPosition(clampedX, viewport.getViewPositionY());
        }
    }

    editor->repaint();
}

void MidiRollComponent::ensureTimerRunning()
{
    if (!isTimerRunning())
        startTimerHz(timerFrequencyHz);
}

void MidiRollComponent::sendAllNotesOff()
{
    if (noteOffCallback)
    {
        for (const auto& active : activeNotes)
        {
            if (active.index >= 0 && active.index < (int)notes.size())
                noteOffCallback(notes[(size_t)active.index].midiNote);
        }
    }
    activeNotes.clear();
}

void MidiRollComponent::updateContentSize()
{
    const int numRows = highestMidiNote - lowestMidiNote + 1;
    const int contentWidth = (int)std::round(labelWidth + loopLengthBeats * beatPixelWidth);
    const int contentHeight = timelineHeight + numRows * rowHeight;
    editor->setSize(contentWidth, contentHeight);
    editor->repaint();
}

void MidiRollComponent::sortNotes()
{
    std::sort(notes.begin(), notes.end(), [](const Note& a, const Note& b)
    {
        if (std::abs(a.startBeat - b.startBeat) > 1.0e-6)
            return a.startBeat < b.startBeat;
        if (a.midiNote != b.midiNote)
            return a.midiNote > b.midiNote;
        return a.lengthBeats > b.lengthBeats;
    });

    sendAllNotesOff();
}

double MidiRollComponent::snapBeat(double beat) const noexcept
{
    const double grid = gridSubdivision;
    return grid * std::floor((beat / grid) + 0.5);
}

double MidiRollComponent::beatFromX(float x) const noexcept
{
    return (x - labelWidth) / beatPixelWidth;
}

float MidiRollComponent::xFromBeat(double beat) const noexcept
{
    return (float)(labelWidth + beat * beatPixelWidth);
}

int MidiRollComponent::midiNoteFromY(float y) const noexcept
{
    const float relative = juce::jlimit(0.0f, (float)((highestMidiNote - lowestMidiNote + 1) * rowHeight - 1), y - timelineHeight);
    const int row = juce::jlimit(0, highestMidiNote - lowestMidiNote, (int)(relative / rowHeight));
    return highestMidiNote - row;
}

int MidiRollComponent::rowFromMidiNote(int midiNote) const noexcept
{
    midiNote = juce::jlimit(lowestMidiNote, highestMidiNote, midiNote);
    return highestMidiNote - midiNote;
}

float MidiRollComponent::yFromMidiNote(int midiNote) const noexcept
{
    return (float)(timelineHeight + rowFromMidiNote(midiNote) * rowHeight);
}

void MidiRollComponent::addNote(const Note& note)
{
    notes.push_back(note);
    sortNotes();
    editor->repaint();
}

void MidiRollComponent::removeNote(int noteIndex)
{
    if (noteIndex < 0 || noteIndex >= (int)notes.size())
        return;

    const int midiNote = notes[(size_t)noteIndex].midiNote;
    notes.erase(notes.begin() + noteIndex);

    for (int i = (int)activeNotes.size() - 1; i >= 0; --i)
    {
        if (activeNotes[(size_t)i].index == noteIndex)
        {
            if (noteOffCallback)
                noteOffCallback(midiNote);
            activeNotes.erase(activeNotes.begin() + i);
            continue;
        }

        if (activeNotes[(size_t)i].index > noteIndex)
            activeNotes[(size_t)i].index -= 1;
    }

    editor->repaint();
}

