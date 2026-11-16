#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Dynamic Range Analyzer

    Measures:
    - Crest Factor (Peak/RMS)
    - Dynamic Range (DR) per EBU R128 / TT DR Meter
    - Micro-dynamics (short-term variations)
    - Macro-dynamics (long-term variations)
    - PLR (Peak to Loudness Ratio)
    - True Peak to LUFS ratio
*/
class DynamicRangeAnalyzer
{
public:
    struct DynamicRangeMeasurement
    {
        // Crest factor measurements
        float crestFactor = 0.0f;           // Peak / RMS (linear)
        float crestFactorDB = 0.0f;         // 20*log10(crestFactor)
        float truePeakCrestFactor = 0.0f;   // TruePeak / RMS

        // Dynamic Range (EBU R128 LRA-based)
        float dynamicRange = 0.0f;          // LRA in LU
        float dynamicRangePercentile = 0.0f; // 95th-10th percentile

        // TT Dynamic Range Meter style
        float drValue = 0.0f;               // DR1-DR20 scale
        float rmsLevel = -70.0f;            // Average RMS dBFS
        float peakLevel = -70.0f;           // Sample peak dBFS
        float truePeakLevel = -70.0f;       // True peak dBFS

        // Micro-dynamics (short-term)
        float microDynamics = 0.0f;         // Short-term crest variation
        float shortTermCrestMin = 0.0f;
        float shortTermCrestMax = 0.0f;

        // Macro-dynamics (long-term)
        float macroDynamics = 0.0f;         // Long-term level variation
        float sectionLevelVariation = 0.0f; // Verse/chorus difference

        // PLR (Peak to Loudness Ratio)
        float plr = 0.0f;                   // TruePeak - IntegratedLUFS

        // Compression detection
        float estimatedCompressionRatio = 1.0f;
        bool likelyOvercompressed = false;
        juce::String compressionAssessment;

        // History for trending
        juce::Array<float> crestHistory;
        juce::Array<float> rmsHistory;
        juce::Array<float> peakHistory;
    };

    using MeasurementCallback = std::function<void(const DynamicRangeMeasurement&)>;

    //==============================================================================
    DynamicRangeAnalyzer (double sampleRate = 44100.0);
    ~DynamicRangeAnalyzer() = default;

    //==============================================================================
    void prepare (double sampleRate, int samplesPerBlock);
    void reset();
    void processBlock (const juce::AudioBuffer<float>& buffer);
    void analyzeFullBuffer (const juce::AudioBuffer<float>& buffer);

    //==============================================================================
    const DynamicRangeMeasurement& getLatestMeasurement() const noexcept { return currentMeasurement; }
    DynamicRangeMeasurement getMeasurement() const noexcept { return currentMeasurement; }

    // Callbacks
    MeasurementCallback onMeasurementUpdate;

    //==============================================================================
    // Assessment helpers
    static juce::String assessDynamicRange (float drValue);
    static juce::String assessCrestFactor (float crestDB);
    static juce::String assessCompression (float estimatedRatio);

private:
    //==============================================================================
    double sampleRate = 44100.0;
    int blockSize = 512;

    // Measurement windows
    struct WindowState
    {
        juce::Array<float> rmsValues;
        juce::Array<float> peakValues;
        juce::Array<float> truePeakValues;
        int writePos = 0;
        int maxSize = 0;
    };

    WindowState shortTermWindow;   // ~3 seconds
    WindowState longTermWindow;    // ~1 minute
    WindowState sessionWindow;     // Full session

    // Current measurement
    DynamicRangeMeasurement currentMeasurement;

    // Smoothing
    float smoothedRMS = -70.0f;
    float smoothedPeak = -70.0f;
    float smoothingFactor = 0.99f;

    // Compression detection
    struct CompressionDetector
    {
        juce::Array<float> levelHistory;
        int historySize = 44100 / 512 * 10; // 10 seconds of blocks
        float lastLevel = -70.0f;

        void reset() { levelHistory.clear(); levelHistory.insertMultiple(0, -70.0f, historySize); }
        void addLevel (float level);
        float estimateRatio() const;
    } compressionDetector;

    //==============================================================================
    void updateMeasurements (const juce::AudioBuffer<float>& buffer);
    void calculateShortTermDynamics();
    void calculateLongTermDynamics();
    void calculateSessionDynamics();
    void detectCompression();
    float calculateCrestFactor (float peak, float rms) const noexcept;
    float calculateDRValue (float lra) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicRangeAnalyzer)
};