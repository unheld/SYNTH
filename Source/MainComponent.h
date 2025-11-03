#pragma once
#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include <memory>

#include "MidiRollComponent.h"

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
    // ===== Synth state =====
    float   phase = 0.0f;
    float   targetFrequency = 220.0f;

    // LFO (vibrato)
    float   lfoPhase = 0.0f;
    float   lfoRateHz = 5.0f;
    float   lfoDepth = 0.03f;
    float   lfoStartPhaseNormalized = 0.0f;

    enum class LfoTriggerMode
    {
        Retrigger = 0,
        FreeRun
    };

    LfoTriggerMode lfoTriggerMode = LfoTriggerMode::Retrigger;

    // Smoothed parameters for a more polished response
    juce::SmoothedValue<float> frequencySmoothed;
    juce::SmoothedValue<float> gainSmoothed;
    juce::SmoothedValue<float> cutoffSmoothed;
    juce::SmoothedValue<float> resonanceSmoothed;
    juce::SmoothedValue<float> stereoWidthSmoothed;
    juce::SmoothedValue<float> lfoDepthSmoothed;
    juce::SmoothedValue<float> driveSmoothed;

    // Output Gain
    float   outputGain = 0.5f;
    float   driveAmount = 0.0f;
    float   crushAmount = 0.0f;
    float   subMixAmount = 0.0f;
    float   envFilterAmount = 0.0f;
    float   chaosAmount = 0.0f;
    float   delayAmount = 0.0f;
    float   autoPanAmount = 0.0f;
    float   glitchProbability = 0.0f;

    // Filter (cutoff + resonance + per-channel IIR)
    float   cutoffHz = 1000.0f;
    float   resonanceQ = 0.707f;
    juce::IIRFilter filterL, filterR;

    float   lfoCutModAmt = 0.0f;
    float   chaosValue = 0.0f;
    int     chaosSamplesRemaining = 0;
    juce::Random random;

    // Envelope
    float   attackMs = 8.0f;
    float   decayMs = 90.0f;
    float   sustainLevel = 0.75f;
    float   releaseMs = 280.0f;

    juce::ADSR amplitudeEnvelope;
    juce::ADSR::Parameters ampEnvParams;

    // Stereo width
    float   stereoWidth = 1.0f;

    int filterUpdateStep = 16;
    int filterUpdateCount = 0;
    double currentSR = 44100.0;

    float waveMorph = 0.0f;
    juce::AudioBuffer<float> scopeBuffer{ 1, 2048 };
    int scopeWritePos = 0;
    float subPhase = 0.0f;
    float detunePhase = 0.0f;
    float autoPanPhase = 0.0f;
    float autoPanRateHz = 0.35f;
    int crushCounter = 0;
    float crushHoldL = 0.0f;
    float crushHoldR = 0.0f;
    juce::AudioBuffer<float> delayBuffer{ 2, 1 };
    int delayWritePosition = 0;
    int maxDelaySamples = 1;
    int glitchSamplesRemaining = 0;
    float glitchHeldL = 0.0f;
    float glitchHeldR = 0.0f;

    // ===== UI Controls =====
    juce::TextButton playButton{ "Play" };
    juce::TextButton stopButton{ "Stop" };
    juce::TextButton restartButton{ "Restart" };
    juce::TextButton importButton{ "Import" };
    juce::TextButton exportButton{ "Export" };
    juce::Slider bpmSlider;
    juce::Label bpmCaption;

    juce::Slider waveKnob, gainKnob, attackKnob, decayKnob, sustainKnob, widthKnob;
    juce::Slider pitchKnob, cutoffKnob, resonanceKnob, releaseKnob;
    juce::Slider lfoKnob, lfoDepthKnob, filterModKnob, lfoModeKnob, lfoStartKnob;
    juce::Slider driveKnob, crushKnob, subMixKnob, envFilterKnob;
    juce::Slider chaosKnob, delayKnob, autoPanKnob, glitchKnob;

    juce::Label waveLabel, waveValue;
    juce::Label gainLabel, gainValue;
    juce::Label attackLabel, attackValue;
    juce::Label decayLabel, decayValue;
    juce::Label sustainLabel, sustainValue;
    juce::Label widthLabel, widthValue;
    juce::Label pitchLabel, pitchValue;
    juce::Label cutoffLabel, cutoffValue;
    juce::Label resonanceLabel, resonanceValue;
    juce::Label releaseLabel, releaseValue;
    juce::Label lfoLabel, lfoValue;
    juce::Label lfoDepthLabel, lfoDepthValue;
    juce::Label filterModLabel, filterModValue;
    juce::Label lfoModeLabel, lfoModeValue;
    juce::Label lfoStartLabel, lfoStartValue;
    juce::Label driveLabel, driveValue;
    juce::Label crushLabel, crushValue;
    juce::Label subMixLabel, subMixValue;
    juce::Label envFilterLabel, envFilterValue;
    juce::Label chaosLabel, chaosValueLabel;
    juce::Label delayLabel, delayValue;
    juce::Label autoPanLabel, autoPanValue;
    juce::Label glitchLabel, glitchValue;

    juce::TextButton audioToggle{ "Audio ON" };
    bool audioEnabled = true;

    MidiRollComponent midiRoll;

    // ===== MIDI keyboard UI =====
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboardComponent { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    // ===== MIDI state (monophonic, last-note priority) =====
    juce::Array<int> noteStack;   // holds pressed MIDI notes
    int currentMidiNote = -1;
    float currentVelocity = 1.0f;
    bool midiGate = false;        // gate controlled by MIDI

    // Scope area cache (so paint knows where to draw when keyboard steals space)
    juce::Rectangle<int> scopeRect;
    juce::Rectangle<int> osc3DRect;
    std::vector<float> waveformSnapshot;

    std::atomic<float> smoothedLevel { 0.0f };
    std::atomic<float> lowBandLevel { 0.0f };
    std::atomic<float> midBandLevel { 0.0f };
    std::atomic<float> highBandLevel { 0.0f };
    std::atomic<float> delayFeedbackVisual { 0.0f };
    std::atomic<float> glitchHoldVisual { 0.0f };

    float meterSmoother = 0.0f;
    float lowBandSmoother = 0.0f;
    float midBandSmoother = 0.0f;
    float highBandSmoother = 0.0f;
    float delayVisualSmoother = 0.0f;
    float glitchVisualSmoother = 0.0f;
    float lowBandState = 0.0f;
    float midBandState = 0.0f;

    // Sequencer
    struct SequencerNote
    {
        int midiNote = 60;
        float velocity = 1.0f;
        double startBeat = 0.0;
        double endBeat = 0.0;
    };

    struct ActiveSequencerNote
    {
        int midiNote = 60;
        double endBeat = 0.0;
    };

    void initialiseTransport();
    void initialiseUi();
    void initialiseSliders();
    void initialiseToggle();
    void setSequencerBpm(double newBpm);
    void startSequencer();
    void stopSequencer(bool sendAllNotesOff);
    void restartSequencer();
    void importMidiSequence();
    void exportMidiSequence();
    void refreshSequencerNotes();
    void processSequencerBlock(int numSamples);
    void handleSequencerClearRequests();

    std::vector<SequencerNote> sequencerNotes;
    std::vector<SequencerNote> sequencerNotesPending;
    std::vector<ActiveSequencerNote> activeSequencerNotes;
    juce::SpinLock sequencerNotesLock;
    std::atomic<bool> sequencerNotesDirty { false };
    std::atomic<bool> sequencerPlayState { false };
    std::atomic<bool> sequencerClearActive { false };
    std::atomic<bool> sequencerResetRequested { false };
    std::atomic<double> sequencerBpmValue { 174.0 };
    double sequencerPositionBeats = 0.0;
    const double sequencerLoopBeats = 16.0;
    std::atomic<double> playheadBeatsAtomic { 0.0 };

    // ===== Helpers =====
    void initialiseMidiInputs();
    void initialiseKeyboard();
    void configureRotarySlider(juce::Slider& slider);
    void configureCaptionLabel(juce::Label& label, const juce::String& text);
    void configureValueLabel(juce::Label& label);
    void updateAmplitudeEnvelope();
    void triggerLfo();

    void resetSmoothers(double sampleRate);
    void setTargetFrequency(float newFrequency, bool force = false);
    void updateFilterCoeffs(double cutoff, double Q);
    void updateFilterStatic();
    inline float renderMorphSample(float ph, float morph, float normPhaseInc) const;
    inline float polyBlep(float t, float dt) const;
    int findZeroCrossingIndex(int searchSpan) const;
    void captureWaveformSnapshot();
    void timerCallback() override;

    inline float sine(float ph) const { return std::sin(ph); }
    inline float tri(float ph)  const { return (2.0f / juce::MathConstants<float>::pi) * std::asin(std::sin(ph)); }

    static inline float midiNoteToFreq(int midiNote)
    {
        // A4 = 440 Hz, MIDI 69
        return 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
    }

    std::unique_ptr<juce::FileChooser> importFileChooser;
    std::unique_ptr<juce::FileChooser> exportFileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
