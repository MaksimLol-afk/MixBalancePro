#pragma once

#include <JuceHeader.h>
#include "DSP/FFTProcessor.h"

//==============================================================================
/**
    Frequency Conflict Detector

    Detects masking and frequency conflicts between tracks/instruments:
    - Spectral overlap analysis
    - Masking threshold calculation (psychoacoustic model)
    - Conflict severity scoring
    - Specific frequency range identification
*/
class ConflictDetector
{
public:
    struct FrequencyConflict
    {
        juce::String trackA;
        juce::String trackB;
        float centerFreq = 0.0f;          // Hz
        float bandwidth = 0.0f;           // Hz
        float severity = 0.0f;            // 0.0 - 1.0
        float trackALevel = 0.0f;         // dB
        float trackBLevel = 0.0f;         // dB
        float maskingThreshold = 0.0f;    // dB
        ConflictType type = ConflictType::SpectralOverlap;

        enum class ConflictType
        {
            SpectralOverlap,      // Both tracks have energy in same band
            Masking,              // Track A masks Track B
            TonalClash,           // Fundamental frequencies clash
            HarmonicConflict,     // Harmonics overlap
            TransientCollision    // Transients at same time
        };

        juce::String getDescription() const
        {
            juce::String desc = trackA + " vs " + trackB + ": ";
            desc << juce::String (centerFreq, 1) << " Hz ±" << juce::String (bandwidth/2, 1) << " Hz";
            desc << " (Severity: " << juce::String (severity * 100, 0) << "%)";
            return desc;
        }

        juce::String getRecommendation() const
        {
            juce::String rec;
            if (type == ConflictType::Masking)
            {
                rec = "Cut " + juce::String (centerFreq, 0) + " Hz on " + trackA + " by " +
                      juce::String (severity * 6 + 2, 1) + " dB (Q: " + juce::String (centerFreq / bandwidth, 1) + ")";
            }
            else if (type == ConflictType::SpectralOverlap)
            {
                rec = "EQ carve: Cut " + juce::String (centerFreq - bandwidth/2, 0) + "-" +
                      juce::String (centerFreq + bandwidth/2, 0) + " Hz on quieter track";
            }
            else if (type == ConflictType::TonalClash)
            {
                rec = "Pitch shift or EQ notch at " + juce::String (centerFreq, 0) + " Hz";
            }
            else
            {
                rec = "Try panning separation or sidechain compression";
            }
            return rec;
        }
    };

    using ConflictCallback = std::function<void(const FrequencyConflict&)>;

    //==============================================================================
    ConflictDetector (double sampleRate = 44100.0);
    ~ConflictDetector() = default;

    //==============================================================================
    void prepare (double sampleRate, int samplesPerBlock);
    void reset();
    void processBlock (const juce::AudioBuffer<float>& buffer);
    void analyzeFullBuffer (const juce::AudioBuffer<float>& buffer);
    void analyzeMultiTrack (const juce::Array<juce::AudioBuffer<float>*>& buffers,
                            const juce::Array<juce::String>& trackNames = {});

    //==============================================================================
    const juce::Array<FrequencyConflict>& getActiveConflicts() const noexcept { return activeConflicts; }
    juce::Array<FrequencyConflict> getConflictsForTrack (const juce::String& trackName) const;

    // Configuration
    void setThreshold (float t) noexcept { conflictThreshold = t; }
    void setSensitivity (float s) noexcept { sensitivity = s; }
    void setPsychoacousticModel (bool enable) noexcept { usePsychoacousticModel = enable; }

    // Callbacks
    ConflictCallback onConflictDetected;

    //==============================================================================
    // Frequency bands for analysis (Bark scale based)
    static constexpr int NUM_BANDS = 24;
    static float getBandCenterFreq (int bandIndex);
    static float getBandwidth (int bandIndex);

private:
    //==============================================================================
    double sampleRate = 44100.0;
    int fftSize = 2048;
    int hopSize = 512;

    // Per-track spectral data
    struct TrackSpectrum
    {
        juce::String name;
        juce::AudioBuffer<float> magnitudeSpectrum;  // [bands][time]
        juce::AudioBuffer<float> phaseSpectrum;
        juce::Array<float> bandEnergy;        // Current energy per band
        juce::Array<float> bandEnergyHistory; // History for averaging
        int historyIndex = 0;
        std::chrono::steady_clock::time_point lastUpdate;
    };

    juce::OwnedArray<TrackSpectrum> trackSpectra;
    juce::AudioBuffer<float> currentSpectrum;

    // Analysis parameters
    float conflictThreshold = 0.3f;
    float sensitivity = 1.0f;
    bool usePsychoacousticModel = true;

    // Active conflicts
    juce::Array<FrequencyConflict> activeConflicts;

    // Psychoacoustic model
    struct MaskingModel
    {
        // Simultaneous masking thresholds (per Bark band)
        juce::Array<float> maskingThreshold;  // dB SPL

        void calculate (const juce::Array<float>& bandEnergy);
        float getMaskingAtBand (int band, float signalLevel) const;
    } maskingModel;

    // FFT processor
    FFTProcessor fftProcessor;

    //==============================================================================
    void updateTrackSpectrum (TrackSpectrum& track, const juce::AudioBuffer<float>& buffer);
    void detectConflicts();
    void detectSpectralOverlap (TrackSpectrum& a, TrackSpectrum& b);
    void detectMasking (TrackSpectrum& a, TrackSpectrum& b);
    void detectTonalClashes (TrackSpectrum& a, TrackSpectrum& b);
    float calculateConflictSeverity (float energyA, float energyB, float maskingThresh) const;
    juce::String freqToNote (float freq) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConflictDetector)
};