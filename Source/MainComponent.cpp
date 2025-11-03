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
    constexpr int controlStripHeight = 128;
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
    waveHistory.clear();

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
    mutationDriveOffset.setCurrentAndTargetValue(0.0f);
    mutationCutoffOffset.setCurrentAndTargetValue(0.0f);
    mutationChaosOffset.setCurrentAndTargetValue(0.0f);
    mutationGlitchOffset.setCurrentAndTargetValue(0.0f);
    mutationMorphOffset.setCurrentAndTargetValue(0.0f);
    bioSenseSmoothed.setCurrentAndTargetValue(bioSenseAmount);

    initialiseUi();
    initialiseMidiInputs();
    initialiseKeyboard();

    triggerMutationTarget(true, true);
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
    waveHistory.clear();
    resetSmoothers(sampleRate);
    updateFilterStatic();
    amplitudeEnvelope.setSampleRate(sampleRate);
    updateAmplitudeEnvelope();
    amplitudeEnvelope.reset();

    maxDelaySamples = juce::jmax(1, (int)std::ceil(sampleRate * 2.0));
    delayBuffer.setSize(2, maxDelaySamples);
    delayBuffer.clear();
    delayWritePosition = 0;

    mutationSamplesUntilNext = 0;
    mutationIdle = true;
    triggerMutationTarget(true, true);
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
    const double mutationRampSeconds = 0.18;
    const double bioSenseRampSeconds = 0.25;

    frequencySmoothed.reset(sampleRate, fastRampSeconds);
    gainSmoothed.reset(sampleRate, fastRampSeconds);
    cutoffSmoothed.reset(sampleRate, filterRampSeconds);
    resonanceSmoothed.reset(sampleRate, filterRampSeconds);
    stereoWidthSmoothed.reset(sampleRate, spatialRampSeconds);
    lfoDepthSmoothed.reset(sampleRate, spatialRampSeconds);
    driveSmoothed.reset(sampleRate, fastRampSeconds);
    mutationDriveOffset.reset(sampleRate, mutationRampSeconds);
    mutationCutoffOffset.reset(sampleRate, mutationRampSeconds);
    mutationChaosOffset.reset(sampleRate, mutationRampSeconds);
    mutationGlitchOffset.reset(sampleRate, mutationRampSeconds);
    mutationMorphOffset.reset(sampleRate, mutationRampSeconds);
    bioSenseSmoothed.reset(sampleRate, bioSenseRampSeconds);

    frequencySmoothed.setCurrentAndTargetValue(targetFrequency);
    gainSmoothed.setCurrentAndTargetValue(outputGain);
    cutoffSmoothed.setCurrentAndTargetValue(cutoffHz);
    resonanceSmoothed.setCurrentAndTargetValue(resonanceQ);
    stereoWidthSmoothed.setCurrentAndTargetValue(stereoWidth);
    lfoDepthSmoothed.setCurrentAndTargetValue(lfoDepth);
    driveSmoothed.setCurrentAndTargetValue(driveAmount);
    mutationDriveOffset.setCurrentAndTargetValue(0.0f);
    mutationCutoffOffset.setCurrentAndTargetValue(0.0f);
    mutationChaosOffset.setCurrentAndTargetValue(0.0f);
    mutationGlitchOffset.setCurrentAndTargetValue(0.0f);
    mutationMorphOffset.setCurrentAndTargetValue(0.0f);
    bioSenseSmoothed.setCurrentAndTargetValue(bioSenseAmount);

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

inline float MainComponent::renderMorphSample(float ph, float morph) const
{
    while (ph >= juce::MathConstants<float>::twoPi) ph -= juce::MathConstants<float>::twoPi;
    if (ph < 0.0f) ph += juce::MathConstants<float>::twoPi;

    const float m = juce::jlimit(0.0f, 1.0f, morph);
    const float seg = 1.0f / 3.0f;

    if (m < seg)
        return juce::jmap(m / seg, sine(ph), tri(ph));
    else if (m < 2.0f * seg)
        return juce::jmap((m - seg) / seg, tri(ph), saw(ph));
    else
        return std::tanh(juce::jmap((m - 2.0f * seg) / seg, saw(ph), sqr(ph)));
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
    const float delayControlBase = juce::jlimit(0.0f, 1.0f, delayAmount);
    const float autoOrbitBase = juce::jlimit(0.0f, 1.0f, autoPanAmount);
    const float chaosBase = juce::jlimit(0.0f, 1.0f, chaosAmount);
    const float glitchBase = juce::jlimit(0.0f, 1.0f, glitchProbability);
    const int delaySamples = (maxDelaySamples > 1)
        ? juce::jlimit(1, maxDelaySamples - 1,
            (int)std::round(juce::jmap((double)delayControlBase, 0.0, 1.0,
                currentSR * 0.03,
                juce::jmin(currentSR * 1.25, (double)maxDelaySamples - 1.0))))
        : 1;

    const bool mutationShouldRun = (mutationDepth > 0.0f && mutationRateHz > 0.0f);
    if (!mutationShouldRun && !mutationIdle)
    {
        triggerMutationTarget(true);
        mutationIdle = true;
        mutationSamplesUntilNext = 0;
    }

    for (int i = 0; i < bufferToFill.numSamples; ++i)
    {
        if (!audioEnabled && amplitudeEnvelope.isActive())
            amplitudeEnvelope.noteOff();

        if (mutationShouldRun)
        {
            if (mutationSamplesUntilNext <= 0)
            {
                triggerMutationTarget(false);
                const double baseSamples = currentSR / juce::jmax(0.01f, mutationRateHz);
                const double jitter = juce::jmap(random.nextFloat(), 0.0f, 1.0f, 0.6, 1.4);
                mutationSamplesUntilNext = juce::jmax(1, (int)std::round(baseSamples * jitter));
                mutationIdle = false;
            }
            --mutationSamplesUntilNext;
        }

        const float baseFrequency = frequencySmoothed.getNextValue();
        const float gain = gainSmoothed.getNextValue() * currentVelocity;
        const float depth = lfoDepthSmoothed.getNextValue();
        const float width = stereoWidthSmoothed.getNextValue();
        const float baseCutoff = cutoffSmoothed.getNextValue();
        const float baseResonance = resonanceSmoothed.getNextValue();
        const float ampEnv = amplitudeEnvelope.getNextSample();
        const float baseDrive = driveSmoothed.getNextValue();
        const float mutDrive = mutationDriveOffset.getNextValue();
        const float mutCutoff = mutationCutoffOffset.getNextValue();
        const float mutChaos = mutationChaosOffset.getNextValue();
        const float mutGlitch = mutationGlitchOffset.getNextValue();
        const float mutMorph = mutationMorphOffset.getNextValue();
        const float bioSense = juce::jlimit(0.0f, 1.0f, bioSenseSmoothed.getNextValue());

        const float drive = juce::jlimit(0.0f, 1.0f, baseDrive + mutDrive);
        const float chaosAmt = juce::jlimit(0.0f, 1.0f, chaosBase + mutChaos);
        const float glitchProbLocal = juce::jlimit(0.0f, 1.0f, glitchBase + mutGlitch + bioSense * 0.45f);
        const float autoOrbitAmt = juce::jlimit(0.0f, 1.0f, autoOrbitBase + bioSense * 0.55f);
        const float delayControl = juce::jlimit(0.0f, 1.0f, delayControlBase + bioSense * 0.4f);
        const float delayMix = juce::jmap(delayControl, 0.0f, 1.0f, 0.0f, 0.72f);
        const float delayFeedback = juce::jmap(delayControl, 0.0f, 1.0f, 0.05f, 0.93f);
        const float mutatedMorph = juce::jlimit(0.0f, 1.0f, waveMorph + mutMorph);
        const float mutatedCutoff = juce::jlimit(80.0f, 16000.0f, baseCutoff * juce::jlimit(0.35f, 2.2f, 1.0f + mutCutoff));

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
            chaosScale = juce::jlimit(0.5f, 1.5f, 1.0f + chaosValue * chaosAmt * 0.12f);
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

        float subPhaseInc = phaseInc * 0.5f;
        float detunePhaseInc = phaseInc * 1.01f;
        subPhase += subPhaseInc;
        detunePhase += detunePhaseInc;
        if (subPhase >= juce::MathConstants<float>::twoPi) subPhase -= juce::MathConstants<float>::twoPi;
        if (detunePhase >= juce::MathConstants<float>::twoPi) detunePhase -= juce::MathConstants<float>::twoPi;

        float primary = renderMorphSample(phase, mutatedMorph);
        float subSample = renderMorphSample(subPhase, mutatedMorph);
        float detuneSample = renderMorphSample(detunePhase, mutatedMorph);
        float combined = juce::jmap(subMixAmt, primary, 0.5f * (primary + subSample + detuneSample));
        float s = combined * gain;

        if (drive > 0.0f)
        {
            float shaped = std::tanh(s * (1.0f + drive * 10.0f));
            s = juce::jmap(drive, 0.0f, 1.0f, s, shaped);
        }

        if (++filterUpdateCount >= filterUpdateStep)
        {
            filterUpdateCount = 0;
            const double modFactor = std::pow(2.0, (double)lfoCutModAmt * (double)lfoS);
            const double envFactor = juce::jlimit(0.1, 4.0, 1.0 + (double)envFilterAmt * (double)ampEnv);
            const double effCut = juce::jlimit(80.0, 16000.0, (double)mutatedCutoff * modFactor * envFactor);
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

        float panMod = autoOrbitAmt * std::sin(autoPanPhase);
        autoPanPhase += autoPanInc;
        if (autoPanPhase >= juce::MathConstants<float>::twoPi) autoPanPhase -= juce::MathConstants<float>::twoPi;

        float dynamicWidth = width * juce::jlimit(0.0f, 3.0f, 1.0f + panMod);
        float mid = 0.5f * (fL + fR);
        float side = 0.5f * (fL - fR) * dynamicWidth;

        float dryL = mid + side;
        float dryR = r ? (mid - side) : dryL;

        float wetL = 0.0f;
        float wetR = 0.0f;
        if (delayControl > 0.0f && maxDelaySamples > 1)
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
            else if (random.nextFloat() < glitchProbLocal * 0.004f)
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

        l[i] = dryL;
        if (r) r[i] = dryR;

        scopeBuffer.setSample(0, scopeWritePos, l[i]);
        scopeWritePos = (scopeWritePos + 1) % scopeBuffer.getNumSamples();
    }
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
            juce::Colour::fromRGB(6, 10, 24), visualBounds.getBottomLeft(),
            juce::Colour::fromRGB(20, 34, 68), visualBounds.getTopRight(), false);
        g.setGradientFill(background);
        g.fillRoundedRectangle(visualBounds, 20.0f);

        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawRoundedRectangle(visualBounds, 20.0f, 1.5f);

        auto plotArea = osc3DRect.reduced(24, 20);
        if (plotArea.getWidth() > 8 && plotArea.getHeight() > 8)
        {
            const float baseWidth = (float)plotArea.getWidth();
            const float baseHeight = (float)plotArea.getHeight();
            const float centreX = (float)plotArea.getCentreX();
            const float baseBottom = (float)plotArea.getBottom();

            auto projectPoint = [&](float xNorm, float yNorm, float depthNorm)
            {
                const float clampedDepth = juce::jlimit(0.0f, 1.0f, depthNorm);
                const float scale = juce::jmap(1.0f - clampedDepth, 0.55f, 1.0f);
                const float skew = clampedDepth * baseWidth * 0.18f;
                const float px = centreX + (xNorm - 0.5f) * baseWidth * scale - skew;
                const float py = baseBottom - clampedDepth * baseHeight * 0.75f - yNorm * baseHeight * scale;
                return juce::Point<float>(px, py);
            };

            juce::Path floor;
            floor.startNewSubPath(projectPoint(0.0f, 1.0f, 0.0f));
            floor.lineTo(projectPoint(1.0f, 1.0f, 0.0f));
            floor.lineTo(projectPoint(1.0f, 1.0f, 1.0f));
            floor.lineTo(projectPoint(0.0f, 1.0f, 1.0f));
            floor.closeSubPath();
            g.setColour(juce::Colours::white.withAlpha(0.03f));
            g.fillPath(floor);

            g.setColour(juce::Colours::white.withAlpha(0.05f));
            const int depthLines = 6;
            for (int d = 0; d <= depthLines; ++d)
            {
                const float depth = (float)d / (float)depthLines;
                auto p1 = projectPoint(0.0f, 0.0f, depth);
                auto p2 = projectPoint(1.0f, 0.0f, depth);
                g.drawLine({ p1, p2 }, 1.0f);
            }

            const int verticalLines = 8;
            for (int v = 0; v <= verticalLines; ++v)
            {
                const float x = (float)v / (float)verticalLines;
                auto p1 = projectPoint(x, 0.0f, 0.0f);
                auto p2 = projectPoint(x, 1.0f, 1.0f);
                g.drawLine({ p1, p2 }, 1.0f);
            }

            if (!waveHistory.empty())
            {
                const size_t historyToDraw = juce::jmin<size_t>(waveHistory.size(), 90);
                for (size_t layer = 0; layer < historyToDraw; ++layer)
                {
                    const auto& samples = waveHistory[layer];
                    if (samples.size() < 2)
                        continue;

                    const float depth = (float)layer / (float)historyToDraw;
                    juce::Path wavePath;

                    for (size_t i = 0; i < samples.size(); ++i)
                    {
                        const float xNorm = samples.size() > 1 ? (float)i / (float)(samples.size() - 1) : 0.0f;
                        const float yNorm = juce::jmap(samples[i], -1.0f, 1.0f, 1.0f, 0.0f);
                        auto projected = projectPoint(xNorm, yNorm, depth);
                        if (i == 0)
                            wavePath.startNewSubPath(projected);
                        else
                            wavePath.lineTo(projected);
                    }

                    const float intensity = juce::jmap(1.0f - depth, 0.18f, 0.9f);
                    juce::Colour colour = juce::Colour::fromFloatRGBA(0.2f, 0.85f, 1.0f, intensity);
                    g.setColour(colour);
                    g.strokePath(wavePath, juce::PathStrokeType(1.6f));
                }

                // Highlight the most recent layer for clarity.
                const auto& latest = waveHistory.front();
                if (latest.size() > 1)
                {
                    juce::Path latestPath;
                    for (size_t i = 0; i < latest.size(); ++i)
                    {
                        const float xNorm = (float)i / (float)(latest.size() - 1);
                        const float yNorm = juce::jmap(latest[i], -1.0f, 1.0f, 1.0f, 0.0f);
                        auto projected = projectPoint(xNorm, yNorm, 0.0f);
                        if (i == 0)
                            latestPath.startNewSubPath(projected);
                        else
                            latestPath.lineTo(projected);
                    }
                    g.setColour(juce::Colour::fromFloatRGBA(0.7f, 1.0f, 1.0f, 0.95f));
                    g.strokePath(latestPath, juce::PathStrokeType(2.4f));
                }
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

    auto revision = mutationRevision.load(std::memory_order_acquire);
    if (revision != mutationRevisionSeen)
    {
        mutationRevisionSeen = revision;

        const bool active = mutationActiveFlag.load(std::memory_order_acquire) != 0;
        juce::String statusText;

        if (!active)
        {
            statusText = "Mutation // dormant";
        }
        else
        {
            auto formatDelta = [](const juce::String& label, float value)
            {
                const float scaled = value * 100.0f;
                juce::String s(label);
                s << " " << (scaled >= 0.0f ? "+" : "") << juce::String(scaled, 1) << "%";
                return s;
            };

            juce::StringArray parts;
            parts.add(formatDelta("drive", mutationDrivePreview.load(std::memory_order_relaxed)));
            parts.add(formatDelta("filter", mutationCutoffPreview.load(std::memory_order_relaxed)));
            parts.add(formatDelta("chaos", mutationChaosPreview.load(std::memory_order_relaxed)));
            parts.add(formatDelta("glitch", mutationGlitchPreview.load(std::memory_order_relaxed)));
            parts.add(formatDelta("morph", mutationMorphPreview.load(std::memory_order_relaxed)));

            statusText = "Mutation // " + parts.joinIntoString(" • ");
        }

        mutationStatusLabel.setText(statusText, juce::dontSendNotification);
    }

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

    waveHistory.push_front(std::move(snapshot));
    const size_t maxHistory = 90;
    if (waveHistory.size() > maxHistory)
        waveHistory.pop_back();
}

void MainComponent::triggerMutationTarget(bool neutralise, bool immediate)
{
    auto setValue = [immediate](juce::SmoothedValue<float>& smooth, float value)
    {
        if (immediate)
            smooth.setCurrentAndTargetValue(value);
        else
            smooth.setTargetValue(value);
    };

    float driveTarget = 0.0f;
    float cutoffTarget = 0.0f;
    float chaosTarget = 0.0f;
    float glitchTarget = 0.0f;
    float morphTarget = 0.0f;

    if (!neutralise)
    {
        const float depth = juce::jlimit(0.0f, 1.0f, mutationDepth);
        auto range = [this](float scale)
        {
            return (random.nextFloat() * 2.0f - 1.0f) * scale;
        };

        driveTarget = range(depth * 0.55f);
        cutoffTarget = range(depth * 0.85f);
        chaosTarget = range(depth * 0.65f);
        glitchTarget = range(depth * 0.6f);
        morphTarget = range(depth * 0.9f);
    }

    setValue(mutationDriveOffset, driveTarget);
    setValue(mutationCutoffOffset, cutoffTarget);
    setValue(mutationChaosOffset, chaosTarget);
    setValue(mutationGlitchOffset, glitchTarget);
    setValue(mutationMorphOffset, morphTarget);

    mutationDrivePreview.store(driveTarget, std::memory_order_relaxed);
    mutationCutoffPreview.store(cutoffTarget, std::memory_order_relaxed);
    mutationChaosPreview.store(chaosTarget, std::memory_order_relaxed);
    mutationGlitchPreview.store(glitchTarget, std::memory_order_relaxed);
    mutationMorphPreview.store(morphTarget, std::memory_order_relaxed);

    mutationActiveFlag.store(neutralise ? 0 : 1, std::memory_order_release);
    mutationIdle = neutralise;
    mutationRevision.store(++mutationRevisionCounter, std::memory_order_release);
}

// ✅ FINAL DEFINITIVE FIX FOR ALL JUCE VERSIONS ✅
void MainComponent::resized()
{
    // Enforce survival layout — prevents overlap
    if (getWidth() < minWidth || getHeight() < minHeight)
        setSize(std::max(getWidth(), minWidth), std::max(getHeight(), minHeight));

    auto area = getLocalBounds().reduced(headerMargin);

    auto bar = area.removeFromTop(headerBarHeight);
    audioToggle.setBounds(bar.getRight() - audioButtonWidth, bar.getY() + 4, audioButtonWidth, audioButtonHeight);
    auto statusArea = bar;
    statusArea.removeFromRight(audioButtonWidth + 12);
    mutationStatusLabel.setBounds(statusArea);

    auto strip = area.removeFromTop(controlStripHeight);
    const int knob = knobSize;
    const int numKnobs = 24;
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
        { &driveLabel, &driveKnob, &driveValue },
        { &crushLabel, &crushKnob, &crushValue },
        { &subMixLabel, &subMixKnob, &subMixValue },
        { &envFilterLabel, &envFilterKnob, &envFilterValue },
        { &chaosLabel, &chaosKnob, &chaosValueLabel },
        { &delayLabel, &delayKnob, &delayValue },
        { &autoPanLabel, &autoPanKnob, &autoPanValue },
        { &glitchLabel, &glitchKnob, &glitchValue },
        { &mutationRateLabel, &mutationRateKnob, &mutationRateValue },
        { &mutationDepthLabel, &mutationDepthKnob, &mutationDepthValue },
        { &bioSenseLabel, &bioSenseKnob, &bioSenseValue }
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
}

void MainComponent::initialiseUi()
{
    mutationStatusLabel.setJustificationType(juce::Justification::centredLeft);
    mutationStatusLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    mutationStatusLabel.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 14.0f, juce::Font::bold));
    mutationStatusLabel.setText("Mutation // dormant", juce::dontSendNotification);
    addAndMakeVisible(mutationStatusLabel);

    initialiseSliders();
    initialiseToggle();
}

void MainComponent::initialiseSliders()
{
    configureRotarySlider(waveKnob);
    waveKnob.setRange(0.0, 1.0);
    waveKnob.setValue(0.0);
    addAndMakeVisible(waveKnob);
    configureCaptionLabel(waveLabel, "Morph DNA");
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
    configureCaptionLabel(gainLabel, "Output Flux");
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
    configureCaptionLabel(attackLabel, "Rise");
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
    configureCaptionLabel(decayLabel, "Fall");
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
    configureCaptionLabel(sustainLabel, "Hold");
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
    configureCaptionLabel(widthLabel, "Spread");
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
    configureCaptionLabel(pitchLabel, "Fundamental");
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
    configureCaptionLabel(cutoffLabel, "Filter Veil");
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
    configureCaptionLabel(resonanceLabel, "Resonance Spine");
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
    configureCaptionLabel(lfoLabel, "Orbit Rate");
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
    configureCaptionLabel(lfoDepthLabel, "Orbit Depth");
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
    configureCaptionLabel(filterModLabel, "Filter Warp");
    configureValueLabel(filterModValue);
    filterModKnob.onValueChange = [this]
    {
        lfoCutModAmt = (float)filterModKnob.getValue();
        filterModValue.setText(juce::String(lfoCutModAmt, 2), juce::dontSendNotification);
    };
    filterModKnob.onValueChange();

    configureRotarySlider(driveKnob);
    driveKnob.setRange(0.0, 1.0);
    driveKnob.setValue(driveAmount);
    addAndMakeVisible(driveKnob);
    configureCaptionLabel(driveLabel, "Drive Ritual");
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
    configureCaptionLabel(crushLabel, "Texture Crush");
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
    configureCaptionLabel(subMixLabel, "Sub Merge");
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
    configureCaptionLabel(envFilterLabel, "Env Warp");
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
    configureCaptionLabel(delayLabel, "Echo Drift");
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
    configureCaptionLabel(autoPanLabel, "Auto-Orbit");
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
    configureCaptionLabel(glitchLabel, "Glitch Pulse");
    configureValueLabel(glitchValue);
    glitchKnob.onValueChange = [this]
    {
        glitchProbability = (float)glitchKnob.getValue();
        glitchValue.setText(juce::String(glitchProbability * 100.0f, 0) + "%", juce::dontSendNotification);
    };
    glitchKnob.onValueChange();

    configureRotarySlider(mutationRateKnob);
    mutationRateKnob.setRange(0.0, 2.5);
    mutationRateKnob.setSkewFactorFromMidPoint(0.45);
    mutationRateKnob.setValue(mutationRateHz);
    addAndMakeVisible(mutationRateKnob);
    configureCaptionLabel(mutationRateLabel, "Mutation Pulse");
    configureValueLabel(mutationRateValue);
    mutationRateKnob.onValueChange = [this]
    {
        mutationRateHz = (float)mutationRateKnob.getValue();
        mutationRateValue.setText(juce::String(mutationRateHz, 2) + " Hz", juce::dontSendNotification);
    };
    mutationRateKnob.onValueChange();

    configureRotarySlider(mutationDepthKnob);
    mutationDepthKnob.setRange(0.0, 1.0);
    mutationDepthKnob.setValue(mutationDepth);
    addAndMakeVisible(mutationDepthKnob);
    configureCaptionLabel(mutationDepthLabel, "Mutation Depth");
    configureValueLabel(mutationDepthValue);
    mutationDepthKnob.onValueChange = [this]
    {
        mutationDepth = (float)mutationDepthKnob.getValue();
        mutationDepthValue.setText(juce::String(mutationDepth * 100.0f, 0) + "%", juce::dontSendNotification);
    };
    mutationDepthKnob.onValueChange();

    configureRotarySlider(bioSenseKnob);
    bioSenseKnob.setRange(0.0, 1.0);
    bioSenseKnob.setValue(bioSenseAmount);
    addAndMakeVisible(bioSenseKnob);
    configureCaptionLabel(bioSenseLabel, "Bio-Sense");
    configureValueLabel(bioSenseValue);
    bioSenseKnob.onValueChange = [this]
    {
        bioSenseAmount = (float)bioSenseKnob.getValue();
        bioSenseSmoothed.setTargetValue(bioSenseAmount);
        bioSenseValue.setText(juce::String(bioSenseAmount * 100.0f, 0) + "%", juce::dontSendNotification);
    };
    bioSenseKnob.onValueChange();
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
    }
}
