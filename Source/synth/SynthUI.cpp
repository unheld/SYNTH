#include "SynthUI.h"

using namespace synth;

SynthUI::SynthUI(SynthEngine& engineRef)
    : engine(engineRef)
{
    initialiseControls();
    refreshFromEngine();
}

void SynthUI::resized()
{
    auto area = getLocalBounds();
    const int knob = config::knobSize;
    const int numKnobs = 23;
    const int colWidth = area.getWidth() / numKnobs;

    struct Item { juce::Label* caption; juce::Slider* knob; juce::Label* value; };
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
    const int labelY = area.getY();
    const int knobY = labelY + labelH + 2;
    const int valueY = knobY + knob + 2;

    for (int i = 0; i < numKnobs; ++i)
    {
        auto column = area.withTrimmedLeft(i * colWidth).withWidth(colWidth);
        const int centreX = column.getCentreX();

        if (auto* caption = items[i].caption)
            caption->setBounds(centreX - colWidth / 2, labelY, colWidth, labelH);

        if (auto* knobPtr = items[i].knob)
            knobPtr->setBounds(centreX - knob / 2, knobY, knob, knob);

        if (auto* value = items[i].value)
            value->setBounds(centreX - colWidth / 2, valueY, colWidth, valueH);
    }
}

void SynthUI::refreshFromEngine()
{
    waveKnob.setValue(engine.getWaveMorph(), juce::dontSendNotification);
    waveKnob.onValueChange();

    gainKnob.setValue(engine.getOutputGain(), juce::dontSendNotification);
    gainKnob.onValueChange();

    attackKnob.setValue(engine.getAttack(), juce::dontSendNotification);
    attackKnob.onValueChange();

    decayKnob.setValue(engine.getDecay(), juce::dontSendNotification);
    decayKnob.onValueChange();

    sustainKnob.setValue(engine.getSustain(), juce::dontSendNotification);
    sustainKnob.onValueChange();

    widthKnob.setValue(engine.getStereoWidth(), juce::dontSendNotification);
    widthKnob.onValueChange();

    pitchKnob.setValue(engine.getTargetFrequency(), juce::dontSendNotification);
    pitchKnob.onValueChange();

    cutoffKnob.setValue(engine.getCutoff(), juce::dontSendNotification);
    cutoffKnob.onValueChange();

    resonanceKnob.setValue(engine.getResonance(), juce::dontSendNotification);
    resonanceKnob.onValueChange();

    releaseKnob.setValue(engine.getRelease(), juce::dontSendNotification);
    releaseKnob.onValueChange();

    lfoKnob.setValue(engine.getLfoRate(), juce::dontSendNotification);
    lfoKnob.onValueChange();

    lfoDepthKnob.setValue(engine.getLfoDepth(), juce::dontSendNotification);
    lfoDepthKnob.onValueChange();

    filterModKnob.setValue(engine.getFilterMod(), juce::dontSendNotification);
    filterModKnob.onValueChange();

    lfoModeKnob.setValue(engine.isLfoFreeRunning() ? 1.0 : 0.0, juce::dontSendNotification);
    lfoModeKnob.onValueChange();

    lfoStartKnob.setValue(engine.getLfoStart(), juce::dontSendNotification);
    lfoStartKnob.onValueChange();

    driveKnob.setValue(engine.getDrive(), juce::dontSendNotification);
    driveKnob.onValueChange();

    crushKnob.setValue(engine.getCrush(), juce::dontSendNotification);
    crushKnob.onValueChange();

    subMixKnob.setValue(engine.getSubMix(), juce::dontSendNotification);
    subMixKnob.onValueChange();

    envFilterKnob.setValue(engine.getEnvelopeFilter(), juce::dontSendNotification);
    envFilterKnob.onValueChange();

    chaosKnob.setValue(engine.getChaos(), juce::dontSendNotification);
    chaosKnob.onValueChange();

    delayKnob.setValue(engine.getDelay(), juce::dontSendNotification);
    delayKnob.onValueChange();

    autoPanKnob.setValue(engine.getAutoPan(), juce::dontSendNotification);
    autoPanKnob.onValueChange();

    glitchKnob.setValue(engine.getGlitch(), juce::dontSendNotification);
    glitchKnob.onValueChange();
}

void SynthUI::initialiseControls()
{
    configureRotarySlider(waveKnob);
    waveKnob.setRange(0.0, 1.0);
    addAndMakeVisible(waveKnob);
    configureCaptionLabel(waveLabel, "Waveform");
    configureValueLabel(waveValue);
    waveKnob.onValueChange = [this]
    {
        engine.setWaveMorph((float)waveKnob.getValue());
        waveValue.setText(juce::String(engine.getWaveMorph(), 2), juce::dontSendNotification);
    };

    configureRotarySlider(gainKnob);
    gainKnob.setRange(0.0, 1.0);
    addAndMakeVisible(gainKnob);
    configureCaptionLabel(gainLabel, "Gain");
    configureValueLabel(gainValue);
    gainKnob.onValueChange = [this]
    {
        engine.setOutputGain((float)gainKnob.getValue());
        gainValue.setText(juce::String(engine.getOutputGain() * 100.0f, 0) + "%", juce::dontSendNotification);
    };

    configureRotarySlider(attackKnob);
    attackKnob.setRange(0.0, 2000.0, 1.0);
    attackKnob.setSkewFactorFromMidPoint(40.0);
    addAndMakeVisible(attackKnob);
    configureCaptionLabel(attackLabel, "Attack");
    configureValueLabel(attackValue);
    attackKnob.onValueChange = [this]
    {
        engine.setAttack((float)attackKnob.getValue());
        attackValue.setText(juce::String(engine.getAttack(), 0) + " ms", juce::dontSendNotification);
    };

    configureRotarySlider(decayKnob);
    decayKnob.setRange(5.0, 4000.0, 1.0);
    decayKnob.setSkewFactorFromMidPoint(200.0);
    addAndMakeVisible(decayKnob);
    configureCaptionLabel(decayLabel, "Decay");
    configureValueLabel(decayValue);
    decayKnob.onValueChange = [this]
    {
        engine.setDecay((float)decayKnob.getValue());
        decayValue.setText(juce::String(engine.getDecay(), 0) + " ms", juce::dontSendNotification);
    };

    configureRotarySlider(sustainKnob);
    sustainKnob.setRange(0.0, 1.0, 0.01);
    addAndMakeVisible(sustainKnob);
    configureCaptionLabel(sustainLabel, "Sustain");
    configureValueLabel(sustainValue);
    sustainKnob.onValueChange = [this]
    {
        engine.setSustain((float)sustainKnob.getValue());
        sustainValue.setText(juce::String(engine.getSustain() * 100.0f, 0) + "%", juce::dontSendNotification);
    };

    configureRotarySlider(widthKnob);
    widthKnob.setRange(0.0, 2.0, 0.01);
    addAndMakeVisible(widthKnob);
    configureCaptionLabel(widthLabel, "Width");
    configureValueLabel(widthValue);
    widthKnob.onValueChange = [this]
    {
        engine.setStereoWidth((float)widthKnob.getValue());
        widthValue.setText(juce::String(engine.getStereoWidth(), 2) + "x", juce::dontSendNotification);
    };

    configureRotarySlider(pitchKnob);
    pitchKnob.setRange(40.0, 5000.0);
    pitchKnob.setSkewFactorFromMidPoint(440.0);
    addAndMakeVisible(pitchKnob);
    configureCaptionLabel(pitchLabel, "Pitch");
    configureValueLabel(pitchValue);
    pitchKnob.onValueChange = [this]
    {
        engine.setTargetFrequency((float)pitchKnob.getValue());
        pitchValue.setText(juce::String(engine.getTargetFrequency(), 1) + " Hz", juce::dontSendNotification);
    };

    configureRotarySlider(cutoffKnob);
    cutoffKnob.setRange(80.0, 10000.0, 1.0);
    cutoffKnob.setSkewFactorFromMidPoint(1000.0);
    addAndMakeVisible(cutoffKnob);
    configureCaptionLabel(cutoffLabel, "Cutoff");
    configureValueLabel(cutoffValue);
    cutoffKnob.onValueChange = [this]
    {
        engine.setCutoff((float)cutoffKnob.getValue());
        cutoffValue.setText(juce::String(engine.getCutoff(), 1) + " Hz", juce::dontSendNotification);
    };

    configureRotarySlider(resonanceKnob);
    resonanceKnob.setRange(0.1, 10.0, 0.01);
    resonanceKnob.setSkewFactorFromMidPoint(0.707);
    addAndMakeVisible(resonanceKnob);
    configureCaptionLabel(resonanceLabel, "Resonance (Q)");
    configureValueLabel(resonanceValue);
    resonanceKnob.onValueChange = [this]
    {
        engine.setResonance((float)resonanceKnob.getValue());
        resonanceValue.setText(juce::String(engine.getResonance(), 2), juce::dontSendNotification);
    };

    configureRotarySlider(releaseKnob);
    releaseKnob.setRange(1.0, 4000.0, 1.0);
    releaseKnob.setSkewFactorFromMidPoint(200.0);
    addAndMakeVisible(releaseKnob);
    configureCaptionLabel(releaseLabel, "Release");
    configureValueLabel(releaseValue);
    releaseKnob.onValueChange = [this]
    {
        engine.setRelease((float)releaseKnob.getValue());
        releaseValue.setText(juce::String(engine.getRelease(), 0) + " ms", juce::dontSendNotification);
    };

    configureRotarySlider(lfoKnob);
    lfoKnob.setRange(0.05, 15.0);
    addAndMakeVisible(lfoKnob);
    configureCaptionLabel(lfoLabel, "LFO Rate");
    configureValueLabel(lfoValue);
    lfoKnob.onValueChange = [this]
    {
        engine.setLfoRate((float)lfoKnob.getValue());
        lfoValue.setText(juce::String(engine.getLfoRate(), 2) + " Hz", juce::dontSendNotification);
    };

    configureRotarySlider(lfoDepthKnob);
    lfoDepthKnob.setRange(0.0, 1.0);
    addAndMakeVisible(lfoDepthKnob);
    configureCaptionLabel(lfoDepthLabel, "LFO Depth");
    configureValueLabel(lfoDepthValue);
    lfoDepthKnob.onValueChange = [this]
    {
        engine.setLfoDepth((float)lfoDepthKnob.getValue());
        lfoDepthValue.setText(juce::String(engine.getLfoDepth(), 2), juce::dontSendNotification);
    };

    configureRotarySlider(filterModKnob);
    filterModKnob.setRange(0.0, 1.0, 0.001);
    addAndMakeVisible(filterModKnob);
    configureCaptionLabel(filterModLabel, "Filter Mod");
    configureValueLabel(filterModValue);
    filterModKnob.onValueChange = [this]
    {
        engine.setFilterMod((float)filterModKnob.getValue());
        filterModValue.setText(juce::String(engine.getFilterMod(), 2), juce::dontSendNotification);
    };

    configureRotarySlider(lfoModeKnob);
    lfoModeKnob.setRange(0.0, 1.0, 1.0);
    addAndMakeVisible(lfoModeKnob);
    configureCaptionLabel(lfoModeLabel, "LFO Mode");
    configureValueLabel(lfoModeValue);
    lfoModeKnob.onValueChange = [this]
    {
        const bool freeRun = juce::approximatelyEqual(lfoModeKnob.getValue(), 1.0);
        engine.setLfoMode(freeRun);
        lfoModeValue.setText(freeRun ? "Loop" : "Retrig", juce::dontSendNotification);
    };

    configureRotarySlider(lfoStartKnob);
    lfoStartKnob.setRange(0.0, 1.0, 0.001);
    addAndMakeVisible(lfoStartKnob);
    configureCaptionLabel(lfoStartLabel, "LFO Start");
    configureValueLabel(lfoStartValue);
    lfoStartKnob.onValueChange = [this]
    {
        engine.setLfoStart((float)lfoStartKnob.getValue());
        const int degrees = juce::roundToInt(engine.getLfoStart() * 360.0f);
        lfoStartValue.setText(juce::String(degrees) + juce::String::charToString(0x00B0), juce::dontSendNotification);
    };

    configureRotarySlider(driveKnob);
    driveKnob.setRange(0.0, 1.0);
    addAndMakeVisible(driveKnob);
    configureCaptionLabel(driveLabel, "Drive");
    configureValueLabel(driveValue);
    driveKnob.onValueChange = [this]
    {
        engine.setDrive((float)driveKnob.getValue());
        driveValue.setText(juce::String(engine.getDrive(), 2), juce::dontSendNotification);
    };

    configureRotarySlider(crushKnob);
    crushKnob.setRange(0.0, 1.0);
    addAndMakeVisible(crushKnob);
    configureCaptionLabel(crushLabel, "Crush");
    configureValueLabel(crushValue);
    crushKnob.onValueChange = [this]
    {
        engine.setCrush((float)crushKnob.getValue());
        crushValue.setText(juce::String(engine.getCrush() * 100.0f, 0) + "%", juce::dontSendNotification);
    };

    configureRotarySlider(subMixKnob);
    subMixKnob.setRange(0.0, 1.0);
    addAndMakeVisible(subMixKnob);
    configureCaptionLabel(subMixLabel, "Sub Mix");
    configureValueLabel(subMixValue);
    subMixKnob.onValueChange = [this]
    {
        engine.setSubMix((float)subMixKnob.getValue());
        subMixValue.setText(juce::String(engine.getSubMix() * 100.0f, 0) + "%", juce::dontSendNotification);
    };

    configureRotarySlider(envFilterKnob);
    envFilterKnob.setRange(-1.0, 1.0, 0.01);
    addAndMakeVisible(envFilterKnob);
    configureCaptionLabel(envFilterLabel, "Env→Cutoff");
    configureValueLabel(envFilterValue);
    envFilterKnob.onValueChange = [this]
    {
        engine.setEnvelopeFilter((float)envFilterKnob.getValue());
        envFilterValue.setText(juce::String(engine.getEnvelopeFilter(), 2), juce::dontSendNotification);
    };

    configureRotarySlider(chaosKnob);
    chaosKnob.setRange(0.0, 1.0);
    addAndMakeVisible(chaosKnob);
    configureCaptionLabel(chaosLabel, "Chaos");
    configureValueLabel(chaosValueLabel);
    chaosKnob.onValueChange = [this]
    {
        engine.setChaos((float)chaosKnob.getValue());
        chaosValueLabel.setText(juce::String(engine.getChaos() * 100.0f, 0) + "%", juce::dontSendNotification);
    };

    configureRotarySlider(delayKnob);
    delayKnob.setRange(0.0, 1.0);
    addAndMakeVisible(delayKnob);
    configureCaptionLabel(delayLabel, "Delay");
    configureValueLabel(delayValue);
    delayKnob.onValueChange = [this]
    {
        engine.setDelay((float)delayKnob.getValue());
        delayValue.setText(juce::String(engine.getDelay() * 100.0f, 0) + "%", juce::dontSendNotification);
    };

    configureRotarySlider(autoPanKnob);
    autoPanKnob.setRange(0.0, 1.0);
    addAndMakeVisible(autoPanKnob);
    configureCaptionLabel(autoPanLabel, "AutoPan");
    configureValueLabel(autoPanValue);
    autoPanKnob.onValueChange = [this]
    {
        engine.setAutoPan((float)autoPanKnob.getValue());
        autoPanValue.setText(juce::String(engine.getAutoPan() * 100.0f, 0) + "%", juce::dontSendNotification);
    };

    configureRotarySlider(glitchKnob);
    glitchKnob.setRange(0.0, 1.0);
    addAndMakeVisible(glitchKnob);
    configureCaptionLabel(glitchLabel, "Glitch");
    configureValueLabel(glitchValue);
    glitchKnob.onValueChange = [this]
    {
        engine.setGlitch((float)glitchKnob.getValue());
        glitchValue.setText(juce::String(engine.getGlitch() * 100.0f, 0) + "%", juce::dontSendNotification);
    };
}

void SynthUI::configureRotarySlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
        juce::MathConstants<float>::pi * 2.8f, true);
}

void SynthUI::configureCaptionLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(label);
}

void SynthUI::configureValueLabel(juce::Label& label)
{
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(label);
}

