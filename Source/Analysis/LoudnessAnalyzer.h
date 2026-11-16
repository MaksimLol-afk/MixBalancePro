#pragma once

#include <JuceHeader.h>
#include "DSP/FFTProcessor.h"

//==============================================================================
/**
    EBU R128 / ITU-R BS.1770 Loudness Analyzer

    Measures:
    - Momentary Loudness (400ms window) - M
    - Short-term Loudness (3s window) - S
    - Integrated Loudness (full duration) - I
    - Loudness Range (LRA)
    - True Peak
*/
class LoudnessAnalyzer
{
public:
    struct LoudnessMeasurement
    {
        float momentaryLUFS = -70.0f;      // M (400ms)
        float shortTermLUFS = -70.0f;      // S (3s)
        float integratedLUFS = -70.0f;     // I (full)
        float loudnessRange = 0.0f;        // LRA
        float truePeakLeft = -70.0f;       // dBTP
        float truePeakRight = -70.0f;      // dBTP
        float truePeakMax = -70.0f;        // dBTP max of both

        // History for LRA calculation
        juce::Array<float> shortTermHistory;
        int historyIndex = 0;

        // Target compliance
        bool meetsStreamingTarget = false;  // -14 LUFS ±1
        bool meetsBroadcastTarget = false;  // -23 LUFS ±0.5
        bool meetsClubTarget = false;       // -6 to -9 LUFS
    };

    using MeasurementCallback = std::function<void(const LoudnessMeasurement&)>;

    //==============================================================================
    LoudnessAnalyzer (double sampleRate = 44100.0);
    ~LoudnessAnalyzer() = default;

    //==============================================================================
    void prepare (double sampleRate, int samplesPerBlock);
    void reset();
    void processBlock (const juce::AudioBuffer<float>& buffer);
    void analyzeFullBuffer (const juce::AudioBuffer<float>& buffer);

    //==============================================================================
    const LoudnessMeasurement& getLatestMeasurement() const noexcept { return currentMeasurement; }
    LoudnessMeasurement getMeasurement() const noexcept { return currentMeasurement; }

    // Target setting
    void setTargetLUFS (float target) noexcept { targetLUFS = target; }
    float getTargetLUFS() const noexcept { return targetLUFS; }

    // Callbacks
    MeasurementCallback onMeasurementUpdate;

    //==============================================================================
    // K-weighting filter coefficients (ITU-R BS.1770)
    static constexpr float kWeightingBiquadCoeffs[5] = {
        // Pre-filter (high shelf at 1.68 kHz, +4 dB)
        1.53512485958697f, -2.69169618940638f, 1.19839281085285f,
        -1.69065929318241f, 0.73248077421585f
    };

    // RLB filter (high-pass at 38 Hz)
    static constexpr float rlbBiquadCoeffs[5] = {
        0.99957121990616f, -1.99914243981232f, 0.99957121990616f,
        -1.99914243981232f, 0.99914243981232f
    };

private:
    //==============================================================================
    double sampleRate = 44100.0;
    int blockSize = 512;

    // Filter states (per channel)
    struct FilterState
    {
        float kWeightingX1 = 0, kWeightingX2 = 0, kWeightingY1 = 0, kWeightingY2 = 0;
        float rlbX1 = 0, rlbX2 = 0, rlbY1 = 0, rlbY2 = 0;
    };
    FilterState leftState, rightState;

    // Measurement windows
    struct WindowState
    {
        juce::Array<float> squaredSamples;  // K-weighted squared samples
        int writePos = 0;
        float sum = 0.0f;
    };

    WindowState momentaryWindow;   // 400ms
    WindowState shortTermWindow;   // 3s
    WindowState integratedWindow;  // Full duration

    int momentaryWindowSize = 0;
    int shortTermWindowSize = 0;

    // True peak detection (4x oversampling)
    struct TruePeakState
    {
        juce::dsp::IIR::Filter<float> upsampleFilter;
        float maxPeak = -70.0f;
    };
    TruePeakState truePeakLeft, truePeakRight;

    // Current measurement
    LoudnessMeasurement currentMeasurement;
    float targetLUFS = -14.0f;

    // LRA calculation
    static constexpr int LRA_HISTORY_SIZE = 1000;  // 1000 * 3s = 50 minutes max
    juce::Array<float> shortTermHistory;
    int historyIndex = 0;

    //==============================================================================
    void applyKWeighting (const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output);
    void updateWindows (const juce::AudioBuffer<float>& kWeighted);
    void updateTruePeak (const juce::AudioBuffer<float>& buffer);
    void calculateMeasurements();
    float calculateLUFS (float meanSquared) const noexcept;
    float calculateLRA() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessAnalyzer)
};