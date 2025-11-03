## 🧬 agents.md
### 🎛 SYNTH Project — Phase 2: MIDI Roll Integration & Playback System

#### 🧠 Core Intent
Transform the `MidiRollComponent` from a visual-only editor into a **functional, timing-accurate MIDI sequencer** that plays back user-drawn notes in sync with the synth engine inside `MainComponent`.

The goal is to replicate an **Ableton-style piano roll** with full real-time modulation control, minimalist design, and modern performance.

---

### 🧩 Current State
- ✅ `MainComponent` handles full synth engine (oscillators, filters, envelopes).  
- ✅ `MidiRollComponent` renders a scrollable grid, supports mouse-based note drawing, and has a looping visual playhead.  
- ⏸ Playback is **visual only** — no MIDI events are being sent to the synth.  
- ⏸ No UI toolbar yet for playback controls (Play / Stop / Restart / BPM display).

---

### 🧭 Phase Objectives
#### **1. Add Playback Toolbar**
- Integrate a minimalist toolbar at the top of `MainComponent`.
- Controls: **Play**, **Stop**, **Restart**, **Import**, **Export**, and **BPM** display.
- Each button calls:
  - `midiRoll->togglePlayback()`  
  - `midiRoll->stopPlayback()`  
  - `midiRoll->startPlayback()` (reset to bar 1)
- BPM display should be dynamic (for future tempo automation).

#### **2. Real MIDI Playback (next step)**
- `MidiRollComponent` should output MIDI note-on / note-off events in sync with the playhead.
- These should feed directly into `MainComponent`’s `keyboardState.processNextMidiBuffer()` for triggering the synth.
- Add optional metronome tick later for debugging timing.

#### **3. Visual and Interaction Improvements**
- Grid highlight for active beats / measures.
- Improved note selection and drag behavior.
- Optional quantization toggle (e.g. 1/16 grid).
- Smooth scroll with the mouse wheel and horizontal drag.

---

### 🧰 Codex Task Instruction
When Codex edits or extends this project:
1. Always keep compatibility with **JUCE 8.0.10** and **Visual Studio 17.14.18**.  
2. Avoid placeholders — all code must compile cleanly.  
3. Maintain minimal, consistent UI.  
4. Every new function should be connected through existing classes (`MainComponent`, `MidiRollComponent`).
5. Next commit should introduce a **fully functional playback toolbar** with minimal styling and tested callbacks.

---

### 🧩 Future Expansion
- Export to `.mid` and import from `.mid`.  
- Add “pattern slots” for multiple MIDI loops.  
- Sync playback with external clock (Ableton Link or host DAW tempo).  
- Modular integration of automation lanes (LFO / envelope / filter cutoff).  
