#include "FFTProcessor.h"
#include <algorithm>

//==============================================================================
FFTProcessor::FFTProcessor (int size)
    : fftSize (size)
{
    // Ensure power of 2
    fftSize = 1 << static_cast<int> (std::ceil (std::log2 (fftSize)));
    hopSize = fftSize / overlapFactor;
}

void FFTProcessor::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;

    // Create FFT object
    int fftOrder = static_cast<int> (std::log2 (fftSize));
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    // Create window
    switch (windowType)
    {
        case WindowType::Hann:
            window = juce::dsp::WindowingFunction<float> (fftSize, juce::dsp::WindowingFunction<float>::hann);
            break;
        case WindowType::Hamming:
            window = juce::dsp::WindowingFunction<float> (fftSize, juce::dsp::WindowingFunction<float>::hamming);
            break;
        case WindowType::Blackman:
            window = juce::dsp::WindowingFunction<float> (fftSize, juce::dsp::WindowingFunction<float>::blackman);
            break;
        case WindowType::BlackmanHarris:
            window = juce::dsp::WindowingFunction<float> (fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris);
            break;
        case WindowType::Rectangular:
            window = juce::dsp::WindowingFunction<float> (fftSize, juce::dsp::WindowingFunction<float>::rectangular);
            break;
    }

    // Allocate buffers
    fftBuffer.setSize (1, fftSize);
    fftBuffer.clear();
    windowBuffer.setSize (1, fftSize);
    complexBuffer.setSize (1, fftSize);
    overlapBuffer.setSize (1, fftSize);
    overlapBuffer.clear();
    overlapWritePos = 0;
}

void FFTProcessor::reset()
{
    fftBuffer.clear();
    windowBuffer.clear();
    complexBuffer.clear();
    overlapBuffer.clear();
    overlapWritePos = 0;
}

//==============================================================================
void FFTProcessor::setFFTSize (int size)
{
    size = 1 << static_cast<int> (std::ceil (std::log2 (size)));
    if (size != fftSize)
    {
        fftSize = size;
        hopSize = fftSize / overlapFactor;
        prepare (sampleRate, hopSize);
    }
}

void FFTProcessor::setWindowType (WindowType type)
{
    if (type != windowType)
    {
        windowType = type;
        prepare (sampleRate, hopSize);
    }
}

void FFTProcessor::setOverlapFactor (int factor)
{
    factor = juce::jlimit (2, 16, factor);
    if (factor != overlapFactor)
    {
        overlapFactor = factor;
        hopSize = fftSize / overlapFactor;
    }
}

//==============================================================================
void FFTProcessor::processBlock (const juce::AudioBuffer<float>& buffer)
{
    // Convert to mono if needed
    juce::AudioBuffer<float> monoBuffer (1, buffer.getNumSamples());
    if (buffer.getNumChannels() > 1)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                sum += buffer.getSample (ch, i);
            monoBuffer.setSample (0, i, sum / buffer.getNumChannels());
        }
    }
    else
    {
        monoBuffer.copyFrom (0, 0, buffer, 0, 0, buffer.getNumSamples());
    }

    updateOverlapBuffer (monoBuffer);
}

juce::AudioBuffer<float> FFTProcessor::getMagnitudeSpectrum (const juce::AudioBuffer<float>& buffer)
{
    auto complexSpec = getComplexSpectrum (buffer);
    int numBins = getNumBins();

    juce::AudioBuffer<float> mag (1, numBins);
    float* magPtr = mag.getWritePointer (0);
    const std::complex<float>* complexPtr = complexSpec.getReadPointer (0);

    for (int bin = 0; bin < numBins; ++bin)
    {
        magPtr[bin] = std::abs (complexPtr[bin]);
    }

    return mag;
}

juce::AudioBuffer<std::complex<float>> FFTProcessor::getComplexSpectrum (const juce::AudioBuffer<float>& buffer)
{
    // Convert to mono
    juce::AudioBuffer<float> monoBuffer (1, buffer.getNumSamples());
    if (buffer.getNumChannels() > 1)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                sum += buffer.getSample (ch, i);
            monoBuffer.setSample (0, i, sum / buffer.getNumChannels());
        }
    }
    else
    {
        monoBuffer.copyFrom (0, 0, buffer, 0, 0, buffer.getNumSamples());
    }

    // Use overlap buffer for continuous processing
    updateOverlapBuffer (monoBuffer);

    // Apply window and compute FFT
    computeFFT (overlapBuffer, complexBuffer);

    // Return only positive frequencies (0 to Nyquist)
    juce::AudioBuffer<std::complex<float>> result (1, getNumBins());
    const std::complex<float>* src = complexBuffer.getReadPointer (0);
    std::complex<float>* dst = result.getWritePointer (0);

    for (int i = 0; i < getNumBins(); ++i)
        dst[i] = src[i];

    return result;
}

juce::AudioBuffer<float> FFTProcessor::getPhaseSpectrum (const juce::AudioBuffer<float>& buffer)
{
    auto complexSpec = getComplexSpectrum (buffer);
    int numBins = getNumBins();

    juce::AudioBuffer<float> phase (1, numBins);
    float* phasePtr = phase.getWritePointer (0);
    const std::complex<float>* complexPtr = complexSpec.getReadPointer (0);

    for (int bin = 0; bin < numBins; ++bin)
    {
        phasePtr[bin] = std::arg (complexPtr[bin]);
    }

    return phase;
}

juce::AudioBuffer<float> FFTProcessor::getPowerSpectrum (const juce::AudioBuffer<float>& buffer)
{
    auto mag = getMagnitudeSpectrum (buffer);
    float* magPtr = mag.getWritePointer (0);
    int numBins = getNumBins();

    for (int bin = 0; bin < numBins; ++bin)
        magPtr[bin] = magPtr[bin] * magPtr[bin];

    return mag;
}

//==============================================================================
float FFTProcessor::getBinFrequency (int bin) const noexcept
{
    return bin * static_cast<float> (sampleRate) / fftSize;
}

int FFTProcessor::getBinForFrequency (float freq) const noexcept
{
    return static_cast<int> (std::round (freq * fftSize / sampleRate));
}

//==============================================================================
void FFTProcessor::applyWindow (juce::AudioBuffer<float>& buffer)
{
    float* data = buffer.getWritePointer (0);
    window.multiplyWithWindowingTable (data, fftSize);
}

void FFTProcessor::computeFFT (const juce::AudioBuffer<float>& input, juce::AudioBuffer<std::complex<float>>& output)
{
    // Copy input to FFT buffer (real part)
    float* fftData = fftBuffer.getWritePointer (0);
    const float* inData = input.getReadPointer (0);

    // Copy and apply window
    for (int i = 0; i < fftSize; ++i)
        fftData[i] = inData[i];

    applyWindow (fftBuffer);

    // Perform FFT
    // JUCE FFT works in-place on complex array
    std::complex<float>* complexData = complexBuffer.getWritePointer (0);

    // Copy real data to complex buffer
    for (int i = 0; i < fftSize; ++i)
        complexData[i] = std::complex<float> (fftData[i], 0.0f);

    // Perform forward FFT
    fft->perform (reinterpret_cast<float*> (complexData), false);
}

void FFTProcessor::updateOverlapBuffer (const juce::AudioBuffer<float>& input)
{
    const int numSamples = input.getNumSamples();
    const float* inData = input.getReadPointer (0);
    float* overlapData = overlapBuffer.getWritePointer (0);

    // Shift existing data and add new samples
    // This implements a circular buffer for overlap-add

    for (int i = 0; i < numSamples; ++i)
    {
        overlapData[overlapWritePos] = inData[i];
        overlapWritePos = (overlapWritePos + 1) % fftSize;
    }

    // If we have enough new data for a hop, we can compute FFT
    // The actual FFT computation happens in getComplexSpectrum
}