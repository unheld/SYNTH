#include "SynthVisualizer.h"

#include "SynthConfig.h"

using namespace synth;

SynthVisualizer::SynthVisualizer(SynthEngine& engineRef)
    : engine(engineRef)
{
}

void SynthVisualizer::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    auto bounds = getLocalBounds();
    int availableHeight = bounds.getHeight();
    int scopeHeight = juce::roundToInt(availableHeight * 0.4f);
    scopeHeight = juce::jlimit(80, availableHeight, scopeHeight);

    const int minVisualHeight = 180;
    if (availableHeight - scopeHeight < minVisualHeight)
        scopeHeight = std::max(std::min(availableHeight, 80), availableHeight - minVisualHeight);
    scopeHeight = std::max(0, std::min(scopeHeight, availableHeight));

    juce::Rectangle<int> scopeArea;
    if (scopeHeight > 0)
        scopeArea = bounds.removeFromBottom(scopeHeight);

    auto osc3DRect = bounds.reduced(12, 12);
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

            juce::ColourGradient sphereGradient(
                juce::Colour::fromRGB(18, 38, 88), sphereArea.getCentre(),
                juce::Colour::fromRGB(3, 6, 16), sphereArea.getBottomRight(), true);
            sphereGradient.addColour(0.1, juce::Colour::fromRGB(24, 70, 140));
            sphereGradient.addColour(0.6, juce::Colour::fromRGB(6, 18, 36));

            g.setGradientFill(sphereGradient);
            g.fillEllipse(sphereArea);

            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.drawEllipse(sphereArea, 1.4f);

            const auto centre = sphereArea.getCentre();
            const float outerRadius = sphereArea.getWidth() * 0.5f;
            const float innerRadius = outerRadius * 0.35f;
            const float activeRadius = outerRadius * 0.92f;

            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.drawEllipse(sphereArea.reduced(outerRadius * 0.18f), 1.0f);

            const auto& waveformSnapshot = engine.getWaveformSnapshot();
            if (!waveformSnapshot.empty())
            {
                juce::Path waveformPath;
                const size_t count = waveformSnapshot.size();

                for (size_t i = 0; i < count; ++i)
                {
                    const float angle = juce::MathConstants<float>::twoPi * (float)i / (float)count;
                    const float sample = juce::jlimit(-1.0f, 1.0f, waveformSnapshot[i]);
                    const float radius = juce::jmap(sample, -1.0f, 1.0f, innerRadius, activeRadius);
                    const float x = centre.x + std::cos(angle) * radius;
                    const float y = centre.y + std::sin(angle) * radius;

                    if (i == 0)
                        waveformPath.startNewSubPath(x, y);
                    else
                        waveformPath.lineTo(x, y);
                }

                waveformPath.closeSubPath();

                g.setColour(juce::Colour::fromFloatRGBA(0.3f, 0.95f, 1.0f, 0.2f));
                g.fillPath(waveformPath);

                g.setColour(juce::Colour::fromFloatRGBA(0.4f, 0.95f, 1.0f, 0.85f));
                g.strokePath(waveformPath, juce::PathStrokeType(1.8f));
            }
        }
    }

    if (!scopeArea.isEmpty())
    {
        auto drawRect = scopeArea.reduced(8, 6);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(drawRect.toFloat(), 8.0f, 1.0f);

        g.setColour(juce::Colours::white.withAlpha(0.85f));
        juce::Path p;

        const int start = engine.findZeroCrossingIndex(engine.getScopeBuffer().getNumSamples() / 2);
        const int W = drawRect.getWidth();
        const int N = engine.getScopeBuffer().getNumSamples();
        const float H = (float)drawRect.getHeight();
        const float Y0 = (float)drawRect.getY();
        const int X0 = drawRect.getX();

        const auto& scopeBuffer = engine.getScopeBuffer();
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

