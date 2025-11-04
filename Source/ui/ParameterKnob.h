#pragma once

#include <JuceHeader.h>

class ParameterKnob : public juce::Component
{
public:
    ParameterKnob(const juce::String& caption = {});

    void setCaption(const juce::String& text);

    juce::Slider& slider() noexcept { return knob; }
    const juce::Slider& slider() const noexcept { return knob; }

    juce::Label& value() noexcept { return valueLabel; }
    const juce::Label& value() const noexcept { return valueLabel; }

    void setTextColour(juce::Colour colour);

    void resized() override;

private:
    juce::Label captionLabel;
    juce::Slider knob;
    juce::Label valueLabel;
};
