#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Utility functions for audio buffer operations
*/
namespace AudioBufferUtils
{
    //==============================================================================
    // Level measurements
    float getRMS (const juce::AudioBuffer<float>& buffer, int channel = 0);
    float getRMS (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples, int channel = 0);
    float getPeak (const juce::AudioBuffer<float>& buffer, int channel = 0);
    float getTruePeak (const juce::AudioBuffer<float>& buffer, int channel = 0);
    float getCrestFactor (const juce::AudioBuffer<float>& buffer, int channel = 0);

    //==============================================================================
    // Buffer operations
    void copyMono (const juce::AudioBuffer<float>& source, juce::AudioBuffer<float>& dest,
                   int sourceChannel = 0, int destChannel = 0);
    void sumToMono (const juce::AudioBuffer<float>& source, juce::AudioBuffer<float>& dest);
    void applyGain (juce::AudioBuffer<float>& buffer, float gainDb);
    void applyGainRamp (juce::AudioBuffer<float>& buffer, float startGainDb, float endGainDb);

    //==============================================================================
    // Channel operations
    void swapChannels (juce::AudioBuffer<float>& buffer, int ch1, int ch2);
    void invertPhase (juce::AudioBuffer<float>& buffer, int channel);
    float getCorrelation (const juce::AudioBuffer<float>& buffer, int ch1, int ch2);

    //==============================================================================
    // Spectral utilities
    float hzToBin (float hz, int fftSize, double sampleRate);
    float binToHz (int bin, int fftSize, double sampleRate);
    float hzToMel (float hz);
    float melToHz (float mel);
    float hzToBark (float hz);
    float barkToHz (float bark);

    //==============================================================================
    // Smoothing
    class LinearSmoother
    {
    public:
        LinearSmoother (float timeConstant = 0.1f, double sampleRate = 44100.0)
        {
            setTimeConstant (timeConstant, sampleRate);
        }

        void setTimeConstant (float tc, double sr)
        {
            coeff = std::exp (-1.0f / (tc * sr));
        }

        float process (float input)
        {
            current = coeff * current + (1.0f - coeff) * input;
            return current;
        }

        void reset (float value = 0.0f) { current = value; }
        float getCurrent() const { return current; }

    private:
        float coeff = 0.0f;
        float current = 0.0f;
    };

    //==============================================================================
    // Window functions
    enum class WindowType
    {
        Rectangular,
        Hann,
        Hamming,
        Blackman,
        BlackmanHarris,
        Kaiser
    };

    void applyWindow (juce::AudioBuffer<float>& buffer, WindowType type, float beta = 0.0f);
    juce::Array<float> generateWindow (int size, WindowType type, float beta = 0.0f);

    //==============================================================================
    // Interpolation
    float interpolateLinear (const float* data, float index, int size);
    float interpolateCubic (const float* data, float index, int size);
    float parabolicPeakInterpolation (const float* data, int peakIndex, int size);

    //==============================================================================
    // Statistics
    float mean (const float* data, int size);
    float variance (const float* data, int size, float meanVal = 0.0f);
    float stdDev (const float* data, int size);
    float skewness (const float* data, int size);
    float kurtosis (const float* data, int size);
    void minMax (const float* data, int size, float& minOut, float& maxOut);

    //==============================================================================
    // Percentile
    float percentile (juce::Array<float>& data, float percentile);  // 0-1

    //==============================================================================
    // Conversion
    inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
    inline float gainToDb (float gain) { return 20.0f * std::log10 (std::max (gain, 1e-10f)); }
    inline float linToDb (float lin) { return 20.0f * std::log10 (std::max (lin, 1e-10f)); }
    inline float dbToLin (float db) { return std::pow (10.0f, db / 20.0f); }
}