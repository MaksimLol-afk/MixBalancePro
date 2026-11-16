#pragma once

#include <JuceHeader.h>
#include "LoudnessAnalyzer.h"
#include "ConflictDetector.h"
#include "DynamicRangeAnalyzer.h"
#include "SpectrumAnalyzer.h"

//==============================================================================
/**
    Intelligent Mixing Recommendation Engine

    Generates actionable recommendations for:
    - EQ adjustments (frequency, gain, Q, type)
    - Panning changes
    - Compression settings (ratio, threshold, attack, release)
    - Saturation/Exciter suggestions
    - Level balancing
    - Stereo width adjustments
*/
class RecommendationEngine
{
public:
    struct Recommendation
    {
        enum class Type
        {
            EQ_Cut,
            EQ_Boost,
            EQ_Notch,
            EQ_HighPass,
            EQ_LowPass,
            Panning,
            Compression,
            Saturation,
            LevelAdjust,
            StereoWidth,
            Sidechain,
            TransientShaping,
            DeEssing,
            MultiBandComp
        };

        enum class Priority
        {
            Critical,   // Must fix - major issue
            High,       // Should fix - noticeable issue
            Medium,     // Could improve - subtle issue
            Low,        // Polish - minor enhancement
            Info        // Informational only
        };

        Type type;
        Priority priority;
        juce::String targetTrack;      // Which track to apply to
        juce::String description;      // Human-readable description
        juce::String technicalDetails; // Parameter values

        // EQ specific
        float frequency = 0.0f;        // Hz
        float gain = 0.0f;             // dB
        float q = 1.0f;                // Q factor
        int filterType = 0;            // 0=Bell, 1=HighShelf, 2=LowShelf, 3=Notch, 4=HP, 5=LP

        // Compression specific
        float ratio = 1.0f;
        float threshold = 0.0f;        // dB
        float attack = 0.0f;           // ms
        float release = 0.0f;          // ms
        float knee = 0.0f;             // dB

        // Panning specific
        float panPosition = 0.0f;      // -1 to 1

        // Saturation specific
        float drive = 0.0f;            // dB
        int saturationType = 0;        // 0=Tube, 1=Tape, 2=Transistor, 3=SoftClip

        // Stereo width
        float width = 1.0f;            // 0=mono, 1=normal, >1=wide

        // Source of recommendation
        juce::String source;           // "Loudness", "Conflict", "Dynamics", "Spectrum"

        // Confidence 0-1
        float confidence = 0.0f;

        juce::String toString() const
        {
            juce::String str;
            static const char* typeNames[] = {
                "EQ Cut", "EQ Boost", "EQ Notch", "High Pass", "Low Pass",
                "Panning", "Compression", "Saturation", "Level Adjust",
                "Stereo Width", "Sidechain", "Transient Shaping", "De-Essing", "Multiband Comp"
            };
            static const char* priorityNames[] = { "Critical", "High", "Medium", "Low", "Info" };

            str << "[" << priorityNames[static_cast<int>(priority)] << "] "
                << typeNames[static_cast<int>(type)] << ": " << description;

            if (!technicalDetails.isEmpty())
                str << " (" << technicalDetails << ")";

            return str;
        }
    };

    //==============================================================================
    RecommendationEngine (MixBalanceProAudioProcessor& processor);
    ~RecommendationEngine() = default;

    //==============================================================================
    void prepare (double sampleRate);
    void reset();
    void clearHistory();

    //==============================================================================
    // Main recommendation generation
    juce::Array<Recommendation> generateFullReport (
        const LoudnessAnalyzer::LoudnessMeasurement& loudness,
        const juce::Array<ConflictDetector::FrequencyConflict>& conflicts,
        const DynamicRangeAnalyzer::DynamicRangeMeasurement& dynamics,
        const SpectrumAnalyzer::SpectrumMeasurement& spectrum
    );

    juce::Array<Recommendation> generateFromConflict (const ConflictDetector::FrequencyConflict& conflict);
    juce::Array<Recommendation> generateFromLoudness (const LoudnessAnalyzer::LoudnessMeasurement& loudness);
    juce::Array<Recommendation> generateFromDynamics (const DynamicRangeAnalyzer::DynamicRangeMeasurement& dynamics);
    juce::Array<Recommendation> generateFromSpectrum (const SpectrumAnalyzer::SpectrumMeasurement& spectrum);
    juce::Array<Recommendation> generateMultiTrackRecommendations (
        const juce::Array<juce::AudioBuffer<float>*>& buffers,
        const juce::Array<juce::String>& trackNames
    );

    //==============================================================================
    const juce::Array<Recommendation>& getAllRecommendations() const noexcept { return allRecommendations; }
    juce::Array<Recommendation> getRecommendationsForTrack (const juce::String& trackName) const;
    juce::Array<Recommendation> getRecommendationsByType (Recommendation::Type type) const;
    juce::Array<Recommendation> getRecommendationsByPriority (Recommendation::Priority priority) const;

private:
    //==============================================================================
    MixBalanceProAudioProcessor& processor;

    juce::Array<Recommendation> allRecommendations;
    int recommendationCounter = 0;

    //==============================================================================
    // EQ Recommendations
    void addEQRecommendation (Recommendation::Type type, const juce::String& track, float freq, float gain, float q,
                              Recommendation::Priority priority, const juce::String& desc, const juce::String& source);
    juce::String freqToString (float freq) const;
    juce::String freqToNote (float freq) const;

    // Conflict-based recommendations
    void processConflictForEQ (const ConflictDetector::FrequencyConflict& conflict);
    void processConflictForPanning (const ConflictDetector::FrequencyConflict& conflict);
    void processConflictForCompression (const ConflictDetector::FrequencyConflict& conflict);
    void processConflictForSaturation (const ConflictDetector::FrequencyConflict& conflict);

    // Loudness-based recommendations
    void processLoudnessTarget (const LoudnessAnalyzer::LoudnessMeasurement& loudness);
    void processTruePeak (const LoudnessAnalyzer::LoudnessMeasurement& loudness);
    void processLRA (const LoudnessAnalyzer::LoudnessMeasurement& loudness);

    // Dynamics-based recommendations
    void processCrestFactor (const DynamicRangeAnalyzer::DynamicRangeMeasurement& dynamics);
    void processDRValue (const DynamicRangeAnalyzer::DynamicRangeMeasurement& dynamics);
    void processCompression (const DynamicRangeAnalyzer::DynamicRangeMeasurement& dynamics);
    void processMicroDynamics (const DynamicRangeAnalyzer::DynamicRangeMeasurement& dynamics);

    // Spectrum-based recommendations
    void processFrequencyBalance (const SpectrumAnalyzer::SpectrumMeasurement& spectrum);
    void processResonances (const SpectrumAnalyzer::SpectrumMeasurement& spectrum);
    void processSpectralFeatures (const SpectrumAnalyzer::SpectrumMeasurement& spectrum);

    // Multi-track recommendations
    void analyzeTrackRoles (const juce::Array<juce::AudioBuffer<float>*>& buffers,
                            const juce::Array<juce::String>& trackNames);
    void recommendTrackLevels (const juce::Array<juce::AudioBuffer<float>*>& buffers,
                               const juce::Array<juce::String>& trackNames);
    void recommendPanningSpread (const juce::Array<juce::AudioBuffer<float>*>& buffers,
                                 const juce::Array<juce::String>& trackNames);

    // Helpers
    Recommendation::Priority severityToPriority (float severity) const;
    float calculateEQGainForConflict (const ConflictDetector::FrequencyConflict& conflict) const;
    float calculateCompressionRatioForDynamics (float drValue) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecommendationEngine)
};