#pragma once

#include <JuceHeader.h>

#include "SynthEngine.h"
#include "SynthConfig.h"

class SynthUI : public juce::Component
{
public:
    explicit SynthUI(SynthEngine& engineRef);

    void resized() override;

    void refreshFromEngine();

    juce::Slider& getWaveSlider() { return waveKnob; }
    juce::Slider& getGainSlider() { return gainKnob; }
    juce::Slider& getAttackSlider() { return attackKnob; }
    juce::Slider& getDecaySlider() { return decayKnob; }
    juce::Slider& getSustainSlider() { return sustainKnob; }
    juce::Slider& getReleaseSlider() { return releaseKnob; }
    juce::Slider& getWidthSlider() { return widthKnob; }
    juce::Slider& getPitchSlider() { return pitchKnob; }
    juce::Slider& getCutoffSlider() { return cutoffKnob; }
    juce::Slider& getResonanceSlider() { return resonanceKnob; }
    juce::Slider& getLfoRateSlider() { return lfoKnob; }
    juce::Slider& getLfoDepthSlider() { return lfoDepthKnob; }
    juce::Slider& getFilterModSlider() { return filterModKnob; }
    juce::Slider& getLfoModeSlider() { return lfoModeKnob; }
    juce::Slider& getLfoStartSlider() { return lfoStartKnob; }
    juce::Slider& getDriveSlider() { return driveKnob; }
    juce::Slider& getCrushSlider() { return crushKnob; }
    juce::Slider& getSubMixSlider() { return subMixKnob; }
    juce::Slider& getEnvFilterSlider() { return envFilterKnob; }
    juce::Slider& getChaosSlider() { return chaosKnob; }
    juce::Slider& getDelaySlider() { return delayKnob; }
    juce::Slider& getAutoPanSlider() { return autoPanKnob; }
    juce::Slider& getGlitchSlider() { return glitchKnob; }

private:
    void initialiseControls();
    void configureRotarySlider(juce::Slider& slider);
    void configureCaptionLabel(juce::Label& label, const juce::String& text);
    void configureValueLabel(juce::Label& label);

    SynthEngine& engine;

    juce::Slider waveKnob, gainKnob, attackKnob, decayKnob, sustainKnob, widthKnob;
    juce::Slider pitchKnob, cutoffKnob, resonanceKnob, releaseKnob;
    juce::Slider lfoKnob, lfoDepthKnob, filterModKnob, lfoModeKnob, lfoStartKnob;
    juce::Slider driveKnob, crushKnob, subMixKnob, envFilterKnob;
    juce::Slider chaosKnob, delayKnob, autoPanKnob, glitchKnob;

    juce::Label waveLabel, waveValue;
    juce::Label gainLabel, gainValue;
    juce::Label attackLabel, attackValue;
    juce::Label decayLabel, decayValue;
    juce::Label sustainLabel, sustainValue;
    juce::Label widthLabel, widthValue;
    juce::Label pitchLabel, pitchValue;
    juce::Label cutoffLabel, cutoffValue;
    juce::Label resonanceLabel, resonanceValue;
    juce::Label releaseLabel, releaseValue;
    juce::Label lfoLabel, lfoValue;
    juce::Label lfoDepthLabel, lfoDepthValue;
    juce::Label filterModLabel, filterModValue;
    juce::Label lfoModeLabel, lfoModeValue;
    juce::Label lfoStartLabel, lfoStartValue;
    juce::Label driveLabel, driveValue;
    juce::Label crushLabel, crushValue;
    juce::Label subMixLabel, subMixValue;
    juce::Label envFilterLabel, envFilterValue;
    juce::Label chaosLabel, chaosValueLabel;
    juce::Label delayLabel, delayValue;
    juce::Label autoPanLabel, autoPanValue;
    juce::Label glitchLabel, glitchValue;
};

