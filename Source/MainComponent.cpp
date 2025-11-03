#include "MainComponent.h"

#include <cmath>
#include <algorithm>

MainComponent::MainComponent()
    : synthUI(engine),
      visualizer(engine),
      keyboardComponent(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setSize(synth::config::defaultWidth, synth::config::defaultHeight);
    setAudioChannels(0, 2);

    initialiseUi();
    initialiseMidiInputs();
    initialiseKeyboard();

    startTimerHz(synth::config::scopeTimerHz);
}

MainComponent::~MainComponent()
{
    auto devices = juce::MidiInput::getAvailableDevices();
    for (auto& d : devices)
        deviceManager.removeMidiInputDeviceCallback(d.identifier, this);

    keyboardState.removeListener(this);
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused(samplesPerBlockExpected);
    engine.prepare(sampleRate, samplesPerBlockExpected);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr)
        return;

    engine.renderNextBlock(*bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples);
}

void MainComponent::releaseResources()
{
    engine.release();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void MainComponent::resized()
{
    if (getWidth() < synth::config::minWidth || getHeight() < synth::config::minHeight)
        setSize(std::max(getWidth(), synth::config::minWidth), std::max(getHeight(), synth::config::minHeight));

    auto area = getLocalBounds().reduced(synth::config::headerMargin);

    auto bar = area.removeFromTop(synth::config::headerBarHeight);
    audioToggle.setBounds(bar.getRight() - synth::config::audioButtonWidth,
                          bar.getY() + 4,
                          synth::config::audioButtonWidth,
                          synth::config::audioButtonHeight);

    auto strip = area.removeFromTop(synth::config::controlStripHeight);
    synthUI.setBounds(strip);

    const int keyboardHeight = juce::jmax(synth::config::keyboardMinHeight, area.getHeight() / 3);
    auto keyboardArea = area.removeFromBottom(keyboardHeight);
    keyboardComponent.setBounds(keyboardArea);

    visualizer.setBounds(area);
}

void MainComponent::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& m)
{
    engine.handleMidiMessage(m);
}

void MainComponent::handleNoteOn(juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity)
{
    engine.noteOn(midiNoteNumber, velocity);
}

void MainComponent::handleNoteOff(juce::MidiKeyboardState*, int, int midiNoteNumber, float)
{
    engine.noteOff(midiNoteNumber);
}

void MainComponent::initialiseUi()
{
    addAndMakeVisible(synthUI);
    addAndMakeVisible(visualizer);
    initialiseToggle();
}

void MainComponent::initialiseToggle()
{
    addAndMakeVisible(audioToggle);
    audioToggle.setClickingTogglesState(true);
    audioToggle.setToggleState(true, juce::dontSendNotification);
    audioToggle.onClick = [this]
    {
        audioEnabled = audioToggle.getToggleState();
        engine.setAudioEnabled(audioEnabled);
        audioToggle.setButtonText(audioEnabled ? "Audio ON" : "Audio OFF");
    };
    audioToggle.onClick();
}

void MainComponent::initialiseMidiInputs()
{
    auto midiInputs = juce::MidiInput::getAvailableDevices();
    for (auto& input : midiInputs)
    {
        deviceManager.removeMidiInputDeviceCallback(input.identifier, this);
        deviceManager.addMidiInputDeviceCallback(input.identifier, this);
        deviceManager.setMidiInputDeviceEnabled(input.identifier, true);
    }
}

void MainComponent::initialiseKeyboard()
{
    addAndMakeVisible(keyboardComponent);
    keyboardComponent.setAvailableRange(36, 84);
    keyboardState.addListener(this);

    keyboardComponent.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xFF2A2A2A));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xFF0E0E0E));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colours::black.withAlpha(0.6f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colours::white.withAlpha(0.08f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colours::white.withAlpha(0.12f));
}

void MainComponent::timerCallback()
{
    engine.captureWaveformSnapshot();
    visualizer.repaint();
}

