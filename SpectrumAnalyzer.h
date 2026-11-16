#pragma once

#include <JuceHeader.h>
#include "DSP/FFTProcessor.h"

//==============================================================================
/**
    Spectrum Analyzer

    Provides:
    - Real-time magnitude spectrum
    - Average/peak hold spectrum
    - Spectral centroid, spread, flux
    - Band energy analysis (1/3 octave, Bark, Mel)
    - Resonance detection
*/
class SpectrumAnalyzer
{
public:
    struct SpectrumMeasurement
    {
        // Main spectrum data
        juce::AudioBuffer<float> magnitudeSpectrum;  // Linear magnitude
        juce::AudioBuffer<float> magnitudeDB;        // dB scale
        juce::AudioBuffer<float> phaseSpectrum;

        // Averaged spectrum (exponential smoothing)
        juce::AudioBuffer<float> averageSpectrum;
        juce::AudioBuffer<float> peakHoldSpectrum;

        // Spectral features
        float spectralCentroid = 0.0f;      // Hz - "brightness"
        float spectralSpread = 0.0f;        // Hz - bandwidth
        float spectralFlux = 0.0f;          // Change rate
        float spectralRolloff = 0.0f;       // Hz - 85% energy
        float spectralFlatness = 0.0f;      // 0-1, noise vs tonal
        float spectralCrest = 0.0f;         // Peak/mean ratio

        // Band energies (1/3 octave)
        static constexpr int NUM_THIRD_OCTAVE = 31;
        juce::Array<float> thirdOctaveBands;
        juce::Array<float> thirdOctaveBandsDB;

        // Bark bands
        static constexpr int NUM_BARK = 24;
        juce::Array<float> barkBands;
        juce::Array<float> barkBandsDB;

        // Resonance detection
        struct Resonance
        {
            float frequency = 0.0f;
            float magnitude = 0.0f;
            float q = 0.0f;
            float prominence = 0.0f;  // How much it stands out
        };
        juce::Array<Resonance> detectedResonances;

        // Frequency balance assessment
        juce::String balanceAssessment;
        float lowEndLevel = 0.0f;    // 20-250 Hz
        float lowMidLevel = 0.0f;    // 250-500 Hz
        float midLevel = 0.0f;       // 500-2000 Hz
        float highMidLevel = 0.0f;   // 2-6 kHz
        float highLevel = 0.0f;      // 6-20 kHz
    };

    using MeasurementCallback = std::function<void(const SpectrumMeasurement&)>;

    //==============================================================================
    SpectrumAnalyzer (double sampleRate = 44100.0);
    ~SpectrumAnalyzer() = default;

    //==============================================================================
    void prepare (double sampleRate, int samplesPerBlock);
    void reset();
    void processBlock (const juce::AudioBuffer<float>& buffer);
    void analyzeFullBuffer (const juce::AudioBuffer<float>& buffer);

    //==============================================================================
    const SpectrumMeasurement& getLatestMeasurement() const noexcept { return currentMeasurement; }
    SpectrumMeasurement getMeasurement() const noexcept { return currentMeasurement; }
    const juce::AudioBuffer<float>& getAverageSpectrum() const noexcept { return currentMeasurement.averageSpectrum; }
    const juce::AudioBuffer<float>& getPeakHoldSpectrum() const noexcept { return currentMeasurement.peakHoldSpectrum; }

    // Configuration
    void setAveragingTime (float seconds) noexcept { averagingTime = seconds; }
    void setPeakHoldTime (float seconds) noexcept { peakHoldTime = seconds; }
    void setFFTSize (int size) { fftSize = size; }

    // Callbacks
    MeasurementCallback onMeasurementUpdate;

    //==============================================================================
    // 1/3 Octave band center frequencies (ISO 266)
    static constexpr float thirdOctaveCenters[31] = {
        20, 25, 31.5, 40, 50, 63, 80, 100,
        125, 160, 200, 250, 315, 400, 500, 630,
        800, 1000, 1250, 1600, 2000, 2500, 3150, 4000,
        5000, 6300, 8000, 10000, 12500, 16000, 20000
    };

private:
    //==============================================================================
    double sampleRate = 44100.0;
    int blockSize = 512;
    int fftSize = 4096;
    int hopSize = 1024;

    // FFT processor
    FFTProcessor fftProcessor;

    // Current measurement
    SpectrumMeasurement currentMeasurement;

    // Smoothing
    float averagingTime = 0.5f;   // seconds
    float peakHoldTime = 2.0f;    // seconds
    float avgAlpha = 0.0f;
    float peakAlpha = 0.0f;

    // Previous spectrum for flux calculation
    juce::AudioBuffer<float> prevMagnitudeSpectrum;

    //==============================================================================
    void computeSpectralFeatures();
    void updateThirdOctaveBands();
    void updateBarkBands();
    void detectResonances();
    void assessFrequencyBalance();
    float hzToBin (float hz) const noexcept;
    float binToHz (int bin) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};