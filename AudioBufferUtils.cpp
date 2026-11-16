#include "AudioBufferUtils.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace AudioBufferUtils
{
//==============================================================================
float getRMS (const juce::AudioBuffer<float>& buffer, int channel)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return 0.0f;

    const float* data = buffer.getReadPointer (channel);
    float sumSq = 0.0f;

    for (int i = 0; i < numSamples; ++i)
        sumSq += data[i] * data[i];

    return std::sqrt (sumSq / numSamples);
}

float getRMS (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples, int channel)
{
    const float* data = buffer.getReadPointer (channel);
    float sumSq = 0.0f;

    int end = juce::jmin (startSample + numSamples, buffer.getNumSamples());
    for (int i = startSample; i < end; ++i)
        sumSq += data[i] * data[i];

    int actualSamples = end - startSample;
    return actualSamples > 0 ? std::sqrt (sumSq / actualSamples) : 0.0f;
}

float getPeak (const juce::AudioBuffer<float>& buffer, int channel)
{
    const float* data = buffer.getReadPointer (channel);
    float peak = 0.0f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
        peak = std::max (peak, std::abs (data[i]));

    return peak;
}

float getTruePeak (const juce::AudioBuffer<float>& buffer, int channel)
{
    const float* data = buffer.getReadPointer (channel);
    float peak = 0.0f;
    int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        peak = std::max (peak, std::abs (data[i]));

        // Parabolic interpolation for inter-sample peaks
        if (i > 0 && i < numSamples - 1)
        {
            float y0 = std::abs (data[i-1]);
            float y1 = std::abs (data[i]);
            float y2 = std::abs (data[i+1]);

            float denom = y0 - 2*y1 + y2;
            if (std::abs (denom) > 1e-6f)
            {
                float offset = 0.5f * (y0 - y2) / denom;
                if (offset > -0.5f && offset < 0.5f)
                {
                    float interpPeak = y1 - 0.25f * (y0 - y2) * offset;
                    peak = std::max (peak, interpPeak);
                }
            }
        }
    }

    return peak;
}

float getCrestFactor (const juce::AudioBuffer<float>& buffer, int channel)
{
    float peak = getPeak (buffer, channel);
    float rms = getRMS (buffer, channel);

    if (rms <= 0.0f) return 1.0f;
    return peak / rms;
}

//==============================================================================
void copyMono (const juce::AudioBuffer<float>& source, juce::AudioBuffer<float>& dest,
               int sourceChannel, int destChannel)
{
    int numSamples = juce::jmin (source.getNumSamples(), dest.getNumSamples());
    dest.copyFrom (destChannel, 0, source, sourceChannel, 0, numSamples);
}

void sumToMono (const juce::AudioBuffer<float>& source, juce::AudioBuffer<float>& dest)
{
    int numSamples = juce::jmin (source.getNumSamples(), dest.getNumSamples());
    dest.clear();

    for (int ch = 0; ch < source.getNumChannels(); ++ch)
    {
        dest.addFrom (0, 0, source, ch, 0, numSamples);
    }

    // Average
    if (source.getNumChannels() > 0)
    {
        float* data = dest.getWritePointer (0);
        float scale = 1.0f / source.getNumChannels();
        for (int i = 0; i < numSamples; ++i)
            data[i] *= scale;
    }
}

void applyGain (juce::AudioBuffer<float>& buffer, float gainDb)
{
    float gain = dbToGain (gainDb);
    buffer.applyGain (gain);
}

void applyGainRamp (juce::AudioBuffer<float>& buffer, float startGainDb, float endGainDb)
{
    float startGain = dbToGain (startGainDb);
    float endGain = dbToGain (endGainDb);
    int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float t = numSamples > 1 ? static_cast<float> (i) / (numSamples - 1) : 0.0f;
            float gain = startGain + (endGain - startGain) * t;
            data[i] *= gain;
        }
    }
}

//==============================================================================
void swapChannels (juce::AudioBuffer<float>& buffer, int ch1, int ch2)
{
    if (ch1 >= buffer.getNumChannels() || ch2 >= buffer.getNumChannels())
        return;

    int numSamples = buffer.getNumSamples();
    float* data1 = buffer.getWritePointer (ch1);
    float* data2 = buffer.getWritePointer (ch2);

    for (int i = 0; i < numSamples; ++i)
        std::swap (data1[i], data2[i]);
}

void invertPhase (juce::AudioBuffer<float>& buffer, int channel)
{
    if (channel >= buffer.getNumChannels())
        return;

    float* data = buffer.getWritePointer (channel);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        data[i] = -data[i];
}

float getCorrelation (const juce::AudioBuffer<float>& buffer, int ch1, int ch2)
{
    if (ch1 >= buffer.getNumChannels() || ch2 >= buffer.getNumChannels())
        return 0.0f;

    const float* data1 = buffer.getReadPointer (ch1);
    const float* data2 = buffer.getReadPointer (ch2);
    int numSamples = buffer.getNumSamples();

    float sum1 = 0.0f, sum2 = 0.0f, sum12 = 0.0f, sum1Sq = 0.0f, sum2Sq = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float v1 = data1[i];
        float v2 = data2[i];
        sum1 += v1;
        sum2 += v2;
        sum12 += v1 * v2;
        sum1Sq += v1 * v1;
        sum2Sq += v2 * v2;
    }

    float n = static_cast<float> (numSamples);
    float cov = sum12 - sum1 * sum2 / n;
    float var1 = sum1Sq - sum1 * sum1 / n;
    float var2 = sum2Sq - sum2 * sum2 / n;

    if (var1 <= 0.0f || var2 <= 0.0f)
        return 0.0f;

    return cov / std::sqrt (var1 * var2);
}

//==============================================================================
float hzToBin (float hz, int fftSize, double sampleRate)
{
    return hz * fftSize / static_cast<float> (sampleRate);
}

float binToHz (int bin, int fftSize, double sampleRate)
{
    return bin * static_cast<float> (sampleRate) / fftSize;
}

float hzToMel (float hz)
{
    return 2595.0f * std::log10 (1.0f + hz / 700.0f);
}

float melToHz (float mel)
{
    return 700.0f * (std::pow (10.0f, mel / 2595.0f) - 1.0f);
}

float hzToBark (float hz)
{
    return 13.0f * std::atan (0.00076f * hz) + 3.5f * std::atan (std::pow (hz / 7500.0f, 2));
}

float barkToHz (float bark)
{
    // Approximate inverse
    return 1960.0f * (bark + 0.53f) / (26.28f - bark);
}

//==============================================================================
void applyWindow (juce::AudioBuffer<float>& buffer, WindowType type, float beta)
{
    int numSamples = buffer.getNumSamples();
    auto window = generateWindow (numSamples, type, beta);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] *= window[i];
    }
}

juce::Array<float> generateWindow (int size, WindowType type, float beta)
{
    juce::Array<float> window (size);

    switch (type)
    {
        case WindowType::Rectangular:
            for (int i = 0; i < size; ++i) window[i] = 1.0f;
            break;

        case WindowType::Hann:
            for (int i = 0; i < size; ++i)
                window[i] = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * i / (size - 1)));
            break;

        case WindowType::Hamming:
            for (int i = 0; i < size; ++i)
                window[i] = 0.54f - 0.46f * std::cos (2.0f * juce::MathConstants<float>::pi * i / (size - 1));
            break;

        case WindowType::Blackman:
            for (int i = 0; i < size; ++i)
            {
                float a = 2.0f * juce::MathConstants<float>::pi * i / (size - 1);
                window[i] = 0.42f - 0.5f * std::cos (a) + 0.08f * std::cos (2.0f * a);
            }
            break;

        case WindowType::BlackmanHarris:
            for (int i = 0; i < size; ++i)
            {
                float a = 2.0f * juce::MathConstants<float>::pi * i / (size - 1);
                window[i] = 0.35875f - 0.48829f * std::cos (a)
                          + 0.14128f * std::cos (2.0f * a) - 0.01168f * std::cos (3.0f * a);
            }
            break;

        case WindowType::Kaiser:
            // Simplified Kaiser window using modified Bessel function approximation
            {
                float alpha = beta;  // beta parameter
                float I0_alpha = 1.0f;  // Approximation for I0(alpha)

                for (int i = 0; i < size; ++i)
                {
                    float x = 2.0f * i / (size - 1) - 1.0f;
                    float val = 1.0f - x * x;
                    if (val > 0)
                        window[i] = std::sqrt (val);  // Simplified
                    else
                        window[i] = 0.0f;
                }
            }
            break;
    }

    return window;
}

//==============================================================================
float interpolateLinear (const float* data, float index, int size)
{
    int i0 = static_cast<int> (std::floor (index));
    int i1 = i0 + 1;

    if (i0 < 0) return data[0];
    if (i1 >= size) return data[size - 1];

    float frac = index - i0;
    return data[i0] + frac * (data[i1] - data[i0]);
}

float interpolateCubic (const float* data, float index, int size)
{
    int i = static_cast<int> (std::floor (index));
    float frac = index - i;

    if (i <= 0) return data[0];
    if (i >= size - 2) return data[size - 1];

    float y0 = data[i-1];
    float y1 = data[i];
    float y2 = data[i+1];
    float y3 = data[i+2];

    // Catmull-Rom spline
    float a0 = y1;
    float a1 = 0.5f * (y2 - y0);
    float a2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float a3 = 1.5f * (y1 - y2) + 0.5f * (y3 - y0);

    return ((a3 * frac + a2) * frac + a1) * frac + a0;
}

float parabolicPeakInterpolation (const float* data, int peakIndex, int size)
{
    if (peakIndex <= 0 || peakIndex >= size - 1)
        return peakIndex;

    float y0 = data[peakIndex - 1];
    float y1 = data[peakIndex];
    float y2 = data[peakIndex + 1];

    float denom = y0 - 2*y1 + y2;
    if (std::abs (denom) < 1e-6f)
        return peakIndex;

    float offset = 0.5f * (y0 - y2) / denom;
    return peakIndex + offset;
}

//==============================================================================
float mean (const float* data, int size)
{
    if (size <= 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < size; ++i) sum += data[i];
    return sum / size;
}

float variance (const float* data, int size, float meanVal)
{
    if (size <= 1) return 0.0f;
    float sumSq = 0.0f;
    for (int i = 0; i < size; ++i)
    {
        float diff = data[i] - meanVal;
        sumSq += diff * diff;
    }
    return sumSq / (size - 1);
}

float stdDev (const float* data, int size)
{
    float m = mean (data, size);
    return std::sqrt (variance (data, size, m));
}

float skewness (const float* data, int size)
{
    if (size <= 2) return 0.0f;
    float m = mean (data, size);
    float sd = stdDev (data, size);
    if (sd <= 0.0f) return 0.0f;

    float sum = 0.0f;
    for (int i = 0; i < size; ++i)
    {
        float z = (data[i] - m) / sd;
        sum += z * z * z;
    }
    return sum / size;
}

float kurtosis (const float* data, int size)
{
    if (size <= 3) return 0.0f;
    float m = mean (data, size);
    float sd = stdDev (data, size);
    if (sd <= 0.0f) return 0.0f;

    float sum = 0.0f;
    for (int i = 0; i < size; ++i)
    {
        float z = (data[i] - m) / sd;
        sum += z * z * z * z;
    }
    return sum / size - 3.0f;  // Excess kurtosis
}

void minMax (const float* data, int size, float& minOut, float& maxOut)
{
    if (size <= 0) { minOut = maxOut = 0.0f; return; }
    minOut = maxOut = data[0];
    for (int i = 1; i < size; ++i)
    {
        minOut = std::min (minOut, data[i]);
        maxOut = std::max (maxOut, data[i]);
    }
}

//==============================================================================
float percentile (juce::Array<float>& data, float percentile)
{
    if (data.size() == 0) return 0.0f;
    percentile = juce::jlimit (0.0f, 1.0f, percentile);

    data.sort();
    int index = static_cast<int> (percentile * (data.size() - 1));
    return data[index];
}
}