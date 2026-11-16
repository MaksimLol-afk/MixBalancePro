#pragma once

#include <JuceHeader.h>
#include "Analysis/LoudnessAnalyzer.h"

//==============================================================================
/**
    Professional Loudness Meter Visualization

    Displays:
    - Momentary (M) - 400ms
    - Short-term (S) - 3s
    - Integrated (I) - Full session
    - LRA (Loudness Range)
    - True Peak meters
    - Target indicators
*/
class LoudnessMeter  : public juce::Component,
                       public juce::Timer
{
public:
    LoudnessMeter (const LoudnessAnalyzer& analyzer);
    ~LoudnessMeter() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    void setTargetLUFS (float target) { targetLUFS = target; repaint(); }
    void setMeterRange (float rangeDb) { meterRange = rangeDb; repaint(); }

private:
    const LoudnessAnalyzer& analyzer;

    // Meter settings
    float targetLUFS = -14.0f;
    float meterRange = 60.0f;  // dB range to display
    float minLevel = -70.0f;

    // Smoothing for visual meters
    float visualMomentary = -70.0f;
    float visualShortTerm = -70.0f;
    float visualIntegrated = -70.0f;
    float visualTruePeakL = -70.0f;
    float visualTruePeakR = -70.0f;

    // Colors
    juce::Colour backgroundColour = juce::Colour (0xFF0d1117);
    juce::Colour borderColour = juce::Colour (0xFF30363d);
    juce::Colour textColour = juce::Colours::white;
    juce::Colour momentaryColour = juce::Colour (0xFF58a6ff);
    juce::Colour shortTermColour = juce::Colour (0xFFa371f7);
    juce::Colour integratedColour = juce::Colour (0xFF3fb950);
    juce::Colour truePeakColour = juce::Colour (0xFFf85149);
    juce::Colour targetColour = juce::Colour (0xFFd29922);
    juce::Colour warningColour = juce::Colour (0xFFf85149);
    juce::Colour goodColour = juce::Colour (0xFF3fb950);

    // Layout
    struct MeterRects
    {
        juce::Rectangle<int> momentary;
        juce::Rectangle<int> shortTerm;
        juce::Rectangle<int> integrated;
        juce::Rectangle<int> lra;
        juce::Rectangle<int> truePeakL;
        juce::Rectangle<int> truePeakR;
        juce::Rectangle<int> target;
        juce::Rectangle<int> compliance;
    } meterRects;

    //==============================================================================
    void updateVisualMeters();
    void drawMeter (juce::Graphics&, const juce::Rectangle<int>&, float level, float target,
                    const juce::Colour& colour, const juce::String& label, const juce::String& unit);
    void drawLRAIndicator (juce::Graphics&, const juce::Rectangle<int>&, float lra);
    void drawTruePeakMeters (juce::Graphics&);
    void drawComplianceIndicators (juce::Graphics&);
    void layoutMeters();

    float levelToY (float level, const juce::Rectangle<int>& bounds) const;
    juce::Colour getLevelColour (float level, float target) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessMeter)
};