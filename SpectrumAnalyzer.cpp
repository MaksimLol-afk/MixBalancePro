#include "SpectrumAnalyzer.h"
#include <algorithm>
#include <cmath>

//==============================================================================
SpectrumAnalyzer::SpectrumAnalyzer (double sr)
    : sampleRate (sr)
{
    prepare (sr, 512);
}

void SpectrumAnalyzer::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    blockSize = samplesPerBlock;

    fftProcessor.prepare (sr, samplesPerBlock);

    // Initialize spectrum buffers
    int numBins = fftSize / 2 + 1;
    currentMeasurement.magnitudeSpectrum.setSize (1, numBins);
    currentMeasurement.magnitudeDB.setSize (1, numBins);
    currentMeasurement.phaseSpectrum.setSize (1, numBins);
    currentMeasurement.averageSpectrum.setSize (1, numBins);
    currentMeasurement.peakHoldSpectrum.setSize (1, numBins);

    currentMeasurement.magnitudeSpectrum.clear();
    currentMeasurement.magnitudeDB.clear();
    currentMeasurement.phaseSpectrum.clear();
    currentMeasurement.averageSpectrum.clear();
    currentMeasurement.peakHoldSpectrum.fill (-120.0f);

    prevMagnitudeSpectrum.setSize (1, numBins);
    prevMagnitudeSpectrum.clear();

    // Initialize band arrays
    currentMeasurement.thirdOctaveBands.resize (31);
    currentMeasurement.thirdOctaveBandsDB.resize (31);
    currentMeasurement.barkBands.resize (24);
    currentMeasurement.barkBandsDB.resize (24);

    // Calculate smoothing coefficients
    avgAlpha = std::exp (-1.0f / (averagingTime * sampleRate / blockSize));
    peakAlpha = std::exp (-1.0f / (peakHoldTime * sampleRate / blockSize));
}

void SpectrumAnalyzer::reset()
{
    prepare (sampleRate, blockSize);
    currentMeasurement = {};
    currentMeasurement.thirdOctaveBands.resize (31);
    currentMeasurement.thirdOctaveBandsDB.resize (31);
    currentMeasurement.barkBands.resize (24);
    currentMeasurement.barkBandsDB.resize (24);
}

//==============================================================================
void SpectrumAnalyzer::processBlock (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    // Convert to mono for analysis
    juce::AudioBuffer<float> monoBuffer (1, numSamples);
    if (numChannels > 1)
    {
        for (int i = 0; i < numSamples; ++i)
            monoBuffer.setSample (0, i, (buffer.getSample(0,i) + buffer.getSample(1,i)) * 0.5f);
    }
    else
    {
        monoBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
    }

    // Get FFT spectrum
    auto spectrum = fftProcessor.getComplexSpectrum (monoBuffer);

    int numBins = spectrum.getNumSamples();
    float* magPtr = currentMeasurement.magnitudeSpectrum.getWritePointer (0);
    float* dbPtr = currentMeasurement.magnitudeDB.getWritePointer (0);
    float* phasePtr = currentMeasurement.phaseSpectrum.getWritePointer (0);
    float* avgPtr = currentMeasurement.averageSpectrum.getWritePointer (0);
    float* peakPtr = currentMeasurement.peakHoldSpectrum.getWritePointer (0);
    const float* prevPtr = prevMagnitudeSpectrum.getReadPointer (0);

    // Process spectrum
    float spectralFlux = 0.0f;

    for (int bin = 0; bin < numBins; ++bin)
    {
        std::complex<float> c = spectrum.getSample (0, bin);
        float mag = std::abs (c);
        float phase = std::arg (c);

        magPtr[bin] = mag;
        dbPtr[bin] = 20.0f * std::log10 (std::max (mag, 1e-10f));
        phasePtr[bin] = phase;

        // Exponential averaging
        avgPtr[bin] = avgAlpha * avgPtr[bin] + (1.0f - avgAlpha) * dbPtr[bin];

        // Peak hold with slow decay
        if (dbPtr[bin] > peakPtr[bin])
            peakPtr[bin] = dbPtr[bin];
        else
            peakPtr[bin] = peakAlpha * peakPtr[bin] + (1.0f - peakAlpha) * dbPtr[bin];

        // Spectral flux (magnitude difference)
        float diff = mag - prevPtr[bin];
        spectralFlux += diff * diff;
    }

    currentMeasurement.spectralFlux = std::sqrt (spectralFlux / numBins);

    // Copy current to previous for next frame
    prevMagnitudeSpectrum.copyFrom (0, 0, currentMeasurement.magnitudeSpectrum, 0, 0, numBins);

    // Compute derived features
    computeSpectralFeatures();
    updateThirdOctaveBands();
    updateBarkBands();
    detectResonances();
    assessFrequencyBalance();

    if (onMeasurementUpdate)
        onMeasurementUpdate (currentMeasurement);
}

void SpectrumAnalyzer::computeSpectralFeatures()
{
    const float* magPtr = currentMeasurement.magnitudeSpectrum.getReadPointer (0);
    const float* dbPtr = currentMeasurement.magnitudeDB.getReadPointer (0);
    int numBins = currentMeasurement.magnitudeSpectrum.getNumSamples();

    // Spectral Centroid (brightness)
    float weightedSum = 0.0f;
    float magnitudeSum = 0.0f;

    for (int bin = 1; bin < numBins; ++bin)  // Skip DC
    {
        float freq = binToHz (bin);
        float mag = magPtr[bin];
        weightedSum += freq * mag;
        magnitudeSum += mag;
    }

    if (magnitudeSum > 0.0f)
        currentMeasurement.spectralCentroid = weightedSum / magnitudeSum;

    // Spectral Spread (bandwidth)
    float spreadSum = 0.0f;
    for (int bin = 1; bin < numBins; ++bin)
    {
        float freq = binToHz (bin);
        float mag = magPtr[bin];
        float diff = freq - currentMeasurement.spectralCentroid;
        spreadSum += diff * diff * mag;
    }

    if (magnitudeSum > 0.0f)
        currentMeasurement.spectralSpread = std::sqrt (spreadSum / magnitudeSum);

    // Spectral Rolloff (85% energy)
    float totalEnergy = 0.0f;
    for (int bin = 1; bin < numBins; ++bin)
        totalEnergy += magPtr[bin] * magPtr[bin];

    float cumulativeEnergy = 0.0f;
    float targetEnergy = 0.85f * totalEnergy;

    for (int bin = 1; bin < numBins; ++bin)
    {
        cumulativeEnergy += magPtr[bin] * magPtr[bin];
        if (cumulativeEnergy >= targetEnergy)
        {
            currentMeasurement.spectralRolloff = binToHz (bin);
            break;
        }
    }

    // Spectral Flatness (geometric mean / arithmetic mean)
    float geoMean = 0.0f;
    float arithMean = 0.0f;
    int validBins = 0;

    for (int bin = 1; bin < numBins; ++bin)
    {
        float mag = magPtr[bin];
        if (mag > 1e-10f)
        {
            geoMean += std::log (mag);
            arithMean += mag;
            validBins++;
        }
    }

    if (validBins > 0)
    {
        geoMean = std::exp (geoMean / validBins);
        arithMean /= validBins;
        currentMeasurement.spectralFlatness = juce::jlimit (0.0f, 1.0f, geoMean / arithMean);
    }

    // Spectral Crest (peak / mean)
    float maxMag = 0.0f;
    for (int bin = 1; bin < numBins; ++bin)
        maxMag = std::max (maxMag, magPtr[bin]);

    if (arithMean > 0.0f)
        currentMeasurement.spectralCrest = maxMag / arithMean;
}

void SpectrumAnalyzer::updateThirdOctaveBands()
{
    const float* dbPtr = currentMeasurement.magnitudeDB.getReadPointer (0);
    int numBins = currentMeasurement.magnitudeDB.getNumSamples();

    for (int band = 0; band < 31; ++band)
    {
        float centerFreq = thirdOctaveCenters[band];
        // 1/3 octave bandwidth = 2^(1/6) ≈ 1.122
        float lowFreq = centerFreq / 1.122462f;
        float highFreq = centerFreq * 1.122462f;

        int lowBin = hzToBin (lowFreq);
        int highBin = hzToBin (highFreq);
        lowBin = juce::jmax (1, lowBin);
        highBin = juce::jmin (numBins - 1, highBin);

        float sum = 0.0f;
        int count = 0;
        for (int bin = lowBin; bin <= highBin; ++bin)
        {
            sum += dbPtr[bin];
            count++;
        }

        if (count > 0)
        {
            currentMeasurement.thirdOctaveBandsDB[band] = sum / count;
            currentMeasurement.thirdOctaveBands[band] = std::pow (10.0f, currentMeasurement.thirdOctaveBandsDB[band] / 20.0f);
        }
        else
        {
            currentMeasurement.thirdOctaveBandsDB[band] = -120.0f;
            currentMeasurement.thirdOctaveBands[band] = 0.0f;
        }
    }
}

void SpectrumAnalyzer::updateBarkBands()
{
    const float* dbPtr = currentMeasurement.magnitudeDB.getReadPointer (0);
    int numBins = currentMeasurement.magnitudeDB.getNumSamples();

    // Bark scale band edges (approximate)
    static const float barkEdges[25] = {
        0, 100, 200, 300, 400, 510, 630, 770,
        920, 1080, 1270, 1480, 1720, 2000, 2320, 2700,
        3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500
    };

    for (int band = 0; band < 24; ++band)
    {
        float lowFreq = barkEdges[band];
        float highFreq = barkEdges[band + 1];

        int lowBin = hzToBin (lowFreq);
        int highBin = hzToBin (highFreq);
        lowBin = juce::jmax (1, lowBin);
        highBin = juce::jmin (numBins - 1, highBin);

        float sum = 0.0f;
        int count = 0;
        for (int bin = lowBin; bin <= highBin; ++bin)
        {
            sum += dbPtr[bin];
            count++;
        }

        if (count > 0)
        {
            currentMeasurement.barkBandsDB[band] = sum / count;
            currentMeasurement.barkBands[band] = std::pow (10.0f, currentMeasurement.barkBandsDB[band] / 20.0f);
        }
        else
        {
            currentMeasurement.barkBandsDB[band] = -120.0f;
            currentMeasurement.barkBands[band] = 0.0f;
        }
    }
}

void SpectrumAnalyzer::detectResonances()
{
    currentMeasurement.detectedResonances.clear();

    const float* dbPtr = currentMeasurement.magnitudeDB.getReadPointer (0);
    const float* avgPtr = currentMeasurement.averageSpectrum.getReadPointer (0);
    int numBins = currentMeasurement.magnitudeDB.getNumSamples();

    // Find peaks that are significantly above the average
    for (int bin = 2; bin < numBins - 2; ++bin)
    {
        float level = dbPtr[bin];
        float avgLevel = avgPtr[bin];

        // Check if it's a local maximum
        if (level > dbPtr[bin-1] && level > dbPtr[bin+1] &&
            level > dbPtr[bin-2] && level > dbPtr[bin+2])
        {
            // Check prominence above average
            float prominence = level - avgLevel;

            if (prominence > 12.0f && level > -40.0f)  // At least 12dB above average
            {
                // Estimate Q factor from bandwidth at -3dB
                float peakLevel = level;
                float targetLevel = peakLevel - 3.0f;

                int leftBin = bin;
                while (leftBin > 0 && dbPtr[leftBin] > targetLevel)
                    leftBin--;

                int rightBin = bin;
                while (rightBin < numBins - 1 && dbPtr[rightBin] > targetLevel)
                    rightBin++;

                float bw = binToHz (rightBin) - binToHz (leftBin);
                float centerFreq = binToHz (bin);
                float q = (bw > 0) ? centerFreq / bw : 0.0f;

                SpectrumMeasurement::Resonance res;
                res.frequency = centerFreq;
                res.magnitude = level;
                res.q = q;
                res.prominence = prominence;

                currentMeasurement.detectedResonances.add (res);
            }
        }
    }

    // Sort by prominence
    auto resonanceComparator = [] (const SpectrumMeasurement::Resonance& a,
                                                     const SpectrumMeasurement::Resonance& b)
    {
        return a.prominence > b.prominence;
    };
    currentMeasurement.detectedResonances.sort (resonanceComparator);

    // Limit to top 10
    while (currentMeasurement.detectedResonances.size() > 10)
        currentMeasurement.detectedResonances.removeLast();
}

void SpectrumAnalyzer::assessFrequencyBalance()
{
    // Calculate levels in key frequency ranges
    auto getBandLevel = [this] (float lowFreq, float highFreq) -> float
    {
        const float* dbPtr = currentMeasurement.magnitudeDB.getReadPointer (0);
        int numBins = currentMeasurement.magnitudeDB.getNumSamples();

        int lowBin = hzToBin (lowFreq);
        int highBin = hzToBin (highFreq);
        lowBin = juce::jmax (1, lowBin);
        highBin = juce::jmin (numBins - 1, highBin);

        float sum = 0.0f;
        int count = 0;
        for (int bin = lowBin; bin <= highBin; ++bin)
        {
            sum += dbPtr[bin];
            count++;
        }
        return count > 0 ? sum / count : -120.0f;
    };

    currentMeasurement.lowEndLevel = getBandLevel (20, 250);
    currentMeasurement.lowMidLevel = getBandLevel (250, 500);
    currentMeasurement.midLevel = getBandLevel (500, 2000);
    currentMeasurement.highMidLevel = getBandLevel (2000, 6000);
    currentMeasurement.highLevel = getBandLevel (6000, 20000);

    // Generate assessment
    juce::String assessment;

    float targetMid = currentMeasurement.midLevel;  // Reference

    // Check each band relative to mid
    auto checkBand = [&] (const char* name, float level, float target, float tolerance)
    {
        float diff = level - target;
        if (diff > tolerance)
            assessment << name << " boosted +" << juce::String (diff, 1) << "dB; ";
        else if (diff < -tolerance)
            assessment << name << " cut " << juce::String (diff, 1) << "dB; ";
    };

    checkBand ("Low", currentMeasurement.lowEndLevel, targetMid, 3.0f);
    checkBand ("LowMid", currentMeasurement.lowMidLevel, targetMid, 2.0f);
    checkBand ("HighMid", currentMeasurement.highMidLevel, targetMid, 2.0f);
    checkBand ("High", currentMeasurement.highLevel, targetMid, 3.0f);

    // Check for common issues
    if (currentMeasurement.lowMidLevel > currentMeasurement.midLevel + 4.0f)
        assessment << "Muddy (excess 250-500Hz); ";

    if (currentMeasurement.highMidLevel < currentMeasurement.midLevel - 6.0f)
        assessment << "Dull (lacking 2-6kHz); ";

    if (currentMeasurement.highLevel > currentMeasurement.highMidLevel + 3.0f)
        assessment << "Harsh (excess 6kHz+); ";

    if (currentMeasurement.lowEndLevel < currentMeasurement.midLevel - 10.0f)
        assessment << "Thin (weak sub-bass); ";

    // Resonance warnings
    for (auto& res : currentMeasurement.detectedResonances)
    {
        if (res.prominence > 18.0f)
        {
            assessment << "Resonance at " << juce::String (res.frequency, 0) << "Hz (Q:" << juce::String (res.q, 1) << "); ";
        }
    }

    if (assessment.isEmpty())
        assessment = "Balanced frequency response";

    currentMeasurement.balanceAssessment = assessment;
}

float SpectrumAnalyzer::hzToBin (float hz) const noexcept
{
    return hz * fftSize / static_cast<float> (sampleRate);
}

float SpectrumAnalyzer::binToHz (int bin) const noexcept
{
    return bin * static_cast<float> (sampleRate) / fftSize;
}

//==============================================================================
void SpectrumAnalyzer::analyzeFullBuffer (const juce::AudioBuffer<float>& buffer)
{
    const int blockSize = 4096;
    const int numSamples = buffer.getNumSamples();

    for (int offset = 0; offset < numSamples; offset += blockSize)
    {
        int currentBlockSize = juce::jmin (blockSize, numSamples - offset);
        juce::AudioBuffer<float> block (buffer.getNumChannels(), currentBlockSize);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            block.copyFrom (ch, 0, buffer, ch, offset, currentBlockSize);

        processBlock (block);
    }
}