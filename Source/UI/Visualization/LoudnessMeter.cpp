#include "LoudnessMeter.h"

//==============================================================================
LoudnessMeter::LoudnessMeter (const LoudnessAnalyzer& a)
    : analyzer (a)
{
    startTimerHz (30);
    layoutMeters();
}

LoudnessMeter::~LoudnessMeter()
{
    stopTimer();
}

void LoudnessMeter::resized()
{
    layoutMeters();
}

//==============================================================================
void LoudnessMeter::timerCallback()
{
    updateVisualMeters();
    repaint();
}

void LoudnessMeter::updateVisualMeters()
{
    auto measurement = analyzer.getLatestMeasurement();

    // Smooth visual updates
    float smoothFactor = 0.15f;
    visualMomentary += (measurement.momentaryLUFS - visualMomentary) * smoothFactor;
    visualShortTerm += (measurement.shortTermLUFS - visualShortTerm) * smoothFactor;
    visualIntegrated += (measurement.integratedLUFS - visualIntegrated) * smoothFactor;
    visualTruePeakL += (measurement.truePeakLeft - visualTruePeakL) * smoothFactor;
    visualTruePeakR += (measurement.truePeakRight - visualTruePeakR) * smoothFactor;
}

void LoudnessMeter::layoutMeters()
{
    auto bounds = getLocalBounds().reduced (10);
    int meterWidth = 35;
    int spacing = 8;
    int labelHeight = 20;
    int valueHeight = 25;

    // Calculate total width needed
    int totalMeters = 6;  // M, S, I, LRA, TP-L, TP-R
    int totalWidth = totalMeters * meterWidth + (totalMeters - 1) * spacing;

    int startX = (bounds.getWidth() - totalWidth) / 2;
    int y = bounds.getY() + labelHeight;
    int height = bounds.getHeight() - labelHeight - valueHeight - 20;

    meterRects.momentary = { startX, y, meterWidth, height };
    meterRects.shortTerm = { startX + meterWidth + spacing, y, meterWidth, height };
    meterRects.integrated = { startX + 2 * (meterWidth + spacing), y, meterWidth, height };
    meterRects.lra = { startX + 3 * (meterWidth + spacing), y, meterWidth, height };
    meterRects.truePeakL = { startX + 4 * (meterWidth + spacing), y, meterWidth, height };
    meterRects.truePeakR = { startX + 5 * (meterWidth + spacing), y, meterWidth, height };

    // Target indicator area
    meterRects.target = { bounds.getX(), bounds.getBottom() - 60, bounds.getWidth(), 30 };

    // Compliance indicators
    meterRects.compliance = { bounds.getX(), bounds.getBottom() - 30, bounds.getWidth(), 25 };
}

//==============================================================================
void LoudnessMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.fillAll (backgroundColour);

    // Border
    g.setColour (borderColour);
    g.drawRect (bounds, 1);

    // Title
    g.setColour (textColour);
    g.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
    g.drawText ("LOUDNESS METER", bounds.removeFromTop (25), juce::Justification::centred);

    // Draw meters
    auto m = analyzer.getLatestMeasurement();

    drawMeter (g, meterRects.momentary, visualMomentary, targetLUFS, momentaryColour, "M", "LUFS");
    drawMeter (g, meterRects.shortTerm, visualShortTerm, targetLUFS, shortTermColour, "S", "LUFS");
    drawMeter (g, meterRects.integrated, visualIntegrated, targetLUFS, integratedColour, "I", "LUFS");
    drawLRAIndicator (g, meterRects.lra, m.loudnessRange);
    drawTruePeakMeters (g);
    drawComplianceIndicators (g);
}

//==============================================================================
void LoudnessMeter::drawMeter (juce::Graphics& g, const juce::Rectangle<int>& bounds,
                               float level, float target, const juce::Colour& colour,
                               const juce::String& label, const juce::String& unit)
{
    auto meterBounds = bounds.reduced (2);

    // Background track
    g.setColour (backgroundColour.brighter (0.1f));
    g.fillRoundedRectangle (meterBounds.toFloat(), 4.0f);

    // Scale lines
    g.setColour (borderColour);
    for (int i = 0; i <= 4; ++i)
    {
        float y = meterBounds.getY() + i * meterBounds.getHeight() / 4.0f;
        g.drawLine (meterBounds.getX(), y, meterBounds.getRight(), y, 0.5f);
    }

    // Target line
    float targetY = levelToY (target, meterBounds);
    g.setColour (targetColour);
    g.drawLine (meterBounds.getX(), targetY, meterBounds.getRight(), targetY, 1.5f, { 5.0f, 3.0f });

    // Target label
    g.setFont (juce::FontOptions (9.0f));
    g.drawText ("TARGET", meterBounds.getX(), targetY - 14, meterBounds.getWidth(), 12,
                juce::Justification::centred);

    // Level fill
    float levelY = levelToY (level, meterBounds);
    float fillHeight = meterBounds.getBottom() - levelY;

    if (fillHeight > 0)
    {
        juce::Colour fillColour = getLevelColour (level, target);
        juce::Rectangle<float> fillRect (meterBounds.getX() + 2, levelY,
                                         meterBounds.getWidth() - 4, fillHeight);
        g.setColour (fillColour);
        g.fillRoundedRectangle (fillRect, 3.0f);

        // Gradient overlay for shine
        juce::ColourGradient grad (fillColour.brighter (0.3f), fillRect.getX(), fillRect.getY(),
                                   fillColour.darker (0.2f), fillRect.getX(), fillRect.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fillRect, 3.0f);
    }

    // Peak hold indicator
    // (Would need to track peak separately)

    // Label and value
    g.setColour (textColour);
    g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
    g.drawText (label, meterBounds.getX(), meterBounds.getBottom() + 2,
                meterBounds.getWidth(), 16, juce::Justification::centred);

    g.setFont (juce::FontOptions (10.0f));
    juce::String valueStr = (level > -60.0f) ? juce::String (level, 1) : "-∞";
    g.drawText (valueStr + " " + unit, meterBounds.getX(), meterBounds.getBottom() + 18,
                meterBounds.getWidth(), 16, juce::Justification::centred);
}

void LoudnessMeter::drawLRAIndicator (juce::Graphics& g, const juce::Rectangle<int>& bounds, float lra)
{
    auto meterBounds = bounds.reduced (2);

    // Background
    g.setColour (backgroundColour.brighter (0.1f));
    g.fillRoundedRectangle (meterBounds.toFloat(), 4.0f);

    // LRA scale (0-20 LU)
    float maxLRA = 20.0f;
    float normalized = juce::jlimit (0.0f, 1.0f, lra / maxLRA);
    float fillHeight = meterBounds.getHeight() * normalized;
    float levelY = meterBounds.getBottom() - fillHeight;

    // Color based on LRA value
    juce::Colour lraColour;
    if (lra > 14.0f) lraColour = goodColour;
    else if (lra > 8.0f) lraColour = targetColour;
    else lraColour = warningColour;

    if (fillHeight > 0)
    {
        juce::Rectangle<float> fillRect (meterBounds.getX() + 2, levelY,
                                         meterBounds.getWidth() - 4, fillHeight);
        g.setColour (lraColour);
        g.fillRoundedRectangle (fillRect, 3.0f);
    }

    // Target zones
    g.setColour (goodColour.withAlpha (0.3f));
    float goodY = levelToY (14.0f, meterBounds);
    g.fillRect (meterBounds.getX(), meterBounds.getY(), meterBounds.getWidth(),
                meterBounds.getY() - goodY);

    // Label
    g.setColour (textColour);
    g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
    g.drawText ("LRA", meterBounds.getX(), meterBounds.getBottom() + 2,
                meterBounds.getWidth(), 16, juce::Justification::centred);

    g.setFont (juce::FontOptions (10.0f));
    juce::String lraStr = (lra > 0.0f) ? juce::String (lra, 1) : "—";
    g.drawText (lraStr + " LU", meterBounds.getX(), meterBounds.getBottom() + 18,
                meterBounds.getWidth(), 16, juce::Justification::centred);
}

void LoudnessMeter::drawTruePeakMeters (juce::Graphics& g)
{
    auto drawTP = [&] (const juce::Rectangle<int>& bounds, float level, const juce::String& label)
    {
        auto meterBounds = bounds.reduced (2);

        g.setColour (backgroundColour.brighter (0.1f));
        g.fillRoundedRectangle (meterBounds.toFloat(), 4.0f);

        // Scale: -10 to +3 dBTP
        float maxTP = 3.0f;
        float minTP = -10.0f;
        float normalized = juce::jlimit (0.0f, 1.0f, (level - minTP) / (maxTP - minTP));
        float fillHeight = meterBounds.getHeight() * normalized;
        float levelY = meterBounds.getBottom() - fillHeight;

        // Color: green->yellow->red
        juce::Colour tpColour;
        if (level <= -3.0f) tpColour = goodColour;
        else if (level <= -1.0f) tpColour = targetColour;
        else tpColour = warningColour;

        if (fillHeight > 0)
        {
            juce::Rectangle<float> fillRect (meterBounds.getX() + 2, levelY,
                                             meterBounds.getWidth() - 4, fillHeight);
            g.setColour (tpColour);
            g.fillRoundedRectangle (fillRect, 3.0f);
        }

        // -1 dBTP line
        float lineY = levelToY (-1.0f, meterBounds, minTP, maxTP);
        g.setColour (targetColour);
        g.drawLine (meterBounds.getX(), lineY, meterBounds.getRight(), lineY, 1.0f, { 3.0f, 2.0f });

        // 0 dBTP line
        lineY = levelToY (0.0f, meterBounds, minTP, maxTP);
        g.setColour (warningColour);
        g.drawLine (meterBounds.getX(), lineY, meterBounds.getRight(), lineY, 1.5f);

        // Label
        g.setColour (textColour);
        g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
        g.drawText (label, meterBounds.getX(), meterBounds.getBottom() + 2,
                    meterBounds.getWidth(), 14, juce::Justification::centred);

        g.setFont (juce::FontOptions (9.0f));
        juce::String valStr = (level > -60.0f) ? juce::String (level, 1) : "-∞";
        g.drawText (valStr, meterBounds.getX(), meterBounds.getBottom() + 16,
                    meterBounds.getWidth(), 14, juce::Justification::centred);
    };

    drawTP (meterRects.truePeakL, visualTruePeakL, "TP L");
    drawTP (meterRects.truePeakR, visualTruePeakR, "TP R");
}

void LoudnessMeter::drawComplianceIndicators (juce::Graphics& g)
{
    auto m = analyzer.getLatestMeasurement();

    struct Indicator
    {
        juce::String label;
        bool pass;
        juce::String detail;
    };

    juce::Array<Indicator> indicators =
    {
        { "Streaming", m.meetsStreamingTarget, juce::String (m.integratedLUFS, 1) + " LUFS" },
        { "Broadcast", m.meetsBroadcastTarget, juce::String (m.integratedLUFS, 1) + " LUFS" },
        { "Club", m.meetsClubTarget, juce::String (m.integratedLUFS, 1) + " LUFS" },
        { "True Peak", m.truePeakMax < -1.0f, juce::String (m.truePeakMax, 1) + " dBTP" }
    };

    int indicatorWidth = meterRects.compliance.getWidth() / indicators.size();
    int y = meterRects.compliance.getY();

    for (int i = 0; i < indicators.size(); ++i)
    {
        auto& ind = indicators[i];
        int x = meterRects.compliance.getX() + i * indicatorWidth;
        auto rect = juce::Rectangle<int> (x + 5, y, indicatorWidth - 10, meterRects.compliance.getHeight());

        // Status circle
        float circleSize = 12.0f;
        float cx = rect.getCentreX();
        float cy = rect.getY() + circleSize / 2 + 2;

        juce::Colour statusColour = ind.pass ? goodColour : warningColour;
        g.setColour (statusColour);
        g.fillEllipse (cx - circleSize/2, cy - circleSize/2, circleSize, circleSize);

        // Checkmark or X
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
        if (ind.pass)
            g.drawText ("✓", cx - 6, cy - 6, 12, 12, juce::Justification::centred);
        else
            g.drawText ("✗", cx - 6, cy - 6, 12, 12, juce::Justification::centred);

        // Label
        g.setColour (textColour);
        g.setFont (juce::FontOptions (9.0f));
        g.drawText (ind.label, rect.getX(), cy + 8, rect.getWidth(), 12, juce::Justification::centred);

        // Detail
        g.setFont (juce::FontOptions (8.0f));
        g.setColour (textColour.withAlpha (0.7f));
        g.drawText (ind.detail, rect.getX(), cy + 20, rect.getWidth(), 10, juce::Justification::centred);
    }
}

//==============================================================================
float LoudnessMeter::levelToY (float level, const juce::Rectangle<int>& bounds) const
{
    return levelToY (level, bounds, minLevel, minLevel + meterRange);
}

float LoudnessMeter::levelToY (float level, const juce::Rectangle<int>& bounds,
                               float min, float max) const
{
    float normalized = juce::jlimit (0.0f, 1.0f, (level - min) / (max - min));
    return bounds.getBottom() - normalized * bounds.getHeight();
}

juce::Colour LoudnessMeter::getLevelColour (float level, float target) const
{
    float diff = level - target;

    if (level > target + 2.0f)
        return warningColour;  // Too loud
    else if (level > target)
        return targetColour;   // Near target
    else if (level > target - 6.0f)
        return integratedColour;  // Good range
    else if (level > target - 20.0f)
        return momentaryColour;   // Quiet but ok
    else
        return textColour.withAlpha (0.5f);  // Very quiet
}