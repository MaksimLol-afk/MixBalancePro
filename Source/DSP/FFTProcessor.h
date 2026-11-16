#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    FFT Processor for real-time spectral analysis

    Features:
    - Configurable FFT size (power of 2)
    - Window functions (Hann, Blackman-Harris, etc.)
    - Overlap-add for smooth analysis
    - Magnitude/phase spectrum output
    - Real-time optimized
*/
class FFTProcessor
{
public:
    //==============================================================================
    FFTProcessor (int fftSize = 2048);
    ~FFTProcessor() = default;

    //==============================================================================
    void prepare (double sampleRate, int samplesPerBlock);
    void reset();

    //==============================================================================
    void setFFTSize (int size);
    void setWindowType (WindowType type);
    void setOverlapFactor (int factor);  // 2, 4, 8

    //==============================================================================
    // Process audio block and compute FFT
    void processBlock (const juce::AudioBuffer<float>& buffer);

    // Get magnitude spectrum (single channel, averaged if multi-channel)
    juce::AudioBuffer<float> getMagnitudeSpectrum (const juce::AudioBuffer<float>& buffer);

    // Get complex spectrum
    juce::AudioBuffer<std::complex<float>> getComplexSpectrum (const juce::AudioBuffer<float>& buffer);

    // Get phase spectrum
    juce::AudioBuffer<float> getPhaseSpectrum (const juce::AudioBuffer<float>& buffer);

    // Get power spectrum
    juce::AudioBuffer<float> getPowerSpectrum (const juce::AudioBuffer<float>& buffer);

    //==============================================================================
    int getFFTSize() const noexcept { return fftSize; }
    int getNumBins() const noexcept { return fftSize / 2 + 1; }
    float getBinFrequency (int bin) const noexcept;
    int getBinForFrequency (float freq) const noexcept;

    //==============================================================================
    enum class WindowType
    {
        Hann,
        Hamming,
        Blackman,
        BlackmanHarris,
        Rectangular
    };

private:
    //==============================================================================
    int fftSize = 2048;
    int hopSize = 512;
    int overlapFactor = 4;
    WindowType windowType = WindowType::Hann;

    double sampleRate = 44100.0;

    // JUCE FFT
    std::unique_ptr<juce::dsp::FFT> fft;
    juce::dsp::WindowingFunction<float> window;

    // Buffers
    juce::AudioBuffer<float> fftBuffer;      // Time domain buffer for FFT
    juce::AudioBuffer<float> windowBuffer;   // Windowed buffer
    juce::AudioBuffer<std::complex<float>> complexBuffer;  // Complex spectrum

    // Overlap buffer for continuous processing
    juce::AudioBuffer<float> overlapBuffer;
    int overlapWritePos = 0;

    //==============================================================================
    void applyWindow (juce::AudioBuffer<float>& buffer);
    void computeFFT (const juce::AudioBuffer<float>& input, juce::AudioBuffer<std::complex<float>>& output);
    void updateOverlapBuffer (const juce::AudioBuffer<float>& input);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FFTProcessor)
};