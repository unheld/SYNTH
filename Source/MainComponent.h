#pragma once
#include <JuceHeader.h>
#include <vector>

#include "synth/SynthConfig.h"
#include "synth/SynthEngine.h"
#include "synth/SynthUI.h"
#include "synth/SynthVisualizer.h"

class MainComponent : public juce::AudioAppComponent,
                      public juce::MidiInputCallback,
                      public juce::MidiKeyboardStateListener,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int, double) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;
    void releaseResources() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // ===== MIDI callbacks =====
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float /*velocity*/) override;

private:
    SynthEngine engine;
    SynthUI synthUI;
    SynthVisualizer visualizer;

    juce::TextButton audioToggle{ "Audio ON" };
    bool audioEnabled = true;

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboardComponent;

    void initialiseUi();
    void initialiseToggle();
    void initialiseMidiInputs();
    void initialiseKeyboard();
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
