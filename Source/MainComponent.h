#pragma once
#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include "MidiRollComponent.h"

class ParameterKnob;
class KnobGroupComponent;
class OscillatorPreviewComponent;
class FilterResponseComponent;
class EnvelopeGraphComponent;
class LfoPreviewComponent;
class EffectIntensityMeter;
class OutputMeterComponent;

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

    enum class FilterType
    {
        LowPass = 0,
        BandPass,
        HighPass,
        Notch
    };

    float getCutoffHz() const noexcept { return cutoffHz; }
    float getResonanceQ() const noexcept { return resonanceQ; }
    float getDriveAmount() const noexcept { return driveAmount; }
    float getOutputGain() const noexcept { return outputGain; }
    float getLfoRateHz() const noexcept { return lfoRateHz; }
    float getLfoDepthAmount() const noexcept { return lfoDepth; }
    float getAttackMs() const noexcept { return attackMs; }
    float getDecayMs() const noexcept { return decayMs; }
    float getSustainLevel() const noexcept { return sustainLevel; }
    float getReleaseMs() const noexcept { return releaseMs; }
    float getCurrentLevel() const noexcept { return smoothedLevel.load(); }
    float getDelayAmount() const noexcept { return delayAmount; }
    float getChaosAmount() const noexcept { return chaosAmount; }
    float getCrushAmount() const noexcept { return crushAmount; }
    float getAutoPanAmount() const noexcept { return autoPanAmount; }
    float getGlitchAmount() const noexcept { return glitchProbability; }
    double getCurrentSampleRate() const noexcept { return currentSR; }
    FilterType getFilterType() const noexcept { return filterType; }
    float renderLfoShape(float phase) const noexcept;

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
    FilterType filterType = FilterType::LowPass;
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
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton restartButton { "Restart" };
    juce::TextButton importButton { "Import" };
    juce::TextButton exportButton { "Export" };
    juce::Label     bpmLabel;

    ParameterKnob waveKnob, gainKnob, attackKnob, decayKnob, sustainKnob, widthKnob;
    ParameterKnob pitchKnob, cutoffKnob, resonanceKnob, releaseKnob;
    ParameterKnob lfoKnob, lfoDepthKnob, filterModKnob, lfoModeKnob, lfoStartKnob;
    ParameterKnob driveKnob, crushKnob, subMixKnob, envFilterKnob;
    ParameterKnob chaosKnob, delayKnob, autoPanKnob, glitchKnob;
    ParameterKnob glideKnob;

    juce::ComboBox filterTypeBox;
    juce::ComboBox lfoDestinationBox;
    juce::ComboBox lfoShapeBox;
    juce::ToggleButton envelopeToFilterToggle { "Use for Filter" };
    juce::ToggleButton envelopeToAmpToggle { "Use for Amp" };
    juce::ToggleButton monoToggle { "Mono" };

    juce::TextButton audioToggle{ "Audio ON" };

    std::unique_ptr<KnobGroupComponent> oscillatorGroup;
    std::unique_ptr<KnobGroupComponent> filterGroup;
    std::unique_ptr<KnobGroupComponent> envelopeGroup;
    std::unique_ptr<KnobGroupComponent> lfoGroup;
    std::unique_ptr<KnobGroupComponent> effectsGroup;
    std::unique_ptr<KnobGroupComponent> masterGroup;

    std::unique_ptr<OscillatorPreviewComponent> oscillatorPreview;
    std::unique_ptr<FilterResponseComponent> filterGraph;
    std::unique_ptr<EnvelopeGraphComponent> envelopeGraph;
    std::unique_ptr<LfoPreviewComponent> lfoPreview;
    std::unique_ptr<EffectIntensityMeter> effectsMeter;
    std::unique_ptr<OutputMeterComponent> outputMeter;
    bool audioEnabled = true;

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
    float glideTimeMs = 0.0f;
    bool envelopeToFilterEnabled = true;
    bool envelopeToAmpEnabled = true;
    bool monoModeEnabled = true;
    int lfoDestinationChoice = 0;
    int lfoShapeChoice = 0;

    // ===== Helpers =====
    void initialiseUi();
    void initialiseSliders();
    void initialiseToolbar();
    void initialiseToggle();
    void initialiseMidiInputs();
    void initialiseKeyboard();
    void configureRotarySlider(juce::Slider& slider);
    void updateAmplitudeEnvelope();
    void triggerLfo();
    void updateGlideSmoother();

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

    std::unique_ptr<MidiRollComponent> midiRoll;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
