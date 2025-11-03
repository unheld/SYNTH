#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
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

    MidiRollComponent()
    {
        setOpaque(true);

        gridComponent = std::make_unique<GridComponent>(*this);
        viewport.setViewedComponent(gridComponent.get(), false);
        viewport.setScrollBarsShown(true, true);
        viewport.setScrollBarThickness(12);
        addAndMakeVisible(viewport);

        bpmSlider.setSliderStyle(juce::Slider::LinearBar);
        bpmSlider.setRange(20.0, 300.0, 0.1);
        bpmSlider.setValue(bpm);
        bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 20);
        bpmSlider.onValueChange = [this]
        {
            setBpm(bpmSlider.getValue());
        };
        addAndMakeVisible(bpmSlider);

        loopLengthSlider.setSliderStyle(juce::Slider::LinearBar);
        loopLengthSlider.setRange(1, 128, 1);
        loopLengthSlider.setValue(loopLengthBeats);
        loopLengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 20);
        loopLengthSlider.onValueChange = [this]
        {
            setLoopLengthInBeats((int)std::round(loopLengthSlider.getValue()));
        };
        addAndMakeVisible(loopLengthSlider);

        bpmLabel.setText("BPM", juce::dontSendNotification);
        bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
        bpmLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(bpmLabel);

        loopLabel.setText("Loop", juce::dontSendNotification);
        loopLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
        loopLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(loopLabel);

        playButton.setTooltip("Start playback");
        playButton.onClick = [this] { play(); };
        addAndMakeVisible(playButton);

        stopButton.setTooltip("Stop playback");
        stopButton.onClick = [this] { stop(); };
        addAndMakeVisible(stopButton);

        restartButton.setTooltip("Restart from bar 1");
        restartButton.onClick = [this] { restart(); };
        addAndMakeVisible(restartButton);

        importButton.setTooltip("Import MIDI file");
        importButton.onClick = [this] { importFromFile(); };
        addAndMakeVisible(importButton);

        exportButton.setTooltip("Export MIDI file");
        exportButton.onClick = [this] { exportToFile(); };
        addAndMakeVisible(exportButton);

        followButton.setTooltip("Scroll the view with the playhead");
        followButton.setToggleState(followEnabled, juce::dontSendNotification);
        followButton.onClick = [this]
        {
            followEnabled = followButton.getToggleState();
        };
        addAndMakeVisible(followButton);

        updateContentSize();
    }

    ~MidiRollComponent() override
    {
        stop();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff101010));
        g.setColour(juce::Colour(0xff1d1d1d));
        g.drawRect(getLocalBounds());
    }

    void resized() override
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
        const int buttonWidth = 76;
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

    void setNoteOnCallback(std::function<void(int, float)> cb)
    {
        noteOnCallback = std::move(cb);
    }

    void setNoteOffCallback(std::function<void(int)> cb)
    {
        noteOffCallback = std::move(cb);
    }

    double getBpm() const noexcept { return bpm; }

    void setBpm(double newBpm)
    {
        bpm = juce::jlimit(20.0, 400.0, newBpm);
        bpmSlider.setValue(bpm, juce::dontSendNotification);
    }

    int getLoopLengthInBeats() const noexcept { return loopLengthBeats; }

    void setLoopLengthInBeats(int newLength)
    {
        newLength = juce::jlimit(1, 256, newLength);
        if (loopLengthBeats == newLength)
            return;

        sendAllNotesOff();
        loopLengthBeats = newLength;
        loopLengthSlider.setValue(loopLengthBeats, juce::dontSendNotification);
        clampAllNotesToLoop();

        const double loop = (double)loopLengthBeats;
        absoluteBeatPosition = std::fmod(absoluteBeatPosition, loop);
        if (absoluteBeatPosition < 0.0)
            absoluteBeatPosition += loop;
        lastAbsoluteBeatPosition = absoluteBeatPosition;
        playheadBeat = absoluteBeatPosition;
        updateContentSize();
        repaintRoll();
        ensureTimerRunning();
    }

    void play()
    {
        if (!playing)
        {
            playing = true;
            lastTimerSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
            ensureTimerRunning();
        }
    }

    void stop()
    {
        if (playing)
            playing = false;

        stopTimer();
        sendAllNotesOff();
        repaintRoll();
    }

    void restart()
    {
        sendAllNotesOff();
        absoluteBeatPosition = 0.0;
        lastAbsoluteBeatPosition = 0.0;
        playheadBeat = 0.0;
        lastTimerSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        repaintRoll();
    }

    void seekToBeat(double beatPosition)
    {
        const double loop = (double)juce::jmax(1, loopLengthBeats);
        beatPosition = juce::jlimit(0.0, loop, beatPosition);

        if (playing)
        {
            const double loopIndex = std::floor(absoluteBeatPosition / loop);
            absoluteBeatPosition = loopIndex * loop + beatPosition;
            lastAbsoluteBeatPosition = absoluteBeatPosition;
        }
        else
        {
            absoluteBeatPosition = beatPosition;
            lastAbsoluteBeatPosition = beatPosition;
        }

        playheadBeat = beatPosition;
        lastTimerSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        repaintRoll();
        if (followEnabled)
            scrollToPlayhead();
    }

    void importFromFile()
    {
        juce::FileChooser chooser("Import MIDI", {}, "*.mid;*.midi");
        if (!chooser.browseForFileToOpen())
            return;

        juce::File file = chooser.getResult();
        juce::FileInputStream input(file);
        if (!input.openedOk())
            return;

        juce::MidiFile midi;
        if (!midi.readFrom(input))
            return;

        if (midi.getNumTracks() == 0)
            return;

        const int timeFormat = midi.getTimeFormat();
        if (timeFormat <= 0)
            return;

        juce::MidiMessageSequence sequence;
        for (int track = 0; track < midi.getNumTracks(); ++track)
            sequence.addSequence(*midi.getTrack(track), 0.0, 0.0, 0);

        sequence.updateMatchedPairs();

        stop();
        notes.clear();
        activeNotes.clear();
        nextNoteId = 1;

        const double ppq = (double)timeFormat;
        double maxBeat = 0.0;

        for (int i = 0; i < sequence.getNumEvents(); ++i)
        {
            const auto* holder = sequence.getEventPointer(i);
            const auto& message = holder->message;
            if (message.isNoteOn())
            {
                const auto* off = holder->noteOffObject;
                if (off != nullptr)
                {
                    Note note;
                    note.midiNote = message.getNoteNumber();
                    note.startBeat = message.getTimeStamp() / ppq;
                    const double endBeat = off->message.getTimeStamp() / ppq;
                    note.lengthBeats = juce::jmax(gridSubdivision, endBeat - note.startBeat);
                    note.velocity = message.getFloatVelocity();
                    addNoteInternal(note);
                    maxBeat = std::max(maxBeat, note.startBeat + note.lengthBeats);
                }
            }
        }

        if (maxBeat > 0.0)
        {
            int suggested = (int)std::ceil(maxBeat);
            setLoopLengthInBeats(juce::jlimit(1, 256, suggested));
        }

        seekToBeat(0.0);
        repaintRoll();
    }

    void exportToFile() const
    {
        if (notes.empty())
            return;

        juce::FileChooser chooser("Export MIDI", {}, "*.mid");
        if (!chooser.browseForFileToSave(true))
            return;

        juce::File file = chooser.getResult();
        if (file.getFileExtension().isEmpty())
            file = file.withFileExtension(".mid");

        juce::MidiFile midi;
        midi.setTicksPerQuarterNote(defaultTicksPerQuarterNote);

        juce::MidiMessageSequence sequence;
        for (const auto& note : notes)
        {
            const double startTicks = note.startBeat * defaultTicksPerQuarterNote;
            const double lengthTicks = note.lengthBeats * defaultTicksPerQuarterNote;
            juce::MidiMessage onMessage = juce::MidiMessage::noteOn(1, note.midiNote, note.velocity);
            onMessage.setTimeStamp(startTicks);
            juce::MidiMessage offMessage = juce::MidiMessage::noteOff(1, note.midiNote);
            offMessage.setTimeStamp(startTicks + lengthTicks);
            sequence.addEvent(onMessage);
            sequence.addEvent(offMessage);
        }

        sequence.updateMatchedPairs();
        midi.addTrack(sequence);

        juce::FileOutputStream output(file);
        if (!output.openedOk())
            return;

        midi.writeTo(output);
    }

    const std::vector<Note>& getNotes() const noexcept { return notes; }

private:
    class GridComponent : public juce::Component
    {
    public:
        explicit GridComponent(MidiRollComponent& ownerRef)
            : owner(ownerRef)
        {
            setInterceptsMouseClicks(true, true);
        }

        void paint(juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds();
            auto timeline = bounds.removeFromTop(timelineHeight);

            g.setColour(juce::Colour(0xff181818));
            g.fillRect(timeline);

            g.setColour(juce::Colour(0xff121212));
            g.fillRect(juce::Rectangle<int>(0, timeline.getBottom(), labelWidth, bounds.getHeight()));

            const double loopLength = std::max(1, owner.loopLengthBeats);
            const int totalBeats = (int)std::ceil(loopLength);

            g.setColour(juce::Colours::white.withAlpha(0.35f));
            for (int beat = 0; beat <= totalBeats; ++beat)
            {
                const float x = owner.xFromBeat((double)beat);
                const float thickness = (beat % 4 == 0) ? 1.6f : 0.9f;
                g.drawLine(x, (float)timeline.getBottom(), x, (float)getHeight(), thickness);

                if (beat < totalBeats)
                {
                    const juce::String label = juce::String(beat + 1);
                    g.drawText(label,
                               (int)x + 4,
                               timeline.getY(),
                               (int)owner.beatPixelWidth - 8,
                               timeline.getHeight(),
                               juce::Justification::centredLeft,
                               false);
                }
            }

            const int rows = highestMidiNote - lowestMidiNote + 1;
            for (int row = 0; row < rows; ++row)
            {
                const int midiNote = highestMidiNote - row;
                const bool isBlack = juce::MidiMessage::isMidiNoteBlack(midiNote);
                const int y = timeline.getBottom() + row * rowHeight;
                juce::Rectangle<int> rowRect(labelWidth, y, getWidth() - labelWidth, rowHeight);

                g.setColour(isBlack ? juce::Colour(0xff1a1a1a) : juce::Colour(0xff202020));
                g.fillRect(rowRect);

                g.setColour(juce::Colour(0xff242424));
                g.drawLine((float)labelWidth,
                           (float)rowRect.getBottom(),
                           (float)getWidth(),
                           (float)rowRect.getBottom(),
                           0.6f);

                const juce::String noteName = juce::MidiMessage::getMidiNoteName(midiNote, true, true, 4);
                g.setColour(juce::Colours::white.withAlpha(0.75f));
                g.drawText(noteName,
                           0,
                           y,
                           labelWidth - 6,
                           rowHeight,
                           juce::Justification::centredRight,
                           false);
            }

            const double subdivisionsPerBeat = 1.0 / gridSubdivision;
            const int totalSubdivisions = (int)std::ceil(loopLength * subdivisionsPerBeat);
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            for (int i = 0; i <= totalSubdivisions; ++i)
            {
                const double beat = i * gridSubdivision;
                const float x = owner.xFromBeat(beat);
                g.drawLine(x,
                           (float)timeline.getBottom(),
                           x,
                           (float)getHeight(),
                           0.5f);
            }

            {
                juce::Graphics::ScopedSaveState clipper(g);
                g.reduceClipRegion(labelWidth, timeline.getHeight(), getWidth() - labelWidth, getHeight() - timeline.getHeight());

                for (const auto& note : owner.notes)
                    paintNote(g, note, note.id == owner.selectedNoteId);
            }

            g.setColour(juce::Colour(0xff81d4fa));
            const float playheadX = owner.xFromBeat(owner.playheadBeat);
            g.drawLine(playheadX,
                       (float)timeline.getY(),
                       playheadX,
                       (float)getHeight(),
                       1.6f);
        }

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (event.mods.isPopupMenu())
            {
                const int noteId = findNoteIdAt(event.position);
                if (noteId >= 0)
                {
                    owner.removeNoteById(noteId);
                    owner.selectedNoteId = -1;
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

            const int noteId = findNoteIdAt(event.position);
            if (noteId >= 0)
            {
                if (auto* note = owner.findNoteById(noteId))
                {
                    owner.selectedNoteId = noteId;
                    dragging = true;
                    const double snapped = owner.snapBeat(owner.beatFromX((float)event.x));
                    dragStartOffset = snapped - note->startBeat;
                    const float noteRight = owner.xFromBeat(note->startBeat + note->lengthBeats);
                    resizing = std::abs(event.position.x - noteRight) <= 8.0f || event.mods.isShiftDown();
                }
            }
            else
            {
                Note note;
                note.midiNote = owner.midiNoteFromY((float)event.y);
                note.startBeat = owner.snapBeat(owner.beatFromX((float)event.x));
                note.startBeat = juce::jlimit(0.0, (double)owner.loopLengthBeats - gridSubdivision, note.startBeat);
                note.lengthBeats = gridSubdivision;
                note.velocity = 0.85f;
                if (auto* added = owner.addNoteInternal(note))
                {
                    owner.selectedNoteId = added->id;
                    dragging = true;
                    resizing = true;
                    dragStartOffset = 0.0;
                }
            }

            repaint();
        }

        void mouseDrag(const juce::MouseEvent& event) override
        {
            if (!dragging)
                return;

            auto* note = owner.findNoteById(owner.selectedNoteId);
            if (note == nullptr)
                return;

            if (resizing)
            {
                double beat = owner.snapBeat(owner.beatFromX((float)event.x));
                beat = std::max(beat, note->startBeat + gridSubdivision);
                note->lengthBeats = beat - note->startBeat;
            }
            else
            {
                double beat = owner.snapBeat(owner.beatFromX((float)event.x)) - dragStartOffset;
                note->startBeat = beat;
                note->midiNote = owner.midiNoteFromY((float)event.y);
            }

            owner.clampNoteToLoop(*note);
            repaint();
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            if (dragging)
            {
                owner.sortNotes();
                repaint();
            }

            dragging = false;
            resizing = false;
        }

    private:
        void paintNote(juce::Graphics& g, const Note& note, bool selected)
        {
            const float x = owner.xFromBeat(note.startBeat);
            const float w = (float)std::max(gridSubdivision * owner.beatPixelWidth, note.lengthBeats * owner.beatPixelWidth);
            const float y = owner.yFromMidiNote(note.midiNote) + 1.0f;
            const float h = (float)rowHeight - 2.0f;

            juce::Rectangle<float> rect(x + 1.0f, y, w - 2.0f, h);
            g.setColour(selected ? juce::Colour(0xfff39c12) : juce::Colour(0xfff2784b));
            g.fillRoundedRectangle(rect, 3.0f);
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.drawRoundedRectangle(rect, 3.0f, 1.2f);
        }

        int findNoteIdAt(juce::Point<float> pos) const
        {
            for (auto it = owner.notes.rbegin(); it != owner.notes.rend(); ++it)
            {
                const float x = owner.xFromBeat(it->startBeat);
                const float w = (float)std::max(gridSubdivision * owner.beatPixelWidth, it->lengthBeats * owner.beatPixelWidth);
                const float y = owner.yFromMidiNote(it->midiNote) + 1.0f;
                const float h = (float)rowHeight - 2.0f;
                juce::Rectangle<float> rect(x + 1.0f, y, w - 2.0f, h);
                if (rect.contains(pos))
                    return it->id;
            }
            return -1;
        }

        MidiRollComponent& owner;
        bool dragging = false;
        bool resizing = false;
        double dragStartOffset = 0.0;
    };

    friend class GridComponent;

    void timerCallback() override
    {
        if (!playing || loopLengthBeats <= 0)
            return;

        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        if (lastTimerSeconds <= 0.0)
            lastTimerSeconds = now;

        const double deltaSeconds = now - lastTimerSeconds;
        lastTimerSeconds = now;

        if (deltaSeconds <= 0.0)
            return;

        const double startBeatAbs = absoluteBeatPosition;
        absoluteBeatPosition += deltaSeconds * (bpm / 60.0);

        processPlaybackWindow(startBeatAbs, absoluteBeatPosition);

        const double loop = (double)juce::jmax(1, loopLengthBeats);
        playheadBeat = std::fmod(absoluteBeatPosition, loop);
        if (playheadBeat < 0.0)
            playheadBeat += loop;

        lastAbsoluteBeatPosition = absoluteBeatPosition;

        if (followEnabled)
            scrollToPlayhead();

        repaintRoll();
    }

    void ensureTimerRunning()
    {
        if (playing && loopLengthBeats > 0)
        {
            if (!isTimerRunning())
                startTimerHz(timerFrequencyHz);
        }
        else
        {
            stopTimer();
        }
    }

    void sendAllNotesOff()
    {
        if (noteOffCallback)
        {
            for (const auto& active : activeNotes)
            {
                if (const auto* note = findNoteById(active.noteId))
                    noteOffCallback(note->midiNote);
            }
        }
        activeNotes.clear();
    }

    void updateContentSize()
    {
        if (gridComponent == nullptr)
            return;

        const int rows = highestMidiNote - lowestMidiNote + 1;
        const int height = timelineHeight + rows * rowHeight;
        const int width = std::max(labelWidth + (int)std::ceil(loopLengthBeats * beatPixelWidth), viewport.getWidth());
        gridComponent->setSize(width, height);
    }

    void repaintRoll()
    {
        if (gridComponent != nullptr)
            gridComponent->repaint();
    }

    void sortNotes()
    {
        std::sort(notes.begin(), notes.end(), [](const Note& a, const Note& b)
        {
            if (a.startBeat == b.startBeat)
                return a.midiNote < b.midiNote;
            return a.startBeat < b.startBeat;
        });
    }

    double snapBeat(double beat) const noexcept
    {
        const double step = gridSubdivision;
        return std::floor((beat / step) + 0.5) * step;
    }

    double beatFromX(float x) const noexcept
    {
        const float adjusted = x - (float)labelWidth;
        return adjusted <= 0.0f ? 0.0 : adjusted / beatPixelWidth;
    }

    float xFromBeat(double beat) const noexcept
    {
        return (float)labelWidth + (float)(beat * beatPixelWidth);
    }

    int midiNoteFromY(float y) const noexcept
    {
        int row = (int)std::floor((y - (float)timelineHeight) / (float)rowHeight);
        const int rows = highestMidiNote - lowestMidiNote + 1;
        row = juce::jlimit(0, rows - 1, row);
        return highestMidiNote - row;
    }

    int rowFromMidiNote(int midiNote) const noexcept
    {
        midiNote = juce::jlimit(lowestMidiNote, highestMidiNote, midiNote);
        return highestMidiNote - midiNote;
    }

    float yFromMidiNote(int midiNote) const noexcept
    {
        const int row = rowFromMidiNote(midiNote);
        return (float)timelineHeight + (float)row * (float)rowHeight;
    }

    Note* addNoteInternal(Note note)
    {
        note.id = nextNoteId++;
        clampNoteToLoop(note);
        notes.push_back(note);
        sortNotes();
        repaintRoll();
        return findNoteById(note.id);
    }

    void removeNoteById(int noteId)
    {
        const Note* existing = findNoteById(noteId);
        if (existing == nullptr)
            return;

        const Note removed = *existing;
        const bool wasActive = isNoteActive(noteId);

        notes.erase(std::remove_if(notes.begin(), notes.end(), [noteId](const Note& n)
        {
            return n.id == noteId;
        }), notes.end());

        activeNotes.erase(std::remove_if(activeNotes.begin(), activeNotes.end(), [noteId](const ActiveNote& active)
        {
            return active.noteId == noteId;
        }), activeNotes.end());

        if (wasActive && noteOffCallback)
            noteOffCallback(removed.midiNote);

        if (selectedNoteId == noteId)
            selectedNoteId = -1;

        repaintRoll();
    }

    Note* findNoteById(int noteId)
    {
        for (auto& note : notes)
            if (note.id == noteId)
                return &note;
        return nullptr;
    }

    const Note* findNoteById(int noteId) const
    {
        for (const auto& note : notes)
            if (note.id == noteId)
                return &note;
        return nullptr;
    }

    void clampNoteToLoop(Note& note) const
    {
        const double loop = (double)juce::jmax(1, loopLengthBeats);
        note.midiNote = juce::jlimit(lowestMidiNote, highestMidiNote, note.midiNote);
        note.startBeat = juce::jlimit(0.0, loop - gridSubdivision, note.startBeat);
        const double maxLength = std::max(gridSubdivision, loop - note.startBeat);
        note.lengthBeats = juce::jlimit(gridSubdivision, maxLength, note.lengthBeats);
    }

    void clampAllNotesToLoop()
    {
        for (auto& note : notes)
            clampNoteToLoop(note);
    }

    void processPlaybackWindow(double startBeatAbs, double endBeatAbs)
    {
        if (notes.empty())
            return;

        removeExpiredNotes(startBeatAbs);

        const double loop = (double)juce::jmax(1, loopLengthBeats);

        for (const auto& note : notes)
        {
            double noteStartAbs = note.startBeat;

            if (noteStartAbs < startBeatAbs - 1.0e-6)
            {
                const double loopsBehind = std::floor((startBeatAbs - noteStartAbs) / loop);
                noteStartAbs += loopsBehind * loop;
                while (noteStartAbs < startBeatAbs - 1.0e-6)
                    noteStartAbs += loop;
            }

            while (noteStartAbs <= endBeatAbs + 1.0e-6)
            {
                triggerNoteStart(note, noteStartAbs, endBeatAbs);
                noteStartAbs += loop;
            }
        }

        removeExpiredNotes(endBeatAbs);
    }

    void triggerNoteStart(const Note& note, double startAbsolute, double windowEnd)
    {
        if (isNoteActive(note.id))
            return;

        if (noteOnCallback)
            noteOnCallback(note.midiNote, note.velocity);

        const double noteEnd = startAbsolute + note.lengthBeats;
        if (noteEnd <= windowEnd + 1.0e-6)
        {
            if (noteOffCallback)
                noteOffCallback(note.midiNote);
        }
        else
        {
            activeNotes.push_back({ note.id, noteEnd });
        }
    }

    void removeExpiredNotes(double threshold)
    {
        for (auto it = activeNotes.begin(); it != activeNotes.end();)
        {
            if (it->endBeatAbsolute <= threshold + 1.0e-6)
            {
                if (const Note* note = findNoteById(it->noteId))
                    triggerNoteOff(*note);
                it = activeNotes.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void triggerNoteOff(const Note& note)
    {
        if (noteOffCallback)
            noteOffCallback(note.midiNote);
    }

    bool isNoteActive(int noteId) const
    {
        return std::any_of(activeNotes.begin(), activeNotes.end(), [noteId](const ActiveNote& active)
        {
            return active.noteId == noteId;
        });
    }

    void scrollToPlayhead()
    {
        if (gridComponent == nullptr)
            return;

        const int playheadX = (int)std::floor(xFromBeat(playheadBeat));
        const int viewWidth = viewport.getViewWidth();
        const int contentWidth = gridComponent->getWidth();
        const int targetX = juce::jlimit(0, std::max(0, contentWidth - viewWidth), playheadX - viewWidth / 3);
        viewport.setViewPosition(targetX, viewport.getViewPositionY());
    }

    struct ActiveNote
    {
        int noteId = 0;
        double endBeatAbsolute = 0.0;
    };

    static constexpr int lowestMidiNote = 36;
    static constexpr int highestMidiNote = 84;
    static constexpr int rowHeight = 26;
    static constexpr int timelineHeight = 24;
    static constexpr int labelWidth = 72;
    static constexpr double beatPixelWidth = 90.0;
    static constexpr double gridSubdivision = 0.25;
    static constexpr int timerFrequencyHz = 120;
    static constexpr int defaultTicksPerQuarterNote = 960;

    std::vector<Note> notes;
    std::vector<ActiveNote> activeNotes;

    std::unique_ptr<GridComponent> gridComponent;
    juce::Viewport viewport;

    juce::Slider bpmSlider;
    juce::Slider loopLengthSlider;
    juce::TextButton playButton{ "Play" };
    juce::TextButton stopButton{ "Stop" };
    juce::TextButton restartButton{ "Restart" };
    juce::TextButton importButton{ "Import MIDI" };
    juce::TextButton exportButton{ "Export MIDI" };
    juce::ToggleButton followButton{ "Follow" };
    juce::Label bpmLabel;
    juce::Label loopLabel;

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
    int selectedNoteId = -1;
    int nextNoteId = 1;
};
