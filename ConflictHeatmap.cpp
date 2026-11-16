#include "ConflictHeatmap.h"

//==============================================================================
ConflictHeatmap::ConflictHeatmap (const ConflictDetector& d)
    : detector (d)
{
    startTimerHz (15);
    setMouseMoveEventsEnabled (true);
}

ConflictHeatmap::~ConflictHeatmap()
{
    stopTimer();
}

void ConflictHeatmap::resized()
{
    layoutComponents();
}

void ConflictHeatmap::timerCallback()
{
    updateCachedConflicts();
    repaint();
}

void ConflictHeatmap::mouseMove (const juce::MouseEvent& e)
{
    if (!layout.heatmap.contains (e.getPosition()))
    {
        hoverTrackPair = -1;
        hoverBand = -1;
        repaint (layout.tooltip);
        return;
    }

    // Find which track pair and band we're over
    int numTrackPairs = trackNames.size() > 1 ? trackNames.size() * (trackNames.size() - 1) / 2 : 1;
    if (numTrackPairs == 0) return;

    float yRel = static_cast<float> (e.y - layout.heatmap.getY()) / layout.heatmap.getHeight();
    hoverTrackPair = static_cast<int> (yRel * numTrackPairs);
    hoverTrackPair = juce::jlimit (0, numTrackPairs - 1, hoverTrackPair);

    hoverBand = xToBand (e.x, layout.heatmap);

    // Find conflict at this position
    for (auto& c : cachedConflicts)
    {
        if (c.trackAIndex == hoverTrackPair / trackNames.size() &&  // Simplified
            c.band == hoverBand && c.severity > minSeverity)
        {
            hoverConflict = ConflictDetector::FrequencyConflict();
            hoverConflict.trackA = c.trackAName;
            hoverConflict.trackB = c.trackBName;
            hoverConflict.centerFreq = c.freq;
            hoverConflict.severity = c.severity;
            hoverConflict.type = c.type;
            break;
        }
    }

    repaint (layout.tooltip);
}

void ConflictHeatmap::mouseExit (const juce::MouseEvent&)
{
    hoverTrackPair = -1;
    hoverBand = -1;
    repaint (layout.tooltip);
}

void ConflictHeatmap::mouseDown (const juce::MouseEvent&)
{
    // Could trigger detailed view or recommendation
}

//==============================================================================
void ConflictHeatmap::updateCachedConflicts()
{
    cachedConflicts.clear();
    trackNames.clear();

    auto conflicts = detector.getActiveConflicts();

    // Extract unique track names
    for (auto& c : conflicts)
    {
        if (!trackNames.contains (c.trackA))
            trackNames.add (c.trackA);
        if (!trackNames.contains (c.trackB))
            trackNames.add (c.trackB);
    }

    // Cache conflicts for rendering
    for (auto& c : conflicts)
    {
        if (c.severity < minSeverity) continue;

        CachedConflict cc;
        cc.trackAName = c.trackA;
        cc.trackBName = c.trackB;
        cc.trackAIndex = trackNames.indexOf (c.trackA);
        cc.trackBIndex = trackNames.indexOf (c.trackB);
        cc.freq = c.centerFreq;
        cc.severity = c.severity;
        cc.type = c.type;

        // Find band index
        cc.band = -1;
        for (int b = 0; b < ConflictDetector::NUM_BANDS; ++b)
        {
            float cf = ConflictDetector::getBandCenterFreq (b);
            float bw = ConflictDetector::getBandwidth (b);
            if (c.centerFreq >= cf - bw/2 && c.centerFreq <= cf + bw/2)
            {
                cc.band = b;
                break;
            }
        }

        cachedConflicts.add (cc);
    }
}

void ConflictHeatmap::layoutComponents()
{
    auto bounds = getLocalBounds().reduced (10);

    layout.title = bounds.removeFromTop (25);
    layout.legend = bounds.removeFromBottom (40);
    layout.freqAxis = bounds.removeFromBottom (30);
    layout.trackAxis = bounds.removeFromLeft (showTrackNames ? 120 : 0);
    layout.heatmap = bounds;
    layout.tooltip = layout.heatmap;
}

//==============================================================================
void ConflictHeatmap::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.fillAll (juce::Colour (0xFF0d1117));
    g.setColour (juce::Colour (0xFF30363d));
    g.drawRect (bounds, 1);

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
    g.drawText ("FREQUENCY CONFLICT HEATMAP", layout.title, juce::Justification::centredLeft);

    drawHeatmap (g);
    drawAxes (g);
    drawLegend (g);
    drawTooltip (g);
}

//==============================================================================
void ConflictHeatmap::drawHeatmap (juce::Graphics& g)
{
    auto bounds = layout.heatmap;
    if (bounds.isEmpty() || trackNames.size() < 2) return;

    int numTrackPairs = trackNames.size() * (trackNames.size() - 1) / 2;
    int numBands = ConflictDetector::NUM_BANDS;

    float cellWidth = static_cast<float> (bounds.getWidth()) / numBands;
    float cellHeight = static_cast<float> (bounds.getHeight()) / numTrackPairs;

    // Draw each conflict as a colored rectangle
    for (auto& c : cachedConflicts)
    {
        if (c.band < 0 || c.band >= numBands) continue;

        // Calculate track pair index
        int pairIndex = c.trackAIndex * (trackNames.size() - 1) + c.trackBIndex - 1;
        if (c.trackAIndex > c.trackBIndex) pairIndex--;
        pairIndex = juce::jlimit (0, numTrackPairs - 1, pairIndex);

        float x = bounds.getX() + c.band * cellWidth;
        float y = bounds.getY() + pairIndex * cellHeight;

        juce::Rectangle<float> cell (x, y, cellWidth, cellHeight);
        juce::Colour color = severityToColour (c.severity).withAlpha (0.8f);
        g.setColour (color);
        g.fillRect (cell);

        // Highlight hovered cell
        if (hoverTrackPair == pairIndex && hoverBand == c.band)
        {
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.drawRect (cell, 2.0f);
        }
    }

    // Grid lines
    g.setColour (juce::Colour (0xFF30363d).withAlpha (0.3f));
    for (int b = 1; b < numBands; ++b)
    {
        float x = bounds.getX() + b * cellWidth;
        g.drawVerticalLine (static_cast<int> (x), bounds.getY(), bounds.getBottom());
    }
    for (int p = 1; p < numTrackPairs; ++p)
    {
        float y = bounds.getY() + p * cellHeight;
        g.drawHorizontalLine (static_cast<int> (y), bounds.getX(), bounds.getRight());
    }
}

void ConflictHeatmap::drawAxes (juce::Graphics& g)
{
    // Frequency axis (bottom)
    auto freqBounds = layout.freqAxis;
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.setFont (juce::FontOptions (9.0f));

    int numBands = ConflictDetector::NUM_BANDS;
    float cellWidth = static_cast<float> (layout.heatmap.getWidth()) / numBands;

    for (int b = 0; b < numBands; b += 2)  // Every 2nd band to avoid crowding
    {
        float freq = ConflictDetector::getBandCenterFreq (b);
        float x = layout.heatmap.getX() + b * cellWidth + cellWidth / 2;

        juce::String label = (freq >= 1000) ? juce::String (freq / 1000.0f, 1) + "k"
                                            : juce::String (freq, 0);

        g.drawText (label,
                    static_cast<int> (x) - 20, freqBounds.getY(),
                    40, freqBounds.getHeight(),
                    juce::Justification::centredTop);
    }

    // "Frequency" label
    g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
    g.drawText ("FREQUENCY (Hz)",
                freqBounds.getX(), freqBounds.getBottom() - 18,
                freqBounds.getWidth(), 16,
                juce::Justification::centred);

    // Track axis (left)
    if (showTrackNames)
    {
        auto trackBounds = layout.trackAxis;
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (juce::FontOptions (9.0f));

        int numTrackPairs = trackNames.size() * (trackNames.size() - 1) / 2;
        float cellHeight = static_cast<float> (layout.heatmap.getHeight()) / numTrackPairs;

        int pairIdx = 0;
        for (int i = 0; i < trackNames.size(); ++i)
        {
            for (int j = i + 1; j < trackNames.size(); ++j)
            {
                float y = layout.heatmap.getY() + pairIdx * cellHeight + cellHeight / 2;
                juce::String label = trackNames[i].substring (0, 12) + " vs " +
                                     trackNames[j].substring (0, 12);

                g.drawText (label,
                            trackBounds.getX() + 5, static_cast<int> (y) - 8,
                            trackBounds.getWidth() - 10, 16,
                            juce::Justification::centredRight);

                pairIdx++;
            }
        }
    }
}

void ConflictHeatmap::drawLegend (juce::Graphics& g)
{
    auto bounds = layout.legend;

    // Severity gradient
    int steps = 20;
    float barWidth = static_cast<float> (bounds.getWidth() - 40) / steps;
    float barHeight = 15.0f;
    float startX = bounds.getX() + 20;
    float y = bounds.getY() + 5;

    for (int i = 0; i < steps; ++i)
    {
        float severity = static_cast<float> (i) / (steps - 1);
        juce::Colour c = severityToColour (severity);
        g.setColour (c);
        g.fillRect (startX + i * barWidth, y, barWidth + 1, barHeight);
    }

    // Labels
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (9.0f));
    g.drawText ("Low", bounds.getX(), y, 20, barHeight, juce::Justification::centredLeft);
    g.drawText ("High", bounds.getRight() - 30, y, 30, barHeight, juce::Justification::centredRight);

    // Type icons
    float iconY = y + barHeight + 5;
    g.setFont (juce::FontOptions (9.0f));

    struct TypeInfo { ConflictDetector::FrequencyConflict::ConflictType type; juce::String name; };
    juce::Array<TypeInfo> types = {
        { ConflictDetector::FrequencyConflict::ConflictType::SpectralOverlap, "Overlap" },
        { ConflictDetector::FrequencyConflict::ConflictType::Masking, "Masking" },
        { ConflictDetector::FrequencyConflict::ConflictType::TonalClash, "Tonal" },
        { ConflictDetector::FrequencyConflict::ConflictType::HarmonicConflict, "Harmonic" },
        { ConflictDetector::FrequencyConflict::ConflictType::TransientCollision, "Transient" }
    };

    float iconX = bounds.getX();
    for (auto& t : types)
    {
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawText (getTypeIcon (t.type) + " " + t.name,
                    iconX, iconY, 100, 14, juce::Justification::centredLeft);
        iconX += 110;
    }
}

void ConflictHeatmap::drawTooltip (juce::Graphics& g)
{
    if (hoverTrackPair < 0 || hoverBand < 0) return;

    // Find conflict at hover position
    ConflictDetector::FrequencyConflict* conflict = nullptr;
    for (auto& c : cachedConflicts)
    {
        // Simplified check
        if (c.band == hoverBand && c.severity > minSeverity)
        {
            // We'd need to map pairIndex back to track names
            // For now, just show the first matching conflict
            break;
        }
    }

    // Simplified tooltip showing band info
    float freq = ConflictDetector::getBandCenterFreq (hoverBand);
    float bw = ConflictDetector::getBandwidth (hoverBand);

    juce::String tip = "Band: " + juce::String (freq, 0) + " Hz ±" + juce::String (bw/2, 0) + " Hz";
    if (hoverTrackPair >= 0 && trackNames.size() >= 2)
    {
        // Would need proper pair index mapping
        tip << "
Pair index: " << hoverTrackPair;
    }

    auto bounds = layout.heatmap;
    juce::Rectangle<int> tipRect (bounds.getX() + 10, bounds.getY() + 10, 250, 60);

    g.setColour (juce::Colour (0xFF0d1117).withAlpha (0.95f));
    g.fillRoundedRectangle (tipRect.toFloat(), 6.0f);
    g.setColour (juce::Colour (0xFF30363d));
    g.drawRoundedRectangle (tipRect.toFloat(), 6.0f, 1.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (tip, tipRect.reduced (10), juce::Justification::centredLeft);
}

//==============================================================================
float ConflictHeatmap::freqToX (float freq, const juce::Rectangle<int>& bounds) const
{
    if (useLogScale)
    {
        float logMin = std::log10 (20.0f);
        float logMax = std::log10 (20000.0f);
        float logFreq = std::log10 (juce::jmax (20.0f, freq));
        float norm = (logFreq - logMin) / (logMax - logMin);
        return bounds.getX() + norm * bounds.getWidth();
    }
    else
    {
        float norm = juce::jlimit (0.0f, 1.0f, freq / 20000.0f);
        return bounds.getX() + norm * bounds.getWidth();
    }
}

int ConflictHeatmap::xToBand (float x, const juce::Rectangle<int>& bounds) const
{
    float relX = (x - bounds.getX()) / static_cast<float> (bounds.getWidth());
    relX = juce::jlimit (0.0f, 1.0f, relX);

    if (useLogScale)
    {
        float logMin = std::log10 (20.0f);
        float logMax = std::log10 (20000.0f);
        float logFreq = logMin + relX * (logMax - logMin);
        float freq = std::pow (10.0f, logFreq);

        // Find closest band
        int bestBand = 0;
        float bestDiff = std::numeric_limits<float>::max();
        for (int b = 0; b < ConflictDetector::NUM_BANDS; ++b)
        {
            float diff = std::abs (ConflictDetector::getBandCenterFreq (b) - freq);
            if (diff < bestDiff)
            {
                bestDiff = diff;
                bestBand = b;
            }
        }
        return bestBand;
    }
    else
    {
        float freq = relX * 20000.0f;
        int bestBand = 0;
        float bestDiff = std::numeric_limits<float>::max();
        for (int b = 0; b < ConflictDetector::NUM_BANDS; ++b)
        {
            float diff = std::abs (ConflictDetector::getBandCenterFreq (b) - freq);
            if (diff < bestDiff)
            {
                bestDiff = diff;
                bestBand = b;
            }
        }
        return bestBand;
    }
}

juce::Colour ConflictHeatmap::severityToColour (float severity) const
{
    // Green -> Yellow -> Orange -> Red
    if (severity < 0.25f)
        return juce::Colour (0xFF3fb950);  // Green
    else if (severity < 0.5f)
        return juce::Colour (0xFFd29922);  // Yellow
    else if (severity < 0.75f)
        return juce::Colour (0xFFf85149);  // Orange/Red
    else
        return juce::Colour (0xFFf85149).brighter();  // Bright red
}

juce::String ConflictHeatmap::getTypeIcon (ConflictDetector::FrequencyConflict::ConflictType type) const
{
    switch (type)
    {
        case ConflictDetector::FrequencyConflict::ConflictType::SpectralOverlap: return "◆";
        case ConflictDetector::FrequencyConflict::ConflictType::Masking: return "▲";
        case ConflictDetector::FrequencyConflict::ConflictType::TonalClash: return "●";
        case ConflictDetector::FrequencyConflict::ConflictType::HarmonicConflict: return "◆";
        case ConflictDetector::FrequencyConflict::ConflictType::TransientCollision: return "⚡";
    }
    return "?";
}