#include "DynamicRangeDisplay.h"

//==============================================================================
DynamicRangeDisplay::DynamicRangeDisplay (const DynamicRangeAnalyzer& a)
    : analyzer (a)
{
    startTimerHz (10);  // Lower refresh rate for history
}

DynamicRangeDisplay::~DynamicRangeDisplay()
{
    stopTimer();
}

void DynamicRangeDisplay::resized()
{
    layoutComponents();
}

void DynamicRangeDisplay::timerCallback()
{
    repaint();
}

void DynamicRangeDisplay::layoutComponents()
{
    auto bounds = getLocalBounds().reduced (10);

    layout.title = bounds.removeFromTop (25);
    layout.drMeter = bounds.removeFromTop (80);
    layout.history = bounds.removeFromTop (bounds.getHeight() / 2);
    layout.metrics = bounds.removeFromTop (80);
    layout.assessment = bounds;
}

//==============================================================================
void DynamicRangeDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.fillAll (bgColour);
    g.setColour (gridColour);
    g.drawRect (bounds, 1);

    // Title
    g.setColour (textColour);
    g.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
    g.drawText ("DYNAMIC RANGE", layout.title, juce::Justification::centredLeft);

    drawDRMeter (g);
    drawHistory (g);
    drawMetrics (g);
    drawAssessment (g);
}

//==============================================================================
void DynamicRangeDisplay::drawDRMeter (juce::Graphics& g)
{
    auto m = analyzer.getLatestMeasurement();
    auto bounds = layout.drMeter.reduced (10);

    // DR Value display (large number in center)
    juce::Colour drColour = getDRColour (m.drValue);

    g.setColour (drColour);
    g.setFont (juce::FontOptions (48.0f).withStyle ("Bold"));
    g.drawText (juce::String (m.drValue, 1),
                bounds.removeFromLeft (bounds.getWidth() / 3),
                juce::Justification::centred);

    // DR Scale (1-20)
    auto scaleBounds = bounds.removeFromLeft (bounds.getWidth() / 2).reduced (10);
    int numSteps = 20;
    float stepWidth = static_cast<float> (scaleBounds.getWidth()) / numSteps;

    for (int i = 1; i <= 20; ++i)
    {
        float x = scaleBounds.getX() + (i - 1) * stepWidth;
        juce::Colour stepColour = getDRColour (static_cast<float> (i));

        juce::Rectangle<float> step (x, scaleBounds.getY() + 10, stepWidth - 1, scaleBounds.getHeight() - 20);
        g.setColour (i <= m.drValue ? stepColour : stepColour.withAlpha (0.2f));
        g.fillRect (step);
    }

    // Labels
    g.setColour (textColour.withAlpha (0.7f));
    g.setFont (juce::FontOptions (9.0f));
    g.drawText ("DR 1", scaleBounds.getX(), scaleBounds.getBottom() + 2, 30, 14, juce::Justification::centred);
    g.drawText ("DR 20", scaleBounds.getRight() - 30, scaleBounds.getBottom() + 2, 40, 14, juce::Justification::centred);
    g.drawText ("COMPRESSED", scaleBounds.getX(), scaleBounds.getY() - 18, scaleBounds.getWidth() / 2, 14, juce::Justification::centredLeft);
    g.drawText ("DYNAMIC", scaleBounds.getCentreX(), scaleBounds.getY() - 18, scaleBounds.getWidth() / 2, 14, juce::Justification::centredRight);

    // Current DR marker
    float markerX = scaleBounds.getX() + (m.drValue - 1) / 19.0f * scaleBounds.getWidth();
    g.setColour (drColour);
    juce::Path triangle;
    triangle.startNewSubPath (markerX, scaleBounds.getY() - 5);
    triangle.lineTo (markerX - 5, scaleBounds.getY() + 5);
    triangle.lineTo (markerX + 5, scaleBounds.getY() + 5);
    triangle.closeSubPath();
    g.fillPath (triangle);

    // PLR display
    auto plrBounds = bounds;
    g.setColour (textColour);
    g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
    g.drawText ("PLR: " + juce::String (m.plr, 1) + " dB",
                plrBounds.removeFromTop (30), juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("RMS: " + juce::String (m.rmsLevel, 1) + " dB  Peak: " + juce::String (m.peakLevel, 1) +
                " dB  True Peak: " + juce::String (m.truePeakLevel, 1) + " dBTP",
                plrBounds, juce::Justification::centredLeft);
}

void DynamicRangeDisplay::drawHistory (juce::Graphics& g)
{
    auto m = analyzer.getLatestMeasurement();
    auto bounds = layout.history.reduced (10);

    // Background
    g.setColour (bgColour.brighter (0.1f));
    g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

    // Grid
    g.setColour (gridColour.withAlpha (0.3f));
    for (int i = 1; i < 4; ++i)
    {
        float y = bounds.getY() + i * bounds.getHeight() / 4.0f;
        g.drawHorizontalLine (static_cast<int> (y), bounds.getX(), bounds.getRight());
    }
    for (int i = 1; i < 10; ++i)
    {
        float x = bounds.getX() + i * bounds.getWidth() / 10.0f;
        g.drawVerticalLine (static_cast<int> (x), bounds.getY(), bounds.getBottom());
    }

    // Draw crest factor history
    auto& history = m.crestHistory;
    if (history.size() > 1)
    {
        juce::Path crestPath;
        bool started = false;

        int pointsToShow = juce::jmin (maxHistoryPoints, history.size());
        int startIdx = history.size() - pointsToShow;

        for (int i = 0; i < pointsToShow; ++i)
        {
            float val = history[(startIdx + i) % history.size()];
            if (val <= 0) continue;

            float x = bounds.getX() + static_cast<float> (i) / (pointsToShow - 1) * bounds.getWidth();
            float y = bounds.getBottom() - juce::jlimit (0.0f, 1.0f, val / 30.0f) * bounds.getHeight();

            if (!started)
            {
                crestPath.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                crestPath.lineTo (x, y);
            }
        }

        if (started)
        {
            g.setColour (crestColour.withAlpha (0.7f));
            g.strokePath (crestPath, juce::PathStrokeType (1.5f));
        }
    }

    // Draw RMS history
    auto& rmsHist = m.rmsHistory;
    if (rmsHist.size() > 1)
    {
        juce::Path rmsPath;
        bool started = false;

        int pointsToShow = juce::jmin (maxHistoryPoints, rmsHist.size());
        int startIdx = rmsHist.size() - pointsToShow;

        for (int i = 0; i < pointsToShow; ++i)
        {
            float val = rmsHist[(startIdx + i) % rmsHist.size()];
            if (val <= -70.0f) continue;

            float x = bounds.getX() + static_cast<float> (i) / (pointsToShow - 1) * bounds.getWidth();
            float y = bounds.getBottom() - juce::jlimit (0.0f, 1.0f, (val + 60.0f) / 60.0f) * bounds.getHeight();

            if (!started)
            {
                rmsPath.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                rmsPath.lineTo (x, y);
            }
        }

        if (started)
        {
            g.setColour (rmsColour.withAlpha (0.5f));
            g.strokePath (rmsPath, juce::PathStrokeType (1.0f));
        }
    }

    // Labels
    g.setColour (textColour.withAlpha (0.7f));
    g.setFont (juce::FontOptions (9.0f));
    g.drawText ("Crest Factor (dB)", bounds.getX(), bounds.getY() - 18, bounds.getWidth() / 2, 14, juce::Justification::centredLeft);
    g.drawText ("RMS Level (dBFS)", bounds.getCentreX(), bounds.getY() - 18, bounds.getWidth() / 2, 14, juce::Justification::centredRight);
}

void DynamicRangeDisplay::drawMetrics (juce::Graphics& g)
{
    auto m = analyzer.getLatestMeasurement();
    auto bounds = layout.metrics.reduced (10);

    g.setColour (textColour);
    g.setFont (juce::FontOptions (10.0f));

    // Left column
    float y = bounds.getY();
    float lineH = 20;

    struct Metric { juce::String label; juce::String value; juce::Colour color; };
    juce::Array<Metric> leftMetrics =
    {
        { "Crest Factor", juce::String (m.crestFactorDB, 1) + " dB", crestColour },
        { "True Peak Crest", juce::String (20.0f * std::log10 (m.truePeakCrestFactor), 1) + " dB", peakColour },
        { "Micro Dynamics", juce::String (m.microDynamics, 1) + " dB", juce::Colour (0xFFa371f7) },
        { "Macro Dynamics", juce::String (m.macroDynamics, 1) + " dB", juce::Colour (0xFFd29922) },
    };

    for (auto& met : leftMetrics)
    {
        g.setColour (met.color);
        g.drawText (met.label, bounds.getX(), y, bounds.getWidth() / 2, lineH, juce::Justification::centredLeft);
        g.setColour (textColour);
        g.drawText (met.value, bounds.getX() + bounds.getWidth() / 2, y, bounds.getWidth() / 2, lineH, juce::Justification::centredRight);
        y += lineH;
    }

    // Right column - compression info
    y = bounds.getY();
    g.setColour (textColour);
    g.drawText ("Est. Comp. Ratio", bounds.getCentreX(), y, bounds.getWidth() / 2, lineH, juce::Justification::centredLeft);
    g.setColour (m.likelyOvercompressed ? drBadColour : drGoodColour);
    g.drawText (juce::String (m.estimatedCompressionRatio, 1) + ":1",
                bounds.getCentreX() + bounds.getWidth() / 2, y, bounds.getWidth() / 2, lineH, juce::Justification::centredRight);

    y += lineH;
    g.setColour (textColour);
    g.drawText ("Section Variation", bounds.getCentreX(), y, bounds.getWidth() / 2, lineH, juce::Justification::centredLeft);
    g.setColour (textColour);
    g.drawText (juce::String (m.sectionLevelVariation, 1) + " dB",
                bounds.getCentreX() + bounds.getWidth() / 2, y, bounds.getWidth() / 2, lineH, juce::Justification::centredRight);
}

void DynamicRangeDisplay::drawAssessment (juce::Graphics& g)
{
    auto m = analyzer.getLatestMeasurement();
    auto bounds = layout.assessment.reduced (10);

    // DR Assessment
    g.setColour (textColour.withAlpha (0.8f));
    g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
    g.drawText ("Dynamic Range Assessment", bounds.removeFromTop (25), juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (10.0f));
    juce::Colour assessColour = m.likelyOvercompressed ? drBadColour :
                                 m.drValue < 10.0f ? drWarnColour : drGoodColour;
    g.setColour (assessColour);
    g.drawText (DynamicRangeAnalyzer::assessDynamicRange (m.drValue),
                bounds.removeFromTop (20), juce::Justification::centredLeft);

    // Crest factor assessment
    g.setColour (textColour.withAlpha (0.7f));
    g.drawText ("Crest Factor: " + DynamicRangeAnalyzer::assessCrestFactor (m.crestFactorDB),
                bounds.removeFromTop (20), juce::Justification::centredLeft);

    // Compression assessment
    g.drawText ("Compression: " + m.compressionAssessment,
                bounds, juce::Justification::centredLeft);
}

juce::Colour DynamicRangeDisplay::getDRColour (float drValue) const
{
    if (drValue >= 14.0f) return drGoodColour;
    if (drValue >= 10.0f) return juce::Colour (0xFF58a6ff);  // Blue
    if (drValue >= 8.0f) return drWarnColour;
    if (drValue >= 6.0f) return juce::Colour (0xFFf85149).darker();  // Dark red
    return drBadColour;
}