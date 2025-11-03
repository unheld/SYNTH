#include "SynthEngine.h"

#include <cmath>

using namespace synth;

SynthEngine::SynthEngine()
{
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
}

void SynthEngine::prepare(double sampleRate, int)
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

void SynthEngine::renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (buffer.getNumChannels() == 0 || numSamples <= 0)
        return;

    buffer.clear(startSample, numSamples);

    auto* l = buffer.getWritePointer(0, startSample);
    auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, startSample) : nullptr;

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

    for (int i = 0; i < numSamples; ++i)
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

        l[i] = dryL;
        if (r) r[i] = dryR;

        scopeBuffer.setSample(0, scopeWritePos, l[i]);
        scopeWritePos = (scopeWritePos + 1) % scopeBuffer.getNumSamples();
    }
}

void SynthEngine::release()
{
    filterL.reset();
    filterR.reset();
    amplitudeEnvelope.reset();
}

void SynthEngine::setAudioEnabled(bool enabled)
{
    audioEnabled = enabled;
}

void SynthEngine::noteOn(int midiNoteNumber, float velocity)
{
    noteStack.addIfNotAlreadyThere(midiNoteNumber);
    currentMidiNote = midiNoteNumber;
    currentVelocity = juce::jlimit(0.0f, 1.0f, velocity);
    setTargetFrequency(midiNoteToFreq(currentMidiNote));
    midiGate = true;
    amplitudeEnvelope.noteOn();
    triggerLfo();
}

void SynthEngine::noteOff(int midiNoteNumber)
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

void SynthEngine::allNotesOff()
{
    noteStack.clear();
    midiGate = false;
    currentMidiNote = -1;
    amplitudeEnvelope.noteOff();
}

void SynthEngine::handleMidiMessage(const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const auto noteNumber = m.getNoteNumber();
        noteOn(noteNumber, m.getVelocity() / 127.0f);
    }
    else if (m.isNoteOff())
    {
        noteOff(m.getNoteNumber());
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        allNotesOff();
    }
}

void SynthEngine::setWaveMorph(float value)
{
    waveMorph = juce::jlimit(0.0f, 1.0f, value);
}

void SynthEngine::setOutputGain(float value)
{
    outputGain = juce::jlimit(0.0f, 1.0f, value);
    gainSmoothed.setTargetValue(outputGain);
}

void SynthEngine::setAttack(float milliseconds)
{
    attackMs = juce::jmax(0.0f, milliseconds);
    updateAmplitudeEnvelope();
}

void SynthEngine::setDecay(float milliseconds)
{
    decayMs = juce::jmax(0.0f, milliseconds);
    updateAmplitudeEnvelope();
}

void SynthEngine::setSustain(float level)
{
    sustainLevel = juce::jlimit(0.0f, 1.0f, level);
    updateAmplitudeEnvelope();
}

void SynthEngine::setRelease(float milliseconds)
{
    releaseMs = juce::jmax(0.0f, milliseconds);
    updateAmplitudeEnvelope();
}

void SynthEngine::setStereoWidth(float width)
{
    stereoWidth = juce::jlimit(0.0f, 2.0f, width);
    stereoWidthSmoothed.setTargetValue(stereoWidth);
}

void SynthEngine::setTargetFrequency(float frequency, bool force)
{
    targetFrequency = juce::jlimit(config::minFrequency, config::maxFrequency, frequency);
    if (force)
        frequencySmoothed.setCurrentAndTargetValue(targetFrequency);
    else
        frequencySmoothed.setTargetValue(targetFrequency);
}

void SynthEngine::setCutoff(float cutoff)
{
    cutoffHz = juce::jlimit(config::minCutoff, config::maxCutoff, cutoff);
    cutoffSmoothed.setTargetValue(cutoffHz);
    filterUpdateCount = filterUpdateStep;
}

void SynthEngine::setResonance(float resonance)
{
    resonanceQ = juce::jlimit(config::minResonance, config::maxResonance, resonance);
    resonanceSmoothed.setTargetValue(resonanceQ);
    filterUpdateCount = filterUpdateStep;
}

void SynthEngine::setLfoRate(float rateHz)
{
    lfoRateHz = juce::jlimit(0.01f, 20.0f, rateHz);
}

void SynthEngine::setLfoDepth(float depth)
{
    lfoDepth = juce::jlimit(0.0f, 1.0f, depth);
    lfoDepthSmoothed.setTargetValue(lfoDepth);
}

void SynthEngine::setFilterMod(float amount)
{
    lfoCutModAmt = juce::jlimit(0.0f, 1.0f, amount);
}

void SynthEngine::setLfoMode(bool freeRun)
{
    lfoTriggerMode = freeRun ? LfoTriggerMode::FreeRun : LfoTriggerMode::Retrigger;
    if (!freeRun)
        triggerLfo();
}

void SynthEngine::setLfoStart(float normalizedPhase)
{
    lfoStartPhaseNormalized = juce::jlimit(0.0f, 1.0f, normalizedPhase);
    triggerLfo();
}

void SynthEngine::setDrive(float amount)
{
    driveAmount = juce::jlimit(0.0f, 1.0f, amount);
    driveSmoothed.setTargetValue(driveAmount);
}

void SynthEngine::setCrush(float amount)
{
    crushAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void SynthEngine::setSubMix(float amount)
{
    subMixAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void SynthEngine::setEnvelopeFilter(float amount)
{
    envFilterAmount = juce::jlimit(-1.0f, 1.0f, amount);
}

void SynthEngine::setChaos(float amount)
{
    chaosAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void SynthEngine::setDelay(float amount)
{
    delayAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void SynthEngine::setAutoPan(float amount)
{
    autoPanAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void SynthEngine::setGlitch(float amount)
{
    glitchProbability = juce::jlimit(0.0f, 1.0f, amount);
}

void SynthEngine::resetSmoothers(double sampleRate)
{
    frequencySmoothed.reset(sampleRate, config::fastRampSeconds);
    gainSmoothed.reset(sampleRate, config::fastRampSeconds);
    cutoffSmoothed.reset(sampleRate, config::filterRampSeconds);
    resonanceSmoothed.reset(sampleRate, config::filterRampSeconds);
    stereoWidthSmoothed.reset(sampleRate, config::spatialRampSeconds);
    lfoDepthSmoothed.reset(sampleRate, config::spatialRampSeconds);
    driveSmoothed.reset(sampleRate, config::fastRampSeconds);

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

void SynthEngine::updateFilterCoeffs(double cutoff, double Q)
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

void SynthEngine::updateFilterStatic()
{
    updateFilterCoeffs(cutoffHz, resonanceQ);
}

void SynthEngine::updateAmplitudeEnvelope()
{
    ampEnvParams.attack = juce::jlimit(0.0005f, 20.0f, attackMs * 0.001f);
    ampEnvParams.decay = juce::jlimit(0.0005f, 20.0f, decayMs * 0.001f);
    ampEnvParams.sustain = juce::jlimit(0.0f, 1.0f, sustainLevel);
    ampEnvParams.release = juce::jlimit(0.0005f, 20.0f, releaseMs * 0.001f);
    amplitudeEnvelope.setParameters(ampEnvParams);
}

void SynthEngine::triggerLfo()
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

float SynthEngine::renderMorphSample(float ph, float morph, float normPhaseInc) const
{
    while (ph >= juce::MathConstants<float>::twoPi) ph -= juce::MathConstants<float>::twoPi;
    if (ph < 0.0f) ph += juce::MathConstants<float>::twoPi;

    const float m = juce::jlimit(0.0f, 1.0f, morph);
    const float seg = 1.0f / 3.0f;

    const float sineSample = sine(ph);
    const float triSample = tri(ph);

    const float dt = juce::jlimit(1.0e-5f, 0.5f, normPhaseInc);
    float t = ph / juce::MathConstants<float>::twoPi;
    t -= std::floor(t);

    float sawSample = 2.0f * t - 1.0f;
    sawSample -= polyBlep(t, dt);
    sawSample = juce::jlimit(-1.2f, 1.2f, sawSample);

    float squareSample = t < 0.5f ? 1.0f : -1.0f;
    squareSample += polyBlep(t, dt);
    float t2 = t + 0.5f;
    t2 -= std::floor(t2);
    squareSample -= polyBlep(t2, dt);
    squareSample = std::tanh(squareSample * 1.15f);

    if (m < seg)
        return juce::jmap(m / seg, sineSample, triSample);
    else if (m < 2.0f * seg)
        return juce::jmap((m - seg) / seg, triSample, sawSample);
    else
        return juce::jmap((m - 2.0f * seg) / seg, sawSample, squareSample);
}

float SynthEngine::polyBlep(float t, float dt) const
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

int SynthEngine::findZeroCrossingIndex(int searchSpan) const
{
    const int N = scopeBuffer.getNumSamples();
    if (N <= 0)
        return 0;

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

void SynthEngine::captureWaveformSnapshot()
{
    const int numSamples = scopeBuffer.getNumSamples();
    if (numSamples <= 0)
        return;

    const int resolution = config::waveformResolution;
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

