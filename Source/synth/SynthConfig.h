#pragma once

namespace synth::config
{
    constexpr int defaultWidth = 960;
    constexpr int defaultHeight = 600;
    constexpr int minWidth = 720;
    constexpr int minHeight = 420;

    constexpr int headerBarHeight = 36;
    constexpr int headerMargin = 16;
    constexpr int audioButtonWidth = 96;
    constexpr int audioButtonHeight = 28;
    constexpr int controlStripHeight = 110;
    constexpr int knobSize = 48;
    constexpr int keyboardMinHeight = 60;
    constexpr int scopeTimerHz = 60;

    constexpr int scopeBufferSize = 2048;
    constexpr int waveformResolution = 160;

    constexpr double fastRampSeconds = 0.02;
    constexpr double filterRampSeconds = 0.06;
    constexpr double spatialRampSeconds = 0.1;

    constexpr float minFrequency = 20.0f;
    constexpr float maxFrequency = 20000.0f;
    constexpr float minCutoff = 20.0f;
    constexpr float maxCutoff = 20000.0f;
    constexpr float minResonance = 0.1f;
    constexpr float maxResonance = 12.0f;
}
