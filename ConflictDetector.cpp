#include "ConflictDetector.h"
#include <algorithm>
#include <cmath>

//==============================================================================
// Static frequency band helpers (Bark scale approximation)
float ConflictDetector::getBandCenterFreq (int bandIndex)
{
    // Bark scale center frequencies (approximate)
    static const float barkCenters[24] = {
        50, 150, 250, 350, 450, 570, 700, 840,
        1000, 1170, 1370, 1600, 1850, 2150, 2500, 2900,
        3400, 4000, 4800, 5800, 7000, 8500, 10500, 13500
    };
    return (bandIndex >= 0 && bandIndex < 24) ? barkCenters[bandIndex] : 0.0f;
}

float ConflictDetector::getBandwidth (int bandIndex)
{
    // Bark band widths (approximate)
    static const float barkWidths[24] = {
        100, 100, 100, 100, 110, 120, 140, 150,
        160, 190, 220, 250, 300, 350, 400, 500,
        600, 700, 800, 900, 1000, 1200, 1500, 2000
    };
    return (bandIndex >= 0 && bandIndex < 24) ? barkWidths[bandIndex] : 100.0f;
}

//==============================================================================
ConflictDetector::ConflictDetector (double sr)
    : sampleRate (sr)
{
    prepare (sr, 512);
}

void ConflictDetector::prepare (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    fftSize = 2048;
    hopSize = 512;

    fftProcessor.prepare (sr, samplesPerBlock);

    // Initialize masking model
    maskingModel.maskingThreshold.resize (NUM_BANDS);
    maskingModel.maskingThreshold.fill (-60.0f);

    // Clear track spectra
    trackSpectra.clear();
    activeConflicts.clear();
}

void ConflictDetector::reset()
{
    prepare (sampleRate, 512);
    for (auto* track : trackSpectra)
    {
        track->bandEnergy.fill (0.0f);
        track->bandEnergyHistory.fill (0.0f);
        track->historyIndex = 0;
    }
    activeConflicts.clear();
}

//==============================================================================
void ConflictDetector::processBlock (const juce::AudioBuffer<float>& buffer)
{
    // For single-track mode, we just update the current track spectrum
    // Multi-track analysis is done via analyzeMultiTrack
    if (trackSpectra.size() == 0)
    {
        auto* track = new TrackSpectrum();
        track->name = "Main";
        track->magnitudeSpectrum.setSize (NUM_BANDS, 1);
        track->phaseSpectrum.setSize (NUM_BANDS, 1);
        track->bandEnergy.resize (NUM_BANDS);
        track->bandEnergy.fill (0.0f);
        track->bandEnergyHistory.resize (NUM_BANDS * 100);
        track->bandEnergyHistory.fill (0.0f);
        trackSpectra.add (track);
    }

    updateTrackSpectrum (*trackSpectra[0], buffer);
    detectConflicts();
}

void ConflictDetector::analyzeMultiTrack (const juce::Array<juce::AudioBuffer<float>*>& buffers,
                                          const juce::Array<juce::String>& trackNames)
{
    // Ensure we have track spectra for all buffers
    while (trackSpectra.size() < buffers.size())
    {
        auto* track = new TrackSpectrum();
        track->name = "Track " + juce::String (trackSpectra.size() + 1);
        track->magnitudeSpectrum.setSize (NUM_BANDS, 1);
        track->phaseSpectrum.setSize (NUM_BANDS, 1);
        track->bandEnergy.resize (NUM_BANDS);
        track->bandEnergy.fill (0.0f);
        track->bandEnergyHistory.resize (NUM_BANDS * 100);
        track->bandEnergyHistory.fill (0.0f);
        trackSpectra.add (track);
    }

    // Update names if provided
    if (trackNames.size() == buffers.size())
    {
        for (int i = 0; i < trackNames.size(); ++i)
            trackSpectra[i]->name = trackNames[i];
    }

    // Update each track's spectrum
    for (int i = 0; i < buffers.size(); ++i)
    {
        if (buffers[i] != nullptr)
            updateTrackSpectrum (*trackSpectra[i], *buffers[i]);
    }

    // Detect conflicts between all pairs
    detectConflicts();
}

void ConflictDetector::updateTrackSpectrum (TrackSpectrum& track, const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0)
        return;

    // Compute FFT for each channel and average
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

    // Get spectrum from FFT processor
    auto spectrum = fftProcessor.getMagnitudeSpectrum (monoBuffer);

    // Map FFT bins to Bark bands
    track.bandEnergy.fill (0.0f);
    const int numBins = spectrum.getNumSamples();

    for (int band = 0; band < NUM_BANDS; ++band)
    {
        float centerFreq = getBandCenterFreq (band);
        float bandwidth = getBandwidth (band);
        float lowFreq = centerFreq - bandwidth * 0.5f;
        float highFreq = centerFreq + bandwidth * 0.5f;

        int lowBin = juce::jmax (0, static_cast<int> (lowFreq * fftSize / sampleRate));
        int highBin = juce::jmin (numBins - 1, static_cast<int> (highFreq * fftSize / sampleRate));

        float sum = 0.0f;
        int count = 0;
        for (int bin = lowBin; bin <= highBin; ++bin)
        {
            sum += spectrum.getSample (0, bin);
            count++;
        }

        if (count > 0)
            track.bandEnergy.getReference (band) = 20.0f * std::log10 (std::max (sum / count, 1e-10f));
        else
            track.bandEnergy.getReference (band) = -120.0f;
    }

    // Update history for averaging
    for (int band = 0; band < NUM_BANDS; ++band)
    {
        int histPos = track.historyIndex * NUM_BANDS + band;
        if (histPos < track.bandEnergyHistory.size())
            track.bandEnergyHistory.getReference (histPos) = track.bandEnergy[band];
    }
    track.historyIndex = (track.historyIndex + 1) % (track.bandEnergyHistory.size() / NUM_BANDS);

    track.lastUpdate = std::chrono::steady_clock::now();
}

//==============================================================================
void ConflictDetector::detectConflicts()
{
    activeConflicts.clear();

    if (trackSpectra.size() < 2)
        return;

    // Calculate masking thresholds from all tracks combined
    juce::Array<float> combinedEnergy (NUM_BANDS);
    combinedEnergy.fill (-120.0f);

    for (auto* track : trackSpectra)
    {
        for (int band = 0; band < NUM_BANDS; ++band)
        {
            combinedEnergy[band] = std::max (combinedEnergy[band], track->bandEnergy[band]);
        }
    }

    if (usePsychoacousticModel)
        maskingModel.calculate (combinedEnergy);

    // Compare all pairs of tracks
    for (int i = 0; i < trackSpectra.size(); ++i)
    {
        for (int j = i + 1; j < trackSpectra.size(); ++j)
        {
            auto* trackA = trackSpectra[i];
            auto* trackB = trackSpectra[j];

            detectSpectralOverlap (*trackA, *trackB);
            detectMasking (*trackA, *trackB);
            detectTonalClashes (*trackA, *trackB);
        }
    }

    // Sort by severity
    auto conflictComparator = [] (const FrequencyConflict& a, const FrequencyConflict& b)
    {
        return a.severity > b.severity;
    };
    activeConflicts.sort (conflictComparator);

    // Limit to top conflicts
    while (activeConflicts.size() > 20)
        activeConflicts.removeLast();
}

void ConflictDetector::detectSpectralOverlap (TrackSpectrum& a, TrackSpectrum& b)
{
    for (int band = 0; band < NUM_BANDS; ++band)
    {
        float energyA = a.bandEnergy[band];
        float energyB = b.bandEnergy[band];

        // Both tracks have significant energy in this band
        if (energyA > -40.0f && energyB > -40.0f)
        {
            float diff = std::abs (energyA - energyB);
            float avg = (energyA + energyB) * 0.5f;

            // Conflict if both are loud and close in level
            if (avg > -30.0f && diff < 10.0f)
            {
                float severity = (1.0f - diff / 10.0f) * (1.0f + (avg + 30.0f) / 30.0f) * 0.5f;
                severity = juce::jlimit (0.0f, 1.0f, severity * sensitivity);

                if (severity > conflictThreshold)
                {
                    FrequencyConflict conflict;
                    conflict.trackA = a.name;
                    conflict.trackB = b.name;
                    conflict.centerFreq = getBandCenterFreq (band);
                    conflict.bandwidth = getBandwidth (band);
                    conflict.severity = severity;
                    conflict.trackALevel = energyA;
                    conflict.trackBLevel = energyB;
                    conflict.type = FrequencyConflict::ConflictType::SpectralOverlap;
                    conflict.maskingThreshold = maskingModel.getMaskingAtBand (band, avg);

                    activeConflicts.add (conflict);

                    if (onConflictDetected)
                        onConflictDetected (conflict);
                }
            }
        }
    }
}

void ConflictDetector::detectMasking (TrackSpectrum& a, TrackSpectrum& b)
{
    // Track A masks Track B if A is significantly louder in a band
    // and B has content that falls below masking threshold

    for (int band = 0; band < NUM_BANDS; ++band)
    {
        float energyA = a.bandEnergy[band];
        float energyB = b.bandEnergy[band];

        if (energyA > energyB + 15.0f && energyB > -60.0f)  // A is 15dB+ louder
        {
            float maskingThresh = maskingModel.getMaskingAtBand (band, energyA);

            if (energyB < maskingThresh)
            {
                // Track B is masked by Track A
                float severity = (maskingThresh - energyB) / 30.0f;  // How deep in masking
                severity = juce::jlimit (0.0f, 1.0f, severity * sensitivity);

                if (severity > conflictThreshold)
                {
                    FrequencyConflict conflict;
                    conflict.trackA = a.name;
                    conflict.trackB = b.name;
                    conflict.centerFreq = getBandCenterFreq (band);
                    conflict.bandwidth = getBandwidth (band);
                    conflict.severity = severity;
                    conflict.trackALevel = energyA;
                    conflict.trackBLevel = energyB;
                    conflict.type = FrequencyConflict::ConflictType::Masking;
                    conflict.maskingThreshold = maskingThresh;

                    activeConflicts.add (conflict);

                    if (onConflictDetected)
                        onConflictDetected (conflict);
                }
            }
        }

        // Check reverse masking (B masks A)
        if (energyB > energyA + 15.0f && energyA > -60.0f)
        {
            float maskingThresh = maskingModel.getMaskingAtBand (band, energyB);

            if (energyA < maskingThresh)
            {
                float severity = (maskingThresh - energyA) / 30.0f;
                severity = juce::jlimit (0.0f, 1.0f, severity * sensitivity);

                if (severity > conflictThreshold)
                {
                    FrequencyConflict conflict;
                    conflict.trackA = b.name;
                    conflict.trackB = a.name;
                    conflict.centerFreq = getBandCenterFreq (band);
                    conflict.bandwidth = getBandwidth (band);
                    conflict.severity = severity;
                    conflict.trackALevel = energyB;
                    conflict.trackBLevel = energyA;
                    conflict.type = FrequencyConflict::ConflictType::Masking;
                    conflict.maskingThreshold = maskingThresh;

                    activeConflicts.add (conflict);

                    if (onConflictDetected)
                        onConflictDetected (conflict);
                }
            }
        }
    }
}

void ConflictDetector::detectTonalClashes (TrackSpectrum& a, TrackSpectrum& b)
{
    // Find prominent tonal components (peaks in spectrum)
    // and check for fundamental/harmonic clashes

    auto findPeaks = [this] (const TrackSpectrum& track) -> juce::Array<std::pair<float, float>>
    {
        juce::Array<std::pair<float, float>> peaks;  // <freq, level>

        for (int band = 1; band < NUM_BANDS - 1; ++band)
        {
            float level = track.bandEnergy[band];
            float prevLevel = track.bandEnergy[band - 1];
            float nextLevel = track.bandEnergy[band + 1];

            // Local maximum
            if (level > prevLevel + 3.0f && level > nextLevel + 3.0f && level > -30.0f)
            {
                peaks.add ({ getBandCenterFreq (band), level });
            }
        }
        return peaks;
    };

    auto peaksA = findPeaks (a);
    auto peaksB = findPeaks (b);

    // Check for clashes
    for (auto& peakA : peaksA)
    {
        for (auto& peakB : peaksB)
        {
            float freqDiff = std::abs (peakA.first - peakB.first);
            float freqRatio = peakA.first / peakB.first;

            // Direct frequency clash (within 5%)
            if (freqDiff / peakA.first < 0.05f)
            {
                float severity = 0.8f * sensitivity;
                if (severity > conflictThreshold)
                {
                    FrequencyConflict conflict;
                    conflict.trackA = a.name;
                    conflict.trackB = b.name;
                    conflict.centerFreq = (peakA.first + peakB.first) * 0.5f;
                    conflict.bandwidth = freqDiff * 2.0f;
                    conflict.severity = severity;
                    conflict.trackALevel = peakA.second;
                    conflict.trackBLevel = peakB.second;
                    conflict.type = FrequencyConflict::ConflictType::TonalClash;
                    conflict.maskingThreshold = maskingModel.getMaskingAtBand (
                        static_cast<int> (conflict.centerFreq * 24 / 24000), peakA.second);

                    activeConflicts.add (conflict);

                    if (onConflictDetected)
                        onConflictDetected (conflict);
                }
            }
            // Harmonic clash (octave, fifth, third)
            else if (std::abs (freqRatio - 2.0f) < 0.02f ||   // Octave
                     std::abs (freqRatio - 1.5f) < 0.02f ||   // Fifth
                     std::abs (freqRatio - 1.25f) < 0.02f)    // Major third
            {
                float severity = 0.5f * sensitivity;
                if (severity > conflictThreshold)
                {
                    FrequencyConflict conflict;
                    conflict.trackA = a.name;
                    conflict.trackB = b.name;
                    conflict.centerFreq = std::min (peakA.first, peakB.first);
                    conflict.bandwidth = std::abs (peakA.first - peakB.first);
                    conflict.severity = severity;
                    conflict.trackALevel = peakA.second;
                    conflict.trackBLevel = peakB.second;
                    conflict.type = FrequencyConflict::ConflictType::HarmonicConflict;
                    conflict.maskingThreshold = maskingModel.getMaskingAtBand (
                        static_cast<int> (conflict.centerFreq * 24 / 24000), peakA.second);

                    activeConflicts.add (conflict);

                    if (onConflictDetected)
                        onConflictDetected (conflict);
                }
            }
        }
    }
}

juce::Array<ConflictDetector::FrequencyConflict> ConflictDetector::getConflictsForTrack (const juce::String& trackName) const
{
    juce::Array<FrequencyConflict> result;
    for (const auto& conflict : activeConflicts)
    {
        if (conflict.trackA == trackName || conflict.trackB == trackName)
            result.add (conflict);
    }
    return result;
}

//==============================================================================
// Psychoacoustic Masking Model Implementation
void ConflictDetector::MaskingModel::calculate (const juce::Array<float>& bandEnergy)
{
    // Simplified simultaneous masking model based on MPEG psychoacoustics
    // For each band, calculate masking threshold from neighbors

    for (int band = 0; band < NUM_BANDS; ++band)
    {
        float signalLevel = bandEnergy[band];
        if (signalLevel < -60.0f)
        {
            maskingThreshold.getReference (band) = -60.0f;
            continue;
        }

        // Spreading function: masking spreads to adjacent bands
        float maxMasking = -120.0f;

        for (int srcBand = 0; srcBand < NUM_BANDS; ++srcBand)
        {
            float srcLevel = bandEnergy[srcBand];
            if (srcLevel < -60.0f)
                continue;

            // Distance in Bark
            float dist = std::abs (band - srcBand);

            // Spreading function (simplified)
            float spread;
            if (dist == 0)
                spread = 0.0f;
            else if (dist == 1)
                spread = -17.0f;  // -17 dB/Bark for first band
            else if (dist <= 3)
                spread = -17.0f - 6.0f * (dist - 1);  // -6 dB/Bark
            else
                spread = -17.0f - 12.0f - 12.0f * (dist - 3);  // -12 dB/Bark beyond

            float masking = srcLevel + spread;
            maxMasking = std::max (maxMasking, masking);
        }

        // Absolute threshold of hearing (simplified)
        float freq = getBandCenterFreq (band);
        float absThresh = 3.64f * std::pow (freq / 1000.0f, -0.8f)
                        - 6.5f * std::exp (-0.6f * std::pow (freq / 1000.0f - 3.3f, 2))
                        + 0.001f * std::pow (freq / 1000.0f, 4);

        maskingThreshold.getReference (band) = std::max (maxMasking, absThresh);
    }
}

float ConflictDetector::MaskingModel::getMaskingAtBand (int band, float signalLevel) const
{
    if (band >= 0 && band < NUM_BANDS)
        return maskingThreshold[band];
    return -60.0f;
}

//==============================================================================
void ConflictDetector::analyzeFullBuffer (const juce::AudioBuffer<float>& buffer)
{
    // Process entire buffer in blocks
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

juce::String ConflictDetector::freqToNote (float freq) const
{
    if (freq <= 0) return "—";
    float noteNum = 12.0f * std::log2 (freq / 440.0f) + 69.0f;
    int note = static_cast<int> (std::round (noteNum));
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int octave = note / 12 - 1;
    return juce::String (names[note % 12]) + juce::String (octave);
}