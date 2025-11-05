#include "OscVisualizerComponent.h"

OscVisualizerComponent::OscVisualizerComponent() = default;
OscVisualizerComponent::~OscVisualizerComponent() = default;

void OscVisualizerComponent::setVisualData(float smoothedLevel, float lowBand, float midBand, float highBand,
    float delayFeedback, float glitchEnergy, float driveAmount,
    float delayAmount, float chaosAmount,
    const std::vector<float>& waveformSnapshot)
{
    this->smoothedLevel = smoothedLevel;
    this->lowBand = lowBand;
    this->midBand = midBand;
    this->highBand = highBand;
    this->delayFeedbackEnergy = delayFeedback;
    this->glitchEnergy = glitchEnergy;
    this->driveAmount = driveAmount;
    this->delayAmount = delayAmount;
    this->chaosAmount = chaosAmount;
    this->waveformSnapshot = waveformSnapshot;
    repaint();
}

void OscVisualizerComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    auto visualBounds = getLocalBounds().toFloat();
    juce::ColourGradient background(
        juce::Colour::fromRGB(8, 10, 22), visualBounds.getBottomLeft(),
        juce::Colour::fromRGB(18, 32, 60), visualBounds.getTopRight(), false);
    g.setGradientFill(background);
    g.fillRoundedRectangle(visualBounds, 20.0f);

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(visualBounds, 20.0f, 1.2f);

    auto sphereBounds = visualBounds.reduced(28.0f, 24.0f);
    const float diameter = juce::jmin(sphereBounds.getWidth(), sphereBounds.getHeight());
    if (diameter <= 8.0f)
        return;

    juce::Rectangle<float> sphereArea(
        sphereBounds.getCentreX() - diameter * 0.5f,
        sphereBounds.getCentreY() - diameter * 0.5f,
        diameter, diameter);

    const float hueBase = std::fmod(juce::jmap(driveAmount, 0.0f, 1.0f, 0.62f, 0.02f) + 1.0f, 1.0f);
    const float brightness = juce::jlimit(0.2f, 1.0f,
        juce::jmap(delayAmount, 0.0f, 1.0f, 0.35f, 0.92f) + smoothedLevel * 0.12f);
    const float saturation = juce::jlimit(0.25f, 1.0f,
        juce::jmap(chaosAmount, 0.0f, 1.0f, 0.55f, 0.95f) + highBand * 0.05f);

    juce::ColourGradient sphereGradient(
        juce::Colour::fromHSV(hueBase, saturation,
            juce::jlimit(0.2f, 1.0f, brightness + smoothedLevel * 0.18f), 1.0f),
        sphereArea.getCentre(),
        juce::Colour::fromHSV(std::fmod(hueBase + 0.11f, 1.0f),
            juce::jlimit(0.25f, 1.0f, saturation * 0.65f + midBand * 0.25f),
            juce::jlimit(0.15f, 1.0f, 0.25f + brightness * 0.65f), 1.0f),
        sphereArea.getBottomRight(), true);

    g.setGradientFill(sphereGradient);
    g.fillEllipse(sphereArea);

    g.setColour(juce::Colour::fromHSV(std::fmod(hueBase + 0.02f, 1.0f),
        juce::jlimit(0.25f, 1.0f, saturation * 0.55f + midBand * 0.2f),
        juce::jlimit(0.15f, 1.0f, 0.4f + brightness * 0.35f + smoothedLevel * 0.2f),
        juce::jlimit(0.1f, 0.6f, 0.22f + smoothedLevel * 0.25f)));
    g.drawEllipse(sphereArea, juce::jlimit(0.8f, 1.8f, 1.0f + highBand * 0.6f));

    const auto centre = sphereArea.getCentre();
    const float outerRadius = sphereArea.getWidth() * 0.5f;
    const float activeRadius = outerRadius * juce::jlimit(0.75f, 1.22f, 0.88f + smoothedLevel * 0.35f);
    const float innerRadius = juce::jlimit(outerRadius * 0.18f, activeRadius * 0.92f,
        outerRadius * juce::jmap(juce::jlimit(0.0f, 1.0f, lowBand), 0.0f, 1.0f, 0.3f, 0.48f));

    if (waveformSnapshot.empty())
        return;

    juce::Path waveformPath;
    const size_t count = waveformSnapshot.size();
    const double timeNow = juce::Time::getMillisecondCounterHiRes() * 0.001;

    for (size_t i = 0; i < count; ++i)
    {
        const float angle = juce::MathConstants<float>::twoPi * (float)i / (float)count;
        const float sample = juce::jlimit(-1.0f, 1.0f, waveformSnapshot[i]);
        const float breathing = std::sin(angle * 2.0f + (float)timeNow * 0.9f)
            * midBand * outerRadius * 0.05f;
        const float jitter = std::sin(angle * 5.0f + (float)timeNow * 3.0f)
            * highBand * outerRadius * (0.03f + glitchEnergy * 0.04f);
        const float warpedRadius = juce::jmap(sample, -1.0f, 1.0f, innerRadius, activeRadius)
            + breathing + jitter;
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
    const float trailSat = juce::jlimit(0.25f, 1.0f,
        saturation * 0.7f + midBand * 0.4f + glitchEnergy * 0.2f);
    const float trailVal = juce::jlimit(0.25f, 1.0f,
        0.3f + brightness * 0.7f + smoothedLevel * 0.25f);

    g.setColour(juce::Colour::fromHSV(trailHue, trailSat, trailVal,
        juce::jlimit(0.15f, 0.85f, 0.25f + smoothedLevel * 0.5f + lowBand * 0.15f)));
    g.fillPath(waveformPath);

    g.setColour(juce::Colour::fromHSV(std::fmod(trailHue + 0.02f, 1.0f),
        juce::jlimit(0.2f, 1.0f, trailSat * 0.85f + highBand * 0.25f),
        juce::jlimit(0.3f, 1.0f, trailVal * 0.85f + highBand * 0.2f), 1.0f));
    g.strokePath(waveformPath,
        juce::PathStrokeType(juce::jlimit(1.1f, 3.6f,
            1.3f + highBand * 2.0f + glitchEnergy * 0.7f)));
}
