#include "ParameterKnob.h"

ParameterKnob::ParameterKnob(const juce::String& caption)
{
    captionLabel.setText(caption, juce::dontSendNotification);
    captionLabel.setJustificationType(juce::Justification::centred);
    captionLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    addAndMakeVisible(captionLabel);

    addAndMakeVisible(knob);

    valueLabel.setJustificationType(juce::Justification::centred);
    valueLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    addAndMakeVisible(valueLabel);
}

void ParameterKnob::setCaption(const juce::String& text)
{
    captionLabel.setText(text, juce::dontSendNotification);
}

void ParameterKnob::setTextColour(juce::Colour colour)
{
    captionLabel.setColour(juce::Label::textColourId, colour);
    valueLabel.setColour(juce::Label::textColourId, colour);
}

void ParameterKnob::resized()
{
    auto bounds = getLocalBounds();
    auto captionArea = bounds.removeFromTop(16);
    captionLabel.setBounds(captionArea);

    auto valueArea = bounds.removeFromBottom(16);
    valueLabel.setBounds(valueArea);

    knob.setBounds(bounds.reduced(4));
}
