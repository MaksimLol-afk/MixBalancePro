#pragma once

#include <JuceHeader.h>
#include "Analysis/ConflictDetector.h"

//==============================================================================
/**
    Frequency Conflict Heatmap Visualization

    Shows conflicts between tracks as a 2D heatmap:
    - X axis: Frequency (log scale)
    - Y axis: Track pairs
    - Color: Conflict severity
    - Interactive: Click for details
*/
class ConflictHeatmap  : public juce::Component,
                         public juce::Timer
{
public:
    ConflictHeatmap (const ConflictDetector& detector);
    ~ConflictHeatmap() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    void setShowTrackNames (bool show) { showTrackNames = show; repaint(); }
    void setFrequencyScale (bool logScale) { useLogScale = logScale; repaint(); }

private:
    const ConflictDetector& detector;

    // Display settings
    bool showTrackNames = true;
    bool useLogScale = true;
    float minSeverity = 0.2f;

    // Hover state
    int hoverTrackPair = -1;
    int hoverBand = -1;
    ConflictDetector::FrequencyConflict hoverConflict;

    // Layout
    struct Layout
    {
        juce::Rectangle<int> title;
        juce::Rectangle<int> heatmap;
        juce::Rectangle<int> freqAxis;
        juce::Rectangle<int> trackAxis;
        juce::Rectangle<int> legend;
        juce::Rectangle<int> tooltip;
    } layout;

    // Cached conflicts for rendering
    struct CachedConflict
    {
        int trackAIndex = 0;
        int trackBIndex = 0;
        int band = 0;
        float severity = 0.0f;
        float freq = 0.0f;
        ConflictDetector::FrequencyConflict::ConflictType type = ConflictDetector::FrequencyConflict::ConflictType::SpectralOverlap;
        juce::String trackAName;
        juce::String trackBName;
    };
    juce::Array<CachedConflict> cachedConflicts;
    juce::Array<juce::String> trackNames;

    //==============================================================================
    void updateCachedConflicts();
    void layoutComponents();
    void drawHeatmap (juce::Graphics&);
    void drawAxes (juce::Graphics&);
    void drawLegend (juce::Graphics&);
    void drawTooltip (juce::Graphics&);

    float freqToX (float freq, const juce::Rectangle<int>& bounds) const;
    int xToBand (float x, const juce::Rectangle<int>& bounds) const;
    juce::Colour severityToColour (float severity) const;
    juce::String getTypeIcon (ConflictDetector::FrequencyConflict::ConflictType type) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConflictHeatmap)
};