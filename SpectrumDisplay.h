#pragma once

#include <JuceHeader.h>
#include "Analysis/SpectrumAnalyzer.h"

//==============================================================================
/**
    Real-time Spectrum Display with peak hold and averaging
*/
class SpectrumDisplay  : public juce::Component,
                         public juce::Timer
{
public:
    SpectrumDisplay (const SpectrumAnalyzer& analyzer);
    ~SpectrumDisplay() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    void setShowAverage (bool show) { showAverage = show; repaint(); }
    void setShowPeakHold (bool show) { showPeakHold = show; repaint(); }
    void setShowThirdOctave (bool show) { showThirdOctave = show; repaint(); }
    void setFrequencyRange (float minHz, float maxHz) { minFreq = minHz; maxFreq = maxHz; repaint(); }
    void setDBRange (float minDb, float maxDb) { minDB = minDb; maxDB = maxDb; repaint(); }

private:
    const SpectrumAnalyzer& analyzer;

    // Display options
    bool showAverage = true;
    bool showPeakHold = true;
    bool showThirdOctave = false;
    float minFreq = 20.0f;
    float maxFreq = 20000.0f;
    float minDB = -120.0f;
    float maxDB = 6.0f;

    // Mouse hover
    int hoverBin = -1;
    float hoverFreq = 0.0f;
    float hoverLevel = 0.0f;

    // Layout
    struct Layout
    {
        juce::Rectangle<int> title;
        juce::Rectangle<int> spectrum;
        juce::Rectangle<int> freqAxis;
        juce::Rectangle<int> levelAxis;
        juce::Rectangle<int> info;
    } layout;

    // Colors
    juce::Colour bgColour = juce::Colour (0xFF0d1117);
    juce::Colour gridColour = juce::Colour (0xFF30363d);
    juce::Colour liveColour = juce::Colour (0xFF58a6ff).withAlpha (0.6f);
    juce::Colour avgColour = juce::Colour (0xFF3fb950);
    juce::Colour peakColour = juce::Colour (0xFFf85149);
    juce::Colour thirdOctaveColour = juce::Colour (0xFFa371f7);
    juce::Colour textColour = juce::Colours::white;

    //==============================================================================
    void layoutComponents();
    void drawSpectrum (juce::Graphics&);
    void drawAxes (juce::Graphics&);
    void drawInfo (juce::Graphics&);
    void drawResonanceMarkers (juce::Graphics&);

    float freqToX (float freq, const juce::Rectangle<int>& bounds) const;
    float dbToY (float db, const juce::Rectangle<int>& bounds) const;
    float xToFreq (float x, const juce::Rectangle<int>& bounds) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};