#pragma once

#include <JuceHeader.h>
#include "Analysis/DynamicRangeAnalyzer.h"

//==============================================================================
/**
    Dynamic Range Display

    Shows:
    - DR meter (TT Dynamic Range style)
    - Crest factor history
    - Micro/macro dynamics
    - Compression detection
    - PLR (Peak to Loudness Ratio)
*/
class DynamicRangeDisplay  : public juce::Component,
                             public juce::Timer
{
public:
    DynamicRangeDisplay (const DynamicRangeAnalyzer& analyzer);
    ~DynamicRangeDisplay() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    void setHistoryLength (int seconds) { historySeconds = seconds; }

private:
    const DynamicRangeAnalyzer& analyzer;

    int historySeconds = 60;
    int maxHistoryPoints = 300;

    // Colors
    juce::Colour bgColour = juce::Colour (0xFF0d1117);
    juce::Colour gridColour = juce::Colour (0xFF30363d);
    juce::Colour drGoodColour = juce::Colour (0xFF3fb950);
    juce::Colour drWarnColour = juce::Colour (0xFFd29922);
    juce::Colour drBadColour = juce::Colour (0xFFf85149);
    juce::Colour crestColour = juce::Colour (0xFF58a6ff);
    juce::Colour rmsColour = juce::Colour (0xFFa371f7);
    juce::Colour peakColour = juce::Colour (0xFFf85149);
    juce::Colour textColour = juce::Colours::white;

    // Layout
    struct Layout
    {
        juce::Rectangle<int> title;
        juce::Rectangle<int> drMeter;
        juce::Rectangle<int> history;
        juce::Rectangle<int> metrics;
        juce::Rectangle<int> assessment;
    } layout;

    //==============================================================================
    void layoutComponents();
    void drawDRMeter (juce::Graphics&);
    void drawHistory (juce::Graphics&);
    void drawMetrics (juce::Graphics&);
    void drawAssessment (juce::Graphics&);
    juce::Colour getDRColour (float drValue) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicRangeDisplay)
};