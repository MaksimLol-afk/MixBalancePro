#include "LoudnessAnalyzer.h"
#include <algorithm>
#include <cmath>

//==============================================================================
LoudnessAnalyzer::LoudnessAnalyzer (double sr)
    : sampleRate (sr)
{
    prepare (sr, 512);
}

void LoudnessAnalyzer::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    blockSize = samplesPerBlock;

    // Calculate window sizes
    momentaryWindowSize = static_cast<int> (0.4 * sampleRate);  // 400ms
    shortTermWindowSize = static_cast<int> (3.0 * sampleRate);  // 3s

    // Initialize windows
    momentaryWindow.squaredSamples.setSize (2, momentaryWindowSize);
    momentaryWindow.squaredSamples.clear();
    momentaryWindow.writePos = 0;
    momentaryWindow.sum = 0.0f;

    shortTermWindow.squaredSamples.setSize (2, shortTermWindowSize);
    shortTermWindow.squaredSamples.clear();
    shortTermWindow.writePos = 0;
    shortTermWindow.sum = 0.0f;

    integratedWindow.squaredSamples.setSize (2, 1);  // Just running sum
    integratedWindow.squaredSamples.clear();
    integratedWindow.writePos = 0;
    integratedWindow.sum = 0.0f;

    // True peak oversampling filters (4x)
    auto upsampleCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate * 4, sampleRate * 0.45);
    truePeakLeft.upsampleFilter.coefficients = *upsampleCoeffs;
    truePeakRight.upsampleFilter.coefficients = *upsampleCoeffs;
    truePeakLeft.upsampleFilter.reset();
    truePeakRight.upsampleFilter.reset();

    // Reset filter states
    leftState = {};
    rightState = {};

    // Reset history
    shortTermHistory.clear();
    shortTermHistory.insertMultiple (0, -70.0f, LRA_HISTORY_SIZE);
    historyIndex = 0;

    // Reset measurement
    currentMeasurement = {};
}

void LoudnessAnalyzer::reset()
{
    prepare (sampleRate, blockSize);
}

//==============================================================================
void LoudnessAnalyzer::applyKWeighting (const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output)
{
    const int numSamples = input.getNumSamples();
    const int numChannels = juce::jmin (input.getNumChannels(), output.getNumChannels());

    // K-weighting filter coefficients (ITU-R BS.1770-4)
    // Pre-filter: High shelf at 1680 Hz, +4 dB
    const float b0 = 1.53512485958697f;
    const float b1 = -2.69169618940638f;
    const float b2 = 1.19839281085285f;
    const float a1 = -1.69065929318241f;
    const float a2 = 0.73248077421585f;

    // RLB filter: High-pass at 38 Hz
    const float rlb_b0 = 0.99957121990616f;
    const float rlb_b1 = -1.99914243981232f;
    const float rlb_b2 = 0.99957121990616f;
    const float rlb_a1 = -1.99914243981232f;
    const float rlb_a2 = 0.99914243981232f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* in = input.getReadPointer (ch);
        float* out = output.getWritePointer (ch);

        FilterState& state = (ch == 0) ? leftState : rightState;

        for (int i = 0; i < numSamples; ++i)
        {
            float x = in[i];

            // K-weighting pre-filter (biquad)
            float y = b0 * x + b1 * state.kWeightingX1 + b2 * state.kWeightingX2
                    - a1 * state.kWeightingY1 - a2 * state.kWeightingY2;

            state.kWeightingX2 = state.kWeightingX1;
            state.kWeightingX1 = x;
            state.kWeightingY2 = state.kWeightingY1;
            state.kWeightingY1 = y;

            // RLB high-pass filter
            float rlb_y = rlb_b0 * y + rlb_b1 * state.rlbX1 + rlb_b2 * state.rlbX2
                        - rlb_a1 * state.rlbY1 - rlb_a2 * state.rlbY2;

            state.rlbX2 = state.rlbX1;
            state.rlbX1 = y;
            state.rlbY2 = state.rlbY1;
            state.rlbY1 = rlb_y;

            out[i] = rlb_y;
        }
    }

    // Channel weighting: Left/Right = 1.0, Center = 1.0, Ls/Rs = 1.41
    // For stereo, we apply 1.0 to both channels
}

void LoudnessAnalyzer::processBlock (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numChannels == 0 || numSamples == 0)
        return;

    // Apply K-weighting
    juce::AudioBuffer<float> kWeighted (numChannels, numSamples);
    applyKWeighting (buffer, kWeighted);

    // Update measurement windows
    updateWindows (kWeighted);

    // Update true peak
    updateTruePeak (buffer);

    // Calculate measurements
    calculateMeasurements();

    // Trigger callback
    if (onMeasurementUpdate)
        onMeasurementUpdate (currentMeasurement);
}

void LoudnessAnalyzer::updateWindows (const juce::AudioBuffer<float>& kWeighted)
{
    const int numSamples = kWeighted.getNumSamples();
    const int numChannels = kWeighted.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* data = kWeighted.getReadPointer (ch);

        // Momentary window (400ms)
        for (int i = 0; i < numSamples; ++i)
        {
            float sq = data[i] * data[i];
            int pos = momentaryWindow.writePos;

            // Remove old value from sum
            momentaryWindow.sum -= momentaryWindow.squaredSamples.getSample (ch, pos);
            // Add new value
            momentaryWindow.sum += sq;
            momentaryWindow.squaredSamples.setSample (ch, pos, sq);

            momentaryWindow.writePos = (pos + 1) % momentaryWindowSize;
        }

        // Short-term window (3s)
        for (int i = 0; i < numSamples; ++i)
        {
            float sq = data[i] * data[i];
            int pos = shortTermWindow.writePos;

            shortTermWindow.sum -= shortTermWindow.squaredSamples.getSample (ch, pos);
            shortTermWindow.sum += sq;
            shortTermWindow.squaredSamples.setSample (ch, pos, sq);

            shortTermWindow.writePos = (pos + 1) % shortTermWindowSize;
        }

        // Integrated window (running sum)
        for (int i = 0; i < numSamples; ++i)
        {
            integratedWindow.sum += data[i] * data[i];
        }
    }
}

void LoudnessAnalyzer::updateTruePeak (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();

    // Simple true peak estimation using 4x oversampling via polyphase
    // For production, use proper 4x oversampling with anti-aliasing

    auto checkPeak = [this, numSamples] (const float* data, TruePeakState& state)
    {
        float maxVal = state.maxPeak;
        for (int i = 0; i < numSamples; ++i)
        {
            // Simple peak detection with 4x interpolation estimate
            float val = std::abs (data[i]);
            if (val > maxVal)
                maxVal = val;

            // Check inter-sample peaks (simple parabolic interpolation)
            if (i > 0 && i < numSamples - 1)
            {
                float y0 = std::abs (data[i-1]);
                float y1 = std::abs (data[i]);
                float y2 = std::abs (data[i+1]);

                // Parabolic interpolation for true peak
                float denom = (y0 - 2*y1 + y2);
                if (std::abs (denom) > 1e-6f)
                {
                    float offset = 0.5f * (y0 - y2) / denom;
                    if (offset > -0.5f && offset < 0.5f)
                    {
                        float interpPeak = y1 - 0.25f * (y0 - y2) * offset;
                        if (interpPeak > maxVal)
                            maxVal = interpPeak;
                    }
                }
            }
        }
        state.maxPeak = maxVal;
    };

    if (buffer.getNumChannels() >= 1)
        checkPeak (buffer.getReadPointer (0), truePeakLeft);
    if (buffer.getNumChannels() >= 2)
        checkPeak (buffer.getReadPointer (1), truePeakRight);

    currentMeasurement.truePeakLeft = 20.0f * std::log10 (std::max (truePeakLeft.maxPeak, 1e-7f));
    currentMeasurement.truePeakRight = 20.0f * std::log10 (std::max (truePeakRight.maxPeak, 1e-7f));
    currentMeasurement.truePeakMax = std::max (currentMeasurement.truePeakLeft, currentMeasurement.truePeakRight);
}

void LoudnessAnalyzer::calculateMeasurements()
{
    // Calculate mean squared for each window
    float momentaryMeanSq = momentaryWindow.sum / momentaryWindowSize;
    float shortTermMeanSq = shortTermWindow.sum / shortTermWindowSize;
    float integratedMeanSq = integratedWindow.sum / std::max (1, integratedWindow.writePos * blockSize);

    // Convert to LUFS
    currentMeasurement.momentaryLUFS = calculateLUFS (momentaryMeanSq);
    currentMeasurement.shortTermLUFS = calculateLUFS (shortTermMeanSq);
    currentMeasurement.integratedLUFS = calculateLUFS (integratedMeanSq);

    // Update short-term history for LRA
    shortTermHistory.set (historyIndex, currentMeasurement.shortTermLUFS);
    historyIndex = (historyIndex + 1) % LRA_HISTORY_SIZE;

    // Calculate LRA
    currentMeasurement.loudnessRange = calculateLRA();

    // Check target compliance
    currentMeasurement.meetsStreamingTarget = std::abs (currentMeasurement.integratedLUFS - (-14.0f)) <= 1.0f;
    currentMeasurement.meetsBroadcastTarget = std::abs (currentMeasurement.integratedLUFS - (-23.0f)) <= 0.5f;
    currentMeasurement.meetsClubTarget = currentMeasurement.integratedLUFS >= -9.0f && currentMeasurement.integratedLUFS <= -6.0f;
}

float LoudnessAnalyzer::calculateLUFS (float meanSquared) const noexcept
{
    if (meanSquared <= 0.0f)
        return -70.0f;

    // LUFS = -0.691 + 10 * log10(meanSquared)
    // Reference: ITU-R BS.1770
    return -0.691f + 10.0f * std::log10 (meanSquared);
}

float LoudnessAnalyzer::calculateLRA() noexcept
{
    // LRA calculation per EBU Tech 3342
    // 1. Collect short-term loudness values
    // 2. Remove values below -70 LUFS (silence)
    // 3. Calculate 10th and 95th percentiles
    // 4. LRA = 95th - 10th percentile

    juce::Array<float> validValues;
    for (float v : shortTermHistory)
    {
        if (v > -70.0f)
            validValues.add (v);
    }

    if (validValues.size() < 10)
        return 0.0f;

    validValues.sort();

    int idx10 = static_cast<int> (0.1f * validValues.size());
    int idx95 = static_cast<int> (0.95f * validValues.size());

    float p10 = validValues[idx10];
    float p95 = validValues[idx95];

    return p95 - p10;
}

//==============================================================================
void LoudnessAnalyzer::analyzeFullBuffer (const juce::AudioBuffer<float>& buffer)
{
    // Process entire buffer at once for offline analysis
    const int numSamples = buffer.getNumSamples();
    const int blockSize = 4096;

    for (int offset = 0; offset < numSamples; offset += blockSize)
    {
        int currentBlockSize = juce::jmin (blockSize, numSamples - offset);
        juce::AudioBuffer<float> block (buffer.getNumChannels(), currentBlockSize);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            block.copyFrom (ch, 0, buffer, ch, offset, currentBlockSize);

        processBlock (block);
    }
}