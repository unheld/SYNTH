#include "MidiRollComponent.h"

//==============================================================================

MidiRollComponent::MidiRollComponent()
{
    setOpaque (true);
    startTimerHz(60); // refresh at ~60fps for playhead
}

//==============================================================================

void MidiRollComponent::clearNotes()
{
    notes.clear();
    repaint();
}

//==============================================================================
// Coordinate helpers

int MidiRollComponent::pitchToY (int midiNote) const
{
    midiNote = juce::jlimit (kMinNote, kMaxNote, midiNote);
    const int indexFromTop = kMaxNote - midiNote;
    return kTopMargin + indexFromTop * kNoteHeight;
}

int MidiRollComponent::yToPitch (int y) const
{
    const int row = juce::jlimit (0, kMaxNote - kMinNote,
                                  (y - kTopMargin) / kNoteHeight);
    return kMaxNote - row;
}

double MidiRollComponent::xToBeat (int x) const
{
    const double worldX = x - kLeftMargin + scrollX;
    return juce::jlimit (0.0, kTotalLengthBeats,
                         worldX / (double) kPixelsPerBeat);
}

int MidiRollComponent::beatToX (double beat) const
{
    const double worldX = beat * (double) kPixelsPerBeat;
    return (int) std::round (worldX - scrollX) + kLeftMargin;
}

int MidiRollComponent::hitTestNote (int x, int y) const
{
    for (int i = (int) notes.size() - 1; i >= 0; --i)
    {
        const auto& n = notes[(size_t) i];
        const int noteY      = pitchToY (n.midiNote);
        const int noteH      = kNoteHeight - 2;
        const int noteX      = beatToX (n.startBeat);
        const int noteWidth  = (int) std::round (n.lengthBeats * kPixelsPerBeat);

        if (juce::Rectangle<int> (noteX, noteY, noteWidth, noteH).contains (x, y))
            return i;
    }
    return -1;
}

//==============================================================================
// Painting

void MidiRollComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (12, 30, 35));

    const auto bounds = getLocalBounds();
    const int  height = bounds.getHeight();

    // Piano-key strip
    juce::Rectangle<int> keyStrip (0, 0, kLeftMargin, height);
    g.setColour (juce::Colour::fromRGB (10, 25, 28));
    g.fillRect (keyStrip);

    // Grid area
    juce::Rectangle<int> grid = bounds.withTrimmedLeft (kLeftMargin);

    // Horizontal note rows
    for (int note = kMinNote; note <= kMaxNote; ++note)
    {
        const int y = pitchToY (note);
        const bool isC = (note % 12) == 0;
        const bool isBlack = juce::MidiMessage::isMidiNoteBlack (note);

        juce::Colour rowColour =
            isBlack ? juce::Colour::fromRGB (18, 32, 35)
                    : juce::Colour::fromRGB (20, 45, 50);

        if (isC)
            rowColour = rowColour.brighter (0.2f);

        g.setColour (rowColour);
        g.fillRect (juce::Rectangle<int> (grid.getX(), y,
                                          grid.getWidth(), kNoteHeight));

        if (isC)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawText (juce::MidiMessage::getMidiNoteName(note, true, true, 4),
                        juce::Rectangle<int> (0, y, keyStrip.getWidth() - 2, kNoteHeight),
                        juce::Justification::centredRight, false);
        }
    }

    // Vertical grid (beats)
    const int maxBeatLines = (int) std::ceil (kTotalLengthBeats) + 1;
    for (int beat = 0; beat <= maxBeatLines; ++beat)
    {
        const int lineX = beatToX ((double) beat);
        if (lineX < grid.getX() || lineX > grid.getRight()) continue;

        const bool isBar = (beat % 4) == 0;
        g.setColour (isBar
                     ? juce::Colours::white.withAlpha (0.18f)
                     : juce::Colours::white.withAlpha (0.09f));
        g.drawVerticalLine (lineX, (float) grid.getY(), (float) grid.getBottom());
    }

    // Notes
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto& n = notes[i];
        const int noteY = pitchToY (n.midiNote) + 1;
        const int noteH = kNoteHeight - 3;
        const int noteX = beatToX (n.startBeat);
        const int noteW = (int) std::max (8.0, std::round (n.lengthBeats * kPixelsPerBeat));

        juce::Rectangle<int> r (noteX, noteY, noteW, noteH);
        const bool isSelected = (int) i == draggingNoteIndex;

        juce::Colour body = juce::Colour::fromRGB (120, 210, 230);
        if (isSelected) body = body.brighter (0.35f);

        g.setColour (body.withAlpha (0.9f));
        g.fillRoundedRectangle (r.toFloat(), 3.0f);

        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (r.toFloat(), 3.0f, 1.0f);
    }

    // Playhead
    if (isPlaying)
    {
        const int playX = beatToX (playheadBeat);
        g.setColour (juce::Colours::yellow.withAlpha (0.8f));
        g.drawLine ((float) playX, 0.0f, (float) playX, (float) height, 2.0f);
    }
}

void MidiRollComponent::resized()
{
}

//==============================================================================
// Playback control

void MidiRollComponent::startPlayback()
{
    if (!isPlaying)
    {
        isPlaying = true;
        playheadBeat = 0.0;
        lastUpdateTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    }
}

void MidiRollComponent::stopPlayback()
{
    isPlaying = false;
    playheadBeat = 0.0;
    repaint();
}

void MidiRollComponent::togglePlayback()
{
    if (isPlaying) stopPlayback();
    else startPlayback();
}

//==============================================================================
// Timer: updates playhead

void MidiRollComponent::timerCallback()
{
    if (isPlaying)
    {
        double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        double dt = now - lastUpdateTime;
        lastUpdateTime = now;

        double beatsPerSecond = bpm / 60.0;
        playheadBeat += beatsPerSecond * dt;

        if (playheadBeat >= kTotalLengthBeats)
            playheadBeat = 0.0; // loop

        repaint();
    }
}

//==============================================================================
// Mouse handling

void MidiRollComponent::mouseDown (const juce::MouseEvent& e)
{
    const int x = e.getPosition().x;
    const int y = e.getPosition().y;

    draggingNoteIndex = hitTestNote (x, y);
    resizingNote = false;
    dragOffsetBeat = 0.0;
    dragStartBeat = xToBeat (x);

    if (draggingNoteIndex >= 0)
    {
        auto& n = notes[(size_t) draggingNoteIndex];
        if (e.mods.isRightButtonDown())
        {
            notes.erase (notes.begin() + draggingNoteIndex);
            draggingNoteIndex = -1;
            repaint();
            return;
        }

        const int noteX = beatToX (n.startBeat);
        const int noteWidth = (int) std::round (n.lengthBeats * kPixelsPerBeat);
        const bool nearRightEdge = (x > noteX + noteWidth - 6);
        resizingNote = nearRightEdge;

        if (!resizingNote)
            dragOffsetBeat = xToBeat (x) - n.startBeat;
    }
    else
    {
        Note n;
        n.midiNote = yToPitch (y);
        n.startBeat = juce::jlimit (0.0, kTotalLengthBeats - 0.25, xToBeat (x));
        n.lengthBeats = 1.0;
        notes.push_back (n);
        draggingNoteIndex = (int) notes.size() - 1;
        resizingNote = true;
    }

    repaint();
}

void MidiRollComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingNoteIndex < 0 || draggingNoteIndex >= (int) notes.size()) return;
    auto& n = notes[(size_t) draggingNoteIndex];
    const auto p = e.getPosition();

    if (resizingNote)
    {
        const double endBeat = juce::jlimit (n.startBeat + 0.1, kTotalLengthBeats, xToBeat (p.x));
        n.lengthBeats = endBeat - n.startBeat;
    }
    else
    {
        const double newStart = juce::jlimit (0.0, kTotalLengthBeats - n.lengthBeats, xToBeat (p.x) - dragOffsetBeat);
        n.startBeat = newStart;
        n.midiNote = yToPitch (p.y);
    }

    repaint();
}

void MidiRollComponent::mouseUp (const juce::MouseEvent&)
{
    draggingNoteIndex = -1;
    resizingNote = false;
}

void MidiRollComponent::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const double totalPixels = kTotalLengthBeats * (double) kPixelsPerBeat;
    const double viewWidth = (double) getWidth();
    const double maxScroll = std::max (0.0, totalPixels - viewWidth);
    const double delta = -wheel.deltaY * 80.0;
    scrollX = juce::jlimit (0.0, maxScroll, scrollX + delta);
    repaint();
}
