#pragma once

#include <JuceHeader.h>

#include "SynthEngine.h"

class SynthVisualizer : public juce::Component
{
public:
    explicit SynthVisualizer(SynthEngine& engineRef);

    void paint(juce::Graphics& g) override;

private:
    SynthEngine& engine;
};

