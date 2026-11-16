#include "ParameterManager.h"

//==============================================================================
ParameterManager::ParameterManager (juce::AudioProcessor& p)
    : processor (p),
      apvts (p, nullptr, "PARAMETERS", createParameterLayout())
{
}

void ParameterManager::initializeParameters()
{
    // Parameters are created in createParameterLayout()
    // This method can be used for post-initialization setup

    // Set up parameter listeners if needed
    // apvts.addParameterListener (ParamIDs::analysisMode, ...);
}

float ParameterManager::getParameterValue (const juce::String& paramID) const
{
    auto* param = apvts.getParameter (paramID);
    return param ? param->getValue() : 0.0f;
}

void ParameterManager::setParameterValue (const juce::String& paramID, float value)
{
    auto* param = apvts.getParameter (paramID);
    if (param)
        param->setValueNotifyingHost (value);
}

juce::String ParameterManager::getParameterID (int index) const
{
    if (index >= 0 && index < apvts.getParameterCount())
    {
        return apvts.getParameter (index)->getParameterID();
    }
    return {};
}

int ParameterManager::getParameterIndex (const juce::String& paramID) const
{
    for (int i = 0; i < apvts.getParameterCount(); ++i)
    {
        if (apvts.getParameter (i)->getParameterID() == paramID)
            return i;
    }
    return -1;
}

//==============================================================================
juce::ValueTree ParameterManager::getState() const
{
    return apvts.copyState();
}

void ParameterManager::setState (const juce::ValueTree& state)
{
    apvts.replaceState (state);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ParameterManager::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Analysis Mode
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::analysisMode,
        "Analysis Mode",
        juce::StringArray { "Real-time", "Capture (30s)", "Offline Analysis" },
        0
    ));

    // Capture Length (minutes)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::captureLength,
        "Capture Length",
        juce::NormalisableRange<float> (0.5f, 10.0f, 0.5f),
        0.5f,
        juce::AudioParameterFloatAttributes().withLabel ("min")
    ));

    // Conflict Detection Threshold
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::conflictThreshold,
        "Conflict Threshold",
        juce::NormalisableRange<float> (0.1f, 1.0f, 0.01f),
        0.3f,
        juce::AudioParameterFloatAttributes().withLabel ("")
    ));

    // Loudness Target
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::loudnessTarget,
        "Loudness Target",
        juce::StringArray { "Streaming (-14 LUFS)", "CD (-9 LUFS)", "Club (-6 LUFS)", "Broadcast (-23 LUFS)", "Custom" },
        0
    ));

    // Show Recommendations
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::showRecommendations,
        "Show Recommendations",
        true
    ));

    // Auto Gain Compensation
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::autoGainComp,
        "Auto Gain Compensation",
        false
    ));

    // Reference Level for metering
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::referenceLevel,
        "Reference Level",
        juce::NormalisableRange<float> (-20.0f, 0.0f, 0.5f),
        -18.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dBFS")
    ));

    // Meter Range
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::meterRange,
        "Meter Range",
        juce::StringArray { "60 dB", "90 dB", "120 dB" },
        1
    ));

    // FFT Size
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::fftSize,
        "FFT Size",
        juce::StringArray { "1024", "2048", "4096", "8192", "16384" },
        2
    ));

    // Averaging Time
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::averagingTime,
        "Averaging Time",
        juce::NormalisableRange<float> (0.1f, 5.0f, 0.1f, 0.5f),
        0.5f,
        juce::AudioParameterFloatAttributes().withLabel ("s")
    ));

    // Peak Hold Time
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::peakHoldTime,
        "Peak Hold Time",
        juce::NormalisableRange<float> (0.5f, 10.0f, 0.1f, 0.5f),
        2.0f,
        juce::AudioParameterFloatAttributes().withLabel ("s")
    ));

    // Psychoacoustic Model
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::psychoacousticModel,
        "Psychoacoustic Model",
        true
    ));

    // Conflict Sensitivity
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::conflictSensitivity,
        "Conflict Sensitivity",
        juce::NormalisableRange<float> (0.1f, 3.0f, 0.1f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel ("x")
    ));

    return { params.begin(), params.end() };
}