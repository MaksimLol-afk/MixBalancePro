#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Parameter Manager for MixBalancePro

    Handles all plugin parameters with proper APVTS integration
*/
class ParameterManager
{
public:
    //==============================================================================
    ParameterManager (juce::AudioProcessor& processor);
    ~ParameterManager() = default;

    //==============================================================================
    void initializeParameters();
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    const juce::AudioProcessorValueTreeState& getAPVTS() const { return apvts; }

    //==============================================================================
    // Parameter access
    float getParameterValue (const juce::String& paramID) const;
    void setParameterValue (const juce::String& paramID, float value);
    juce::String getParameterID (int index) const;
    int getParameterIndex (const juce::String& paramID) const;

    //==============================================================================
    // State management
    juce::ValueTree getState() const;
    void setState (const juce::ValueTree& state);

    //==============================================================================
    // Parameter layout creation
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==============================================================================
    // Parameter IDs (static for compile-time safety)
    struct ParamIDs
    {
        static constexpr auto analysisMode = "analysisMode";
        static constexpr auto captureLength = "captureLength";
        static constexpr auto conflictThreshold = "conflictThreshold";
        static constexpr auto loudnessTarget = "loudnessTarget";
        static constexpr auto showRecommendations = "showRecommendations";
        static constexpr auto autoGainComp = "autoGainComp";
        static constexpr auto referenceLevel = "referenceLevel";
        static constexpr auto meterRange = "meterRange";
        static constexpr auto fftSize = "fftSize";
        static constexpr auto averagingTime = "averagingTime";
        static constexpr auto peakHoldTime = "peakHoldTime";
        static constexpr auto psychoacousticModel = "psychoacousticModel";
        static constexpr auto conflictSensitivity = "conflictSensitivity";
    };

private:
    //==============================================================================
    juce::AudioProcessor& processor;
    juce::AudioProcessorValueTreeState apvts;

    // Parameter value cache for fast access
    mutable std::map<juce::String, float> valueCache;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterManager)
};