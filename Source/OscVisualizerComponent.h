#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>

class OscVisualizerComponent : public juce::Component
{
public:
    OscVisualizerComponent();
    ~OscVisualizerComponent() override;

    void setVisualData(float smoothedLevel, float lowBand, float midBand, float highBand,
                       float delayFeedback, float glitchEnergy, float driveAmount,
                       float delayAmount, float chaosAmount,
                       const std::vector<float>& waveformSnapshot);

    void paint(juce::Graphics& g) override;

private:
    float smoothedLevel = 0.0f;
    float lowBand = 0.0f;
    float midBand = 0.0f;
    float highBand = 0.0f;
    float delayFeedbackEnergy = 0.0f;
    float glitchEnergy = 0.0f;
    float driveAmount = 0.0f;
    float delayAmount = 0.0f;
    float chaosAmount = 0.0f;
    std::vector<float> waveformSnapshot;
};
