#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MixBalanceProAudioProcessor::MixBalanceProAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)  // For reference track
    ),
    loudnessAnalyzer (getSampleRate()),
    conflictDetector (getSampleRate()),
    dynamicRangeAnalyzer (getSampleRate()),
    spectrumAnalyzer (getSampleRate()),
    recommendationEngine (*this),
    fftProcessor (2048),
    parameterManager (*this),
    presetManager (*this),
    smoothedRMSLeft (0.1),
    smoothedRMSRight (0.1),
    smoothedLUFS (0.5)
{
    // Initialize parameter manager (creates all parameters)
    parameterManager.initializeParameters();

    // Set up analyzer callbacks
    loudnessAnalyzer.onMeasurementUpdate = [this] (const LoudnessMeasurement& m)
    {
        smoothedLUFS.setTargetValue (m.integratedLUFS);
    };

    conflictDetector.onConflictDetected = [this] (const FrequencyConflict& conflict)
    {
        if (onNewRecommendation)
        {
            auto rec = recommendationEngine.generateFromConflict (conflict);
            onNewRecommendation (rec);
        }
    };

    dynamicRangeAnalyzer.onMeasurementUpdate = [this] (const DynamicRangeMeasurement& m)
    {
        // Trigger recommendation if dynamic range is too low
        if (m.crestFactor < 3.0f && onNewRecommendation)
        {
            auto rec = recommendationEngine.generateDynamicRangeRecovery (m);
            onNewRecommendation (rec);
        }
    };

    // Initialize capture buffer
    captureBuffer.setSize (2, captureLength);
    captureBuffer.clear();
}

MixBalanceProAudioProcessor::~MixBalanceProAudioProcessor() = default;

//==============================================================================
void MixBalanceProAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Prepare all analyzers
    loudnessAnalyzer.prepare (sampleRate, samplesPerBlock);
    conflictDetector.prepare (sampleRate, samplesPerBlock);
    dynamicRangeAnalyzer.prepare (sampleRate, samplesPerBlock);
    spectrumAnalyzer.prepare (sampleRate, samplesPerBlock);
    fftProcessor.prepare (sampleRate, samplesPerBlock);
    recommendationEngine.prepare (sampleRate);

    // Reset smoothing
    smoothedRMSLeft.reset (sampleRate, 0.1);
    smoothedRMSRight.reset (sampleRate, 0.1);
    smoothedLUFS.reset (sampleRate, 0.5);

    // Clear capture buffer
    captureBuffer.clear();
    capturePosition = 0;
}

void MixBalanceProAudioProcessor::releaseResources()
{
    loudnessAnalyzer.reset();
    conflictDetector.reset();
    dynamicRangeAnalyzer.reset();
    spectrumAnalyzer.reset();
    fftProcessor.reset();
}

bool MixBalanceProAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Support stereo and mono
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Input must match output
    if (layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
        return false;

    return true;
}

void MixBalanceProAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    // Get sidechain input if connected (for reference track comparison)
    auto* sidechainBus = getBus (true, 1); // Sidechain input
    juce::AudioBuffer<float>* sidechainBuffer = nullptr;
    if (sidechainBus && sidechainBus->isEnabled())
    {
        // In a real implementation, you'd get the sidechain buffer from the host
        // For now, we'll use the main input as reference
    }

    // Update smoothed RMS meters
    if (numChannels >= 2)
    {
        float rmsL = 0.0f, rmsR = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            rmsL += buffer.getSample (0, i) * buffer.getSample (0, i);
            rmsR += buffer.getSample (1, i) * buffer.getSample (1, i);
        }
        smoothedRMSLeft.setTargetValue (std::sqrt (rmsL / numSamples));
        smoothedRMSRight.setTargetValue (std::sqrt (rmsR / numSamples));
    }

    // Process based on analysis mode
    switch (currentMode)
    {
        case AnalysisMode::RealTime:
            updateAnalyzers (buffer);
            break;

        case AnalysisMode::Capture:
            if (isCapturing)
            {
                int remaining = captureLength - capturePosition;
                int toCopy = juce::jmin (numSamples, remaining);

                if (numChannels >= 1)
                    buffer.copyFrom (0, capturePosition, buffer, 0, 0, toCopy);
                if (numChannels >= 2)
                    buffer.copyFrom (1, capturePosition, buffer, 1, 0, toCopy);

                capturePosition += toCopy;

                if (capturePosition >= captureLength)
                {
                    isCapturing = false;
                    processCaptureBuffer();
                }
            }
            // Still run real-time analysis during capture
            updateAnalyzers (buffer);
            break;

        case AnalysisMode::Offline:
            // Offline mode processes entire buffer at once
            updateAnalyzers (buffer);
            break;
    }

    // Update track references for multi-track analysis
    updateTrackReferences();

    // Notify UI of updates
    if (onAnalysisUpdate)
        onAnalysisUpdate();
}

void MixBalanceProAudioProcessor::updateAnalyzers (const juce::AudioBuffer<float>& buffer)
{
    // Run all analyzers
    loudnessAnalyzer.processBlock (buffer);
    conflictDetector.processBlock (buffer);
    dynamicRangeAnalyzer.processBlock (buffer);
    spectrumAnalyzer.processBlock (buffer);

    // Process FFT for visualization
    fftProcessor.processBlock (buffer);
}

//==============================================================================
juce::AudioProcessorEditor* MixBalanceProAudioProcessor::createEditor()
{
    return new MixBalanceProAudioProcessorEditor (*this);
}

bool MixBalanceProAudioProcessor::hasEditor() const
{
    return true;
}

//==============================================================================
const juce::String MixBalanceProAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MixBalanceProAudioProcessor::acceptsMidi() const { return false; }
bool MixBalanceProAudioProcessor::producesMidi() const { return false; }
bool MixBalanceProAudioProcessor::isMidiEffect() const { return false; }
double MixBalanceProAudioProcessor::getTailLengthSeconds() const { return 0.0; }

//==============================================================================
int MixBalanceProAudioProcessor::getNumPrograms() { return 1; }
int MixBalanceProAudioProcessor::getCurrentProgram() { return 0; }
void MixBalanceProAudioProcessor::setCurrentProgram (int) {}
const juce::String MixBalanceProAudioProcessor::getProgramName (int) { return {}; }
void MixBalanceProAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void MixBalanceProAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save parameters
    auto state = parameterManager.getState();
    // Save analyzer settings
    state.setProperty ("analysisMode", static_cast<int> (currentMode), nullptr);
    state.setProperty ("captureLength", captureLength, nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MixBalanceProAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
    {
        juce::ValueTree state = juce::ValueTree::fromXml (*xmlState);
        parameterManager.setState (state);

        if (state.hasProperty ("analysisMode"))
            currentMode = static_cast<AnalysisMode> (static_cast<int> (state.getProperty ("analysisMode")));
        if (state.hasProperty ("captureLength"))
            captureLength = state.getProperty ("captureLength");
    }
}

//==============================================================================
void MixBalanceProAudioProcessor::parameterValueChanged (int parameterIndex, float newValue)
{
    const auto paramID = parameterManager.getParameterID (parameterIndex);

    if (paramID == paramAnalysisMode)
    {
        currentMode = static_cast<AnalysisMode> (static_cast<int> (newValue));
    }
    else if (paramID == paramCaptureLength)
    {
        captureLength = static_cast<int> (newValue * 44100 * 60); // Convert minutes to samples
        captureBuffer.setSize (2, captureLength);
        captureBuffer.clear();
        capturePosition = 0;
    }
    else if (paramID == paramConflictThreshold)
    {
        conflictDetector.setThreshold (newValue);
    }
    else if (paramID == paramLoudnessTarget)
    {
        loudnessAnalyzer.setTargetLUFS (newValue - 24.0f); // Convert 0-24 range to -24 to 0
    }
}

void MixBalanceProAudioProcessor::setAnalysisMode (AnalysisMode mode)
{
    currentMode = mode;
    if (mode == AnalysisMode::Capture)
    {
        isCapturing = true;
        capturePosition = 0;
        captureBuffer.clear();
    }
}

void MixBalanceProAudioProcessor::requestFullAnalysis()
{
    setAnalysisMode (AnalysisMode::Capture);
}

void MixBalanceProAudioProcessor::clearAnalysisHistory()
{
    loudnessAnalyzer.reset();
    conflictDetector.reset();
    dynamicRangeAnalyzer.reset();
    spectrumAnalyzer.reset();
    recommendationEngine.clearHistory();
}

void MixBalanceProAudioProcessor::processCaptureBuffer()
{
    // Run comprehensive analysis on captured audio
    loudnessAnalyzer.analyzeFullBuffer (captureBuffer);
    conflictDetector.analyzeFullBuffer (captureBuffer);
    dynamicRangeAnalyzer.analyzeFullBuffer (captureBuffer);
    spectrumAnalyzer.analyzeFullBuffer (captureBuffer);

    generateRecommendations();
}

void MixBalanceProAudioProcessor::generateRecommendations()
{
    // Get all measurements
    auto loudness = loudnessAnalyzer.getLatestMeasurement();
    auto conflicts = conflictDetector.getActiveConflicts();
    auto dynamics = dynamicRangeAnalyzer.getLatestMeasurement();
    auto spectrum = spectrumAnalyzer.getAverageSpectrum();

    // Generate comprehensive recommendations
    auto recommendations = recommendationEngine.generateFullReport (
        loudness, conflicts, dynamics, spectrum
    );

    for (const auto& rec : recommendations)
    {
        if (onNewRecommendation)
            onNewRecommendation (rec);
    }
}

//==============================================================================
// Multi-track support
void MixBalanceProAudioProcessor::addTrackReference (const juce::String& trackName, juce::AudioProcessor* processor)
{
    TrackReference ref;
    ref.name = trackName;
    ref.processor = processor;
    ref.lastUpdate = std::chrono::steady_clock::now();
    trackReferences.add (ref);
}

void MixBalanceProAudioProcessor::removeTrackReference (const juce::String& trackName)
{
    for (int i = trackReferences.size() - 1; i >= 0; --i)
        if (trackReferences.getReference (i).name == trackName)
            trackReferences.remove (i);
}

void MixBalanceProAudioProcessor::analyzeTrackGroup (const juce::Array<juce::String>& trackNames)
{
    // Collect buffers from referenced tracks
    juce::Array<juce::AudioBuffer<float>*> buffers;
    for (const auto& name : trackNames)
    {
        for (auto& ref : trackReferences)
        {
            if (ref.name == name && ref.lastBuffer.getNumSamples() > 0)
            {
                buffers.add (&ref.lastBuffer);
                break;
            }
        }
    }

    if (buffers.size() >= 2)
    {
        conflictDetector.analyzeMultiTrack (buffers);
        recommendationEngine.generateMultiTrackRecommendations (buffers, trackNames);
    }
}

void MixBalanceProAudioProcessor::updateTrackReferences()
{
    // In a real implementation, this would pull audio from other plugin instances
    // via shared memory or host-provided track audio access
    // For now, this is a placeholder for the multi-track analysis feature
}