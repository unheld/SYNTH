#include "MidiRollComponent.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double kQuantiseStepBeats = 0.25; // Sixteenth notes
    constexpr int kMinimumLoopBeats = 1;
    constexpr int kMaximumLoopBeats = 256;
    constexpr double kEpsilon = 1.0e-6;
    constexpr int kTimerIntervalMs = 30;
    constexpr float kEdgeResizePixels = 8.0f;
}

//==============================================================================
class MidiRollComponent::GridComponent : public juce::Component
{
public:
    explicit GridComponent(MidiRollComponent& ownerRef)
        : owner(ownerRef)
    {
        setOpaque(false);
        setInterceptsMouseClicks(true, true);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff151515));

        auto totalRows = owner.getTotalNoteRows();
        auto rowHeight = owner.noteHeight;
        auto keyboardWidth = owner.keyboardWidth;

        for (int row = 0; row < totalRows; ++row)
        {
            const int midiNote = owner.rowToMidiNote(row);
            const bool isBlack = juce::MidiMessage::isMidiNoteBlack(midiNote);
            const int y = row * rowHeight;

            juce::Rectangle<int> laneBounds(0, y, getWidth(), rowHeight);

            g.setColour(isBlack ? juce::Colour(0xff1f1f1f) : juce::Colour(0xff222222));
            g.fillRect(laneBounds.withTrimmedRight(laneBounds.getWidth() - keyboardWidth));

            g.setColour(isBlack ? juce::Colour(0xff181818) : juce::Colour(0xff141414));
            g.fillRect(laneBounds.withTrimmedLeft(keyboardWidth));

            g.setColour(juce::Colour(0xff0d0d0d));
            g.drawLine(0.0f, (float)y, (float)getWidth(), (float)y);

            g.setColour(juce::Colours::white.withAlpha(isBlack ? 0.6f : 0.85f));
            auto labelBounds = laneBounds.withWidth(keyboardWidth).reduced(6, 0);
            g.drawFittedText(juce::MidiMessage::getMidiNoteName(midiNote, true, true, 4),
                             labelBounds, juce::Justification::centredLeft, 1);
        }

        const int beatsToDisplay = owner.loopLengthBeats;
        const int subdivisions = 4; // quarter-beat lines
        const int verticalHeight = getHeight();
        for (int i = 0; i <= beatsToDisplay * subdivisions; ++i)
        {
            const double beatValue = (double)i / (double)subdivisions;
            const int x = (int)std::round(owner.beatToPixel(beatValue));
            const bool strong = (i % subdivisions) == 0;
            g.setColour(strong ? juce::Colour(0xff2e2e2e) : juce::Colour(0xff232323));
            g.drawLine((float)x, 0.0f, (float)x, (float)verticalHeight);
        }

        for (const auto& note : owner.notes)
        {
            auto rect = noteToRect(note);
            juce::Colour fill = juce::Colour(0xff4aa9ff);
            juce::Colour border = juce::Colour(0xff12324f);

            g.setColour(fill);
            g.fillRoundedRectangle(rect, 4.0f);
            g.setColour(border);
            g.drawRoundedRectangle(rect, 4.0f, 1.2f);

            g.setColour(juce::Colours::white);
            auto labelBounds = rect.toNearestInt().reduced(4, 2);
            g.drawFittedText(juce::MidiMessage::getMidiNoteName(note.midiNote, true, true, 4),
                             labelBounds, juce::Justification::centredLeft, 1);
        }

        const int playheadX = (int)std::round(owner.beatToPixel(playheadBeat));
        g.setColour(juce::Colour(0xffff6d2d));
        g.drawLine((float)playheadX, 0.0f, (float)playheadX, (float)getHeight(), 2.0f);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragMode = DragMode::None;
        dragNoteId = 0;

        const int midiNote = positionToMidiNote((float)event.position.y);
        const double beat = owner.quantiseBeat(positionToBeat((float)event.position.x));

        if (event.mods.isRightButtonDown())
        {
            if (auto* note = owner.findNoteAtPosition(midiNote, beat))
            {
                owner.removeNoteById(note->id);
                owner.notifyNotesChanged();
            }
            return;
        }

        if (auto* note = owner.findNoteAtPosition(midiNote, beat))
        {
            dragNoteId = note->id;
            dragInitialStart = note->startBeat;
            dragInitialLength = note->lengthBeats;
            dragStartBeat = beat;

            const double leftEdgeBeat = note->startBeat;
            const double rightEdgeBeat = note->startBeat + note->lengthBeats;
            const double pixelToBeat = kEdgeResizePixels / owner.pixelsPerBeat;

            if (std::abs(beat - leftEdgeBeat) <= pixelToBeat)
                dragMode = DragMode::ResizingStart;
            else if (std::abs(beat - rightEdgeBeat) <= pixelToBeat)
                dragMode = DragMode::ResizingEnd;
            else
                dragMode = DragMode::Moving;
        }
        else
        {
            auto& note = owner.createNote(midiNote, beat, owner.minimumNoteLength, 0.8f);
            dragNoteId = note.id;
            dragInitialStart = note.startBeat;
            dragInitialLength = note.lengthBeats;
            dragStartBeat = beat;
            dragMode = DragMode::Creating;
            owner.notifyNotesChanged();
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (dragMode == DragMode::None)
            return;

        auto* note = owner.findNoteById(dragNoteId);
        if (note == nullptr)
            return;

        const double beat = owner.quantiseBeat(positionToBeat((float)event.position.x));
        const int midiNote = positionToMidiNote((float)event.position.y);

        switch (dragMode)
        {
            case DragMode::Moving:
            {
                const double beatDelta = beat - dragStartBeat;
                note->startBeat = dragInitialStart + beatDelta;
                note->midiNote = juce::jlimit(owner.lowestMidiNote, owner.highestMidiNote, midiNote);
                owner.enforceLoopBounds(*note);
                break;
            }
            case DragMode::ResizingStart:
            {
                const double maxStart = dragInitialStart + dragInitialLength - owner.minimumNoteLength;
                note->startBeat = juce::jlimit(0.0, maxStart, beat);
                note->lengthBeats = dragInitialLength + (dragInitialStart - note->startBeat);
                owner.enforceLoopBounds(*note);
                break;
            }
            case DragMode::ResizingEnd:
            {
                const double maxLength = owner.loopLengthBeats - dragInitialStart;
                const double newLength = juce::jlimit(owner.minimumNoteLength, maxLength, beat - dragInitialStart);
                note->lengthBeats = newLength;
                owner.enforceLoopBounds(*note);
                break;
            }
            case DragMode::Creating:
            {
                double start = std::min(dragStartBeat, beat);
                double end = std::max(dragStartBeat, beat);
                if (end - start < owner.minimumNoteLength)
                    end = start + owner.minimumNoteLength;
                note->startBeat = start;
                note->lengthBeats = end - start;
                note->midiNote = juce::jlimit(owner.lowestMidiNote, owner.highestMidiNote, midiNote);
                owner.enforceLoopBounds(*note);
                break;
            }
            case DragMode::None:
                break;
        }

        owner.notifyNotesChanged();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragMode != DragMode::None)
            owner.notifyNotesChanged();

        dragMode = DragMode::None;
        dragNoteId = 0;
    }

    void mouseDoubleClick(const juce::MouseEvent& event) override
    {
        const int midiNote = positionToMidiNote((float)event.position.y);
        const double beat = owner.quantiseBeat(positionToBeat((float)event.position.x));
        if (auto* note = owner.findNoteAtPosition(midiNote, beat))
        {
            owner.removeNoteById(note->id);
            owner.notifyNotesChanged();
        }
    }

    void setPlayheadBeat(double beat)
    {
        playheadBeat = beat;
        repaint();
    }

private:
    enum class DragMode
    {
        None,
        Creating,
        Moving,
        ResizingStart,
        ResizingEnd
    };

    MidiRollComponent& owner;
    DragMode dragMode = DragMode::None;
    int dragNoteId = 0;
    double dragStartBeat = 0.0;
    double dragInitialStart = 0.0;
    double dragInitialLength = 0.0;
    double playheadBeat = 0.0;

    double positionToBeat(float x) const
    {
        const float local = juce::jmax(0.0f, x - (float)owner.keyboardWidth);
        return local / owner.pixelsPerBeat;
    }

    int positionToMidiNote(float y) const
    {
        const int row = juce::jlimit(0, owner.getTotalNoteRows() - 1,
                                     (int)std::floor(y / (float)owner.noteHeight));
        return owner.rowToMidiNote(row);
    }

    juce::Rectangle<float> noteToRect(const MidiRollComponent::Note& note) const
    {
        const int row = owner.midiNoteToRow(note.midiNote);
        const float x = (float)owner.beatToPixel(note.startBeat);
        const float width = (float)std::max(owner.minimumNoteLength * owner.pixelsPerBeat,
                                            note.lengthBeats * owner.pixelsPerBeat);
        const float y = (float)row * (float)owner.noteHeight;
        const float height = (float)owner.noteHeight;
        return { x + 1.0f, y + 1.5f, width - 2.0f, height - 3.0f };
    }
};

//==============================================================================
MidiRollComponent::MidiRollComponent()
{
    setOpaque(true);

    gridComponent = std::make_unique<GridComponent>(*this);
    viewport.setViewedComponent(gridComponent.get(), false);
    viewport.setScrollBarsShown(true, true);
    viewport.setScrollBarThickness(12);
    addAndMakeVisible(viewport);

    bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
    bpmLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bpmLabel);

    bpmSlider.setSliderStyle(juce::Slider::LinearBar);
    bpmSlider.setRange(20.0, 300.0, 0.1);
    bpmSlider.setValue(bpm);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 20);
    bpmSlider.onValueChange = [this]
    {
        setBpm(bpmSlider.getValue());
    };
    addAndMakeVisible(bpmSlider);

    loopLabel.setText("Loop", juce::dontSendNotification);
    loopLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
    loopLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(loopLabel);

    loopLengthSlider.setSliderStyle(juce::Slider::LinearBar);
    loopLengthSlider.setRange(kMinimumLoopBeats, kMaximumLoopBeats, 1.0);
    loopLengthSlider.setValue(loopLengthBeats);
    loopLengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 20);
    loopLengthSlider.onValueChange = [this]
    {
        setLoopLengthInBeats((int)std::round(loopLengthSlider.getValue()));
    };
    addAndMakeVisible(loopLengthSlider);

    playButton.setButtonText("Play");
    playButton.onClick = [this] { play(); };
    addAndMakeVisible(playButton);

    stopButton.setButtonText("Stop");
    stopButton.onClick = [this] { stop(); };
    addAndMakeVisible(stopButton);

    restartButton.setButtonText("Restart");
    restartButton.onClick = [this] { restart(); };
    addAndMakeVisible(restartButton);

    followButton.setButtonText("Follow");
    followButton.setToggleState(followEnabled, juce::dontSendNotification);
    followButton.onClick = [this]
    {
        followEnabled = followButton.getToggleState();
        if (followEnabled)
            ensurePlayheadVisible();
    };
    addAndMakeVisible(followButton);

    importButton.setButtonText("Import MIDI");
    importButton.onClick = [this] { importFromFile(); };
    addAndMakeVisible(importButton);

    exportButton.setButtonText("Export MIDI");
    exportButton.onClick = [this] { exportToFile(); };
    addAndMakeVisible(exportButton);

    updateContentSize();
    refreshTransportButtons();
}

MidiRollComponent::~MidiRollComponent()
{
    stop();
}

void MidiRollComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101010));
    g.setColour(juce::Colour(0xff1d1d1d));
    g.drawRect(getLocalBounds());
}

void MidiRollComponent::resized()
{
    auto bounds = getLocalBounds().reduced(8, 6);
    auto controlRow = bounds.removeFromTop(34);

    auto bpmArea = controlRow.removeFromLeft(180);
    bpmLabel.setBounds(bpmArea.removeFromLeft(44));
    bpmSlider.setBounds(bpmArea);

    controlRow.removeFromLeft(8);
    auto loopArea = controlRow.removeFromLeft(200);
    loopLabel.setBounds(loopArea.removeFromLeft(48));
    loopLengthSlider.setBounds(loopArea);

    controlRow.removeFromLeft(8);
    const int buttonWidth = 80;
    playButton.setBounds(controlRow.removeFromLeft(buttonWidth));
    controlRow.removeFromLeft(4);
    stopButton.setBounds(controlRow.removeFromLeft(buttonWidth));
    controlRow.removeFromLeft(4);
    restartButton.setBounds(controlRow.removeFromLeft(buttonWidth));

    controlRow.removeFromLeft(8);
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
    loopLengthBeats = juce::jlimit(kMinimumLoopBeats, kMaximumLoopBeats, newLength);
    loopLengthSlider.setValue(loopLengthBeats, juce::dontSendNotification);
    updateContentSize();
    resetPlaybackState();
}

void MidiRollComponent::play()
{
    if (playing)
        return;

    playing = true;
    startTimer(kTimerIntervalMs);
    refreshTransportButtons();
}

void MidiRollComponent::stop()
{
    if (!playing)
    {
        sendAllActiveNotesOff();
        refreshTransportButtons();
        return;
    }

    stopTimer();
    playing = false;
    sendAllActiveNotesOff();
    refreshTransportButtons();
    gridComponent->setPlayheadBeat(playheadBeat);
}

void MidiRollComponent::restart()
{
    resetPlaybackState();
}

void MidiRollComponent::importFromFile()
{
    juce::FileChooser chooser("Import MIDI", juce::File(), "*.mid;*.midi");
    if (!chooser.browseForFileToOpen())
        return;

    juce::File file = chooser.getResult();
    juce::FileInputStream input(file);
    if (!input.openedOk())
        return;

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(input))
        return;

    midiFile.convertTimestampTicksToSeconds();

    notes.clear();
    nextNoteId = 1;

    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        if (auto* track = midiFile.getTrack(t))
        {
            track->updateMatchedPairs();

            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                auto* event = track->getEventPointer(i);
                if (event == nullptr)
                    continue;

                if (!event->message.isNoteOn())
                    continue;

                auto* off = event->noteOffObject;
                if (off == nullptr)
                    continue;

                const double startSeconds = event->message.getTimeStamp();
                const double endSeconds = off->message.getTimeStamp();
                if (endSeconds <= startSeconds + kEpsilon)
                    continue;

                const double startBeat = (startSeconds * bpm) / 60.0;
                const double lengthBeat = juce::jmax(minimumNoteLength, (endSeconds - startSeconds) * bpm / 60.0);

                Note& note = createNote(event->message.getNoteNumber(), startBeat, lengthBeat,
                                        event->message.getVelocity());
                enforceLoopBounds(note);
            }
        }
    }

    if (!notes.empty())
    {
        double furthest = 0.0;
        for (const auto& note : notes)
            furthest = std::max(furthest, note.startBeat + note.lengthBeats);
        setLoopLengthInBeats((int)std::ceil(furthest));
    }

    notifyNotesChanged();
}

void MidiRollComponent::exportToFile()
{
    juce::FileChooser chooser("Export MIDI", juce::File(), "*.mid");
    if (!chooser.browseForFileToSave(true))
        return;

    juce::File file = chooser.getResult();

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(960);

    juce::MidiMessageSequence sequence;
    for (const auto& note : notes)
    {
        const double startSeconds = (note.startBeat * 60.0) / bpm;
        const double endSeconds = ((note.startBeat + note.lengthBeats) * 60.0) / bpm;
        sequence.addEvent(juce::MidiMessage::noteOn(1, note.midiNote, note.velocity), startSeconds);
        sequence.addEvent(juce::MidiMessage::noteOff(1, note.midiNote), endSeconds);
    }

    sequence.sort();
    midiFile.addTrack(sequence);

    juce::FileOutputStream output(file);
    if (!output.openedOk())
        return;

    midiFile.writeTo(output);
}

void MidiRollComponent::timerCallback()
{
    if (!playing)
        return;

    previousAbsoluteBeat = absoluteBeat;
    const double beatsPerSecond = bpm / 60.0;
    const double deltaBeats = beatsPerSecond * (kTimerIntervalMs / 1000.0);
    absoluteBeat += deltaBeats;

    handlePlaybackAdvance(previousAbsoluteBeat, absoluteBeat);

    if (loopLengthBeats > 0)
        playheadBeat = std::fmod(absoluteBeat, (double)loopLengthBeats);
    else
        playheadBeat = 0.0;

    gridComponent->setPlayheadBeat(playheadBeat);
    ensurePlayheadVisible();
}

MidiRollComponent::Note& MidiRollComponent::createNote(int midiNote, double startBeat, double lengthBeats, float velocity)
{
    Note note;
    note.id = nextNoteId++;
    note.midiNote = juce::jlimit(lowestMidiNote, highestMidiNote, midiNote);
    note.startBeat = juce::jmax(0.0, startBeat);
    note.lengthBeats = std::max(minimumNoteLength, lengthBeats);
    note.velocity = juce::jlimit(0.0f, 1.0f, velocity);
    enforceLoopBounds(note);
    notes.push_back(note);
    return notes.back();
}

void MidiRollComponent::removeNoteById(int noteId)
{
    stopNotePlayback(noteId);
    notes.erase(std::remove_if(notes.begin(), notes.end(), [noteId](const Note& n) { return n.id == noteId; }), notes.end());
}

MidiRollComponent::Note* MidiRollComponent::findNoteById(int noteId)
{
    auto it = std::find_if(notes.begin(), notes.end(), [noteId](const Note& n) { return n.id == noteId; });
    return it == notes.end() ? nullptr : &(*it);
}

MidiRollComponent::Note* MidiRollComponent::findNoteAtPosition(int midiNote, double beat)
{
    for (auto& note : notes)
    {
        if (note.midiNote != midiNote)
            continue;

        if (beat + kEpsilon >= note.startBeat && beat < note.startBeat + note.lengthBeats + kEpsilon)
            return &note;
    }
    return nullptr;
}

void MidiRollComponent::notifyNotesChanged()
{
    sortNotesByPosition();
    updateContentSize();
    gridComponent->repaint();
}

void MidiRollComponent::sortNotesByPosition()
{
    std::sort(notes.begin(), notes.end(), [](const Note& a, const Note& b)
    {
        if (std::abs(a.startBeat - b.startBeat) > kEpsilon)
            return a.startBeat < b.startBeat;
        if (a.midiNote != b.midiNote)
            return a.midiNote > b.midiNote;
        return a.id < b.id;
    });
}

void MidiRollComponent::enforceLoopBounds(Note& note)
{
    note.startBeat = juce::jlimit(0.0, (double)loopLengthBeats - minimumNoteLength, note.startBeat);
    note.lengthBeats = juce::jlimit(minimumNoteLength, (double)loopLengthBeats - note.startBeat, note.lengthBeats);
}

double MidiRollComponent::quantiseBeat(double beat) const
{
    const double steps = std::round(beat / kQuantiseStepBeats);
    return juce::jlimit(0.0, (double)loopLengthBeats, steps * kQuantiseStepBeats);
}

int MidiRollComponent::rowToMidiNote(int row) const noexcept
{
    return juce::jlimit(lowestMidiNote, highestMidiNote, highestMidiNote - row);
}

int MidiRollComponent::midiNoteToRow(int midiNote) const noexcept
{
    return juce::jlimit(0, getTotalNoteRows() - 1, highestMidiNote - midiNote);
}

int MidiRollComponent::getTotalNoteRows() const noexcept
{
    return highestMidiNote - lowestMidiNote + 1;
}

double MidiRollComponent::beatToPixel(double beat) const noexcept
{
    return keyboardWidth + beat * pixelsPerBeat;
}

void MidiRollComponent::handlePlaybackAdvance(double previousAbs, double newAbs)
{
    if (notes.empty())
        return;

    const int prevCycle = loopLengthBeats > 0 ? (int)std::floor(previousAbs / loopLengthBeats) : 0;
    const int currentCycle = loopLengthBeats > 0 ? (int)std::floor(newAbs / loopLengthBeats) : 0;

    for (auto& note : notes)
    {
        for (int cycle = prevCycle; cycle <= currentCycle; ++cycle)
        {
            if (cycle < 0)
                continue;

            const double noteStart = note.startBeat + cycle * loopLengthBeats;
            const double noteEnd = noteStart + note.lengthBeats;

            if (noteStart >= previousAbs - kEpsilon && noteStart < newAbs + kEpsilon)
                startNotePlayback(note, noteEnd);

            if (noteEnd >= previousAbs - kEpsilon && noteEnd < newAbs + kEpsilon)
                stopNotePlayback(note.id);
        }
    }
}

void MidiRollComponent::startNotePlayback(const Note& note, double absoluteEndBeat)
{
    if (noteOnCallback)
        noteOnCallback(note.midiNote, note.velocity);

    activeNotes.push_back({ note.id, absoluteEndBeat });
}

void MidiRollComponent::stopNotePlayback(int noteId)
{
    auto it = std::find_if(activeNotes.begin(), activeNotes.end(), [noteId](const ActiveNote& n) { return n.id == noteId; });
    if (it == activeNotes.end())
        return;

    if (auto* note = findNoteById(noteId))
    {
        if (noteOffCallback)
            noteOffCallback(note->midiNote);
    }

    activeNotes.erase(it);
}

void MidiRollComponent::sendAllActiveNotesOff()
{
    if (!noteOffCallback)
    {
        activeNotes.clear();
        return;
    }

    for (const auto& active : activeNotes)
    {
        if (auto* note = findNoteById(active.id))
            noteOffCallback(note->midiNote);
    }
    activeNotes.clear();
}

void MidiRollComponent::updateContentSize()
{
    if (gridComponent == nullptr)
        return;

    const int contentWidth = keyboardWidth + (int)std::ceil(loopLengthBeats * pixelsPerBeat);
    const int contentHeight = getTotalNoteRows() * noteHeight;
    gridComponent->setSize(contentWidth, contentHeight);
    viewport.setViewPosition(juce::jlimit(0, std::max(0, contentWidth - viewport.getWidth()), viewport.getViewPositionX()),
                             juce::jlimit(0, std::max(0, contentHeight - viewport.getHeight()), viewport.getViewPositionY()));
}

void MidiRollComponent::ensurePlayheadVisible()
{
    if (!followEnabled)
        return;

    const int playheadX = (int)std::round(beatToPixel(playheadBeat));
    const int viewX = viewport.getViewPositionX();
    const int viewWidth = viewport.getViewWidth();

    const int margin = viewWidth / 6;
    int targetX = viewX;

    if (playheadX < viewX + margin)
        targetX = juce::jlimit(0, gridComponent->getWidth() - viewWidth, playheadX - margin);
    else if (playheadX > viewX + viewWidth - margin)
        targetX = juce::jlimit(0, gridComponent->getWidth() - viewWidth, playheadX - viewWidth + margin);

    if (targetX != viewX)
        viewport.setViewPosition(targetX, viewport.getViewPositionY());
}

void MidiRollComponent::refreshTransportButtons()
{
    playButton.setEnabled(!playing);
    stopButton.setEnabled(playing);
}

void MidiRollComponent::resetPlaybackState()
{
    previousAbsoluteBeat = 0.0;
    absoluteBeat = 0.0;
    playheadBeat = 0.0;
    sendAllActiveNotesOff();
    gridComponent->setPlayheadBeat(playheadBeat);
    ensurePlayheadVisible();
}
