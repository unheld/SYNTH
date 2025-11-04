#include "MainComponent.h"
#include <cmath>
#include <algorithm>
#include <complex>

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

class KnobGroupComponent : public juce::Component
{
public:
    KnobGroupComponent(const juce::String& titleText = {}, juce::Colour accentColour = juce::Colours::white)
        : title(titleText), accent(accentColour)
    {
        titleLabel.setText(titleText, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::left);
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
        addAndMakeVisible(titleLabel);
    }

    void setAccentColour(juce::Colour newColour)
    {
        accent = newColour;
        repaint();
    }

    void setTitle(const juce::String& newTitle)
    {
        title = newTitle;
        titleLabel.setText(title, juce::dontSendNotification);
    }

    void setVisualComponent(juce::Component* component)
    {
        if (visualComponent == component)
            return;

        if (visualComponent != nullptr)
            removeChildComponent(visualComponent);

        visualComponent = component;

        if (visualComponent != nullptr)
            addAndMakeVisible(visualComponent);
    }

    void setHeaderComponent(juce::Component* component)
    {
        if (headerComponent == component)
            return;

        if (headerComponent != nullptr)
            removeChildComponent(headerComponent);

        headerComponent = component;

        if (headerComponent != nullptr)
        {
            addAndMakeVisible(headerComponent);
        }
    }

    void addFooterComponent(juce::Component& component)
    {
        footerComponents.push_back(&component);
        addAndMakeVisible(component);
    }

    void addKnob(ParameterKnob& knob)
    {
        knobComponents.push_back(&knob);
        addAndMakeVisible(knob);
    }

    void setKnobColumns(int columns) noexcept
    {
        knobColumns = std::max(1, columns);
        resized();
    }

    void setVisualHeightRatio(float ratio) noexcept
    {
        visualHeightRatio = juce::jlimit(0.0f, 0.85f, ratio);
        resized();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto backgroundColour = accent.withAlpha(0.12f);
        g.setColour(backgroundColour);
        g.fillRoundedRectangle(bounds, 14.0f);

        g.setColour(accent.withAlpha(isHovered ? 0.8f : 0.5f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 14.0f, 1.5f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(12);

        auto headerArea = bounds.removeFromTop(28);
        auto titleArea = headerArea;

        if (headerComponent != nullptr)
        {
            const int headerWidth = std::min(headerArea.getWidth() / 2, 140);
            auto componentArea = headerArea.removeFromRight(headerWidth);
            headerComponent->setBounds(componentArea);
            titleArea = headerArea;
        }

        titleLabel.setBounds(titleArea.reduced(0, 0));

        if (visualComponent != nullptr)
        {
            const int visualHeight = (int)std::round(bounds.getHeight() * visualHeightRatio);
            auto visualArea = bounds.removeFromTop(visualHeight).reduced(4);
            visualComponent->setBounds(visualArea);
        }

        if (!footerComponents.empty())
        {
            const int footerHeight = 24 * (int)footerComponents.size();
            auto footerArea = bounds.removeFromBottom(footerHeight + 4);
            footerArea.removeFromTop(4);

            for (auto* comp : footerComponents)
            {
                if (comp != nullptr)
                {
                    auto row = footerArea.removeFromTop(24);
                    comp->setBounds(row.removeFromLeft(row.getWidth()));
                }
            }
        }

        if (!knobComponents.empty())
        {
            const int columns = knobColumns > 0 ? knobColumns : std::max(1, (int)std::ceil(std::sqrt((double)knobComponents.size())));
            const int rows = (int)std::ceil((double)knobComponents.size() / (double)columns);

            auto knobArea = bounds;
            const int rowHeight = rows > 0 ? knobArea.getHeight() / rows : knobArea.getHeight();

            int index = 0;
            for (int row = 0; row < rows; ++row)
            {
                auto rowArea = knobArea.removeFromTop(rowHeight);
                if (row < rows - 1)
                    knobArea.removeFromTop(6);

                const int colWidth = columns > 0 ? rowArea.getWidth() / columns : rowArea.getWidth();

                for (int col = 0; col < columns; ++col)
                {
                    if (index >= (int)knobComponents.size())
                        break;

                    auto cell = rowArea.removeFromLeft(colWidth);
                    cell = cell.reduced(4);
                    knobComponents[(size_t)index]->setBounds(cell);
                    ++index;
                }
            }
        }
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        isHovered = true;
        repaint();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        isHovered = false;
        repaint();
    }

private:
    juce::String title;
    juce::Colour accent;
    juce::Label titleLabel;
    juce::Component* headerComponent = nullptr;
    juce::Component* visualComponent = nullptr;
    std::vector<ParameterKnob*> knobComponents;
    std::vector<juce::Component*> footerComponents;
    int knobColumns = 3;
    float visualHeightRatio = 0.35f;
    bool isHovered = false;
};

class OscillatorPreviewComponent : public juce::Component
{
public:
    void setWaveform(const std::vector<float>* data) noexcept { waveform = data; }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.2f);

        if (waveform == nullptr || waveform->empty())
            return;

        juce::Path path;
        const auto& data = *waveform;
        const int N = (int)data.size();
        const float width = bounds.getWidth();
        const float height = bounds.getHeight();
        const float centreY = bounds.getCentreY();

        for (int i = 0; i < N; ++i)
        {
            const float x = bounds.getX() + (width * (float)i / (float)(N - 1));
            const float sample = juce::jlimit(-1.0f, 1.0f, data[(size_t)i]);
            const float y = centreY - sample * (height * 0.45f);
            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }

        g.setColour(juce::Colours::cyan.withAlpha(0.9f));
        g.strokePath(path, juce::PathStrokeType(2.0f));
    }

private:
    const std::vector<float>* waveform = nullptr;
};

class FilterResponseComponent : public juce::Component
{
public:
    explicit FilterResponseComponent(MainComponent& ownerRef)
        : owner(ownerRef)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.2f);

        juce::Path response;
        const int numPoints = std::max(64, (int)bounds.getWidth());
        const double sr = std::max(44100.0, owner.getCurrentSampleRate());
        const double cutoff = owner.getCutoffHz();
        const double resonance = owner.getResonanceQ();
        const auto type = owner.getFilterType();

        for (int i = 0; i < numPoints; ++i)
        {
            const double norm = (double)i / (double)(numPoints - 1);
            const double freq = 20.0 * std::pow(1000.0, norm * std::log10(sr / 20.0));
            const double magnitude = getMagnitude(freq, sr, cutoff, resonance, type);
            const float dB = juce::Decibels::gainToDecibels(magnitude, -60.0f);
            const float y = juce::jmap(dB, -24.0f, 12.0f, bounds.getBottom() - 6.0f, bounds.getY() + 6.0f);
            const float x = bounds.getX() + (float)norm * (bounds.getWidth());
            if (i == 0)
                response.startNewSubPath(x, y);
            else
                response.lineTo(x, y);
        }

        g.setColour(juce::Colours::yellow.withAlpha(0.9f));
        g.strokePath(response, juce::PathStrokeType(2.0f));

        const float cutoffX = (float)juce::jmap(std::log(cutoff), std::log(20.0), std::log(sr * 0.5), bounds.getX(), bounds.getRight());
        g.setColour(juce::Colours::yellow.withAlpha(0.5f));
        g.drawVerticalLine((int)cutoffX, bounds.getY(), bounds.getBottom());
    }

private:
    static double getMagnitude(double freq, double sampleRate, double cutoff, double q, MainComponent::FilterType type)
    {
        const double w0 = juce::MathConstants<double>::twoPi * cutoff / sampleRate;
        const double cosw0 = std::cos(w0);
        const double sinw0 = std::sin(w0);
        const double alpha = sinw0 / (2.0 * q);

        double b0 = 0.0, b1 = 0.0, b2 = 0.0;
        double a0 = 1.0, a1 = 0.0, a2 = 0.0;

        switch (type)
        {
            case MainComponent::FilterType::BandPass:
                b0 = alpha;
                b1 = 0.0;
                b2 = -alpha;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cosw0;
                a2 = 1.0 - alpha;
                break;
            case MainComponent::FilterType::HighPass:
                b0 = (1.0 + cosw0) * 0.5;
                b1 = -(1.0 + cosw0);
                b2 = (1.0 + cosw0) * 0.5;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cosw0;
                a2 = 1.0 - alpha;
                break;
            case MainComponent::FilterType::Notch:
                b0 = 1.0;
                b1 = -2.0 * cosw0;
                b2 = 1.0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cosw0;
                a2 = 1.0 - alpha;
                break;
            case MainComponent::FilterType::LowPass:
            default:
                b0 = (1.0 - cosw0) * 0.5;
                b1 = 1.0 - cosw0;
                b2 = (1.0 - cosw0) * 0.5;
                a0 = 1.0 + alpha;
                a1 = -2.0 * cosw0;
                a2 = 1.0 - alpha;
                break;
        }

        const double omega = juce::MathConstants<double>::twoPi * freq / sampleRate;
        const std::complex<double> z = std::exp(std::complex<double>(0.0, -omega));
        const std::complex<double> z2 = std::exp(std::complex<double>(0.0, -2.0 * omega));
        const std::complex<double> numerator = b0 + b1 * z + b2 * z2;
        const std::complex<double> denominator = 1.0 + a1 * z + a2 * z2;
        const double magnitude = std::abs(numerator / denominator);
        return magnitude;
    }

    MainComponent& owner;
};

class EnvelopeGraphComponent : public juce::Component
{
public:
    explicit EnvelopeGraphComponent(MainComponent& ownerRef) : owner(ownerRef) {}

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.2f);

        juce::Path envPath;
        const float total = owner.getAttackMs() + owner.getDecayMs() + owner.getReleaseMs() + 1.0f;
        const float attackX = owner.getAttackMs() / total;
        const float decayX = owner.getDecayMs() / total;
        const float releaseX = owner.getReleaseMs() / total;
        const float sustain = owner.getSustainLevel();

        auto start = bounds.getBottomLeft();
        envPath.startNewSubPath(start);
        envPath.lineTo(bounds.getX() + attackX * bounds.getWidth(), bounds.getY());
        envPath.lineTo(bounds.getX() + (attackX + decayX) * bounds.getWidth(), bounds.getY() + (1.0f - sustain) * bounds.getHeight());
        envPath.lineTo(bounds.getRight() - releaseX * bounds.getWidth(), bounds.getY() + (1.0f - sustain) * bounds.getHeight());
        envPath.lineTo(bounds.getRight(), bounds.getBottom());

        g.setColour(juce::Colours::orange);
        g.strokePath(envPath, juce::PathStrokeType(2.0f));
    }

private:
    MainComponent& owner;
};

class LfoPreviewComponent : public juce::Component, private juce::Timer
{
public:
    explicit LfoPreviewComponent(MainComponent& ownerRef) : owner(ownerRef)
    {
        startTimerHz(24);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.2f);

        juce::Path lfoPath;
        const int points = std::max(32, (int)bounds.getWidth());
        const float depth = owner.getLfoDepthAmount();

        for (int i = 0; i < points; ++i)
        {
            const float norm = (float)i / (float)(points - 1);
            const float angle = (norm * juce::MathConstants<float>::twoPi) + phase;
            const float sample = owner.renderLfoShape(angle);
            const float y = juce::jmap(sample * depth, -1.0f, 1.0f, bounds.getBottom() - 6.0f, bounds.getY() + 6.0f);
            const float x = bounds.getX() + norm * bounds.getWidth();
            if (i == 0)
                lfoPath.startNewSubPath(x, y);
            else
                lfoPath.lineTo(x, y);
        }

        g.setColour(juce::Colours::blue);
        g.strokePath(lfoPath, juce::PathStrokeType(2.0f));
    }

    void timerCallback() override
    {
        phase += (float)juce::MathConstants<double>::twoPi * (owner.getLfoRateHz() / 60.0f) * 0.25f;
        if (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
        repaint();
    }

private:
    MainComponent& owner;
    float phase = 0.0f;
};

class EffectIntensityMeter : public juce::Component, private juce::Timer
{
public:
    explicit EffectIntensityMeter(MainComponent& ownerRef) : owner(ownerRef)
    {
        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.2f);

        const float metrics[4] =
        {
            owner.getCrushAmount(),
            owner.getAutoPanAmount(),
            owner.getDelayAmount(),
            owner.getGlitchAmount()
        };

        const juce::String labels[4] = { "Distortion", "Chorus", "Delay", "Reverb" };

        const float barWidth = bounds.getWidth() / 4.0f;

        for (int i = 0; i < 4; ++i)
        {
            auto bar = bounds.removeFromLeft(barWidth);
            if (i < 3)
                bounds.removeFromLeft(2.0f);

            const float value = juce::jlimit(0.0f, 1.0f, metrics[i]);
            auto filled = bar.withY(bar.getBottom() - value * bar.getHeight());

            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.fillRect(bar);
            g.setColour(juce::Colours::white.withAlpha(0.65f));
            g.fillRect(filled);

            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            g.drawFittedText(labels[i], bar.toNearestInt(), juce::Justification::centredBottom, 1);
        }
    }

    void timerCallback() override
    {
        repaint();
    }

private:
    MainComponent& owner;
};

class OutputMeterComponent : public juce::Component, private juce::Timer
{
public:
    explicit OutputMeterComponent(MainComponent& ownerRef) : owner(ownerRef)
    {
        startTimerHz(60);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.2f);

        const float level = juce::jlimit(0.0f, 1.0f, owner.getCurrentLevel());
        const auto meter = bounds.withX(bounds.getX() + bounds.getWidth() * 0.2f)
                                 .withWidth(bounds.getWidth() * 0.6f);
        auto filled = meter.withY(meter.getBottom() - level * meter.getHeight());

        g.setColour(juce::Colours::darkgrey.withAlpha(0.4f));
        g.fillRect(meter);
        g.setColour(juce::Colours::limegreen.withAlpha(0.7f));
        g.fillRect(filled);
    }

    void timerCallback() override
    {
        repaint();
    }

private:
    MainComponent& owner;
};

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

    midiRoll = std::make_unique<MidiRollComponent>();
    addAndMakeVisible (midiRoll.get());

    initialiseUi();
    initialiseMidiInputs();
    initialiseKeyboard();

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

    double b0 = 0.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cw;
    double a2 = 1.0 - alpha;

    switch (filterType)
    {
        case FilterType::BandPass:
            b0 = alpha;
            b1 = 0.0;
            b2 = -alpha;
            break;
        case FilterType::HighPass:
            b0 = (1.0 + cw) * 0.5;
            b1 = -(1.0 + cw);
            b2 = (1.0 + cw) * 0.5;
            break;
        case FilterType::Notch:
            b0 = 1.0;
            b1 = -2.0 * cw;
            b2 = 1.0;
            break;
        case FilterType::LowPass:
        default:
            b0 = (1.0 - cw) * 0.5;
            b1 = 1.0 - cw;
            b2 = (1.0 - cw) * 0.5;
            break;
    }

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
    const double glideSeconds = monoModeEnabled ? juce::jlimit(0.001, 0.6, (double)glideTimeMs * 0.001) : 0.002;
    const double gainRampSeconds = 0.02;
    const double filterRampSeconds = 0.06;
    const double spatialRampSeconds = 0.1;

    frequencySmoothed.reset(sampleRate, glideSeconds);
    gainSmoothed.reset(sampleRate, gainRampSeconds);
    cutoffSmoothed.reset(sampleRate, filterRampSeconds);
    resonanceSmoothed.reset(sampleRate, filterRampSeconds);
    stereoWidthSmoothed.reset(sampleRate, spatialRampSeconds);
    lfoDepthSmoothed.reset(sampleRate, spatialRampSeconds);
    driveSmoothed.reset(sampleRate, gainRampSeconds);

    frequencySmoothed.setCurrentAndTargetValue(targetFrequency);
    gainSmoothed.setCurrentAndTargetValue(outputGain);
    cutoffSmoothed.setCurrentAndTargetValue(cutoffHz);
    resonanceSmoothed.setCurrentAndTargetValue(resonanceQ);
    stereoWidthSmoothed.setCurrentAndTargetValue(stereoWidth);
    lfoDepthSmoothed.setCurrentAndTargetValue(lfoDepth);
    driveSmoothed.setCurrentAndTargetValue(driveAmount);

    filterL.reset();
    filterR.reset();

    updateGlideSmoother();
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

float MainComponent::renderLfoShape(float phase) const noexcept
{
    float wrapped = std::fmod(phase, juce::MathConstants<float>::twoPi);
    if (wrapped < 0.0f)
        wrapped += juce::MathConstants<float>::twoPi;

    const float norm = wrapped / juce::MathConstants<float>::twoPi;

    switch (lfoShapeChoice)
    {
        case 1:
        {
            const float tri = 1.0f - 4.0f * std::abs(norm - 0.5f);
            return juce::jlimit(-1.0f, 1.0f, tri);
        }
        case 2:
            return norm < 0.5f ? 1.0f : -1.0f;
        default:
            return std::sin(wrapped);
    }
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr || bufferToFill.buffer->getNumChannels() == 0)
        return;

    bufferToFill.buffer->clear(bufferToFill.startSample, bufferToFill.numSamples);

    juce::MidiBuffer midiRollBuffer;
    if (midiRoll)
        midiRoll->renderNextMidiBlock(midiRollBuffer, bufferToFill.numSamples, currentSR);

    keyboardState.processNextMidiBuffer(midiRollBuffer, bufferToFill.startSample, bufferToFill.numSamples, true);

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
        const float rawEnv = amplitudeEnvelope.getNextSample();
        const float ampEnv = envelopeToAmpEnabled ? rawEnv : 1.0f;
        const float filterEnv = envelopeToFilterEnabled ? rawEnv : 0.0f;
        const float drive = driveSmoothed.getNextValue();

        float lfoS = renderLfoShape(lfoPhase);
        float vibrato = (lfoDestinationChoice == 0) ? (1.0f + depth * lfoS) : 1.0f;
        float ampLfo = 1.0f;
        if (lfoDestinationChoice == 2)
            ampLfo = juce::jlimit(0.0f, 2.0f, juce::jmap(lfoS, -1.0f, 1.0f, 1.0f - depth, 1.0f + depth));
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
            float filterModDepth = lfoCutModAmt;
            if (lfoDestinationChoice == 1)
                filterModDepth = juce::jlimit(0.0f, 1.5f, lfoCutModAmt + depth);

            const double modFactor = std::pow(2.0, (double)filterModDepth * (double)lfoS);
            const double envFactor = juce::jlimit(0.1, 4.0, 1.0 + (double)envFilterAmt * (double)filterEnv);
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

        const float ampValue = juce::jlimit(0.0f, 2.0f, ampEnv * ampLfo);
        fL *= ampValue;
        fR *= ampValue;

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
    if (oscillatorPreview != nullptr)
        oscillatorPreview->repaint();
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

    const int maxGroupsHeight = std::min(area.getHeight(), 360);
    auto groupsArea = area.removeFromTop(maxGroupsHeight);
    area.removeFromTop(8);

    if (oscillatorGroup && filterGroup && envelopeGroup && lfoGroup && effectsGroup && masterGroup)
    {
        juce::Grid grid;
        grid.autoColumns = juce::Grid::TrackInfo(juce::Grid::Fr(1));
        grid.autoRows = juce::Grid::TrackInfo(juce::Grid::Fr(1));
        grid.templateColumns = { juce::Grid::TrackInfo(juce::Grid::Fr(3)),
                                 juce::Grid::TrackInfo(juce::Grid::Fr(3)),
                                 juce::Grid::TrackInfo(juce::Grid::Fr(2)) };
        grid.templateRows = { juce::Grid::TrackInfo(juce::Grid::Fr(1)),
                              juce::Grid::TrackInfo(juce::Grid::Fr(1)) };

        grid.items.addArray({
            juce::GridItem(*oscillatorGroup).withMargin(juce::GridItem::Margin(4)),
            juce::GridItem(*filterGroup).withMargin(juce::GridItem::Margin(4)),
            juce::GridItem(*envelopeGroup).withMargin(juce::GridItem::Margin(4)),
            juce::GridItem(*lfoGroup).withMargin(juce::GridItem::Margin(4)),
            juce::GridItem(*effectsGroup).withMargin(juce::GridItem::Margin(4)),
            juce::GridItem(*masterGroup).withMargin(juce::GridItem::Margin(4))
        });

        grid.performLayout(groupsArea);
    }

    if (midiRoll)
    {
        const int rollHeight = juce::jlimit(160, 260, area.getHeight() / 3);
        auto rollArea = area.removeFromTop(rollHeight);
        midiRoll->setBounds(rollArea);
        area.removeFromTop(8);
    }

    int kbH = std::max(keyboardMinHeight, area.getHeight() / 4);
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

    const double bpmToDisplay = midiRoll ? midiRoll->getBpm() : (double) defaultBpmDisplay;
    bpmLabel.setText(juce::String(juce::roundToInt(bpmToDisplay)) + " BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType(juce::Justification::centred);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    bpmLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));


    bpmLabel.setBorderSize(juce::BorderSize<int>());
    addAndMakeVisible(bpmLabel);

    updatePlayLabel();
}

void MainComponent::initialiseSliders()
{
    const juce::Colour oscColour { 0xFF4A90FF };
    const juce::Colour filterColour { 0xFF4CE080 };
    const juce::Colour envColour { 0xFFFF9F4A };
    const juce::Colour lfoColour { 0xFF67C1FF };
    const juce::Colour fxColour { 0xFFC46BFF };
    const juce::Colour masterColour { 0xFFF5D96B };

    oscillatorGroup = std::make_unique<KnobGroupComponent>("Oscillator", oscColour);
    filterGroup = std::make_unique<KnobGroupComponent>("Filter", filterColour);
    envelopeGroup = std::make_unique<KnobGroupComponent>("Envelope", envColour);
    lfoGroup = std::make_unique<KnobGroupComponent>("LFO", lfoColour);
    effectsGroup = std::make_unique<KnobGroupComponent>("Effects", fxColour);
    masterGroup = std::make_unique<KnobGroupComponent>("Master", masterColour);

    addAndMakeVisible(*oscillatorGroup);
    addAndMakeVisible(*filterGroup);
    addAndMakeVisible(*envelopeGroup);
    addAndMakeVisible(*lfoGroup);
    addAndMakeVisible(*effectsGroup);
    addAndMakeVisible(*masterGroup);

    oscillatorPreview = std::make_unique<OscillatorPreviewComponent>();
    oscillatorPreview->setWaveform(&waveformSnapshot);
    oscillatorGroup->setVisualComponent(oscillatorPreview.get());
    oscillatorGroup->setVisualHeightRatio(0.45f);
    oscillatorGroup->setKnobColumns(3);

    filterGraph = std::make_unique<FilterResponseComponent>(*this);
    filterGroup->setVisualComponent(filterGraph.get());
    filterGroup->setVisualHeightRatio(0.45f);
    filterGroup->setKnobColumns(2);

    envelopeGraph = std::make_unique<EnvelopeGraphComponent>(*this);
    envelopeGroup->setVisualComponent(envelopeGraph.get());
    envelopeGroup->setVisualHeightRatio(0.5f);
    envelopeGroup->setKnobColumns(2);

    lfoPreview = std::make_unique<LfoPreviewComponent>(*this);
    lfoGroup->setVisualComponent(lfoPreview.get());
    lfoGroup->setVisualHeightRatio(0.45f);
    lfoGroup->setKnobColumns(3);

    effectsMeter = std::make_unique<EffectIntensityMeter>(*this);
    effectsGroup->setVisualComponent(effectsMeter.get());
    effectsGroup->setVisualHeightRatio(0.4f);
    effectsGroup->setKnobColumns(2);

    outputMeter = std::make_unique<OutputMeterComponent>(*this);
    masterGroup->setVisualComponent(outputMeter.get());
    masterGroup->setVisualHeightRatio(0.5f);
    masterGroup->setKnobColumns(2);

    auto configureKnob = [this](ParameterKnob& knob, const juce::String& caption)
    {
        knob.setCaption(caption);
        knob.setTextColour(juce::Colours::white.withAlpha(0.9f));
        configureRotarySlider(knob.slider());
    };

    // Oscillator knobs
    configureKnob(waveKnob, "Waveform");
    waveKnob.slider().setRange(0.0, 1.0);
    waveKnob.slider().setTooltip("Blend oscillator waveform");
    waveKnob.slider().onValueChange = [this]
    {
        waveMorph = (float)waveKnob.slider().getValue();
        waveKnob.value().setText(juce::String(waveMorph, 2), juce::dontSendNotification);
        waveKnob.slider().setTooltip("Waveform morph: " + waveKnob.value().getText());
        if (oscillatorPreview != nullptr)
            oscillatorPreview->repaint();
    };
    waveKnob.slider().setValue(waveMorph);
    oscillatorGroup->addKnob(waveKnob);

    configureKnob(pitchKnob, "Tune");
    pitchKnob.slider().setRange(40.0, 5000.0);
    pitchKnob.slider().setSkewFactorFromMidPoint(440.0);
    pitchKnob.slider().setTooltip("Oscillator tuning (Hz)");
    pitchKnob.slider().onValueChange = [this]
    {
        setTargetFrequency((float)pitchKnob.slider().getValue());
        const juce::String text = juce::String(targetFrequency, 1) + " Hz";
        pitchKnob.value().setText(text, juce::dontSendNotification);
        pitchKnob.slider().setTooltip("Tune: " + text);
    };
    pitchKnob.slider().setValue(targetFrequency);
    oscillatorGroup->addKnob(pitchKnob);

    configureKnob(widthKnob, "Spread");
    widthKnob.slider().setRange(0.0, 2.0, 0.01);
    widthKnob.slider().setTooltip("Stereo spread amount");
    widthKnob.slider().onValueChange = [this]
    {
        stereoWidth = (float)widthKnob.slider().getValue();
        stereoWidthSmoothed.setTargetValue(stereoWidth);
        const juce::String text = juce::String(stereoWidth, 2) + "x";
        widthKnob.value().setText(text, juce::dontSendNotification);
        widthKnob.slider().setTooltip("Spread: " + text);
    };
    widthKnob.slider().setValue(stereoWidth);
    oscillatorGroup->addKnob(widthKnob);

    configureKnob(subMixKnob, "Morph Blend");
    subMixKnob.slider().setRange(0.0, 1.0);
    subMixKnob.slider().setTooltip("Blend sub and detuned layers");
    subMixKnob.slider().onValueChange = [this]
    {
        subMixAmount = (float)subMixKnob.slider().getValue();
        const juce::String text = juce::String(subMixAmount * 100.0f, 0) + "%";
        subMixKnob.value().setText(text, juce::dontSendNotification);
        subMixKnob.slider().setTooltip("Morph blend: " + text);
    };
    subMixKnob.slider().setValue(subMixAmount);
    oscillatorGroup->addKnob(subMixKnob);

    configureKnob(chaosKnob, "Chaos");
    chaosKnob.slider().setRange(0.0, 1.0);
    chaosKnob.slider().setTooltip("Spectral chaos amount");
    chaosKnob.slider().onValueChange = [this]
    {
        chaosAmount = (float)chaosKnob.slider().getValue();
        const juce::String text = juce::String(chaosAmount * 100.0f, 0) + "%";
        chaosKnob.value().setText(text, juce::dontSendNotification);
        chaosKnob.slider().setTooltip("Chaos: " + text);
    };
    chaosKnob.slider().setValue(chaosAmount);
    oscillatorGroup->addKnob(chaosKnob);

    // Filter section
    configureKnob(cutoffKnob, "Cutoff");
    cutoffKnob.slider().setRange(80.0, 10000.0, 1.0);
    cutoffKnob.slider().setSkewFactorFromMidPoint(1000.0);
    cutoffKnob.slider().setTooltip("Filter cutoff frequency");
    cutoffKnob.slider().onValueChange = [this]
    {
        cutoffHz = (float)cutoffKnob.slider().getValue();
        cutoffSmoothed.setTargetValue(cutoffHz);
        const juce::String text = juce::String(cutoffHz, 1) + " Hz";
        cutoffKnob.value().setText(text, juce::dontSendNotification);
        cutoffKnob.slider().setTooltip("Cutoff: " + text);
        filterUpdateCount = filterUpdateStep;
        if (filterGraph != nullptr)
            filterGraph->repaint();
    };
    cutoffKnob.slider().setValue(cutoffHz);
    filterGroup->addKnob(cutoffKnob);

    configureKnob(resonanceKnob, "Resonance");
    resonanceKnob.slider().setRange(0.1, 10.0, 0.01);
    resonanceKnob.slider().setSkewFactorFromMidPoint(0.707);
    resonanceKnob.slider().setTooltip("Resonance (Q)");
    resonanceKnob.slider().onValueChange = [this]
    {
        resonanceQ = (float)resonanceKnob.slider().getValue();
        resonanceQ = std::max(0.1f, resonanceQ);
        resonanceSmoothed.setTargetValue(resonanceQ);
        const juce::String text = juce::String(resonanceQ, 2);
        resonanceKnob.value().setText(text, juce::dontSendNotification);
        resonanceKnob.slider().setTooltip("Resonance: " + text);
        filterUpdateCount = filterUpdateStep;
        if (filterGraph != nullptr)
            filterGraph->repaint();
    };
    resonanceKnob.slider().setValue(resonanceQ);
    filterGroup->addKnob(resonanceKnob);

    configureKnob(driveKnob, "Drive");
    driveKnob.slider().setRange(0.0, 1.0);
    driveKnob.slider().setTooltip("Filter drive / saturation");
    driveKnob.slider().onValueChange = [this]
    {
        driveAmount = (float)driveKnob.slider().getValue();
        driveSmoothed.setTargetValue(driveAmount);
        const juce::String text = juce::String(driveAmount * 100.0f, 0) + "%";
        driveKnob.value().setText(text, juce::dontSendNotification);
        driveKnob.slider().setTooltip("Drive: " + text);
    };
    driveKnob.slider().setValue(driveAmount);
    filterGroup->addKnob(driveKnob);

    configureKnob(envFilterKnob, "Env Amount");
    envFilterKnob.slider().setRange(-1.0, 1.0, 0.01);
    envFilterKnob.slider().setTooltip("Envelope to cutoff amount");
    envFilterKnob.slider().onValueChange = [this]
    {
        envFilterAmount = (float)envFilterKnob.slider().getValue();
        const juce::String text = juce::String(envFilterAmount, 2);
        envFilterKnob.value().setText(text, juce::dontSendNotification);
        envFilterKnob.slider().setTooltip("Env->Filter: " + text);
    };
    envFilterKnob.slider().setValue(envFilterAmount);
    filterGroup->addKnob(envFilterKnob);

    filterTypeBox.clear(juce::dontSendNotification);
    filterTypeBox.addItem("Low-pass", 1);
    filterTypeBox.addItem("Band-pass", 2);
    filterTypeBox.addItem("High-pass", 3);
    filterTypeBox.addItem("Notch", 4);
    filterTypeBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    filterTypeBox.setJustificationType(juce::Justification::centred);
    filterTypeBox.setTooltip("Filter topology");
    filterTypeBox.onChange = [this]
    {
        filterType = static_cast<FilterType>(juce::jlimit(0, 3, filterTypeBox.getSelectedId() - 1));
        updateFilterStatic();
        if (filterGraph != nullptr)
            filterGraph->repaint();
    };
    filterTypeBox.setSelectedId(static_cast<int>(filterType) + 1, juce::dontSendNotification);
    filterGroup->setHeaderComponent(&filterTypeBox);

    // Envelope section
    configureKnob(attackKnob, "Attack");
    attackKnob.slider().setRange(0.0, 2000.0, 1.0);
    attackKnob.slider().setSkewFactorFromMidPoint(40.0);
    attackKnob.slider().setTooltip("Attack time");
    attackKnob.slider().onValueChange = [this]
    {
        attackMs = (float)attackKnob.slider().getValue();
        const juce::String text = juce::String(attackMs, 0) + " ms";
        attackKnob.value().setText(text, juce::dontSendNotification);
        attackKnob.slider().setTooltip("Attack: " + text);
        updateAmplitudeEnvelope();
        if (envelopeGraph != nullptr)
            envelopeGraph->repaint();
    };
    attackKnob.slider().setValue(attackMs);
    envelopeGroup->addKnob(attackKnob);

    configureKnob(decayKnob, "Decay");
    decayKnob.slider().setRange(5.0, 4000.0, 1.0);
    decayKnob.slider().setSkewFactorFromMidPoint(200.0);
    decayKnob.slider().setTooltip("Decay time");
    decayKnob.slider().onValueChange = [this]
    {
        decayMs = (float)decayKnob.slider().getValue();
        const juce::String text = juce::String(decayMs, 0) + " ms";
        decayKnob.value().setText(text, juce::dontSendNotification);
        decayKnob.slider().setTooltip("Decay: " + text);
        updateAmplitudeEnvelope();
        if (envelopeGraph != nullptr)
            envelopeGraph->repaint();
    };
    decayKnob.slider().setValue(decayMs);
    envelopeGroup->addKnob(decayKnob);

    configureKnob(sustainKnob, "Sustain");
    sustainKnob.slider().setRange(0.0, 1.0, 0.01);
    sustainKnob.slider().setTooltip("Sustain level");
    sustainKnob.slider().onValueChange = [this]
    {
        sustainLevel = (float)sustainKnob.slider().getValue();
        const juce::String text = juce::String(sustainLevel * 100.0f, 0) + "%";
        sustainKnob.value().setText(text, juce::dontSendNotification);
        sustainKnob.slider().setTooltip("Sustain: " + text);
        updateAmplitudeEnvelope();
        if (envelopeGraph != nullptr)
            envelopeGraph->repaint();
    };
    sustainKnob.slider().setValue(sustainLevel);
    envelopeGroup->addKnob(sustainKnob);

    configureKnob(releaseKnob, "Release");
    releaseKnob.slider().setRange(1.0, 4000.0, 1.0);
    releaseKnob.slider().setSkewFactorFromMidPoint(200.0);
    releaseKnob.slider().setTooltip("Release time");
    releaseKnob.slider().onValueChange = [this]
    {
        releaseMs = (float)releaseKnob.slider().getValue();
        const juce::String text = juce::String(releaseMs, 0) + " ms";
        releaseKnob.value().setText(text, juce::dontSendNotification);
        releaseKnob.slider().setTooltip("Release: " + text);
        updateAmplitudeEnvelope();
        if (envelopeGraph != nullptr)
            envelopeGraph->repaint();
    };
    releaseKnob.slider().setValue(releaseMs);
    envelopeGroup->addKnob(releaseKnob);

    envelopeToFilterToggle.setToggleState(envelopeToFilterEnabled, juce::dontSendNotification);
    envelopeToFilterToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    envelopeToFilterToggle.onClick = [this]
    {
        envelopeToFilterEnabled = envelopeToFilterToggle.getToggleState();
    };
    envelopeGroup->addFooterComponent(envelopeToFilterToggle);

    envelopeToAmpToggle.setToggleState(envelopeToAmpEnabled, juce::dontSendNotification);
    envelopeToAmpToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    envelopeToAmpToggle.onClick = [this]
    {
        envelopeToAmpEnabled = envelopeToAmpToggle.getToggleState();
    };
    envelopeGroup->addFooterComponent(envelopeToAmpToggle);

    // LFO section
    configureKnob(lfoKnob, "Rate");
    lfoKnob.slider().setRange(0.05, 15.0);
    lfoKnob.slider().setTooltip("LFO rate");
    lfoKnob.slider().onValueChange = [this]
    {
        lfoRateHz = (float)lfoKnob.slider().getValue();
        const juce::String text = juce::String(lfoRateHz, 2) + " Hz";
        lfoKnob.value().setText(text, juce::dontSendNotification);
        lfoKnob.slider().setTooltip("LFO rate: " + text);
        if (lfoPreview != nullptr)
            lfoPreview->repaint();
    };
    lfoKnob.slider().setValue(lfoRateHz);
    lfoGroup->addKnob(lfoKnob);

    configureKnob(lfoDepthKnob, "Depth");
    lfoDepthKnob.slider().setRange(0.0, 1.0);
    lfoDepthKnob.slider().setTooltip("LFO depth");
    lfoDepthKnob.slider().onValueChange = [this]
    {
        lfoDepth = (float)lfoDepthKnob.slider().getValue();
        lfoDepthSmoothed.setTargetValue(lfoDepth);
        const juce::String text = juce::String(lfoDepth, 2);
        lfoDepthKnob.value().setText(text, juce::dontSendNotification);
        lfoDepthKnob.slider().setTooltip("LFO depth: " + text);
        if (lfoPreview != nullptr)
            lfoPreview->repaint();
    };
    lfoDepthKnob.slider().setValue(lfoDepth);
    lfoGroup->addKnob(lfoDepthKnob);

    configureKnob(filterModKnob, "Filter Mod");
    filterModKnob.slider().setRange(0.0, 1.0, 0.001);
    filterModKnob.slider().setTooltip("LFO modulation depth");
    filterModKnob.slider().onValueChange = [this]
    {
        lfoCutModAmt = (float)filterModKnob.slider().getValue();
        const juce::String text = juce::String(lfoCutModAmt, 2);
        filterModKnob.value().setText(text, juce::dontSendNotification);
        filterModKnob.slider().setTooltip("Filter mod: " + text);
    };
    filterModKnob.slider().setValue(lfoCutModAmt);
    lfoGroup->addKnob(filterModKnob);

    configureKnob(lfoModeKnob, "Trigger");
    lfoModeKnob.slider().setRange(0.0, 1.0, 1.0);
    lfoModeKnob.slider().setTooltip("LFO retrigger mode");
    lfoModeKnob.slider().onValueChange = [this]
    {
        const bool freeRun = juce::approximatelyEqual((float)lfoModeKnob.slider().getValue(), 1.0f);
        lfoTriggerMode = freeRun ? LfoTriggerMode::FreeRun : LfoTriggerMode::Retrigger;
        const juce::String text = freeRun ? "Free" : "Retrig";
        lfoModeKnob.value().setText(text, juce::dontSendNotification);
        lfoModeKnob.slider().setTooltip("Trigger: " + text);
        if (!freeRun)
            triggerLfo();
    };
    lfoModeKnob.slider().setValue((lfoTriggerMode == LfoTriggerMode::FreeRun) ? 1.0 : 0.0);
    lfoGroup->addKnob(lfoModeKnob);

    configureKnob(lfoStartKnob, "Start Phase");
    lfoStartKnob.slider().setRange(0.0, 1.0, 0.001);
    lfoStartKnob.slider().setTooltip("LFO starting phase");
    lfoStartKnob.slider().onValueChange = [this]
    {
        lfoStartPhaseNormalized = (float)lfoStartKnob.slider().getValue();
        const int degrees = juce::roundToInt(lfoStartPhaseNormalized * 360.0f);
        const juce::String text = juce::String(degrees) + juce::String::charToString(0x00B0);
        lfoStartKnob.value().setText(text, juce::dontSendNotification);
        lfoStartKnob.slider().setTooltip("Start phase: " + text);
        triggerLfo();
    };
    lfoStartKnob.slider().setValue(lfoStartPhaseNormalized);
    lfoGroup->addKnob(lfoStartKnob);

    lfoShapeBox.clear(juce::dontSendNotification);
    lfoShapeBox.addItem("Sine", 1);
    lfoShapeBox.addItem("Triangle", 2);
    lfoShapeBox.addItem("Square", 3);
    lfoShapeBox.setTooltip("LFO waveform");
    lfoShapeBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    lfoShapeBox.setJustificationType(juce::Justification::centred);
    lfoShapeBox.onChange = [this]
    {
        lfoShapeChoice = juce::jlimit(0, 2, lfoShapeBox.getSelectedId() - 1);
        if (lfoPreview != nullptr)
            lfoPreview->repaint();
    };
    lfoShapeBox.setSelectedId(lfoShapeChoice + 1, juce::dontSendNotification);
    lfoGroup->setHeaderComponent(&lfoShapeBox);

    lfoDestinationBox.clear(juce::dontSendNotification);
    lfoDestinationBox.addItem("Pitch", 1);
    lfoDestinationBox.addItem("Filter", 2);
    lfoDestinationBox.addItem("Amp", 3);
    lfoDestinationBox.setTooltip("Primary modulation destination");
    lfoDestinationBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    lfoDestinationBox.setJustificationType(juce::Justification::centred);
    lfoDestinationBox.onChange = [this]
    {
        lfoDestinationChoice = juce::jlimit(0, 2, lfoDestinationBox.getSelectedId() - 1);
    };
    lfoDestinationBox.setSelectedId(lfoDestinationChoice + 1, juce::dontSendNotification);
    lfoGroup->addFooterComponent(lfoDestinationBox);

    // Effects section
    configureKnob(crushKnob, "Distortion");
    crushKnob.slider().setRange(0.0, 1.0);
    crushKnob.slider().setTooltip("Bit-crush intensity");
    crushKnob.slider().onValueChange = [this]
    {
        crushAmount = (float)crushKnob.slider().getValue();
        const juce::String text = juce::String(crushAmount * 100.0f, 0) + "%";
        crushKnob.value().setText(text, juce::dontSendNotification);
        crushKnob.slider().setTooltip("Distortion: " + text);
    };
    crushKnob.slider().setValue(crushAmount);
    effectsGroup->addKnob(crushKnob);

    configureKnob(autoPanKnob, "Chorus");
    autoPanKnob.slider().setRange(0.0, 1.0);
    autoPanKnob.slider().setTooltip("Stereo modulation amount");
    autoPanKnob.slider().onValueChange = [this]
    {
        autoPanAmount = (float)autoPanKnob.slider().getValue();
        const juce::String text = juce::String(autoPanAmount * 100.0f, 0) + "%";
        autoPanKnob.value().setText(text, juce::dontSendNotification);
        autoPanKnob.slider().setTooltip("Chorus: " + text);
    };
    autoPanKnob.slider().setValue(autoPanAmount);
    effectsGroup->addKnob(autoPanKnob);

    configureKnob(delayKnob, "Delay");
    delayKnob.slider().setRange(0.0, 1.0);
    delayKnob.slider().setTooltip("Delay mix and feedback");
    delayKnob.slider().onValueChange = [this]
    {
        delayAmount = (float)delayKnob.slider().getValue();
        const juce::String text = juce::String(delayAmount * 100.0f, 0) + "%";
        delayKnob.value().setText(text, juce::dontSendNotification);
        delayKnob.slider().setTooltip("Delay: " + text);
    };
    delayKnob.slider().setValue(delayAmount);
    effectsGroup->addKnob(delayKnob);

    configureKnob(glitchKnob, "Reverb");
    glitchKnob.slider().setRange(0.0, 1.0);
    glitchKnob.slider().setTooltip("Ambient glitch / space");
    glitchKnob.slider().onValueChange = [this]
    {
        glitchProbability = (float)glitchKnob.slider().getValue();
        const juce::String text = juce::String(glitchProbability * 100.0f, 0) + "%";
        glitchKnob.value().setText(text, juce::dontSendNotification);
        glitchKnob.slider().setTooltip("Reverb: " + text);
    };
    glitchKnob.slider().setValue(glitchProbability);
    effectsGroup->addKnob(glitchKnob);

    // Master section
    configureKnob(gainKnob, "Output");
    gainKnob.slider().setRange(0.0, 1.0);
    gainKnob.slider().setTooltip("Output gain");
    gainKnob.slider().onValueChange = [this]
    {
        outputGain = (float)gainKnob.slider().getValue();
        gainSmoothed.setTargetValue(outputGain);
        const juce::String text = juce::String(outputGain * 100.0f, 0) + "%";
        gainKnob.value().setText(text, juce::dontSendNotification);
        gainKnob.slider().setTooltip("Output: " + text);
    };
    gainKnob.slider().setValue(outputGain);
    masterGroup->addKnob(gainKnob);

    configureKnob(glideKnob, "Glide");
    glideKnob.slider().setRange(0.0, 250.0, 1.0);
    glideKnob.slider().setSkewFactorFromMidPoint(40.0);
    glideKnob.slider().setTooltip("Portamento time");
    glideKnob.slider().onValueChange = [this]
    {
        glideTimeMs = (float)glideKnob.slider().getValue();
        const juce::String text = juce::String(glideTimeMs, 0) + " ms";
        glideKnob.value().setText(text, juce::dontSendNotification);
        glideKnob.slider().setTooltip("Glide: " + text);
        updateGlideSmoother();
    };
    glideKnob.slider().setValue(glideTimeMs);
    masterGroup->addKnob(glideKnob);
    glideKnob.setEnabled(monoModeEnabled);
    glideKnob.slider().setEnabled(monoModeEnabled);

    monoToggle.setToggleState(monoModeEnabled, juce::dontSendNotification);
    monoToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    monoToggle.onClick = [this]
    {
        monoModeEnabled = monoToggle.getToggleState();
        glideKnob.setEnabled(monoModeEnabled);
        glideKnob.slider().setEnabled(monoModeEnabled);
        updateGlideSmoother();
    };
    masterGroup->addFooterComponent(monoToggle);
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

void MainComponent::updateGlideSmoother()
{
    const double sr = currentSR > 0.0 ? currentSR : 44100.0;
    const double seconds = monoModeEnabled
        ? juce::jlimit(0.001, 0.6, (double)glideTimeMs * 0.001)
        : 0.002;
    frequencySmoothed.reset(sr, seconds);
    frequencySmoothed.setCurrentAndTargetValue(targetFrequency);
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
