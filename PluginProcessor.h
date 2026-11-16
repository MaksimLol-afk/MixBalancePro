#pragma once

#include <JuceHeader.h>
#include "Analysis/LoudnessAnalyzer.h"
#include "Analysis/ConflictDetector.h"
#include "Analysis/DynamicRangeAnalyzer.h"
#include "Analysis/RecommendationEngine.h"
#include "Analysis/SpectrumAnalyzer.h"
#include "DSP/FFTProcessor.h"
#include "Utils/ParameterManager.h"
#include "Utils/PresetManager.h"

class MixBalanceProAudioProcessor;

//==============================================================================
/**
    MixBalancePro - Professional Mix Balance Analyzer

    Analyzes loudness, frequency conflicts, dynamic range and provides
    intelligent mixing recommendations (EQ, panning, saturation, compression).
*/
using Recommendation = RecommendationEngine::Recommendation;

class MixBalanceProAudioProcessor  : public juce::AudioProcessor,
                                     public juce::AudioProcessorParameter::Listener
{
public:
    enum class AnalysisMode
    {
        RealTime,
        Capture,
        Offline
    };

    //==============================================================================
    MixBalanceProAudioProcessor();
    ~MixBalanceProAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter listener
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override {}

    //==============================================================================
    // Analysis accessors
    const LoudnessAnalyzer& getLoudnessAnalyzer() const noexcept { return loudnessAnalyzer; }
    const ConflictDetector& getConflictDetector() const noexcept { return conflictDetector; }
    const DynamicRangeAnalyzer& getDynamicRangeAnalyzer() const noexcept { return dynamicRangeAnalyzer; }
    const RecommendationEngine& getRecommendationEngine() const noexcept { return recommendationEngine; }
    const SpectrumAnalyzer& getSpectrumAnalyzer() const noexcept { return spectrumAnalyzer; }

    ParameterManager& getParameterManager() noexcept { return parameterManager; }
    PresetManager& getPresetManager() noexcept { return presetManager; }

    struct TrackReference
    {
        juce::String name;
        juce::AudioProcessor* processor = nullptr;
        juce::AudioBuffer<float> lastBuffer;
        std::chrono::steady_clock::time_point lastUpdate;
    };
    juce::Array<TrackReference>& getTrackReferences() noexcept { return trackReferences; }
    const juce::Array<TrackReference>& getTrackReferences() const noexcept { return trackReferences; }

    // Analysis control
    void setAnalysisMode (AnalysisMode mode);
    void requestFullAnalysis();
    void clearAnalysisHistory();

    // Track management (for multi-track conflict detection)
    void addTrackReference (const juce::String& trackName, juce::AudioProcessor* processor);
    void removeTrackReference (const juce::String& trackName);
    void analyzeTrackGroup (const juce::Array<juce::String>& trackNames);

    // Callbacks for UI
    std::function<void()> onAnalysisUpdate;
    std::function<void(const Recommendation&)> onNewRecommendation;

private:
    //==============================================================================
    //==============================================================================
    // Core analyzers
    LoudnessAnalyzer loudnessAnalyzer;
    ConflictDetector conflictDetector;
    DynamicRangeAnalyzer dynamicRangeAnalyzer;
    SpectrumAnalyzer spectrumAnalyzer;
    RecommendationEngine recommendationEngine;

    // DSP
    FFTProcessor fftProcessor;

    // Managers
    ParameterManager parameterManager;
    PresetManager presetManager;

    // Analysis state
    AnalysisMode currentMode = AnalysisMode::RealTime;
    juce::AudioBuffer<float> captureBuffer;
    int capturePosition = 0;
    int captureLength = 44100 * 30; // 30 seconds at 44.1kHz
    bool isCapturing = false;

    // Track references for multi-track analysis
    juce::Array<TrackReference> trackReferences;

    // Smoothing for meters
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedRMSLeft;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedRMSRight;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedLUFS;

    //==============================================================================
    void updateAnalyzers (const juce::AudioBuffer<float>& buffer);
    void processCaptureBuffer();
    void generateRecommendations();
    void updateTrackReferences();

    // Parameter IDs
    static constexpr auto paramAnalysisMode = "analysisMode";
    static constexpr auto paramCaptureLength = "captureLength";
    static constexpr auto paramConflictThreshold = "conflictThreshold";
    static constexpr auto paramLoudnessTarget = "loudnessTarget";
    static constexpr auto paramShowRecommendations = "showRecommendations";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixBalanceProAudioProcessor)
};