#include "MainComponent.h"
#include <cmath>
#include <algorithm>

namespace
{
    constexpr int defaultWidth = 960;
    constexpr int defaultHeight = 600;
    constexpr int minWidth = 720;
    constexpr int minHeight = 420;
    constexpr int headerBarHeight = 36;
    constexpr int headerMargin = 16;
    constexpr int audioButtonWidth = 96;
    constexpr int audioButtonHeight = 28;
    constexpr int toolbarButtonWidth = 72;
    constexpr int toolbarButtonHeight = 28;
    constexpr int toolbarSpacing = 8;
    constexpr int bpmLabelWidth = 84;
    constexpr int defaultBpmDisplay = 120;
    constexpr int controlStripHeight = 110;
    constexpr int knobSize = 48;
    constexpr int keyboardMinHeight = 60;
    constexpr int scopeTimerHz = 60;
}

//==============================================================================
MainComponent::MainComponent()
{
    setSize(defaultWidth, defaultHeight);
    setAudioChannels(0, 2);

    scopeBuffer.clear();
    waveformSnapshot.clear();

    ampEnvParams.attack = attackMs * 0.001f;
    ampEnvParams.decay = decayMs * 0.001f;
    ampEnvParams.sustain = sustainLevel;
    ampEnvParams.release = releaseMs * 0.001f;
    amplitudeEnvelope.setParameters(ampEnvParams);

    frequencySmoothed.setCurrentAndTargetValue(targetFrequency);
    gainSmoothed.setCurrentAndTargetValue(outputGain);
    cutoffSmoothed.setCurrentAndTargetValue(cutoffHz);
    resonanceSmoothed.setCurrentAndTargetValue(resonanceQ);
    stereoWidthSmoothed.setCurrentAndTargetValue(stereoWidth);
    lfoDepthSmoothed.setCurrentAndTargetValue(lfoDepth);
    driveSmoothed.setCurrentAndTargetValue(driveAmount);

    initialiseUi();
    initialiseMidiInputs();
    initialiseKeyboard();
    
midiRoll = std::make_unique<MidiRollComponent>();
addAndMakeVisible(midiRoll.get());

    startTimerHz(scopeTimerHz);
}

MainComponent::~MainComponent()
{
    auto devices = juce::MidiInput::getAvailableDevices();
    for (auto& d : devices)
        deviceManager.removeMidiInputDeviceCallback(d.identifier, this);

    keyboardState.removeListener(this);
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay(int, double sampleRate)
{
    currentSR = sampleRate;
    phase = 0.0f;
    lfoPhase = 0.0f;
    scopeWritePos = 0;
    filterUpdateCount = 0;
    subPhase = 0.0f;
    detunePhase = 0.0f;
    autoPanPhase = 0.0f;
    crushCounter = 0;
    crushHoldL = 0.0f;
    crushHoldR = 0.0f;
    chaosValue = 0.0f;
    chaosSamplesRemaining = 0;
    glitchSamplesRemaining = 0;
    glitchHeldL = glitchHeldR = 0.0f;
    waveformSnapshot.clear();
    resetSmoothers(sampleRate);
    updateFilterStatic();
    amplitudeEnvelope.setSampleRate(sampleRate);
    updateAmplitudeEnvelope();
    amplitudeEnvelope.reset();
    triggerLfo();

    maxDelaySamples = juce::jmax(1, (int)std::ceil(sampleRate * 2.0));
    delayBuffer.setSize(2, maxDelaySamples);
    delayBuffer.clear();
    delayWritePosition = 0;
}

void MainComponent::updateFilterCoeffs(double cutoff, double Q)
{
    cutoff = juce::jlimit(20.0, 20000.0, cutoff);
    Q = juce::jlimit(0.1, 12.0, Q);

    const double w0 = juce::MathConstants<double>::twoPi * cutoff / currentSR;
    const double cw = std::cos(w0);
    const double sw = std::sin(w0);
    const double alpha = sw / (2.0 * Q);

    double b0 = (1.0 - cw) * 0.5;
    double b1 = 1.0 - cw;
    double b2 = (1.0 - cw) * 0.5;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cw;
    double a2 = 1.0 - alpha;

    juce::IIRCoefficients c(b0 / a0, b1 / a0, b2 / a0,
        1.0, a1 / a0, a2 / a0);

    filterL.setCoefficients(c);
    filterR.setCoefficients(c);
}

void MainComponent::updateFilterStatic()
{
    updateFilterCoeffs(cutoffHz, resonanceQ);
}

void MainComponent::resetSmoothers(double sampleRate)
{
    const double fastRampSeconds = 0.02;
    const double filterRampSeconds = 0.06;
    const double spatialRampSeconds = 0.1;

    frequencySmoothed.reset(sampleRate, fastRampSeconds);
    gainSmoothed.reset(sampleRate, fastRampSeconds);
    cutoffSmoothed.reset(sampleRate, filterRampSeconds);
    resonanceSmoothed.reset(sampleRate, filterRampSeconds);
    stereoWidthSmoothed.reset(sampleRate, spatialRampSeconds);
    lfoDepthSmoothed.reset(sampleRate, spatialRampSeconds);
    driveSmoothed.reset(sampleRate, fastRampSeconds);

    frequencySmoothed.setCurrentAndTargetValue(targetFrequency);
    gainSmoothed.setCurrentAndTargetValue(outputGain);
    cutoffSmoothed.setCurrentAndTargetValue(cutoffHz);
    resonanceSmoothed.setCurrentAndTargetValue(resonanceQ);
    stereoWidthSmoothed.setCurrentAndTargetValue(stereoWidth);
    lfoDepthSmoothed.setCurrentAndTargetValue(lfoDepth);
    driveSmoothed.setCurrentAndTargetValue(driveAmount);

    filterL.reset();
    filterR.reset();
}

void MainComponent::setTargetFrequency(float newFrequency, bool force)
{
    targetFrequency = juce::jlimit(20.0f, 20000.0f, newFrequency);

    if (force)
        frequencySmoothed.setCurrentAndTargetValue(targetFrequency);
    else
        frequencySmoothed.setTargetValue(targetFrequency);
}

inline float MainComponent::polyBlep(float t, float dt) const
{
    if (dt <= 0.0f)
        return 0.0f;

    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }

    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }

    return 0.0f;
}

inline float MainComponent::renderMorphSample(float ph, float morph, float normPhaseInc) const
{
    // ===== Fractal Oscillator Core (drop‑in) =====
    // Keeps original 4‑shape morph, then layers a small number of
    // band‑limited sine partials whose behaviour is driven by:
    //   chaosAmount  -> number of partials / irregularity
    //   subMixAmount -> even/odd bias (tone colour)
    //   lfoDepth     -> gentle weight motion via lfoPhase
    //
    // No header/UI changes. Safe for JUCE 8 / C++17.

    // --- Phase wrap ---
    while (ph >= juce::MathConstants<float>::twoPi) ph -= juce::MathConstants<float>::twoPi;
    if (ph < 0.0f) ph += juce::MathConstants<float>::twoPi;

    const float m = juce::jlimit(0.0f, 1.0f, morph);
    const float seg = 1.0f / 3.0f;

    // --- Base waves (existing) ---
    const float sineSample = sine(ph);
    const float triSample  = tri(ph);

    // Normalised phase increment per sample in cycles (0..0.5 usually)
    const float dt = juce::jlimit(1.0e-6f, 0.5f, normPhaseInc);

    // Fractional phase [0,1)
    float t = ph / juce::MathConstants<float>::twoPi;
    t -= std::floor(t);

    // BLEP anti-aliased saw
    float sawSample = 2.0f * t - 1.0f;
    sawSample -= polyBlep(t, dt);
    sawSample = juce::jlimit(-1.2f, 1.2f, sawSample);

    // BLEP anti-aliased square (50% duty)
    float squareSample = t < 0.5f ? 1.0f : -1.0f;
    squareSample += polyBlep(t, dt);
    float t2 = t + 0.5f; t2 -= std::floor(t2);
    squareSample -= polyBlep(t2, dt);
    squareSample = std::tanh(squareSample * 1.15f);

    // Smooth morph across four shapes
    float base;
    if (m < seg)            base = juce::jmap(m / seg,                 sineSample,  triSample);
    else if (m < 2.0f*seg)  base = juce::jmap((m - seg) / seg,         triSample,   sawSample);
    else                    base = juce::jmap((m - 2.0f*seg) / seg,    sawSample,   squareSample);

    // --- Fractal layering (new) ---
    const float chaos   = juce::jlimit(0.0f, 1.0f, chaosAmount);
    const float spread  = juce::jlimit(0.0f, 1.0f, subMixAmount);   // colour bias
    const float motion  = juce::jlimit(0.0f, 1.0f, lfoDepth);       // gentle movement

    // Max usable harmonic by Nyquist: k * dt < 0.5 => k < 0.5/dt
    int maxNyquistH = (int)std::floor(0.5f / dt);
    maxNyquistH = juce::jlimit(1, 64, maxNyquistH); // safety cap

    // Choose small number of partials based on chaos (1..6) but never exceed Nyquist
    const int desired = 1 + (int)std::round(chaos * 5.0f);
    const int partials = juce::jlimit(1, juce::jmin(6, maxNyquistH), desired);

    // Amplitude rolloff (steeper at low chaos)
    const float rolloff = juce::jmap(chaos, 0.0f, 1.0f, 0.75f, 0.45f);

    // Even/odd emphasis via spread
    const float evenBias = juce::jlimit(0.0f, 1.0f, spread * 0.85f);
    const float oddBias  = juce::jlimit(0.0f, 1.0f, 1.0f - spread * 0.65f);

    // Subtle time motion ties timbre to LFO state (read‑only usage)
    const float timeMod = std::sin(lfoPhase) * motion * 0.25f;

    float layered = base;
    float norm = 1.0f;

    // Sum a few sine partials (band-limited) with biased weights.
    // Using sines keeps alias low; additional BLEP not needed on harmonics.
    for (int k = 2; k <= partials + 1; ++k)
    {
        if (k > maxNyquistH) break;

        const bool isEven = (k % 2 == 0);
        const float bias = isEven ? evenBias : oddBias;

        // Weight decays with power; add tiny motion and chaos wobble
        const float wobble = 1.0f + 0.12f * chaos * std::sin((float)k * (lfoPhase + 0.37f)) + timeMod;
        const float w = std::pow(rolloff, (float)(k - 1)) * juce::jlimit(0.0f, 1.0f, 0.6f + 0.4f * bias) * wobble;

        layered += w * std::sin((float)k * ph);
        norm += w;
    }

    // Normalise and blend with original; layerMix scales with chaos
    layered = juce::jlimit(-1.5f, 1.5f, layered / juce::jmax(1.0f, norm));
    const float layerMix = juce::jlimit(0.0f, 1.0f, juce::jmap(chaos, 0.0f, 1.0f, 0.0f, 0.85f));

    float out = juce::jmap(layerMix, base, layered);

    // Final tiny soft clip to keep headroom consistent
    return std::tanh(out * 1.1f);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr || bufferToFill.buffer->getNumChannels() == 0)
        return;

    bufferToFill.buffer->clear(bufferToFill.startSample, bufferToFill.numSamples);

    auto* l = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
    auto* r = bufferToFill.buffer->getNumChannels() > 1
        ? bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample) : nullptr;

    const float lfoInc = juce::MathConstants<float>::twoPi * lfoRateHz / (float)currentSR;
    const float autoPanInc = juce::MathConstants<float>::twoPi * autoPanRateHz / (float)currentSR;
    const float crushAmt = juce::jlimit(0.0f, 1.0f, crushAmount);
    const float subMixAmt = juce::jlimit(0.0f, 1.0f, subMixAmount);
    const float envFilterAmt = juce::jlimit(-1.0f, 1.0f, envFilterAmount);
    const float chaosAmt = juce::jlimit(0.0f, 1.0f, chaosAmount);
    const float delayAmtLocal = juce::jlimit(0.0f, 1.0f, delayAmount);
    const float autoPanAmt = juce::jlimit(0.0f, 1.0f, autoPanAmount);
    const float glitchProbLocal = juce::jlimit(0.0f, 1.0f, glitchProbability);
    const float delayMix = juce::jmap(delayAmtLocal, 0.0f, 1.0f, 0.0f, 0.65f);
    const float delayFeedback = juce::jmap(delayAmtLocal, 0.0f, 1.0f, 0.05f, 0.88f);
    const int delaySamples = (maxDelaySamples > 1)
        ? juce::jlimit(1, maxDelaySamples - 1,
            (int)std::round(juce::jmap((double)delayAmtLocal, 0.0, 1.0,
                currentSR * 0.03,
                juce::jmin(currentSR * 1.25, (double)maxDelaySamples - 1.0))))
        : 1;

    float blockPeak = 0.0f;
    float lowState = lowBandState;
    float midState = midBandState;
    float lowAccum = 0.0f;
    float midAccum = 0.0f;
    float highAccum = 0.0f;
    float glitchActivity = glitchSamplesRemaining > 0 ? 1.0f : 0.0f;

    for (int i = 0; i < bufferToFill.numSamples; ++i)
    {
        if (!audioEnabled && amplitudeEnvelope.isActive())
            amplitudeEnvelope.noteOff();

        const float baseFrequency = frequencySmoothed.getNextValue();
        const float gain = gainSmoothed.getNextValue() * currentVelocity;
        const float depth = lfoDepthSmoothed.getNextValue();
        const float width = stereoWidthSmoothed.getNextValue();
        const float baseCutoff = cutoffSmoothed.getNextValue();
        const float baseResonance = resonanceSmoothed.getNextValue();
        const float ampEnv = amplitudeEnvelope.getNextSample();
        const float drive = driveSmoothed.getNextValue();

        float lfoS = std::sin(lfoPhase);
        float vibrato = 1.0f + (depth * lfoS);
        lfoPhase += lfoInc;
        if (lfoPhase >= juce::MathConstants<float>::twoPi) lfoPhase -= juce::MathConstants<float>::twoPi;

        float chaosScale = 1.0f;
        if (chaosAmt > 0.0f)
        {
            if (chaosSamplesRemaining <= 0)
            {
                const int span = juce::jmax(1, (int)std::round(juce::jmap(chaosAmt, 0.0f, 1.0f,
                    (float)currentSR * 0.18f,
                    (float)currentSR * 0.01f)));
                chaosSamplesRemaining = span;
                chaosValue = random.nextFloat() * 2.0f - 1.0f;
            }
            chaosScale = juce::jlimit(0.7f, 1.3f, 1.0f + chaosValue * chaosAmt * 0.10f);
            --chaosSamplesRemaining;
        }
        else
        {
            chaosValue = 0.0f;
            chaosSamplesRemaining = 0;
        }

        const float effectiveFrequency = baseFrequency * chaosScale;
        const float phaseInc = juce::MathConstants<float>::twoPi * (effectiveFrequency * vibrato) / (float)currentSR;
        phase += phaseInc;
        if (phase >= juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;

        float subPhaseInc = phaseInc * 0.5f;
        float detunePhaseInc = phaseInc * 1.01f;
        subPhase += subPhaseInc;
        detunePhase += detunePhaseInc;
        if (subPhase >= juce::MathConstants<float>::twoPi) subPhase -= juce::MathConstants<float>::twoPi;
        if (detunePhase >= juce::MathConstants<float>::twoPi) detunePhase -= juce::MathConstants<float>::twoPi;

        const float normInc = phaseInc / juce::MathConstants<float>::twoPi;
        const float subNormInc = subPhaseInc / juce::MathConstants<float>::twoPi;
        const float detuneNormInc = detunePhaseInc / juce::MathConstants<float>::twoPi;

        float primary = renderMorphSample(phase, waveMorph, normInc);
        float subSample = renderMorphSample(subPhase, waveMorph, subNormInc);
        float detuneSample = renderMorphSample(detunePhase, waveMorph, detuneNormInc);
        float stacked = juce::jlimit(-1.0f, 1.0f,
            primary * 0.55f + subSample * 0.35f + detuneSample * 0.35f);
        float combined = juce::jmap(subMixAmt, primary, stacked);
        float s = combined * gain;

        if (drive > 0.0f)
        {
            float preGain = 1.5f + drive * 9.0f;
            float softClip = std::tanh(s * preGain);
            float evenHarmonics = std::tanh((s * preGain) * 0.6f) * 0.8f;
            float shaped = juce::jlimit(-1.0f, 1.0f, 0.65f * softClip + 0.35f * evenHarmonics);
            s = juce::jmap(drive, 0.0f, 1.0f, s, shaped);
        }

        if (++filterUpdateCount >= filterUpdateStep)
        {
            filterUpdateCount = 0;
            const double modFactor = std::pow(2.0, (double)lfoCutModAmt * (double)lfoS);
            const double envFactor = juce::jlimit(0.1, 4.0, 1.0 + (double)envFilterAmt * (double)ampEnv);
            const double effCut = juce::jlimit(80.0, 14000.0, (double)baseCutoff * modFactor * envFactor);
            updateFilterCoeffs(effCut, (double)baseResonance);
        }

        float fL = filterL.processSingleSampleRaw(s);
        float fR = (r ? filterR.processSingleSampleRaw(s) : fL);

        if (crushAmt > 0.0f)
        {
            if (crushCounter <= 0)
            {
                const int downsampleFactor = juce::jmax(1, (int)std::round(juce::jmap(crushAmt, 0.0f, 1.0f, 1.0f, 32.0f)));
                crushCounter = downsampleFactor;
                crushHoldL = fL;
                crushHoldR = fR;
            }

            float levels = juce::jmap(crushAmt, 0.0f, 1.0f, 2048.0f, 6.0f);
            float crushedL = std::round(crushHoldL * levels) / levels;
            float crushedR = std::round(crushHoldR * levels) / levels;
            fL = juce::jmap(crushAmt, 0.0f, 1.0f, fL, crushedL);
            fR = juce::jmap(crushAmt, 0.0f, 1.0f, fR, crushedR);
            --crushCounter;
        }
        else
        {
            crushCounter = 0;
        }

        fL *= ampEnv;
        fR *= ampEnv;

        float panMod = autoPanAmt * std::sin(autoPanPhase);
        autoPanPhase += autoPanInc;
        if (autoPanPhase >= juce::MathConstants<float>::twoPi) autoPanPhase -= juce::MathConstants<float>::twoPi;

        float dynamicWidth = width * juce::jlimit(0.0f, 3.0f, 1.0f + panMod);
        float mid = 0.5f * (fL + fR);
        float side = 0.5f * (fL - fR) * dynamicWidth;

        float dryL = mid + side;
        float dryR = r ? (mid - side) : dryL;

        float wetL = 0.0f;
        float wetR = 0.0f;
        if (delayAmtLocal > 0.0f && maxDelaySamples > 1)
        {
            const int readPos = (delayWritePosition - delaySamples + maxDelaySamples) % maxDelaySamples;
            wetL = delayBuffer.getSample(0, readPos);
            wetR = delayBuffer.getNumChannels() > 1 ? delayBuffer.getSample(1, readPos) : wetL;

            delayBuffer.setSample(0, delayWritePosition, dryL + wetL * delayFeedback);
            delayBuffer.setSample(1, delayWritePosition, dryR + wetR * delayFeedback);
            delayWritePosition = (delayWritePosition + 1) % maxDelaySamples;

            dryL = dryL * (1.0f - delayMix) + wetL * delayMix;
            dryR = dryR * (1.0f - delayMix) + wetR * delayMix;
        }
        else if (maxDelaySamples > 1)
        {
            delayBuffer.setSample(0, delayWritePosition, dryL);
            delayBuffer.setSample(1, delayWritePosition, dryR);
            delayWritePosition = (delayWritePosition + 1) % maxDelaySamples;
        }

        if (glitchProbLocal > 0.0f)
        {
            if (glitchSamplesRemaining > 0)
            {
                --glitchSamplesRemaining;
                dryL = glitchHeldL;
                dryR = glitchHeldR;
            }
            else if (random.nextFloat() < glitchProbLocal * 0.01f)
            {
                glitchSamplesRemaining = juce::jmax(4, (int)std::round(juce::jmap(glitchProbLocal, 0.0f, 1.0f,
                    12.0f,
                    (float)currentSR * 0.08f)));
                glitchHeldL = dryL;
                glitchHeldR = dryR;
            }
        }
        else
        {
            glitchSamplesRemaining = 0;
        }

        const float mono = r ? 0.5f * (dryL + dryR) : dryL;
        blockPeak = juce::jmax(blockPeak, std::abs(mono));

        lowState += 0.04f * (mono - lowState);
        const float highPass = mono - lowState;
        midState += 0.08f * (highPass - midState);
        const float highComponent = highPass - midState;

        lowAccum += std::abs(lowState);
        midAccum += std::abs(midState);
        highAccum += std::abs(highComponent);

        if (glitchSamplesRemaining > 0)
            glitchActivity = 1.0f;

        l[i] = dryL;
        if (r) r[i] = dryR;

        scopeBuffer.setSample(0, scopeWritePos, l[i]);
        scopeWritePos = (scopeWritePos + 1) % scopeBuffer.getNumSamples();
    }

    lowBandState = lowState;
    midBandState = midState;

    const float invSamples = bufferToFill.numSamples > 0 ? 1.0f / (float)bufferToFill.numSamples : 0.0f;
    const float lowAvg = juce::jlimit(0.0f, 1.5f, lowAccum * invSamples);
    const float midAvg = juce::jlimit(0.0f, 1.5f, midAccum * invSamples);
    const float highAvg = juce::jlimit(0.0f, 1.5f, highAccum * invSamples);
    const float peak = juce::jlimit(0.0f, 1.2f, blockPeak);
    const float delayEnergy = juce::jlimit(0.0f, 1.0f, juce::jmap(delayFeedback, 0.05f, 0.88f, 0.0f, 1.0f));

    auto smoothValue = [](float current, float target, float attack, float release)
    {
        const float coeff = target > current ? attack : release;
        return current + (target - current) * coeff;
    };

    meterSmoother = smoothValue(meterSmoother, peak, 0.45f, 0.08f);
    lowBandSmoother = smoothValue(lowBandSmoother, lowAvg, 0.3f, 0.05f);
    midBandSmoother = smoothValue(midBandSmoother, midAvg, 0.25f, 0.05f);
    highBandSmoother = smoothValue(highBandSmoother, highAvg, 0.2f, 0.04f);
    delayVisualSmoother = smoothValue(delayVisualSmoother, delayEnergy, 0.2f, 0.06f);
    glitchVisualSmoother = smoothValue(glitchVisualSmoother, glitchActivity, 0.35f, 0.08f);

    smoothedLevel.store(meterSmoother);
    lowBandLevel.store(lowBandSmoother);
    midBandLevel.store(midBandSmoother);
    highBandLevel.store(highBandSmoother);
    delayFeedbackVisual.store(delayVisualSmoother);
    glitchHoldVisual.store(glitchVisualSmoother);
}

void MainComponent::releaseResources()
{
    filterL.reset();
    filterR.reset();
    amplitudeEnvelope.reset();
}

int MainComponent::findZeroCrossingIndex(int searchSpan) const
{
    const int N = scopeBuffer.getNumSamples();
    int idx = (scopeWritePos - searchSpan + N) % N;

    float prev = scopeBuffer.getSample(0, idx);
    for (int s = 1; s < searchSpan; ++s)
    {
        const int i = (idx + s) % N;
        const float cur = scopeBuffer.getSample(0, i);
        if (prev < 0.0f && cur >= 0.0f)
            return i;
        prev = cur;
    }
    return (scopeWritePos + 1) % N;
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    if (!osc3DRect.isEmpty())
    {
        auto visualBounds = osc3DRect.toFloat();
        juce::ColourGradient background(
            juce::Colour::fromRGB(8, 10, 22), visualBounds.getBottomLeft(),
            juce::Colour::fromRGB(18, 32, 60), visualBounds.getTopRight(), false);
        g.setGradientFill(background);
        g.fillRoundedRectangle(visualBounds, 20.0f);

        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(visualBounds, 20.0f, 1.2f);

        auto sphereBounds = visualBounds.reduced(28.0f, 24.0f);
        const float diameter = juce::jmin(sphereBounds.getWidth(), sphereBounds.getHeight());

        if (diameter > 8.0f)
        {
            juce::Rectangle<float> sphereArea(
                sphereBounds.getCentreX() - diameter * 0.5f,
                sphereBounds.getCentreY() - diameter * 0.5f,
                diameter,
                diameter);

            const float driveNorm = juce::jlimit(0.0f, 1.0f, driveAmount);
            const float delayNorm = juce::jlimit(0.0f, 1.0f, delayAmount);
            const float chaosNorm = juce::jlimit(0.0f, 1.0f, chaosAmount);

            const float level = juce::jlimit(0.0f, 1.0f, smoothedLevel.load());
            const float lowBand = juce::jlimit(0.0f, 1.2f, lowBandLevel.load());
            const float midBand = juce::jlimit(0.0f, 1.2f, midBandLevel.load());
            const float highBand = juce::jlimit(0.0f, 1.2f, highBandLevel.load());
            const float delayFeedbackEnergy = juce::jlimit(0.0f, 1.0f, delayFeedbackVisual.load());
            const float glitchEnergy = juce::jlimit(0.0f, 1.0f, glitchHoldVisual.load());

            const float hueBase = std::fmod(juce::jmap(driveNorm, 0.0f, 1.0f, 0.62f, 0.02f) + 1.0f, 1.0f);
            const float brightness = juce::jlimit(0.2f, 1.0f, juce::jmap(delayNorm, 0.0f, 1.0f, 0.35f, 0.92f) + level * 0.12f);
            const float saturation = juce::jlimit(0.25f, 1.0f, juce::jmap(chaosNorm, 0.0f, 1.0f, 0.55f, 0.95f) + highBand * 0.05f);

            juce::ColourGradient sphereGradient(
                juce::Colour::fromHSV(hueBase, saturation, juce::jlimit(0.2f, 1.0f, brightness + level * 0.18f), 1.0f), sphereArea.getCentre(),
                juce::Colour::fromHSV(std::fmod(hueBase + 0.11f, 1.0f), juce::jlimit(0.25f, 1.0f, saturation * 0.65f + midBand * 0.25f),
                    juce::jlimit(0.15f, 1.0f, 0.25f + brightness * 0.65f), 1.0f),
                sphereArea.getBottomRight(), true);
            sphereGradient.addColour(0.18f, juce::Colour::fromHSV(std::fmod(hueBase + 0.18f, 1.0f),
                juce::jlimit(0.3f, 1.0f, saturation * 0.85f + highBand * 0.25f),
                juce::jlimit(0.2f, 1.0f, 0.3f + brightness * 0.55f + level * 0.12f), 1.0f));
            sphereGradient.addColour(0.78f, juce::Colour::fromHSV(std::fmod(hueBase + 0.32f, 1.0f),
                juce::jlimit(0.2f, 1.0f, saturation * 0.5f + delayFeedbackEnergy * 0.35f),
                juce::jlimit(0.1f, 1.0f, 0.18f + brightness * 0.45f), 1.0f));

            g.setGradientFill(sphereGradient);
            g.fillEllipse(sphereArea);

            g.setColour(juce::Colour::fromHSV(std::fmod(hueBase + 0.02f, 1.0f),
                juce::jlimit(0.25f, 1.0f, saturation * 0.55f + midBand * 0.2f),
                juce::jlimit(0.15f, 1.0f, 0.4f + brightness * 0.35f + level * 0.2f),
                juce::jlimit(0.1f, 0.6f, 0.22f + level * 0.25f)));
            g.drawEllipse(sphereArea, juce::jlimit(0.8f, 1.8f, 1.0f + highBand * 0.6f));

            const auto centre = sphereArea.getCentre();
            const float outerRadius = sphereArea.getWidth() * 0.5f;
            const float activeRadius = outerRadius * juce::jlimit(0.75f, 1.22f, 0.88f + level * 0.35f);
            const float innerRadius = juce::jlimit(outerRadius * 0.18f, activeRadius * 0.92f,
                outerRadius * juce::jmap(juce::jlimit(0.0f, 1.0f, lowBand), 0.0f, 1.0f, 0.3f, 0.48f));

            g.setColour(juce::Colour::fromHSV(std::fmod(hueBase + 0.07f, 1.0f),
                juce::jlimit(0.2f, 1.0f, saturation * 0.45f + midBand * 0.3f),
                juce::jlimit(0.1f, 0.6f, 0.18f + brightness * 0.25f + lowBand * 0.1f),
                juce::jlimit(0.05f, 0.35f, 0.12f + level * 0.15f)));
            g.drawEllipse(sphereArea.reduced(outerRadius * 0.18f), juce::jlimit(0.6f, 1.4f, 0.8f + midBand * 0.4f));

            if (!waveformSnapshot.empty())
            {
                juce::Path waveformPath;
                const size_t count = waveformSnapshot.size();
                const double timeNow = juce::Time::getMillisecondCounterHiRes() * 0.001;

                for (size_t i = 0; i < count; ++i)
                {
                    const float angle = juce::MathConstants<float>::twoPi * (float)i / (float)count;
                    const float sample = juce::jlimit(-1.0f, 1.0f, waveformSnapshot[i]);
                    const float breathing = std::sin(angle * 2.0f + (float)timeNow * 0.9f) * midBand * outerRadius * 0.05f;
                    const float jitter = std::sin(angle * 5.0f + (float)timeNow * 3.0f) * highBand * outerRadius * (0.03f + glitchEnergy * 0.04f);
                    const float warpedRadius = juce::jmap(sample, -1.0f, 1.0f, innerRadius, activeRadius) + breathing + jitter;
                    const float radius = juce::jlimit(innerRadius * 0.7f, outerRadius * 1.2f, warpedRadius);
                    const float x = centre.x + std::cos(angle) * radius;
                    const float y = centre.y + std::sin(angle) * radius;

                    if (i == 0)
                        waveformPath.startNewSubPath(x, y);
                    else
                        waveformPath.lineTo(x, y);
                }

                waveformPath.closeSubPath();

                const float trailHue = std::fmod(hueBase + 0.18f + highBand * 0.05f, 1.0f);
                const float trailSat = juce::jlimit(0.25f, 1.0f, saturation * 0.7f + midBand * 0.4f + glitchEnergy * 0.2f);
                const float trailVal = juce::jlimit(0.25f, 1.0f, 0.3f + brightness * 0.7f + level * 0.25f);

                g.setColour(juce::Colour::fromHSV(trailHue, trailSat, trailVal, juce::jlimit(0.15f, 0.85f, 0.25f + level * 0.5f + lowBand * 0.15f)));
                g.fillPath(waveformPath);

                g.setColour(juce::Colour::fromHSV(std::fmod(trailHue + 0.02f, 1.0f),
                    juce::jlimit(0.2f, 1.0f, trailSat * 0.85f + highBand * 0.25f),
                    juce::jlimit(0.3f, 1.0f, trailVal * 0.85f + highBand * 0.2f), 1.0f));
                g.strokePath(waveformPath, juce::PathStrokeType(juce::jlimit(1.1f, 3.6f, 1.3f + highBand * 2.0f + glitchEnergy * 0.7f)));

                const float orbitRadius = outerRadius * juce::jlimit(1.05f, 1.6f, 1.2f + delayFeedbackEnergy * 0.45f);
                const float orbitAngle = (float)(timeNow * 0.6);
                const float orbitSkew = juce::jlimit(0.7f, 1.25f, 0.92f + midBand * 0.25f);
                juce::Point<float> sat1(
                    centre.x + std::sin(orbitAngle) * orbitRadius,
                    centre.y + std::cos(orbitAngle) * orbitRadius * orbitSkew);

                juce::Colour sat1Colour = juce::Colour::fromHSV(std::fmod(hueBase + 0.05f, 1.0f),
                    juce::jlimit(0.35f, 1.0f, 0.65f + delayFeedbackEnergy * 0.35f),
                    juce::jlimit(0.25f, 1.0f, 0.45f + brightness * 0.45f + level * 0.25f),
                    juce::jlimit(0.15f, 0.9f, 0.28f + delayFeedbackEnergy * 0.55f));

                g.setColour(sat1Colour.withAlpha(juce::jlimit(0.1f, 0.9f, sat1Colour.getFloatAlpha())));
                g.fillEllipse(juce::Rectangle<float>(10.0f, 10.0f).withCentre(sat1));

                juce::Path feedbackLink;
                feedbackLink.startNewSubPath(centre);
                feedbackLink.lineTo(sat1);
                g.setColour(sat1Colour.withAlpha(juce::jlimit(0.08f, 0.6f, 0.2f + delayFeedbackEnergy * 0.45f)));
                g.strokePath(feedbackLink, juce::PathStrokeType(1.2f));

                const float glitchOrbit = outerRadius * juce::jlimit(1.2f, 1.85f, 1.35f + highBand * 0.35f + glitchEnergy * 0.2f);
                const float glitchAngle = (float)(timeNow * 1.05);
                const float jitterAmount = glitchEnergy * outerRadius * 0.16f;
                juce::Point<float> sat2(
                    centre.x + std::cos(glitchAngle) * glitchOrbit + std::sin((float)timeNow * 7.0f) * jitterAmount,
                    centre.y + std::sin(glitchAngle) * glitchOrbit + std::cos((float)timeNow * 5.0f) * jitterAmount);

                juce::Colour sat2Colour = juce::Colour::fromHSV(std::fmod(hueBase + 0.22f, 1.0f),
                    juce::jlimit(0.35f, 1.0f, 0.75f + glitchEnergy * 0.3f + highBand * 0.25f),
                    juce::jlimit(0.3f, 1.0f, 0.35f + brightness * 0.55f + highBand * 0.25f),
                    juce::jlimit(0.15f, 0.95f, 0.22f + glitchEnergy * 0.65f));

                g.setColour(sat2Colour.withAlpha(juce::jlimit(0.12f, 0.95f, sat2Colour.getFloatAlpha())));
                g.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre(sat2));

                juce::Path glitchLink;
                glitchLink.startNewSubPath(centre);
                glitchLink.lineTo(sat2);
                g.setColour(sat2Colour.withAlpha(juce::jlimit(0.08f, 0.7f, 0.18f + glitchEnergy * 0.6f)));
                g.strokePath(glitchLink, juce::PathStrokeType(1.0f));
            }
        }
    }

    if (!scopeRect.isEmpty())
    {
        auto drawRect = scopeRect;
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(drawRect.toFloat(), 8.0f, 1.0f);

        g.setColour(juce::Colours::white.withAlpha(0.85f));
        juce::Path p;

        const int start = findZeroCrossingIndex(scopeBuffer.getNumSamples() / 2);
        const int W = drawRect.getWidth();
        const int N = scopeBuffer.getNumSamples();
        const float H = (float)drawRect.getHeight();
        const float Y0 = (float)drawRect.getY();
        const int X0 = drawRect.getX();

        for (int x = 0; x < W; ++x)
        {
            const int i = (start + x) % N;
            const float s = scopeBuffer.getSample(0, i);
            const float y = juce::jmap(s, -1.0f, 1.0f, Y0 + H, Y0);
            if (x == 0) p.startNewSubPath((float)X0, y);
            else p.lineTo((float)(X0 + x), y);
        }
        g.strokePath(p, juce::PathStrokeType(2.f));
    }
}

void MainComponent::timerCallback()
{
    captureWaveformSnapshot();
    repaint();
}

void MainComponent::captureWaveformSnapshot()
{
    const int numSamples = scopeBuffer.getNumSamples();
    if (numSamples <= 0)
        return;

    const int resolution = 160;
    const int start = findZeroCrossingIndex(numSamples / 2);
    const int step = juce::jmax(1, numSamples / resolution);

    std::vector<float> snapshot;
    snapshot.reserve((size_t)resolution);

    for (int i = 0; i < resolution; ++i)
    {
        const int idx = (start + i * step) % numSamples;
        snapshot.push_back(scopeBuffer.getSample(0, idx));
    }

    waveformSnapshot = std::move(snapshot);
}

// ✅ FINAL DEFINITIVE FIX FOR ALL JUCE VERSIONS ✅
void MainComponent::resized()
{
    // Enforce survival layout — prevents overlap
    if (getWidth() < minWidth || getHeight() < minHeight)
        setSize(std::max(getWidth(), minWidth), std::max(getHeight(), minHeight));

    auto area = getLocalBounds().reduced(headerMargin);

    auto bar = area.removeFromTop(headerBarHeight);

    const int audioX = bar.getRight() - audioButtonWidth;
    const int audioY = bar.getY() + (bar.getHeight() - audioButtonHeight) / 2;
    audioToggle.setBounds(audioX, audioY, audioButtonWidth, audioButtonHeight);

    int buttonX = bar.getX();
    buttonX += headerMargin;
    const int buttonY = bar.getY() + (bar.getHeight() - toolbarButtonHeight) / 2;
    const int rightLimit = audioX - toolbarSpacing;

    auto placeButton = [buttonY, rightLimit](juce::Component& component, int width, int& x)
    {
        const int available = rightLimit - x;
        const int clampedWidth = available > 0 ? std::min(width, available) : 0;
        component.setBounds(x, buttonY, clampedWidth, toolbarButtonHeight);
        x += clampedWidth + toolbarSpacing;
    };

    placeButton(playButton, toolbarButtonWidth, buttonX);
    placeButton(stopButton, toolbarButtonWidth, buttonX);
    placeButton(restartButton, toolbarButtonWidth, buttonX);
    placeButton(importButton, toolbarButtonWidth, buttonX);
    placeButton(exportButton, toolbarButtonWidth, buttonX);

    const int bpmAvailable = rightLimit - buttonX;
    const int bpmWidth = bpmAvailable > 0 ? std::min(bpmLabelWidth, bpmAvailable) : 0;
    bpmLabel.setBounds(buttonX, buttonY, bpmWidth, toolbarButtonHeight);

    auto strip = area.removeFromTop(controlStripHeight);
    const int knob = knobSize;
    const int numKnobs = 23;
    const int colWidth = strip.getWidth() / numKnobs;

    struct Item { juce::Label* L; juce::Slider* S; juce::Label* V; };
    Item items[numKnobs] = {
        { &waveLabel, &waveKnob, &waveValue },
        { &gainLabel, &gainKnob, &gainValue },
        { &attackLabel, &attackKnob, &attackValue },
        { &decayLabel, &decayKnob, &decayValue },
        { &sustainLabel, &sustainKnob, &sustainValue },
        { &widthLabel, &widthKnob, &widthValue },
        { &pitchLabel, &pitchKnob, &pitchValue },
        { &cutoffLabel, &cutoffKnob, &cutoffValue },
        { &resonanceLabel, &resonanceKnob, &resonanceValue },
        { &releaseLabel, &releaseKnob, &releaseValue },
        { &lfoLabel, &lfoKnob, &lfoValue },
        { &lfoDepthLabel, &lfoDepthKnob, &lfoDepthValue },
        { &filterModLabel, &filterModKnob, &filterModValue },
        { &lfoModeLabel, &lfoModeKnob, &lfoModeValue },
        { &lfoStartLabel, &lfoStartKnob, &lfoStartValue },
        { &driveLabel, &driveKnob, &driveValue },
        { &crushLabel, &crushKnob, &crushValue },
        { &subMixLabel, &subMixKnob, &subMixValue },
        { &envFilterLabel, &envFilterKnob, &envFilterValue },
        { &chaosLabel, &chaosKnob, &chaosValueLabel },
        { &delayLabel, &delayKnob, &delayValue },
        { &autoPanLabel, &autoPanKnob, &autoPanValue },
        { &glitchLabel, &glitchKnob, &glitchValue }
    };

    const int labelH = 14;
    const int valueH = 14;
    const int labelY = strip.getY();
    const int knobY = labelY + labelH + 2;
    const int valueY = knobY + knob + 2;

    for (int i = 0; i < numKnobs; ++i)
    {
        const int x = strip.getX() + i * colWidth + (colWidth - knob) / 2;
        items[i].L->setBounds(x, labelY, knob, labelH);
        items[i].S->setBounds(x, knobY, knob, knob);
        items[i].V->setBounds(x, valueY, knob, valueH);
    }

    if (midiRoll)
{
    auto rollHeight = 200;
    auto rollArea = area.removeFromBottom(rollHeight);
    midiRoll->setBounds(rollArea);
}

    
    int kbH = std::max(keyboardMinHeight, area.getHeight() / 5);
    auto kbArea = area.removeFromBottom(kbH);
    
    
    keyboardComponent.setBounds(kbArea);

    float keyW = juce::jlimit(16.0f, 40.0f, kbArea.getWidth() / 20.0f);
    keyboardComponent.setKeyWidth(keyW);

    int desiredScopeHeight = kbArea.getHeight();
    int availableHeight = area.getHeight();
    int scopeHeight = std::min(desiredScopeHeight, availableHeight);
    scopeHeight = std::max(scopeHeight, std::min(availableHeight, 80));
    const int minVisualHeight = 180;
    if (availableHeight - scopeHeight < minVisualHeight)
        scopeHeight = std::max(std::min(availableHeight, 80), availableHeight - minVisualHeight);
    scopeHeight = std::max(0, std::min(scopeHeight, availableHeight));

    juce::Rectangle<int> scopeArea;
    if (scopeHeight > 0)
        scopeArea = area.removeFromBottom(scopeHeight);
    else
        scopeArea = {};

    scopeRect = scopeArea.isEmpty() ? juce::Rectangle<int>() : scopeArea.reduced(8, 6);
    osc3DRect = area.reduced(12, 12);
    
    if (midiRoll)
    midiRoll->setBounds(getLocalBounds().removeFromBottom(200));
}




void MainComponent::initialiseUi()
{
    initialiseToolbar();
    initialiseSliders();
    initialiseToggle();
}

void MainComponent::initialiseToolbar()
{
    auto configureButton = [this](juce::TextButton& button)
    {
        button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        button.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.9f));
        button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
       

        button.setWantsKeyboardFocus(false);
        button.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible(button);
    };

    auto updatePlayLabel = [this]()
    {
        const bool playing = midiRoll && midiRoll->isCurrentlyPlaying();
        playButton.setButtonText(playing ? "Pause" : "Play");
    };

    configureButton(playButton);
    configureButton(stopButton);
    configureButton(restartButton);
    configureButton(importButton);
    configureButton(exportButton);

    playButton.onClick = [this, updatePlayLabel]()
    {
        if (midiRoll)
            midiRoll->togglePlayback();

        updatePlayLabel();
    };

    stopButton.onClick = [this, updatePlayLabel]()
    {
        if (midiRoll)
            midiRoll->stopPlayback();

        updatePlayLabel();
    };

    restartButton.onClick = [this, updatePlayLabel]()
    {
        if (midiRoll)
        {
            midiRoll->stopPlayback();
            midiRoll->startPlayback();
        }

        updatePlayLabel();
    };

    importButton.onClick = []
    {
        juce::Logger::outputDebugString("MIDI import requested");
    };

    exportButton.onClick = []
    {
        juce::Logger::outputDebugString("MIDI export requested");
    };

    bpmLabel.setText(juce::String(defaultBpmDisplay) + " BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType(juce::Justification::centred);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    bpmLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));


    bpmLabel.setBorderSize(juce::BorderSize<int>());
    addAndMakeVisible(bpmLabel);

    updatePlayLabel();
}

void MainComponent::initialiseSliders()
{
    configureRotarySlider(waveKnob);
    waveKnob.setRange(0.0, 1.0);
    waveKnob.setValue(0.0);
    addAndMakeVisible(waveKnob);
    configureCaptionLabel(waveLabel, "Waveform");
    configureValueLabel(waveValue);
    waveKnob.onValueChange = [this]
    {
        waveMorph = (float)waveKnob.getValue();
        waveValue.setText(juce::String(waveMorph, 2), juce::dontSendNotification);
    };
    waveKnob.onValueChange();

    configureRotarySlider(gainKnob);
    gainKnob.setRange(0.0, 1.0);
    gainKnob.setValue(outputGain);
    addAndMakeVisible(gainKnob);
    configureCaptionLabel(gainLabel, "Gain");
    configureValueLabel(gainValue);
    gainKnob.onValueChange = [this]
    {
        outputGain = (float)gainKnob.getValue();
        gainSmoothed.setTargetValue(outputGain);
        gainValue.setText(juce::String(outputGain * 100.0f, 0) + "%", juce::dontSendNotification);
    };
    gainKnob.onValueChange();

    configureRotarySlider(attackKnob);
    attackKnob.setRange(0.0, 2000.0, 1.0);
    attackKnob.setSkewFactorFromMidPoint(40.0);
    attackKnob.setValue(attackMs);
    addAndMakeVisible(attackKnob);
    configureCaptionLabel(attackLabel, "Attack");
    configureValueLabel(attackValue);
    attackKnob.onValueChange = [this]
    {
        attackMs = (float)attackKnob.getValue();
        attackValue.setText(juce::String(attackMs, 0) + " ms", juce::dontSendNotification);
        updateAmplitudeEnvelope();
    };
    attackKnob.onValueChange();

    configureRotarySlider(decayKnob);
    decayKnob.setRange(5.0, 4000.0, 1.0);
    decayKnob.setSkewFactorFromMidPoint(200.0);
    decayKnob.setValue(decayMs);
    addAndMakeVisible(decayKnob);
    configureCaptionLabel(decayLabel, "Decay");
    configureValueLabel(decayValue);
    decayKnob.onValueChange = [this]
    {
        decayMs = (float)decayKnob.getValue();
        decayValue.setText(juce::String(decayMs, 0) + " ms", juce::dontSendNotification);
        updateAmplitudeEnvelope();
    };
    decayKnob.onValueChange();

    configureRotarySlider(sustainKnob);
    sustainKnob.setRange(0.0, 1.0, 0.01);
    sustainKnob.setValue(sustainLevel);
    addAndMakeVisible(sustainKnob);
    configureCaptionLabel(sustainLabel, "Sustain");
    configureValueLabel(sustainValue);
    sustainKnob.onValueChange = [this]
    {
        sustainLevel = (float)sustainKnob.getValue();
        sustainValue.setText(juce::String(sustainLevel * 100.0f, 0) + "%", juce::dontSendNotification);
        updateAmplitudeEnvelope();
    };
    sustainKnob.onValueChange();

    configureRotarySlider(widthKnob);
    widthKnob.setRange(0.0, 2.0, 0.01);
    widthKnob.setValue(stereoWidth);
    addAndMakeVisible(widthKnob);
    configureCaptionLabel(widthLabel, "Width");
    configureValueLabel(widthValue);
    widthKnob.onValueChange = [this]
    {
        stereoWidth = (float)widthKnob.getValue();
        stereoWidthSmoothed.setTargetValue(stereoWidth);
        widthValue.setText(juce::String(stereoWidth, 2) + "x", juce::dontSendNotification);
    };
    widthKnob.onValueChange();

    configureRotarySlider(pitchKnob);
    pitchKnob.setRange(40.0, 5000.0);
    pitchKnob.setSkewFactorFromMidPoint(440.0);
    pitchKnob.setValue(220.0);
    addAndMakeVisible(pitchKnob);
    configureCaptionLabel(pitchLabel, "Pitch");
    configureValueLabel(pitchValue);
    pitchKnob.onValueChange = [this]
    {
        setTargetFrequency((float)pitchKnob.getValue());
        pitchValue.setText(juce::String(targetFrequency, 1) + " Hz", juce::dontSendNotification);
    };
    pitchKnob.onValueChange();

    configureRotarySlider(cutoffKnob);
    cutoffKnob.setRange(80.0, 10000.0, 1.0);
    cutoffKnob.setSkewFactorFromMidPoint(1000.0);
    cutoffKnob.setValue(cutoffHz);
    addAndMakeVisible(cutoffKnob);
    configureCaptionLabel(cutoffLabel, "Cutoff");
    configureValueLabel(cutoffValue);
    cutoffKnob.onValueChange = [this]
    {
        cutoffHz = (float)cutoffKnob.getValue();
        cutoffSmoothed.setTargetValue(cutoffHz);
        cutoffValue.setText(juce::String(cutoffHz, 1) + " Hz", juce::dontSendNotification);
        filterUpdateCount = filterUpdateStep;
    };
    cutoffKnob.onValueChange();

    configureRotarySlider(resonanceKnob);
    resonanceKnob.setRange(0.1, 10.0, 0.01);
    resonanceKnob.setSkewFactorFromMidPoint(0.707);
    resonanceKnob.setValue(resonanceQ);
    addAndMakeVisible(resonanceKnob);
    configureCaptionLabel(resonanceLabel, "Resonance (Q)");
    configureValueLabel(resonanceValue);
    resonanceKnob.onValueChange = [this]
    {
        resonanceQ = (float)resonanceKnob.getValue();
        if (resonanceQ < 0.1f) resonanceQ = 0.1f;
        resonanceSmoothed.setTargetValue(resonanceQ);
        resonanceValue.setText(juce::String(resonanceQ, 2), juce::dontSendNotification);
        filterUpdateCount = filterUpdateStep;
    };
    resonanceKnob.onValueChange();

    configureRotarySlider(releaseKnob);
    releaseKnob.setRange(1.0, 4000.0, 1.0);
    releaseKnob.setSkewFactorFromMidPoint(200.0);
    releaseKnob.setValue(releaseMs);
    addAndMakeVisible(releaseKnob);
    configureCaptionLabel(releaseLabel, "Release");
    configureValueLabel(releaseValue);
    releaseKnob.onValueChange = [this]
    {
        releaseMs = (float)releaseKnob.getValue();
        releaseValue.setText(juce::String(releaseMs, 0) + " ms", juce::dontSendNotification);
        updateAmplitudeEnvelope();
    };
    releaseKnob.onValueChange();

    configureRotarySlider(lfoKnob);
    lfoKnob.setRange(0.05, 15.0);
    lfoKnob.setValue(lfoRateHz);
    addAndMakeVisible(lfoKnob);
    configureCaptionLabel(lfoLabel, "LFO Rate");
    configureValueLabel(lfoValue);
    lfoKnob.onValueChange = [this]
    {
        lfoRateHz = (float)lfoKnob.getValue();
        lfoValue.setText(juce::String(lfoRateHz, 2) + " Hz", juce::dontSendNotification);
    };
    lfoKnob.onValueChange();

    configureRotarySlider(lfoDepthKnob);
    lfoDepthKnob.setRange(0.0, 1.0);
    lfoDepthKnob.setValue(lfoDepth);
    addAndMakeVisible(lfoDepthKnob);
    configureCaptionLabel(lfoDepthLabel, "LFO Depth");
    configureValueLabel(lfoDepthValue);
    lfoDepthKnob.onValueChange = [this]
    {
        lfoDepth = (float)lfoDepthKnob.getValue();
        lfoDepthSmoothed.setTargetValue(lfoDepth);
        lfoDepthValue.setText(juce::String(lfoDepth, 2), juce::dontSendNotification);
    };
    lfoDepthKnob.onValueChange();

    configureRotarySlider(filterModKnob);
    filterModKnob.setRange(0.0, 1.0, 0.001);
    filterModKnob.setValue(lfoCutModAmt);
    addAndMakeVisible(filterModKnob);
    configureCaptionLabel(filterModLabel, "Filter Mod");
    configureValueLabel(filterModValue);
    filterModKnob.onValueChange = [this]
    {
        lfoCutModAmt = (float)filterModKnob.getValue();
        filterModValue.setText(juce::String(lfoCutModAmt, 2), juce::dontSendNotification);
    };
    filterModKnob.onValueChange();

    configureRotarySlider(lfoModeKnob);
    lfoModeKnob.setRange(0.0, 1.0, 1.0);
    lfoModeKnob.setValue((lfoTriggerMode == LfoTriggerMode::FreeRun) ? 1.0 : 0.0);
    addAndMakeVisible(lfoModeKnob);
    configureCaptionLabel(lfoModeLabel, "LFO Mode");
    configureValueLabel(lfoModeValue);
    lfoModeKnob.onValueChange = [this]
    {
        const bool freeRun = juce::approximatelyEqual(lfoModeKnob.getValue(), 1.0);
        lfoTriggerMode = freeRun ? LfoTriggerMode::FreeRun : LfoTriggerMode::Retrigger;
        lfoModeValue.setText(freeRun ? "Loop" : "Retrig", juce::dontSendNotification);
        if (!freeRun)
            triggerLfo();
    };
    lfoModeKnob.onValueChange();

    configureRotarySlider(lfoStartKnob);
    lfoStartKnob.setRange(0.0, 1.0, 0.001);
    lfoStartKnob.setValue(lfoStartPhaseNormalized);
    addAndMakeVisible(lfoStartKnob);
    configureCaptionLabel(lfoStartLabel, "LFO Start");
    configureValueLabel(lfoStartValue);
    lfoStartKnob.onValueChange = [this]
    {
        lfoStartPhaseNormalized = (float)lfoStartKnob.getValue();
        const int degrees = juce::roundToInt(lfoStartPhaseNormalized * 360.0);
        lfoStartValue.setText(juce::String(degrees) + juce::String::charToString(0x00B0), juce::dontSendNotification);
        triggerLfo();
    };
    lfoStartKnob.onValueChange();

    configureRotarySlider(driveKnob);
    driveKnob.setRange(0.0, 1.0);
    driveKnob.setValue(driveAmount);
    addAndMakeVisible(driveKnob);
    configureCaptionLabel(driveLabel, "Drive");
    configureValueLabel(driveValue);
    driveKnob.onValueChange = [this]
    {
        driveAmount = (float)driveKnob.getValue();
        driveSmoothed.setTargetValue(driveAmount);
        driveValue.setText(juce::String(driveAmount, 2), juce::dontSendNotification);
    };
    driveKnob.onValueChange();

    configureRotarySlider(crushKnob);
    crushKnob.setRange(0.0, 1.0);
    crushKnob.setValue(crushAmount);
    addAndMakeVisible(crushKnob);
    configureCaptionLabel(crushLabel, "Crush");
    configureValueLabel(crushValue);
    crushKnob.onValueChange = [this]
    {
        crushAmount = (float)crushKnob.getValue();
        crushValue.setText(juce::String(crushAmount * 100.0f, 0) + "%", juce::dontSendNotification);
    };
    crushKnob.onValueChange();

    configureRotarySlider(subMixKnob);
    subMixKnob.setRange(0.0, 1.0);
    subMixKnob.setValue(subMixAmount);
    addAndMakeVisible(subMixKnob);
    configureCaptionLabel(subMixLabel, "Sub Mix");
    configureValueLabel(subMixValue);
    subMixKnob.onValueChange = [this]
    {
        subMixAmount = (float)subMixKnob.getValue();
        subMixValue.setText(juce::String(subMixAmount * 100.0f, 0) + "%", juce::dontSendNotification);
    };
    subMixKnob.onValueChange();

    configureRotarySlider(envFilterKnob);
    envFilterKnob.setRange(-1.0, 1.0, 0.01);
    envFilterKnob.setValue(envFilterAmount);
    addAndMakeVisible(envFilterKnob);
    configureCaptionLabel(envFilterLabel, "Env->Filter");
    configureValueLabel(envFilterValue);
    envFilterKnob.onValueChange = [this]
    {
        envFilterAmount = (float)envFilterKnob.getValue();
        envFilterValue.setText(juce::String(envFilterAmount, 2), juce::dontSendNotification);
    };
    envFilterKnob.onValueChange();

    configureRotarySlider(chaosKnob);
    chaosKnob.setRange(0.0, 1.0);
    chaosKnob.setValue(chaosAmount);
    addAndMakeVisible(chaosKnob);
    configureCaptionLabel(chaosLabel, "Chaos");
    configureValueLabel(chaosValueLabel);
    chaosKnob.onValueChange = [this]
    {
        chaosAmount = (float)chaosKnob.getValue();
        chaosValueLabel.setText(juce::String(chaosAmount * 100.0f, 0) + "%", juce::dontSendNotification);
    };
    chaosKnob.onValueChange();

    configureRotarySlider(delayKnob);
    delayKnob.setRange(0.0, 1.0);
    delayKnob.setValue(delayAmount);
    addAndMakeVisible(delayKnob);
    configureCaptionLabel(delayLabel, "Delay");
    configureValueLabel(delayValue);
    delayKnob.onValueChange = [this]
    {
        delayAmount = (float)delayKnob.getValue();
        delayValue.setText(juce::String(delayAmount * 100.0f, 0) + "%", juce::dontSendNotification);
    };
    delayKnob.onValueChange();

    configureRotarySlider(autoPanKnob);
    autoPanKnob.setRange(0.0, 1.0);
    autoPanKnob.setValue(autoPanAmount);
    addAndMakeVisible(autoPanKnob);
    configureCaptionLabel(autoPanLabel, "Auto-Pan");
    configureValueLabel(autoPanValue);
    autoPanKnob.onValueChange = [this]
    {
        autoPanAmount = (float)autoPanKnob.getValue();
        autoPanValue.setText(juce::String(autoPanAmount * 100.0f, 0) + "%", juce::dontSendNotification);
    };
    autoPanKnob.onValueChange();

    configureRotarySlider(glitchKnob);
    glitchKnob.setRange(0.0, 1.0);
    glitchKnob.setValue(glitchProbability);
    addAndMakeVisible(glitchKnob);
    configureCaptionLabel(glitchLabel, "Glitch");
    configureValueLabel(glitchValue);
    glitchKnob.onValueChange = [this]
    {
        glitchProbability = (float)glitchKnob.getValue();
        glitchValue.setText(juce::String(glitchProbability * 100.0f, 0) + "%", juce::dontSendNotification);
    };
    glitchKnob.onValueChange();
}

void MainComponent::initialiseToggle()
{
    audioToggle.setClickingTogglesState(true);
    audioToggle.setToggleState(true, juce::dontSendNotification);
    audioToggle.onClick = [this]
    {
        audioEnabled = audioToggle.getToggleState();
        audioToggle.setButtonText(audioEnabled ? "Audio ON" : "Audio OFF");
        if (!audioEnabled)
        {
            midiGate = false;
            amplitudeEnvelope.noteOff();
        }
    };
    audioToggle.setButtonText("Audio ON");
    addAndMakeVisible(audioToggle);
}

void MainComponent::initialiseMidiInputs()
{
    auto devices = juce::MidiInput::getAvailableDevices();
    for (auto& d : devices)
    {
        deviceManager.setMidiInputDeviceEnabled(d.identifier, true);
        deviceManager.addMidiInputDeviceCallback(d.identifier, this);
    }
}

void MainComponent::initialiseKeyboard()
{
    addAndMakeVisible(keyboardComponent);
    keyboardState.addListener(this);
    keyboardComponent.setMidiChannel(1);
    keyboardComponent.setAvailableRange(0, 127);

    keyboardComponent.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xFF2A2A2A));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xFF0E0E0E));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colours::black.withAlpha(0.6f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colours::white.withAlpha(0.08f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colours::white.withAlpha(0.12f));
}

void MainComponent::configureRotarySlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
        juce::MathConstants<float>::pi * 2.8f, true);
}

void MainComponent::configureCaptionLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(label);
}

void MainComponent::configureValueLabel(juce::Label& label)
{
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(label);
}

void MainComponent::updateAmplitudeEnvelope()
{
    ampEnvParams.attack = juce::jlimit(0.0005f, 20.0f, attackMs * 0.001f);
    ampEnvParams.decay = juce::jlimit(0.0005f, 20.0f, decayMs * 0.001f);
    ampEnvParams.sustain = juce::jlimit(0.0f, 1.0f, sustainLevel);
    ampEnvParams.release = juce::jlimit(0.0005f, 20.0f, releaseMs * 0.001f);
    amplitudeEnvelope.setParameters(ampEnvParams);
}

void MainComponent::triggerLfo()
{
    if (lfoTriggerMode == LfoTriggerMode::Retrigger)
    {
        const float wrapped = juce::jlimit(0.0f, 1.0f, lfoStartPhaseNormalized);
        lfoPhase = juce::MathConstants<float>::twoPi * wrapped;
        while (lfoPhase >= juce::MathConstants<float>::twoPi)
            lfoPhase -= juce::MathConstants<float>::twoPi;
        if (lfoPhase < 0.0f)
            lfoPhase += juce::MathConstants<float>::twoPi;
    }
}

//==============================================================================
// MIDI Input handlers stay unchanged
void MainComponent::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const auto noteNumber = m.getNoteNumber();
        noteStack.addIfNotAlreadyThere(noteNumber);
        currentMidiNote = noteNumber;
        currentVelocity = juce::jlimit(0.0f, 1.0f, m.getVelocity() / 127.0f);
        setTargetFrequency(midiNoteToFreq(currentMidiNote));
        midiGate = true;
        amplitudeEnvelope.noteOn();
        triggerLfo();
    }
    else if (m.isNoteOff())
    {
        noteStack.removeFirstMatchingValue(m.getNoteNumber());
        if (noteStack.isEmpty())
        {
            midiGate = false;
            currentMidiNote = -1;
            amplitudeEnvelope.noteOff();
        }
        else
        {
            currentMidiNote = noteStack.getLast();
            setTargetFrequency(midiNoteToFreq(currentMidiNote));
            midiGate = true;
            amplitudeEnvelope.noteOn();
            triggerLfo();
        }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        noteStack.clear();
        midiGate = false;
        currentMidiNote = -1;
        amplitudeEnvelope.noteOff();
    }
}

void MainComponent::handleNoteOn(juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity)
{
    noteStack.addIfNotAlreadyThere(midiNoteNumber);
    currentMidiNote = midiNoteNumber;
    currentVelocity = juce::jlimit(0.0f, 1.0f, velocity);
    setTargetFrequency(midiNoteToFreq(currentMidiNote));
    midiGate = true;
    amplitudeEnvelope.noteOn();
    triggerLfo();
}

void MainComponent::handleNoteOff(juce::MidiKeyboardState*, int, int midiNoteNumber, float)
{
    noteStack.removeFirstMatchingValue(midiNoteNumber);
    if (noteStack.isEmpty())
    {
        midiGate = false;
        currentMidiNote = -1;
        amplitudeEnvelope.noteOff();
    }
    else
    {
        currentMidiNote = noteStack.getLast();
        setTargetFrequency(midiNoteToFreq(currentMidiNote));
        midiGate = true;
        amplitudeEnvelope.noteOn();
        triggerLfo();
    }
}
