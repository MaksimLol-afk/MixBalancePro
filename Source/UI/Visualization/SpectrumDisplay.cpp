#include "SpectrumDisplay.h"

//==============================================================================
SpectrumDisplay::SpectrumDisplay (const SpectrumAnalyzer& a)
    : analyzer (a)
{
    startTimerHz (30);
    setMouseMoveEventsEnabled (true);
}

SpectrumDisplay::~SpectrumDisplay()
{
    stopTimer();
}

void SpectrumDisplay::resized()
{
    layoutComponents();
}

void SpectrumDisplay::timerCallback()
{
    repaint();
}

void SpectrumDisplay::mouseMove (const juce::MouseEvent& e)
{
    if (layout.spectrum.contains (e.getPosition()))
    {
        hoverFreq = xToFreq (e.x, layout.spectrum);
        // Find closest bin level from average spectrum
        auto avgSpec = analyzer.getAverageSpectrum();
        int numBins = avgSpec.getNumSamples();
        float binFreq = xToFreq (e.x, layout.spectrum);
        int bin = static_cast<int> (binFreq * numBins * 2.0f / 44100.0f); // approximate
        bin = juce::jlimit (0, numBins - 1, bin);
        hoverLevel = avgSpec.getSample (0, bin);
        hoverBin = bin;
    }
    else
    {
        hoverBin = -1;
    }
    repaint (layout.info);
}

void SpectrumDisplay::mouseExit (const juce::MouseEvent&)
{
    hoverBin = -1;
    repaint (layout.info);
}

//==============================================================================
void SpectrumDisplay::layoutComponents()
{
    auto bounds = getLocalBounds().reduced (10);

    layout.title = bounds.removeFromTop (25);
    layout.info = bounds.removeFromBottom (60);
    layout.freqAxis = bounds.removeFromBottom (30);
    layout.levelAxis = bounds.removeFromLeft (50);
    layout.spectrum = bounds;
}

//==============================================================================
void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.fillAll (bgColour);
    g.setColour (gridColour);
    g.drawRect (bounds, 1);

    // Title
    g.setColour (textColour);
    g.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
    g.drawText ("REAL-TIME SPECTRUM", layout.title, juce::Justification::centredLeft);

    drawSpectrum (g);
    drawAxes (g);
    drawInfo (g);
}

//==============================================================================
void SpectrumDisplay::drawSpectrum (juce::Graphics& g)
{
    auto bounds = layout.spectrum;
    if (bounds.isEmpty()) return;

    // Clip to spectrum area
    g.reduceClipRegion (bounds);

    // Get spectra
    auto avgSpec = analyzer.getAverageSpectrum();
    auto peakSpec = analyzer.getPeakHoldSpectrum();
    auto measurement = analyzer.getLatestMeasurement();

    int numBins = avgSpec.getNumSamples();
    if (numBins == 0) return;

    const float* avgData = avgSpec.getReadPointer (0);
    const float* peakData = peakSpec.getReadPointer (0);

    // Frequency to X mapping (log scale)
    auto freqToX = [this, &bounds] (float freq) -> float
    {
        float logMin = std::log10 (minFreq);
        float logMax = std::log10 (maxFreq);
        float logFreq = std::log10 (juce::jmax (minFreq, freq));
        float norm = (logFreq - logMin) / (logMax - logMin);
        return bounds.getX() + norm * bounds.getWidth();
    };

    // Draw grid
    g.setColour (gridColour.withAlpha (0.3f));
    float dbStep = 12.0f;
    for (float db = minDB; db <= maxDB; db += dbStep)
    {
        float y = dbToY (db, bounds);
        g.drawHorizontalLine (static_cast<int> (y), bounds.getX(), bounds.getRight());
    }

    // Frequency grid lines
    for (int octave = 1; octave <= 10; ++octave)
    {
        float freq = 31.25f * std::pow (2.0f, octave);  // 31.25, 62.5, 125, 250...
        if (freq >= minFreq && freq <= maxFreq)
        {
            float x = freqToX (freq);
            g.drawVerticalLine (static_cast<int> (x), bounds.getY(), bounds.getBottom());
        }
    }

    // Draw peak hold first (behind)
    if (showPeakHold)
    {
        juce::Path peakPath;
        bool started = false;

        for (int bin = 0; bin < numBins; ++bin)
        {
            float freq = bin * 44100.0f / (numBins * 2.0f);
            if (freq < minFreq || freq > maxFreq) continue;

            float level = peakData[bin];
            if (level < minDB) continue;

            float x = freqToX (freq);
            float y = dbToY (level, bounds);

            if (!started)
            {
                peakPath.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                peakPath.lineTo (x, y);
            }
        }

        if (started)
        {
            g.setColour (peakColour.withAlpha (0.5f));
            g.strokePath (peakPath, juce::PathStrokeType (1.0f));
        }
    }

    // Draw average spectrum
    if (showAverage)
    {
        juce::Path avgPath;
        bool started = false;

        for (int bin = 0; bin < numBins; ++bin)
        {
            float freq = bin * 44100.0f / (numBins * 2.0f);
            if (freq < minFreq || freq > maxFreq) continue;

            float level = avgData[bin];
            if (level < minDB) continue;

            float x = freqToX (freq);
            float y = dbToY (level, bounds);

            if (!started)
            {
                avgPath.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                avgPath.lineTo (x, y);
            }
        }

        if (started)
        {
            g.setColour (avgColour);
            g.strokePath (avgPath, juce::PathStrokeType (1.5f));
        }
    }

    // Draw 1/3 octave bands
    if (showThirdOctave)
    {
        g.setColour (thirdOctaveColour);
        for (int band = 0; band < 31; ++band)
        {
            float freq = SpectrumAnalyzer::thirdOctaveCenters[band];
            if (freq < minFreq || freq > maxFreq) continue;

            float level = measurement.thirdOctaveBandsDB[band];
            if (level < minDB) continue;

            float x = freqToX (freq);
            float y = dbToY (level, bounds);

            // Draw as vertical bar
            float barWidth = bounds.getWidth() * 0.015f;
            juce::Rectangle<float> bar (x - barWidth/2, y, barWidth, bounds.getBottom() - y);
            g.fillRect (bar);
        }
    }

    // Draw resonance markers
    drawResonanceMarkers (g);

    // Hover indicator
    if (hoverBin >= 0 && hoverBin < numBins)
    {
        float freq = hoverBin * 44100.0f / (numBins * 2.0f);
        if (freq >= minFreq && freq <= maxFreq)
        {
            float x = freqToX (freq);
            float y = dbToY (hoverLevel, bounds);

            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawVerticalLine (static_cast<int> (x), bounds.getY(), bounds.getBottom());
            g.drawHorizontalLine (static_cast<int> (y), bounds.getX(), bounds.getRight());
            g.fillEllipse (x - 4, y - 4, 8, 8);
        }
    }
}

void SpectrumDisplay::drawResonanceMarkers (juce::Graphics& g)
{
    auto measurement = analyzer.getLatestMeasurement();
    auto bounds = layout.spectrum;

    for (auto& res : measurement.detectedResonances)
    {
        if (res.frequency < minFreq || res.frequency > maxFreq) continue;

        float x = freqToX (res.frequency, bounds);
        float y = dbToY (res.magnitude, bounds);

        // Resonance marker - diamond shape
        juce::Path diamond;
        diamond.startNewSubPath (x, y - 8);
        diamond.lineTo (x + 6, y);
        diamond.lineTo (x, y + 8);
        diamond.lineTo (x - 6, y);
        diamond.closeSubPath();

        juce::Colour color = res.prominence > 20.0f ? juce::Colour (0xFFf85149) : juce::Colour (0xFFd29922);
        g.setColour (color.withAlpha (0.9f));
        g.fillPath (diamond);
        g.setColour (juce::Colours::white);
        g.strokePath (diamond, juce::PathStrokeType (1.0f));

        // Frequency label
        g.setFont (juce::FontOptions (8.0f));
        g.drawText (juce::String (res.frequency, 0) + "Hz",
                    static_cast<int> (x) - 30, static_cast<int> (y) - 25, 60, 12,
                    juce::Justification::centred);
    }
}

//==============================================================================
void SpectrumDisplay::drawAxes (juce::Graphics& g)
{
    // Frequency axis (bottom)
    auto freqBounds = layout.freqAxis;
    g.setColour (textColour.withAlpha (0.7f));
    g.setFont (juce::FontOptions (9.0f));

    // Major frequency labels
    float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (float freq : freqs)
    {
        if (freq >= minFreq && freq <= maxFreq)
        {
            float x = freqToX (freq, layout.spectrum);
            juce::String label = (freq >= 1000) ? juce::String (freq / 1000.0f, 1) + "k"
                                                : juce::String (freq, 0);
            g.drawText (label,
                        static_cast<int> (x) - 20, freqBounds.getY(),
                        40, freqBounds.getHeight(),
                        juce::Justification::centredTop);
        }
    }

    // Level axis (left)
    auto levelBounds = layout.levelAxis;
    g.setFont (juce::FontOptions (8.0f));
    for (float db = minDB; db <= maxDB; db += 12.0f)
    {
        float y = dbToY (db, layout.spectrum);
        if (y >= layout.spectrum.getY() && y <= layout.spectrum.getBottom())
        {
            g.drawText (juce::String (db, 0),
                        levelBounds.getX(), static_cast<int> (y) - 8,
                        levelBounds.getWidth(), 16,
                        juce::Justification::centredRight);
        }
    }
}

void SpectrumDisplay::drawInfo (juce::Graphics& g)
{
    auto bounds = layout.info;
    auto measurement = analyzer.getLatestMeasurement();

    g.setColour (textColour.withAlpha (0.8f));
    g.setFont (juce::FontOptions (10.0f));

    juce::String info;
    info << "Centroid: " << juce::String (measurement.spectralCentroid, 0) << " Hz  ";
    info << "Spread: " << juce::String (measurement.spectralSpread, 0) << " Hz  ";
    info << "Rolloff: " << juce::String (measurement.spectralRolloff, 0) << " Hz  ";
    info << "Flatness: " << juce::String (measurement.spectralFlatness, 2) << "  ";
    info << "Crest: " << juce::String (measurement.spectralCrest, 1);

    g.drawText (info, bounds.reduced (10).removeFromTop (20), juce::Justification::centredLeft);

    // Balance assessment
    g.setColour (textColour.withAlpha (0.6f));
    g.setFont (juce::FontOptions (9.0f));
    g.drawText (measurement.balanceAssessment,
                bounds.reduced (10).removeFromBottom (35), juce::Justification::centredLeft);

    // Resonance count
    g.setColour (measurement.detectedResonances.size() > 0 ? juce::Colour (0xFFf85149) : juce::Colour (0xFF3fb950));
    g.drawText ("Resonances: " + juce::String (measurement.detectedResonances.size()),
                bounds.reduced (10).removeFromBottom (15).removeFromLeft (100),
                juce::Justification::centredLeft);

    // Hover info
    if (hoverBin >= 0)
    {
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
        juce::String hoverInfo = "Freq: " + juce::String (hoverFreq, 1) + " Hz  Level: " + juce::String (hoverLevel, 1) + " dB";
        g.drawText (hoverInfo, bounds.reduced (10).removeFromBottom (15).removeFromRight (100),
                    juce::Justification::centredRight);
    }
}

//==============================================================================
float SpectrumDisplay::freqToX (float freq, const juce::Rectangle<int>& bounds) const
{
    float logMin = std::log10 (minFreq);
    float logMax = std::log10 (maxFreq);
    float logFreq = std::log10 (juce::jmax (minFreq, freq));
    float norm = (logFreq - logMin) / (logMax - logMin);
    return bounds.getX() + norm * bounds.getWidth();
}

float SpectrumDisplay::dbToY (float db, const juce::Rectangle<int>& bounds) const
{
    float norm = juce::jlimit (0.0f, 1.0f, (db - minDB) / (maxDB - minDB));
    return bounds.getBottom() - norm * bounds.getHeight();
}

float SpectrumDisplay::xToFreq (float x, const juce::Rectangle<int>& bounds) const
{
    float norm = (x - bounds.getX()) / static_cast<float> (bounds.getWidth());
    norm = juce::jlimit (0.0f, 1.0f, norm);
    float logMin = std::log10 (minFreq);
    float logMax = std::log10 (maxFreq);
    float logFreq = logMin + norm * (logMax - logMin);
    return std::pow (10.0f, logFreq);
}