#include "DynamicRangeAnalyzer.h"
#include <algorithm>
#include <cmath>

//==============================================================================
DynamicRangeAnalyzer::DynamicRangeAnalyzer (double sr)
    : sampleRate (sr)
{
    prepare (sr, 512);
}

void DynamicRangeAnalyzer::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    blockSize = samplesPerBlock;

    // Short-term window: ~3 seconds
    int shortTermBlocks = static_cast<int> (3.0 * sampleRate / blockSize);
    shortTermWindow.maxSize = shortTermBlocks;
    shortTermWindow.rmsValues.insertMultiple (0, -70.0f, shortTermBlocks);
    shortTermWindow.peakValues.insertMultiple (0, -70.0f, shortTermBlocks);
    shortTermWindow.truePeakValues.insertMultiple (0, -70.0f, shortTermBlocks);

    // Long-term window: ~1 minute
    int longTermBlocks = static_cast<int> (60.0 * sampleRate / blockSize);
    longTermWindow.maxSize = longTermBlocks;
    longTermWindow.rmsValues.insertMultiple (0, -70.0f, longTermBlocks);
    longTermWindow.peakValues.insertMultiple (0, -70.0f, longTermBlocks);
    longTermWindow.truePeakValues.insertMultiple (0, -70.0f, longTermBlocks);

    // Session window: grow dynamically
    sessionWindow.maxSize = 10000;
    sessionWindow.rmsValues.insertMultiple (0, -70.0f, 1000);
    sessionWindow.peakValues.insertMultiple (0, -70.0f, 1000);
    sessionWindow.truePeakValues.insertMultiple (0, -70.0f, 1000);

    compressionDetector.reset();

    currentMeasurement = {};
    currentMeasurement.crestHistory.insertMultiple (0, 0.0f, 100);
    currentMeasurement.rmsHistory.insertMultiple (0, -70.0f, 100);
    currentMeasurement.peakHistory.insertMultiple (0, -70.0f, 100);
}

void DynamicRangeAnalyzer::reset()
{
    prepare (sampleRate, blockSize);
    currentMeasurement = {};
}

//==============================================================================
void DynamicRangeAnalyzer::processBlock (const juce::AudioBuffer<float>& buffer)
{
    updateMeasurements (buffer);
    calculateShortTermDynamics();
    calculateLongTermDynamics();
    detectCompression();

    // Update history for trending
    int histPos = currentMeasurement.crestHistory.size() > 0 ? 0 : 0;
    // Simple circular buffer for history
    static int historyIndex = 0;
    if (historyIndex >= currentMeasurement.crestHistory.size())
        historyIndex = 0;

    currentMeasurement.crestHistory.set (historyIndex, currentMeasurement.crestFactorDB);
    currentMeasurement.rmsHistory.set (historyIndex, currentMeasurement.rmsLevel);
    currentMeasurement.peakHistory.set (historyIndex, currentMeasurement.peakLevel);
    historyIndex++;

    if (onMeasurementUpdate)
        onMeasurementUpdate (currentMeasurement);
}

void DynamicRangeAnalyzer::updateMeasurements (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    // Calculate RMS and peak for this block
    float sumSq = 0.0f;
    float peak = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* data = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float sample = data[i];
            sumSq += sample * sample;
            peak = std::max (peak, std::abs (sample));
        }
    }

    float rms = std::sqrt (sumSq / (numSamples * numChannels));
    float rmsDB = 20.0f * std::log10 (std::max (rms, 1e-10f));
    float peakDB = 20.0f * std::log10 (std::max (peak, 1e-10f));

    // True peak estimation (parabolic interpolation)
    float truePeak = peak;
    // Check inter-sample peaks
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* data = buffer.getReadPointer (ch);
        for (int i = 1; i < numSamples - 1; ++i)
        {
            float y0 = std::abs (data[i-1]);
            float y1 = std::abs (data[i]);
            float y2 = std::abs (data[i+1]);

            float denom = y0 - 2*y1 + y2;
            if (std::abs (denom) > 1e-6f)
            {
                float offset = 0.5f * (y0 - y2) / denom;
                if (offset > -0.5f && offset < 0.5f)
                {
                    float interpPeak = y1 - 0.25f * (y0 - y2) * offset;
                    truePeak = std::max (truePeak, interpPeak);
                }
            }
        }
    }
    float truePeakDB = 20.0f * std::log10 (std::max (truePeak, 1e-10f));

    // Exponential smoothing
    smoothedRMS = smoothingFactor * smoothedRMS + (1.0f - smoothingFactor) * rmsDB;
    smoothedPeak = smoothingFactor * smoothedPeak + (1.0f - smoothingFactor) * peakDB;

    // Store in windows
    auto storeInWindow = [this] (WindowState& window, float rms, float peak, float truePeak)
    {
        int pos = window.writePos % window.maxSize;
        if (pos < window.rmsValues.size())
        {
            window.rmsValues.set (pos, rms);
            window.peakValues.set (pos, peak);
            window.truePeakValues.set (pos, truePeak);
        }
        else
        {
            window.rmsValues.add (rms);
            window.peakValues.add (peak);
            window.truePeakValues.add (truePeak);
        }
        window.writePos++;
    };

    storeInWindow (shortTermWindow, rmsDB, peakDB, truePeakDB);
    storeInWindow (longTermWindow, rmsDB, peakDB, truePeakDB);
    storeInWindow (sessionWindow, rmsDB, peakDB, truePeakDB);

    // Update current measurement
    currentMeasurement.rmsLevel = smoothedRMS;
    currentMeasurement.peakLevel = smoothedPeak;
    currentMeasurement.truePeakLevel = truePeakDB;

    // Crest factor
    currentMeasurement.crestFactor = calculateCrestFactor (peak, rms);
    currentMeasurement.crestFactorDB = 20.0f * std::log10 (std::max (currentMeasurement.crestFactor, 1e-10f));
    currentMeasurement.truePeakCrestFactor = truePeak / std::max (rms, 1e-10f);

    // PLR (Peak to Loudness Ratio) - will be updated when loudness is available
    // currentMeasurement.plr = currentMeasurement.truePeakLevel - integratedLUFS;
}

void DynamicRangeAnalyzer::calculateShortTermDynamics()
{
    int validCount = std::min (shortTermWindow.writePos, shortTermWindow.maxSize);
    if (validCount < 10)
        return;

    // Find min/max crest factor in short term window
    float minCrest = 100.0f;
    float maxCrest = -100.0f;
    float minRMS = 100.0f;
    float maxRMS = -100.0f;

    for (int i = 0; i < validCount; ++i)
    {
        float rms = shortTermWindow.rmsValues[i];
        float peak = shortTermWindow.peakValues[i];
        if (rms > -70.0f && peak > -70.0f)
        {
            float crest = peak - rms;  // In dB
            minCrest = std::min (minCrest, crest);
            maxCrest = std::max (maxCrest, crest);
            minRMS = std::min (minRMS, rms);
            maxRMS = std::max (maxRMS, rms);
        }
    }

    currentMeasurement.shortTermCrestMin = minCrest;
    currentMeasurement.shortTermCrestMax = maxCrest;
    currentMeasurement.microDynamics = maxCrest - minCrest;

    // RMS variation (macro dynamics in short term)
    currentMeasurement.sectionLevelVariation = maxRMS - minRMS;
}

void DynamicRangeAnalyzer::calculateLongTermDynamics()
{
    int validCount = std::min (longTermWindow.writePos, longTermWindow.maxSize);
    if (validCount < 50)
        return;

    // Collect RMS values for LRA calculation
    juce::Array<float> rmsValues;
    for (int i = 0; i < validCount; ++i)
    {
        float rms = longTermWindow.rmsValues[i];
        if (rms > -60.0f)  // Ignore silence
            rmsValues.add (rms);
    }

    if (rmsValues.size() < 10)
        return;

    rmsValues.sort();

    // LRA = 95th - 10th percentile (in LU, which equals dB for this purpose)
    int idx10 = static_cast<int> (0.1f * rmsValues.size());
    int idx95 = static_cast<int> (0.95f * rmsValues.size());

    float p10 = rmsValues[idx10];
    float p95 = rmsValues[idx95];

    currentMeasurement.dynamicRange = p95 - p10;
    currentMeasurement.dynamicRangePercentile = currentMeasurement.dynamicRange;

    // Convert to DR1-DR20 scale
    currentMeasurement.drValue = calculateDRValue (currentMeasurement.dynamicRange);

    // Macro dynamics (long-term level variation)
    float minRMS = rmsValues[0];
    float maxRMS = rmsValues[rmsValues.size() - 1];
    currentMeasurement.macroDynamics = maxRMS - minRMS;
}

float DynamicRangeAnalyzer::calculateCrestFactor (float peak, float rms) const noexcept
{
    if (rms <= 0.0f)
        return 1.0f;
    return peak / rms;
}

float DynamicRangeAnalyzer::calculateDRValue (float lra) const noexcept
{
    // Map LRA (in LU) to DR1-DR20 scale
    // DR20 = very dynamic (LRA > 20), DR1 = completely crushed (LRA < 1)
    float dr = 20.0f - lra;
    return juce::jlimit (1.0f, 20.0f, dr);
}

//==============================================================================
void DynamicRangeAnalyzer::detectCompression()
{
    // Add current level to compression detector
    compressionDetector.addLevel (currentMeasurement.rmsLevel);

    // Estimate compression ratio from level stability
    float estimatedRatio = compressionDetector.estimateRatio();
    currentMeasurement.estimatedCompressionRatio = estimatedRatio;

    // Assess if overcompressed
    currentMeasurement.likelyOvercompressed = (currentMeasurement.drValue < 6.0f) ||
                                               (currentMeasurement.crestFactorDB < 6.0f) ||
                                               (estimatedRatio > 4.0f);

    currentMeasurement.compressionAssessment = assessCompression (estimatedRatio);
}

//==============================================================================
void DynamicRangeAnalyzer::CompressionDetector::addLevel (float level)
{
    int pos = levelHistory.size() > 0 ? 0 : 0;
    // Circular buffer
    static int index = 0;
    if (index >= historySize)
        index = 0;

    if (index < levelHistory.size())
        levelHistory.set (index, level);
    else
        levelHistory.add (level);

    index++;
    lastLevel = level;
}

float DynamicRangeAnalyzer::CompressionDetector::estimateRatio() const
{
    if (levelHistory.size() < 10)
        return 1.0f;

    // Calculate standard deviation of levels
    float sum = 0.0f;
    int count = 0;
    for (float l : levelHistory)
    {
        if (l > -60.0f)
        {
            sum += l;
            count++;
        }
    }

    if (count < 10)
        return 1.0f;

    float mean = sum / count;

    float variance = 0.0f;
    for (float l : levelHistory)
    {
        if (l > -60.0f)
        {
            float diff = l - mean;
            variance += diff * diff;
        }
    }
    variance /= count;
    float stdDev = std::sqrt (variance);

    // Low stdDev = high compression
    // Map stdDev to estimated ratio
    // stdDev > 6dB = uncompressed (ratio ~1)
    // stdDev < 2dB = heavily compressed (ratio > 10)
    float ratio;
    if (stdDev > 6.0f)
        ratio = 1.0f;
    else if (stdDev > 3.0f)
        ratio = 2.0f + (6.0f - stdDev) / 3.0f * 3.0f;  // 2:1 to 5:1
    else
        ratio = 5.0f + (3.0f - stdDev) / 3.0f * 15.0f; // 5:1 to 20:1

    return juce::jlimit (1.0f, 50.0f, ratio);
}

//==============================================================================
juce::String DynamicRangeAnalyzer::assessDynamicRange (float drValue)
{
    if (drValue >= 14) return "Excellent - Very Dynamic";
    if (drValue >= 12) return "Good - Dynamic";
    if (drValue >= 10) return "Fair - Moderately Compressed";
    if (drValue >= 8)  return "Poor - Compressed";
    if (drValue >= 6)  return "Bad - Heavily Compressed";
    return "Terrible - Crushed";
}

juce::String DynamicRangeAnalyzer::assessCrestFactor (float crestDB)
{
    if (crestDB >= 18) return "Excellent - Wide Dynamics";
    if (crestDB >= 14) return "Good - Healthy Dynamics";
    if (crestDB >= 10) return "Fair - Some Compression";
    if (crestDB >= 6)  return "Poor - Compressed";
    return "Bad - Overcompressed";
}

juce::String DynamicRangeAnalyzer::assessCompression (float estimatedRatio)
{
    if (estimatedRatio <= 1.5f) return "None to Light";
    if (estimatedRatio <= 3.0f) return "Moderate";
    if (estimatedRatio <= 6.0f) return "Heavy";
    if (estimatedRatio <= 12.0f) return "Very Heavy";
    return "Extreme / Limiting";
}

//==============================================================================
void DynamicRangeAnalyzer::analyzeFullBuffer (const juce::AudioBuffer<float>& buffer)
{
    const int blockSize = 4096;
    const int numSamples = buffer.getNumSamples();

    for (int offset = 0; offset < numSamples; offset += blockSize)
    {
        int currentBlockSize = juce::jmin (blockSize, numSamples - offset);
        juce::AudioBuffer<float> block (buffer.getNumChannels(), currentBlockSize);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            block.copyFrom (ch, 0, buffer, ch, offset, currentBlockSize);

        processBlock (block);
    }
}