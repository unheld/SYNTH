#pragma once

#include <JuceHeader.h>
#include <vector>

#include "SynthConfig.h"

class SynthEngine
{
public:
    SynthEngine();

    void prepare(double sampleRate, int samplesPerBlock);
    void renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void release();

    void setAudioEnabled(bool enabled);

    void noteOn(int midiNoteNumber, float velocity);
    void noteOff(int midiNoteNumber);
    void allNotesOff();

    void handleMidiMessage(const juce::MidiMessage& message);

    // Parameter setters
    void setWaveMorph(float value);
    void setOutputGain(float value);
    void setAttack(float milliseconds);
    void setDecay(float milliseconds);
    void setSustain(float level);
    void setRelease(float milliseconds);
    void setStereoWidth(float width);
    void setTargetFrequency(float frequency, bool force = false);
    void setCutoff(float cutoff);
    void setResonance(float resonance);
    void setLfoRate(float rateHz);
    void setLfoDepth(float depth);
    void setFilterMod(float amount);
    void setLfoMode(bool freeRun);
    void setLfoStart(float normalizedPhase);
    void setDrive(float amount);
    void setCrush(float amount);
    void setSubMix(float amount);
    void setEnvelopeFilter(float amount);
    void setChaos(float amount);
    void setDelay(float amount);
    void setAutoPan(float amount);
    void setGlitch(float amount);

    float getWaveMorph() const noexcept { return waveMorph; }
    float getOutputGain() const noexcept { return outputGain; }
    float getAttack() const noexcept { return attackMs; }
    float getDecay() const noexcept { return decayMs; }
    float getSustain() const noexcept { return sustainLevel; }
    float getRelease() const noexcept { return releaseMs; }
    float getStereoWidth() const noexcept { return stereoWidth; }
    float getCutoff() const noexcept { return cutoffHz; }
    float getResonance() const noexcept { return resonanceQ; }
    float getTargetFrequency() const noexcept { return targetFrequency; }
    float getLfoRate() const noexcept { return lfoRateHz; }
    float getLfoDepth() const noexcept { return lfoDepth; }
    float getFilterMod() const noexcept { return lfoCutModAmt; }
    float getLfoStart() const noexcept { return lfoStartPhaseNormalized; }
    bool  isLfoFreeRunning() const noexcept { return lfoTriggerMode == LfoTriggerMode::FreeRun; }
    float getDrive() const noexcept { return driveAmount; }
    float getCrush() const noexcept { return crushAmount; }
    float getSubMix() const noexcept { return subMixAmount; }
    float getEnvelopeFilter() const noexcept { return envFilterAmount; }
    float getChaos() const noexcept { return chaosAmount; }
    float getDelay() const noexcept { return delayAmount; }
    float getAutoPan() const noexcept { return autoPanAmount; }
    float getGlitch() const noexcept { return glitchProbability; }

    const juce::AudioBuffer<float>& getScopeBuffer() const noexcept { return scopeBuffer; }
    int getScopeWritePosition() const noexcept { return scopeWritePos; }
    const std::vector<float>& getWaveformSnapshot() const noexcept { return waveformSnapshot; }
    void captureWaveformSnapshot();
    int findZeroCrossingIndex(int searchSpan) const;

    static inline float midiNoteToFreq(int midiNote)
    {
        return 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
    }

private:
    enum class LfoTriggerMode
    {
        Retrigger = 0,
        FreeRun
    };

    void resetSmoothers(double sampleRate);
    void updateFilterCoeffs(double cutoff, double Q);
    void updateFilterStatic();
    void updateAmplitudeEnvelope();
    void triggerLfo();
    inline float renderMorphSample(float ph, float morph, float normPhaseInc) const;
    inline float polyBlep(float t, float dt) const;
    inline float sine(float ph) const { return std::sin(ph); }
    inline float tri(float ph) const { return (2.0f / juce::MathConstants<float>::pi) * std::asin(std::sin(ph)); }

    juce::Random random;

    // core state
    bool audioEnabled = true;
    double currentSR = 44100.0;

    float phase = 0.0f;
    float subPhase = 0.0f;
    float detunePhase = 0.0f;
    float targetFrequency = 220.0f;

    juce::SmoothedValue<float> frequencySmoothed;
    juce::SmoothedValue<float> gainSmoothed;
    juce::SmoothedValue<float> cutoffSmoothed;
    juce::SmoothedValue<float> resonanceSmoothed;
    juce::SmoothedValue<float> stereoWidthSmoothed;
    juce::SmoothedValue<float> lfoDepthSmoothed;
    juce::SmoothedValue<float> driveSmoothed;

    float outputGain = 0.5f;
    float driveAmount = 0.0f;
    float crushAmount = 0.0f;
    float subMixAmount = 0.0f;
    float envFilterAmount = 0.0f;
    float chaosAmount = 0.0f;
    float delayAmount = 0.0f;
    float autoPanAmount = 0.0f;
    float glitchProbability = 0.0f;

    float cutoffHz = 1000.0f;
    float resonanceQ = 0.707f;

    juce::IIRFilter filterL;
    juce::IIRFilter filterR;

    float lfoPhase = 0.0f;
    float lfoRateHz = 5.0f;
    float lfoDepth = 0.03f;
    float lfoCutModAmt = 0.0f;
    float lfoStartPhaseNormalized = 0.0f;
    LfoTriggerMode lfoTriggerMode = LfoTriggerMode::Retrigger;

    float chaosValue = 0.0f;
    int chaosSamplesRemaining = 0;

    juce::ADSR amplitudeEnvelope;
    juce::ADSR::Parameters ampEnvParams;
    float attackMs = 8.0f;
    float decayMs = 90.0f;
    float sustainLevel = 0.75f;
    float releaseMs = 280.0f;

    float stereoWidth = 1.0f;

    int filterUpdateStep = 16;
    int filterUpdateCount = 0;

    juce::AudioBuffer<float> scopeBuffer { 1, synth::config::scopeBufferSize };
    int scopeWritePos = 0;
    std::vector<float> waveformSnapshot;

    float autoPanPhase = 0.0f;
    float autoPanRateHz = 0.35f;

    int crushCounter = 0;
    float crushHoldL = 0.0f;
    float crushHoldR = 0.0f;

    juce::AudioBuffer<float> delayBuffer { 2, 1 };
    int delayWritePosition = 0;
    int maxDelaySamples = 1;

    int glitchSamplesRemaining = 0;
    float glitchHeldL = 0.0f;
    float glitchHeldR = 0.0f;

    // MIDI state
    juce::Array<int> noteStack;
    int currentMidiNote = -1;
    float currentVelocity = 1.0f;
    bool midiGate = false;
};

