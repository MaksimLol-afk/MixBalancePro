#include "RecommendationEngine.h"
#include "PluginProcessor.h"
#include <algorithm>
#include <cmath>

//==============================================================================
RecommendationEngine::RecommendationEngine (MixBalanceProAudioProcessor& p)
    : processor (p)
{
}

void RecommendationEngine::prepare (double sampleRate)
{
    // Initialize any sample-rate dependent calculations
}

void RecommendationEngine::reset()
{
    allRecommendations.clear();
    recommendationCounter = 0;
}

void RecommendationEngine::clearHistory()
{
    allRecommendations.clear();
}

//==============================================================================
juce::Array<RecommendationEngine::Recommendation> RecommendationEngine::generateFullReport (
    const LoudnessAnalyzer::LoudnessMeasurement& loudness,
    const juce::Array<ConflictDetector::FrequencyConflict>& conflicts,
    const DynamicRangeAnalyzer::DynamicRangeMeasurement& dynamics,
    const SpectrumAnalyzer::SpectrumMeasurement& spectrum
)
{
    allRecommendations.clear();
    recommendationCounter = 0;

    // Process each analysis domain
    processLoudnessTarget (loudness);
    processTruePeak (loudness);
    processLRA (loudness);

    for (const auto& conflict : conflicts)
    {
        processConflictForEQ (conflict);
        processConflictForPanning (conflict);
        processConflictForCompression (conflict);
        processConflictForSaturation (conflict);
    }

    processCrestFactor (dynamics);
    processDRValue (dynamics);
    processCompression (dynamics);
    processMicroDynamics (dynamics);

    processFrequencyBalance (spectrum);
    processResonances (spectrum);
    processSpectralFeatures (spectrum);

    // Sort by priority
    allRecommendations.sort ([] (const Recommendation& a, const Recommendation& b)
    {
        int pa = static_cast<int> (a.priority);
        int pb = static_cast<int> (b.priority);
        if (pa != pb) return pa < pb;
        return a.confidence > b.confidence;
    });

    return allRecommendations;
}

juce::Array<RecommendationEngine::Recommendation> RecommendationEngine::generateFromConflict (
    const ConflictDetector::FrequencyConflict& conflict)
{
    juce::Array<Recommendation> recs;
    processConflictForEQ (conflict);
    processConflictForPanning (conflict);
    processConflictForCompression (conflict);
    processConflictForSaturation (conflict);

    // Return only the ones generated from this conflict
    juce::Array<Recommendation> result;
    for (const auto& rec : allRecommendations)
    {
        if (rec.source == "Conflict" &&
            (rec.targetTrack == conflict.trackA || rec.targetTrack == conflict.trackB) &&
            std::abs (rec.frequency - conflict.centerFreq) < conflict.bandwidth)
        {
            result.add (rec);
        }
    }
    return result;
}

juce::Array<RecommendationEngine::Recommendation> RecommendationEngine::generateFromLoudness (
    const LoudnessAnalyzer::LoudnessMeasurement& loudness)
{
    juce::Array<Recommendation> recs;
    int beforeCount = allRecommendations.size();

    processLoudnessTarget (loudness);
    processTruePeak (loudness);
    processLRA (loudness);

    for (int i = beforeCount; i < allRecommendations.size(); ++i)
        recs.add (allRecommendations[i]);

    return recs;
}

juce::Array<RecommendationEngine::Recommendation> RecommendationEngine::generateFromDynamics (
    const DynamicRangeAnalyzer::DynamicRangeMeasurement& dynamics)
{
    juce::Array<Recommendation> recs;
    int beforeCount = allRecommendations.size();

    processCrestFactor (dynamics);
    processDRValue (dynamics);
    processCompression (dynamics);
    processMicroDynamics (dynamics);

    for (int i = beforeCount; i < allRecommendations.size(); ++i)
        recs.add (allRecommendations[i]);

    return recs;
}

juce::Array<RecommendationEngine::Recommendation> RecommendationEngine::generateFromSpectrum (
    const SpectrumAnalyzer::SpectrumMeasurement& spectrum)
{
    juce::Array<Recommendation> recs;
    int beforeCount = allRecommendations.size();

    processFrequencyBalance (spectrum);
    processResonances (spectrum);
    processSpectralFeatures (spectrum);

    for (int i = beforeCount; i < allRecommendations.size(); ++i)
        recs.add (allRecommendations[i]);

    return recs;
}

juce::Array<RecommendationEngine::Recommendation> RecommendationEngine::generateMultiTrackRecommendations (
    const juce::Array<juce::AudioBuffer<float>*>& buffers,
    const juce::Array<juce::String>& trackNames)
{
    juce::Array<Recommendation> recs;
    int beforeCount = allRecommendations.size();

    analyzeTrackRoles (buffers, trackNames);
    recommendTrackLevels (buffers, trackNames);
    recommendPanningSpread (buffers, trackNames);

    for (int i = beforeCount; i < allRecommendations.size(); ++i)
        recs.add (allRecommendations[i]);

    return recs;
}

//==============================================================================
juce::Array<RecommendationEngine::Recommendation> RecommendationEngine::getRecommendationsForTrack (const juce::String& trackName) const
{
    juce::Array<Recommendation> result;
    for (const auto& rec : allRecommendations)
    {
        if (rec.targetTrack == trackName || rec.targetTrack.isEmpty())
            result.add (rec);
    }
    return result;
}

juce::Array<RecommendationEngine::Recommendation> RecommendationEngine::getRecommendationsByType (Recommendation::Type type) const
{
    juce::Array<Recommendation> result;
    for (const auto& rec : allRecommendations)
    {
        if (rec.type == type)
            result.add (rec);
    }
    return result;
}

juce::Array<RecommendationEngine::Recommendation> RecommendationEngine::getRecommendationsByPriority (Recommendation::Priority priority) const
{
    juce::Array<Recommendation> result;
    for (const auto& rec : allRecommendations)
    {
        if (rec.priority == priority)
            result.add (rec);
    }
    return result;
}

//==============================================================================
void RecommendationEngine::addEQRecommendation (Recommendation::Type type, const juce::String& track, float freq, float gain, float q,
                                                Recommendation::Priority priority, const juce::String& desc, const juce::String& source)
{
    Recommendation rec;
    rec.type = type;
    rec.priority = priority;
    rec.targetTrack = track;
    rec.description = desc;
    rec.source = source;
    rec.frequency = freq;
    rec.gain = gain;
    rec.q = q;
    rec.filterType = (type == Recommendation::Type::EQ_Notch) ? 3 :
                     (type == Recommendation::Type::EQ_HighPass) ? 4 :
                     (type == Recommendation::Type::EQ_LowPass) ? 5 : 0;

    // Technical details
    juce::String tech;
    tech << freqToString (freq) << ", " << juce::String (gain, 1) << "dB, Q=" << juce::String (q, 2);
    rec.technicalDetails = tech;

    // Confidence based on priority
    static const float priorityConfidence[] = { 0.95f, 0.85f, 0.7f, 0.55f, 0.4f };
    rec.confidence = priorityConfidence[static_cast<int> (priority)];

    allRecommendations.add (rec);
    recommendationCounter++;
}

juce::String RecommendationEngine::freqToString (float freq) const
{
    if (freq >= 1000)
        return juce::String (freq / 1000.0f, 1) + "kHz";
    return juce::String (freq, 0) + "Hz";
}

juce::String RecommendationEngine::freqToNote (float freq) const
{
    if (freq <= 0) return "—";
    float noteNum = 12.0f * std::log2 (freq / 440.0f) + 69.0f;
    int note = static_cast<int> (std::round (noteNum));
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int octave = note / 12 - 1;
    return juce::String (names[note % 12]) + juce::String (octave);
}

//==============================================================================
// Conflict-based recommendations
void RecommendationEngine::processConflictForEQ (const ConflictDetector::FrequencyConflict& conflict)
{
    // Determine which track to cut based on levels
    juce::String targetTrack;
    float cutGain;

    if (conflict.trackALevel > conflict.trackBLevel + 3.0f)
    {
        // Track A is louder - cut track A
        targetTrack = conflict.trackA;
        cutGain = -calculateEQGainForConflict (conflict);
    }
    else if (conflict.trackBLevel > conflict.trackALevel + 3.0f)
    {
        // Track B is louder - cut track B
        targetTrack = conflict.trackB;
        cutGain = -calculateEQGainForConflict (conflict);
    }
    else
    {
        // Similar levels - suggest cutting both slightly
        targetTrack = conflict.trackA;
        cutGain = -calculateEQGainForConflict (conflict) * 0.6f;
    }

    float q = conflict.centerFreq / conflict.bandwidth;
    q = juce::jlimit (0.5f, 10.0f, q);

    auto priority = severityToPriority (conflict.severity);

    juce::String desc;
    if (conflict.type == ConflictDetector::FrequencyConflict::ConflictType::Masking)
    {
        desc = "Cut " + freqToString (conflict.centerFreq) + " to unmask " + conflict.trackB;
    }
    else
    {
        desc = "Carve " + freqToString (conflict.centerFreq) + " to resolve overlap with " +
               (targetTrack == conflict.trackA ? conflict.trackB : conflict.trackA);
    }

    addEQRecommendation (Recommendation::Type::EQ_Cut, targetTrack,
                         conflict.centerFreq, cutGain, q,
                         priority, desc, "Conflict");

    // If masking, also suggest boost on masked track
    if (conflict.type == ConflictDetector::FrequencyConflict::ConflictType::Masking)
    {
        juce::String maskedTrack = (targetTrack == conflict.trackA) ? conflict.trackB : conflict.trackA;
        float boostGain = std::abs (cutGain) * 0.5f;

        addEQRecommendation (Recommendation::Type::EQ_Boost, maskedTrack,
                             conflict.centerFreq, boostGain, q * 1.5f,
                             Recommendation::Priority::Medium,
                             "Boost to restore masked " + freqToString (conflict.centerFreq),
                             "Conflict");
    }
}

void RecommendationEngine::processConflictForPanning (const ConflictDetector::FrequencyConflict& conflict)
{
    // Suggest panning separation for conflicts in mid-range
    if (conflict.centerFreq > 200 && conflict.centerFreq < 8000 && conflict.severity > 0.5f)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::Panning;
        rec.priority = Recommendation::Priority::Medium;
        rec.targetTrack = conflict.trackA;
        rec.description = "Pan " + conflict.trackA + " away from " + conflict.trackB +
                          " to create spatial separation at " + freqToString (conflict.centerFreq);
        rec.source = "Conflict";
        rec.panPosition = 0.5f;  // Suggest 50% pan
        rec.technicalDetails = "Pan: 50% " + (conflict.trackALevel > conflict.trackBLevel ? "Left" : "Right");
        rec.confidence = 0.6f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }
}

void RecommendationEngine::processConflictForCompression (const ConflictDetector::FrequencyConflict& conflict)
{
    // Suggest sidechain compression for masking conflicts
    if (conflict.type == ConflictDetector::FrequencyConflict::ConflictType::Masking && conflict.severity > 0.6f)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::Sidechain;
        rec.priority = Recommendation::Priority::High;
        rec.targetTrack = conflict.trackA;  // The masker
        rec.description = "Sidechain " + conflict.trackA + " to " + conflict.trackB +
                          " at " + freqToString (conflict.centerFreq) + " to reduce masking";
        rec.source = "Conflict";
        rec.ratio = 2.5f;
        rec.threshold = conflict.trackALevel - 6.0f;
        rec.attack = 5.0f;
        rec.release = 50.0f;
        rec.technicalDetails = "Ratio: 2.5:1, Thresh: " + juce::String (rec.threshold, 1) + "dB, Attack: 5ms, Release: 50ms";
        rec.confidence = 0.75f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }
}

void RecommendationEngine::processConflictForSaturation (const ConflictDetector::FrequencyConflict& conflict)
{
    // Suggest saturation to add harmonics and differentiate similar sounds
    if (conflict.type == ConflictDetector::FrequencyConflict::ConflictType::SpectralOverlap &&
        conflict.severity > 0.4f && conflict.centerFreq > 500 && conflict.centerFreq < 5000)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::Saturation;
        rec.priority = Recommendation::Priority::Low;
        rec.targetTrack = conflict.trackB;  // The quieter one
        rec.description = "Add subtle saturation to " + conflict.trackB +
                          " to create harmonic distinction from " + conflict.trackA;
        rec.source = "Conflict";
        rec.drive = 3.0f;
        rec.saturationType = 1;  // Tape
        rec.technicalDetails = "Drive: 3dB, Type: Tape";
        rec.confidence = 0.5f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }
}

//==============================================================================
// Loudness-based recommendations
void RecommendationEngine::processLoudnessTarget (const LoudnessAnalyzer::LoudnessMeasurement& loudness)
{
    if (!loudness.meetsStreamingTarget && loudness.integratedLUFS > -70.0f)
    {
        float diff = loudness.integratedLUFS - (-14.0f);  // Target -14 LUFS
        Recommendation::Priority priority = std::abs (diff) > 4.0f ? Recommendation::Priority::Critical :
                                            std::abs (diff) > 2.0f ? Recommendation::Priority::High :
                                            Recommendation::Priority::Medium;

        Recommendation rec;
        rec.type = Recommendation::Type::LevelAdjust;
        rec.priority = priority;
        rec.targetTrack = "Master";
        rec.description = "Adjust master level by " + juce::String (-diff, 1) + "dB to hit -14 LUFS streaming target";
        rec.source = "Loudness";
        rec.gain = -diff;
        rec.technicalDetails = "Current: " + juce::String (loudness.integratedLUFS, 1) + " LUFS, Target: -14 LUFS";
        rec.confidence = 0.9f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }

    if (!loudness.meetsBroadcastTarget && loudness.integratedLUFS > -70.0f)
    {
        float diff = loudness.integratedLUFS - (-23.0f);
        Recommendation rec;
        rec.type = Recommendation::Type::LevelAdjust;
        rec.priority = Recommendation::Priority::Medium;
        rec.targetTrack = "Master";
        rec.description = "For broadcast: adjust level by " + juce::String (-diff, 1) + "dB to hit -23 LUFS";
        rec.source = "Loudness";
        rec.gain = -diff;
        rec.technicalDetails = "Current: " + juce::String (loudness.integratedLUFS, 1) + " LUFS, Target: -23 LUFS";
        rec.confidence = 0.8f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }
}

void RecommendationEngine::processTruePeak (const LoudnessAnalyzer::LoudnessMeasurement& loudness)
{
    if (loudness.truePeakMax > -1.0f && loudness.truePeakMax < 0.0f)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::LevelAdjust;
        rec.priority = Recommendation::Priority::High;
        rec.targetTrack = "Master";
        rec.description = "Reduce true peak by " + juce::String (loudness.truePeakMax + 1.0f, 1) + "dB to stay below -1 dBTP";
        rec.source = "Loudness";
        rec.gain = -(loudness.truePeakMax + 1.0f);
        rec.technicalDetails = "True Peak: " + juce::String (loudness.truePeakMax, 1) + " dBTP";
        rec.confidence = 0.95f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }
    else if (loudness.truePeakMax >= 0.0f)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::LevelAdjust;
        rec.priority = Recommendation::Priority::Critical;
        rec.targetTrack = "Master";
        rec.description = "CLIPPING DETECTED! Reduce level by at least " + juce::String (loudness.truePeakMax + 1.0f, 1) + "dB";
        rec.source = "Loudness";
        rec.gain = -(loudness.truePeakMax + 1.0f);
        rec.technicalDetails = "True Peak: " + juce::String (loudness.truePeakMax, 1) + " dBTP (OVER 0!)";
        rec.confidence = 1.0f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }
}

void RecommendationEngine::processLRA (const LoudnessAnalyzer::LoudnessMeasurement& loudness)
{
    if (loudness.loudnessRange < 3.0f && loudness.integratedLUFS > -70.0f)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::Compression;
        rec.priority = Recommendation::Priority::Medium;
        rec.targetTrack = "Master";
        rec.description = "LRA is only " + juce::String (loudness.loudnessRange, 1) + " LU - consider reducing compression for more dynamics";
        rec.source = "Loudness";
        rec.ratio = 0.8f;  // Suggest expansion
        rec.technicalDetails = "LRA: " + juce::String (loudness.loudnessRange, 1) + " LU (target > 5 LU)";
        rec.confidence = 0.7f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }
    else if (loudness.loudnessRange > 20.0f)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::Compression;
        rec.priority = Recommendation::Priority::Low;
        rec.targetTrack = "Master";
        rec.description = "LRA is " + juce::String (loudness.loudnessRange, 1) + " LU - consider gentle compression for consistency";
        rec.source = "Loudness";
        rec.ratio = 1.5f;
        rec.technicalDetails = "LRA: " + juce::String (loudness.loudnessRange, 1) + " LU (target < 15 LU for streaming)";
        rec.confidence = 0.5f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }
}

//==============================================================================
// Dynamics-based recommendations
void RecommendationEngine::processCrestFactor (const DynamicRangeAnalyzer::DynamicRangeMeasurement& dynamics)
{
    if (dynamics.crestFactorDB < 6.0f)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::Compression;
        rec.priority = dynamics.crestFactorDB < 3.0f ? Recommendation::Priority::Critical : Recommendation::Priority::High;
        rec.targetTrack = "Master";
        rec.description = "Crest factor only " + juce::String (dynamics.crestFactorDB, 1) + "dB - reduce limiting/compression";
        rec.source = "Dynamics";
        rec.ratio = 0.7f;  // Expansion
        rec.technicalDetails = "Crest Factor: " + juce::String (dynamics.crestFactorDB, 1) + "dB (target > 12dB)";
        rec.confidence = 0.85f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }
    else if (dynamics.crestFactorDB > 20.0f)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::Compression;
        rec.priority = Recommendation::Priority::Low;
        rec.targetTrack = "Master";
        rec.description = "High crest factor (" + juce::String (dynamics.crestFactorDB, 1) + "dB) - gentle compression could add punch";
        rec.source = "Dynamics";
        rec.ratio = 1.5f;
        rec.technicalDetails = "Crest Factor: " + juce::String (dynamics.crestFactorDB, 1) + "dB";
        rec.confidence = 0.5f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }
}

void RecommendationEngine::processDRValue (const DynamicRangeAnalyzer::DynamicRangeMeasurement& dynamics)
{
    if (dynamics.drValue < 6.0f)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::Compression;
        rec.priority = Recommendation::Priority::Critical;
        rec.targetTrack = "Master";
        rec.description = "DR value of " + juce::String (dynamics.drValue, 1) + " indicates over-compression - reduce limiting";
        rec.source = "Dynamics";
        rec.ratio = 0.6f;
        rec.technicalDetails = "DR: " + juce::String (dynamics.drValue, 1) + " (target > 10)";
        rec.confidence = 0.9f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }
    else if (dynamics.drValue < 10.0f)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::Compression;
        rec.priority = Recommendation::Priority::High;
        rec.targetTrack = "Master";
        rec.description = "DR value of " + juce::String (dynamics.drValue, 1) + " is compressed - ease off the limiter";
        rec.source = "Dynamics";
        rec.ratio = 0.8f;
        rec.technicalDetails = "DR: " + juce::String (dynamics.drValue, 1) + " (target > 12 for streaming)";
        rec.confidence = 0.8f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }
}

void RecommendationEngine::processCompression (const DynamicRangeAnalyzer::DynamicRangeMeasurement& dynamics)
{
    if (dynamics.likelyOvercompressed)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::Compression;
        rec.priority = Recommendation::Priority::Critical;
        rec.targetTrack = "Master";
        rec.description = "Over-compression detected (" + dynamics.compressionAssessment + ") - reduce ratio/threshold";
        rec.source = "Dynamics";
        rec.ratio = 1.0f / dynamics.estimatedCompressionRatio;  // Inverse to expand
        rec.technicalDetails = "Est. ratio: " + juce::String (dynamics.estimatedCompressionRatio, 1) + ":1";
        rec.confidence = 0.85f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }

    // Transient shaping recommendation
    if (dynamics.shortTermCrestMax - dynamics.shortTermCrestMin < 3.0f)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::TransientShaping;
        rec.priority = Recommendation::Priority::Medium;
        rec.targetTrack = "Drums/Bass";
        rec.description = "Low micro-dynamics - add transient shaper to drums for punch";
        rec.source = "Dynamics";
        rec.technicalDetails = "Micro-dynamics range: " + juce::String (dynamics.microDynamics, 1) + "dB";
        rec.confidence = 0.6f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }
}

void RecommendationEngine::processMicroDynamics (const DynamicRangeAnalyzer::DynamicRangeMeasurement& dynamics)
{
    if (dynamics.microDynamics < 2.0f)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::TransientShaping;
        rec.priority = Recommendation::Priority::Medium;
        rec.targetTrack = "Percussive elements";
        rec.description = "Very low micro-dynamics - use transient shaper to restore attack";
        rec.source = "Dynamics";
        rec.technicalDetails = "Micro-dynamics: " + juce::String (dynamics.microDynamics, 1) + "dB";
        rec.confidence = 0.65f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }
}

//==============================================================================
// Spectrum-based recommendations
void RecommendationEngine::processFrequencyBalance (const SpectrumAnalyzer::SpectrumMeasurement& spectrum)
{
    float midRef = spectrum.midLevel;

    // Low end
    if (spectrum.lowEndLevel < midRef - 8.0f)
    {
        addEQRecommendation (Recommendation::Type::EQ_Boost, "Master/Bass",
                             80.0f, 3.0f, 0.7f,
                             Recommendation::Priority::Medium,
                             "Boost sub-bass around 80Hz for weight",
                             "Spectrum");
    }
    else if (spectrum.lowEndLevel > midRef + 4.0f)
    {
        addEQRecommendation (Recommendation::Type::EQ_Cut, "Master/Bass",
                             60.0f, -2.0f, 0.5f,
                             Recommendation::Priority::Medium,
                             "Cut sub-bass buildup around 60Hz",
                             "Spectrum");
    }

    // Low-mid (mud)
    if (spectrum.lowMidLevel > midRef + 3.0f)
    {
        addEQRecommendation (Recommendation::Type::EQ_Cut, "Master/Instruments",
                             300.0f, -3.0f, 1.0f,
                             Recommendation::Priority::High,
                             "Cut 250-400Hz to reduce muddiness",
                             "Spectrum");
    }

    // High-mid (presence)
    if (spectrum.highMidLevel < midRef - 4.0f)
    {
        addEQRecommendation (Recommendation::Type::EQ_Boost, "Master/Vocals",
                             3500.0f, 2.5f, 1.2f,
                             Recommendation::Priority::Medium,
                             "Boost 3-4kHz for presence and clarity",
                             "Spectrum");
    }

    // High end (air/harshness)
    if (spectrum.highLevel > spectrum.highMidLevel + 3.0f)
    {
        addEQRecommendation (Recommendation::Type::EQ_Cut, "Master",
                             8000.0f, -2.0f, 0.7f,
                             Recommendation::Priority::Medium,
                             "Cut harshness around 8kHz",
                             "Spectrum");
    }
    else if (spectrum.highLevel < midRef - 8.0f)
    {
        addEQRecommendation (Recommendation::Type::EQ_HighPass, "Master",
                             12000.0f, 0.0f, 0.5f,
                             Recommendation::Priority::Low,
                             "Add air shelf above 12kHz for sparkle",
                             "Spectrum");
    }
}

void RecommendationEngine::processResonances (const SpectrumAnalyzer::SpectrumMeasurement& spectrum)
{
    for (const auto& res : spectrum.detectedResonances)
    {
        if (res.prominence > 15.0f)
        {
            Recommendation::Priority priority = res.prominence > 25.0f ? Recommendation::Priority::Critical :
                                                res.prominence > 20.0f ? Recommendation::Priority::High :
                                                Recommendation::Priority::Medium;

            addEQRecommendation (Recommendation::Type::EQ_Notch, "Problem Track",
                                 res.frequency, -6.0f - res.prominence * 0.2f, res.q,
                                 priority,
                                 "Notch resonance at " + freqToString (res.frequency) +
                                 " (" + freqToNote (res.frequency) + ") Q=" + juce::String (res.q, 1),
                                 "Spectrum");
        }
    }
}

void RecommendationEngine::processSpectralFeatures (const SpectrumAnalyzer::SpectrumMeasurement& spectrum)
{
    // Spectral centroid too low = dull
    if (spectrum.spectralCentroid < 1500.0f && spectrum.midLevel > -40.0f)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::Saturation;
        rec.priority = Recommendation::Priority::Medium;
        rec.targetTrack = "Dull tracks";
        rec.description = "Low spectral centroid (" + juce::String (spectrum.spectralCentroid, 0) + "Hz) - add exciter/saturation for brightness";
        rec.source = "Spectrum";
        rec.drive = 4.0f;
        rec.saturationType = 0;  // Tube
        rec.technicalDetails = "Centroid: " + juce::String (spectrum.spectralCentroid, 0) + "Hz";
        rec.confidence = 0.6f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }

    // Spectral flatness high = noise-like, might need gating
    if (spectrum.spectralFlatness > 0.7f)
    {
        Recommendation rec;
        rec.type = Recommendation::Type::LevelAdjust;
        rec.priority = Recommendation::Priority::Low;
        rec.targetTrack = "Noisy tracks";
        rec.description = "High spectral flatness - consider noise gate or cleanup";
        rec.source = "Spectrum";
        rec.technicalDetails = "Flatness: " + juce::String (spectrum.spectralFlatness, 2);
        rec.confidence = 0.5f;

        allRecommendations.add (rec);
        recommendationCounter++;
    }
}

//==============================================================================
// Multi-track recommendations
void RecommendationEngine::analyzeTrackRoles (const juce::Array<juce::AudioBuffer<float>*>& buffers,
                                              const juce::Array<juce::String>& trackNames)
{
    // Analyze each track to guess its role (kick, snare, bass, vocal, etc.)
    // This would use spectral analysis and transient detection
    // Placeholder for future implementation
}

void RecommendationEngine::recommendTrackLevels (const juce::Array<juce::AudioBuffer<float>*>& buffers,
                                                 const juce::Array<juce::String>& trackNames)
{
    // Calculate RMS for each track and suggest level balance
    for (int i = 0; i < buffers.size(); ++i)
    {
        auto* buf = buffers[i];
        if (!buf) continue;

        float sumSq = 0.0f;
        for (int ch = 0; ch < buf->getNumChannels(); ++ch)
        {
            const float* data = buf->getReadPointer (ch);
            for (int s = 0; s < buf->getNumSamples(); ++s)
                sumSq += data[s] * data[s];
        }
        float rms = std::sqrt (sumSq / (buf->getNumSamples() * buf->getNumChannels()));
        float rmsDB = 20.0f * std::log10 (std::max (rms, 1e-10f));

        // Store for level balancing suggestions
        // This is a simplified version - real implementation would use
        // genre-appropriate level relationships
    }
}

void RecommendationEngine::recommendPanningSpread (const juce::Array<juce::AudioBuffer<float>*>& buffers,
                                                   const juce::Array<juce::String>& trackNames)
{
    // Suggest panning positions based on track count and roles
    // Placeholder for future implementation
}

//==============================================================================
RecommendationEngine::Priority RecommendationEngine::severityToPriority (float severity) const
{
    if (severity > 0.8f) return Priority::Critical;
    if (severity > 0.6f) return Priority::High;
    if (severity > 0.4f) return Priority::Medium;
    if (severity > 0.2f) return Priority::Low;
    return Priority::Info;
}

float RecommendationEngine::calculateEQGainForConflict (const ConflictDetector::FrequencyConflict& conflict) const
{
    // Gain reduction based on severity and level difference
    float baseGain = 3.0f + conflict.severity * 6.0f;  // 3-9dB
    float levelDiff = std::abs (conflict.trackALevel - conflict.trackBLevel);
    return juce::jlimit (1.0f, 12.0f, baseGain + levelDiff * 0.3f);
}

float RecommendationEngine::calculateCompressionRatioForDynamics (float drValue) const
{
    // Suggest compression ratio based on DR
    if (drValue > 14) return 1.0f;      // No compression needed
    if (drValue > 10) return 1.5f;      // Gentle
    if (drValue > 8) return 2.0f;       // Light
    if (drValue > 6) return 3.0f;       // Moderate
    return 4.0f;                        // Heavy (but don't recommend this)
}