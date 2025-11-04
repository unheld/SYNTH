#include "MidiRollComponent.h"

#include <algorithm>
#include <cmath>

namespace
{
    // Simple grid quantisation: 1/16-note by default
    constexpr int kQuantizeDivision = 16; // 16 -> sixteenth notes

    void quantizeNote(MidiRollComponent::Note& n, double maxBeats)
    {
        if (kQuantizeDivision <= 0 || maxBeats <= 0.0)
            return;

        // 4 beats per bar in 4/4; grid spacing in beats
        const double beatGrid = 4.0 / static_cast<double>(kQuantizeDivision);

        auto snapToGrid = [beatGrid](double v)
        {
            return std::round(v / beatGrid) * beatGrid;
        };

        n.startBeat   = snapToGrid(n.startBeat);
        n.lengthBeats = std::max(beatGrid, snapToGrid(n.lengthBeats));

        // Keep note inside loop range
        if (n.startBeat < 0.0)
            n.startBeat = 0.0;

        if (n.startBeat > maxBeats - beatGrid)
            n.startBeat = std::max(0.0, maxBeats - beatGrid);

        if (n.startBeat + n.lengthBeats > maxBeats)
            n.lengthBeats = std::max(beatGrid, maxBeats - n.startBeat);
    }
}

//==============================================================================

MidiRollComponent::MidiRollComponent()
{
    setOpaque(true);
    startTimerHz(60); // refresh at ~60fps for playhead

    updateLoopLengthFromNotes();
}

//==============================================================================

void MidiRollComponent::clearNotes()
{
    {
        const juce::SpinLock::ScopedLockType lock(noteMutex);
        notes.clear();
    }

    updateLoopLengthFromNotes();
    flushActiveNotes.store(true);
    repaint();
}

//==============================================================================
// Coordinate helpers

int MidiRollComponent::pitchToY(int midiNote) const
{
    midiNote = juce::jlimit(kMinNote, kMaxNote, midiNote);
    const int indexFromTop = kMaxNote - midiNote;
    const double worldY = static_cast<double>(kTopMargin)
                        + static_cast<double>(indexFromTop) * static_cast<double>(kNoteHeight);
    return static_cast<int>(std::round(worldY - scrollY));
}

int MidiRollComponent::yToPitch(int y) const
{
    const double worldY = juce::jlimit(0.0, getContentHeight() - 1.0,
                                       static_cast<double>(y) + scrollY);
    const double normalised = (worldY - static_cast<double>(kTopMargin))
                            / static_cast<double>(kNoteHeight);
    int row = static_cast<int>(std::floor(normalised));
    row = juce::jlimit(0, kMaxNote - kMinNote, row);
    return kMaxNote - row;
}

double MidiRollComponent::xToBeat(int x) const
{
    const double pixelsPerBeat = getPixelsPerBeat();
    if (pixelsPerBeat <= 0.0)
        return 0.0;

    const double worldX = static_cast<double>(x) - static_cast<double>(kLeftMargin);
    const double beat = worldX / pixelsPerBeat;
    return juce::jlimit(0.0, kMaxLoopBeats, beat);
}

int MidiRollComponent::beatToX(double beat) const
{
    const double pixelsPerBeat = getPixelsPerBeat();
    const double worldX = juce::jlimit(0.0, kMaxLoopBeats, beat) * pixelsPerBeat;
    return static_cast<int>(std::round(worldX)) + kLeftMargin;
}

double MidiRollComponent::getLoopLengthBeats() const noexcept
{
    const double beats = loopLengthBeats.load();
    return juce::jlimit(kMinLoopBeats, kMaxLoopBeats, beats);
}

double MidiRollComponent::getPixelsPerBeat() const noexcept
{
    const int availableWidth = std::max(1, getWidth() - kLeftMargin);
    return static_cast<double>(availableWidth) / getLoopLengthBeats();
}

double MidiRollComponent::getContentHeight() const noexcept
{
    const int rows = (kMaxNote - kMinNote + 1);
    return static_cast<double>(kTopMargin)
         + static_cast<double>(rows) * static_cast<double>(kNoteHeight)
         + static_cast<double>(kTopMargin);
}

void MidiRollComponent::clampVerticalScroll()
{
    const double maxScroll = std::max(0.0, getContentHeight() - static_cast<double>(getHeight()));
    scrollY = juce::jlimit(0.0, maxScroll, scrollY);
}

void MidiRollComponent::setLoopLengthBeats(double beats)
{
    beats = juce::jlimit(kMinLoopBeats, kMaxLoopBeats, beats);
    const double current = loopLengthBeats.load();

    if (std::abs(current - beats) < 1.0e-6)
        return;

    loopLengthBeats.store(beats);

    double playhead = playheadBeat.load();
    if (playhead >= beats)
    {
        playhead = std::fmod(playhead, beats);
        if (playhead < 0.0)
            playhead += beats;
        playheadBeat.store(playhead);
    }

    repaint();
}

void MidiRollComponent::updateLoopLengthFromNotes()
{
    double maxBeat = 0.0;
    {
        const juce::SpinLock::ScopedLockType lock(noteMutex);
        for (const auto& note : notes)
        {
            const double length = std::max(0.0, note.lengthBeats);
            maxBeat = std::max(maxBeat, note.startBeat + length);
        }
    }

    const double bars = std::ceil(maxBeat / 4.0);
    const double newLength = std::max(kMinLoopBeats,
                                      bars > 0.0 ? bars * 4.0 : kMinLoopBeats);
    setLoopLengthBeats(newLength);
}

int MidiRollComponent::hitTestNoteUnlocked(int x, int y) const
{
    for (int i = static_cast<int>(notes.size()) - 1; i >= 0; --i)
    {
        const auto& n = notes[static_cast<size_t>(i)];
        const int noteY     = pitchToY(n.midiNote);
        const int noteH     = kNoteHeight - 2;
        const int noteX     = beatToX(n.startBeat);
        const int noteWidth = static_cast<int>(std::round(n.lengthBeats * getPixelsPerBeat()));

        if (juce::Rectangle<int>(noteX, noteY, noteWidth, noteH).contains(x, y))
            return i;
    }
    return -1;
}

int MidiRollComponent::hitTestNote(int x, int y) const
{
    const juce::SpinLock::ScopedLockType lock(noteMutex);
    return hitTestNoteUnlocked(x, y);
}

//==============================================================================
// Painting

void MidiRollComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(12, 30, 35));

    const auto bounds = getLocalBounds();
    const int  height = bounds.getHeight();
    const double pixelsPerBeat = getPixelsPerBeat();
    const double totalBeats    = getLoopLengthBeats();

    std::vector<Note> noteSnapshot;
    {
        const juce::SpinLock::ScopedLockType lock(noteMutex);
        noteSnapshot = notes;
    }

    // Piano-key strip
    juce::Rectangle<int> keyStrip(0, 0, kLeftMargin, height);
    g.setColour(juce::Colour::fromRGB(10, 25, 28));
    g.fillRect(keyStrip);

    // Grid area
    juce::Rectangle<int> grid = bounds.withTrimmedLeft(kLeftMargin);

    // Horizontal note rows
    for (int note = kMinNote; note <= kMaxNote; ++note)
    {
        const int y = pitchToY(note);
        if (y >= height || y + kNoteHeight <= 0)
            continue;

        const bool isC     = (note % 12) == 0;
        const bool isBlack = juce::MidiMessage::isMidiNoteBlack(note);

        juce::Colour rowColour =
            isBlack ? juce::Colour::fromRGB(18, 32, 35)
                    : juce::Colour::fromRGB(20, 45, 50);

        if (isC)
            rowColour = rowColour.brighter(0.2f);

        g.setColour(rowColour);
        g.fillRect(juce::Rectangle<int>(grid.getX(), y,
                                        grid.getWidth(), kNoteHeight));

        if (isC)
        {
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.drawText(juce::MidiMessage::getMidiNoteName(note, true, true, 4),
                       juce::Rectangle<int>(0, y, keyStrip.getWidth() - 2, kNoteHeight),
                       juce::Justification::centredRight, false);
        }
    }

    // Vertical grid (beats)
    const int maxBeatLines = static_cast<int>(std::ceil(totalBeats)) + 1;
    for (int beat = 0; beat <= maxBeatLines; ++beat)
    {
        const int lineX = beatToX(static_cast<double>(beat));
        if (lineX < grid.getX() || lineX > grid.getRight())
            continue;

        const bool isBar = (beat % 4) == 0;
        g.setColour(isBar
                    ? juce::Colours::white.withAlpha(0.18f)
                    : juce::Colours::white.withAlpha(0.09f));
        g.drawVerticalLine(lineX, static_cast<float>(grid.getY()),
                           static_cast<float>(grid.getBottom()));
    }

    // Notes
    for (size_t i = 0; i < noteSnapshot.size(); ++i)
    {
        const auto& n = noteSnapshot[i];
        const int noteY = pitchToY(n.midiNote) + 1;
        const int noteH = kNoteHeight - 3;
        const int noteX = beatToX(n.startBeat);
        const int noteW = static_cast<int>(std::max(8.0, std::round(n.lengthBeats * pixelsPerBeat)));

        juce::Rectangle<int> r(noteX, noteY, noteW, noteH);
        const bool isSelected = static_cast<int>(i) == draggingNoteIndex;

        juce::Colour body = juce::Colour::fromRGB(120, 210, 230);
        if (isSelected)
            body = body.brighter(0.35f);

        g.setColour(body.withAlpha(0.9f));
        g.fillRoundedRectangle(r.toFloat(), 3.0f);

        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawRoundedRectangle(r.toFloat(), 3.0f, 1.0f);
    }

    // Playhead
    if (isPlaying.load())
    {
        const int playX = beatToX(playheadBeat.load());
        g.setColour(juce::Colours::yellow.withAlpha(0.8f));
        g.drawLine(static_cast<float>(playX), 0.0f,
                   static_cast<float>(playX), static_cast<float>(height), 2.0f);
    }
}

void MidiRollComponent::resized()
{
    clampVerticalScroll();
}

//==============================================================================
// Playback control

void MidiRollComponent::startPlayback()
{
    if (!isCurrentlyPlaying())
    {
        playheadBeat.store(0.0);
        flushActiveNotes.store(true);
        isPlaying.store(true);
    }
}

void MidiRollComponent::stopPlayback()
{
    isPlaying.store(false);
    playheadBeat.store(0.0);
    flushActiveNotes.store(true);
    repaint();
}

void MidiRollComponent::togglePlayback()
{
    if (isCurrentlyPlaying())
        stopPlayback();
    else
        startPlayback();
}

//==============================================================================
void MidiRollComponent::renderNextMidiBlock(juce::MidiBuffer& buffer,
                                            int numSamples,
                                            double sampleRate)
{
    if (numSamples <= 0 || sampleRate <= 0.0)
        return;

    if (flushActiveNotes.exchange(false))
    {
        for (int midiNote : activeNotes)
            buffer.addEvent(juce::MidiMessage::noteOff(1, midiNote), 0);

        activeNotes.clear();
    }

    if (!isCurrentlyPlaying())
        return;

    const double beatsPerSecond = bpm / 60.0;
    const double beatsPerSample = beatsPerSecond / sampleRate;
    const double blockBeats     = beatsPerSample * static_cast<double>(numSamples);

    if (blockBeats <= 0.0)
        return;

    double startBeat = playheadBeat.load();

    std::vector<Note> noteSnapshot;
    {
        const juce::SpinLock::ScopedLockType lock(noteMutex);
        noteSnapshot = notes;
    }

    const double totalBeats = getLoopLengthBeats();
    if (totalBeats <= 0.0)
        return;

    auto normaliseBeat = [totalBeats](double beat)
    {
        double b = std::fmod(beat, totalBeats);
        if (b < 0.0)
            b += totalBeats;
        return b;
    };

    startBeat = normaliseBeat(startBeat);

    auto addEventIfInBlock = [&](double rawBeat, bool isNoteOn, int midiNote)
    {
        double beat = normaliseBeat(rawBeat);

        double deltaBeats = beat - startBeat;
        while (deltaBeats < 0.0)
            deltaBeats += totalBeats;

        if (deltaBeats < 0.0 || deltaBeats >= blockBeats)
            return;

        int sample = static_cast<int>(std::round(deltaBeats / beatsPerSample));
        sample = juce::jlimit(0, std::max(0, numSamples - 1), sample);

        if (isNoteOn)
        {
            buffer.addEvent(juce::MidiMessage::noteOn(1, midiNote, static_cast<juce::uint8>(100)), sample);
            if (std::find(activeNotes.begin(), activeNotes.end(), midiNote) == activeNotes.end())
                activeNotes.push_back(midiNote);
        }
        else
        {
            buffer.addEvent(juce::MidiMessage::noteOff(1, midiNote), sample);
            activeNotes.erase(std::remove(activeNotes.begin(), activeNotes.end(), midiNote), activeNotes.end());
        }
    };

    for (const auto& note : noteSnapshot)
    {
        const double noteLength = std::max(0.0, note.lengthBeats);
        addEventIfInBlock(note.startBeat, true, note.midiNote);
        addEventIfInBlock(note.startBeat + noteLength, false, note.midiNote);
    }

    double newBeat = startBeat + blockBeats;
    newBeat = std::fmod(newBeat, totalBeats);
    if (newBeat < 0.0)
        newBeat += totalBeats;

    playheadBeat.store(newBeat);
}

//==============================================================================
// Timer: updates playhead

void MidiRollComponent::timerCallback()
{
    if (isCurrentlyPlaying())
        repaint();
}

//==============================================================================
// Mouse handling

void MidiRollComponent::mouseDown(const juce::MouseEvent& e)
{
    const int x = e.getPosition().x;
    const int y = e.getPosition().y;

    draggingNoteIndex = hitTestNote(x, y);
    resizingNote = false;
    dragOffsetBeat = 0.0;
    dragStartBeat = xToBeat(x);
    bool shouldUpdateLoop = false;

    if (draggingNoteIndex >= 0)
    {
        const juce::SpinLock::ScopedLockType lock(noteMutex);

        if (draggingNoteIndex >= 0 && draggingNoteIndex < static_cast<int>(notes.size()))
        {
            auto& n = notes[static_cast<size_t>(draggingNoteIndex)];
            if (e.mods.isRightButtonDown())
            {
                notes.erase(notes.begin() + draggingNoteIndex);
                draggingNoteIndex = -1;
                flushActiveNotes.store(true);
                shouldUpdateLoop = true;
            }
            else
            {
                const int noteX = beatToX(n.startBeat);
                const int noteWidth = static_cast<int>(std::round(n.lengthBeats * getPixelsPerBeat()));
                const bool nearRightEdge = (x > noteX + noteWidth - 6);
                resizingNote = nearRightEdge;

                if (!resizingNote)
                    dragOffsetBeat = xToBeat(x) - n.startBeat;
            }
        }
    }
    else
    {
        Note n;
        n.midiNote    = yToPitch(y);
        n.startBeat   = juce::jlimit(0.0, kMaxLoopBeats - 0.25, xToBeat(x));
        n.lengthBeats = 1.0;

        quantizeNote(n, getLoopLengthBeats());

        {
            const juce::SpinLock::ScopedLockType lock(noteMutex);
            notes.push_back(n);
            draggingNoteIndex = static_cast<int>(notes.size()) - 1;
        }
        resizingNote = true;
        shouldUpdateLoop = true;
    }

    if (shouldUpdateLoop)
        updateLoopLengthFromNotes();

    repaint();
}

void MidiRollComponent::mouseDrag(const juce::MouseEvent& e)
{
    bool changed = false;

    {
        const juce::SpinLock::ScopedLockType lock(noteMutex);

        if (draggingNoteIndex < 0 || draggingNoteIndex >= static_cast<int>(notes.size()))
            return;

        auto& n = notes[static_cast<size_t>(draggingNoteIndex)];
        const auto p = e.getPosition();

        if (resizingNote)
        {
            const double endBeat = juce::jlimit(n.startBeat + 0.1, kMaxLoopBeats, xToBeat(p.x));
            n.lengthBeats = endBeat - n.startBeat;
        }
        else
        {
            const double newStart = juce::jlimit(0.0, kMaxLoopBeats - n.lengthBeats, xToBeat(p.x) - dragOffsetBeat);
            n.startBeat = newStart;
            n.midiNote  = yToPitch(p.y);
        }

        quantizeNote(n, getLoopLengthBeats());

        changed = true;
    }

    if (changed)
    {
        updateLoopLengthFromNotes();
        repaint();
    }
}

void MidiRollComponent::mouseUp(const juce::MouseEvent&)
{
    draggingNoteIndex = -1;
    resizingNote = false;
}

void MidiRollComponent::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (std::abs(wheel.deltaY) < std::abs(wheel.deltaX))
        return;

    const double maxScroll = std::max(0.0, getContentHeight() - static_cast<double>(getHeight()));
    if (maxScroll <= 0.0)
        return;

    const double delta = -wheel.deltaY * 80.0;
    scrollY = juce::jlimit(0.0, maxScroll, scrollY + delta);
    repaint();
}
