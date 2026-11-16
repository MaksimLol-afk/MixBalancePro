# MixBalancePro - Professional Mix Balance Analyzer

A VST3 plugin for FL Studio (and other DAWs) that provides comprehensive mix analysis including:

- **Loudness Analysis** (EBU R128 / ITU-R BS.1770): Momentary, Short-term, Integrated LUFS, LRA, True Peak
- **Frequency Conflict Detection**: Spectral overlap, masking, tonal clashes, harmonic conflicts between tracks
- **Dynamic Range Analysis**: Crest factor, DR (TT Dynamic Range), micro/macro dynamics, PLR, compression detection
- **Real-time Spectrum Display**: Live FFT with peak hold, averaging, 1/3 octave bands, resonance detection
- **Intelligent Recommendations**: Actionable EQ, panning, compression, saturation, level adjustments with specific parameters

## Features

### Loudness Metering (EBU R128 Compliant)
- Momentary (400ms), Short-term (3s), Integrated loudness
- Loudness Range (LRA) calculation
- True Peak detection with inter-sample peak interpolation
- Target compliance: Streaming (-14 LUFS), CD (-9 LUFS), Club (-6 LUFS), Broadcast (-23 LUFS)

### Frequency Conflict Detection
- 24 Bark-scale frequency bands
- Psychoacoustic masking model
- Conflict types: Spectral Overlap, Masking, Tonal Clash, Harmonic Conflict, Transient Collision
- Multi-track analysis support
- Visual heatmap with severity coloring

### Dynamic Range Analysis
- Crest Factor (Peak/RMS)
- DR Value (1-20 scale per TT Dynamic Range Meter)
- Micro-dynamics (short-term crest variation)
- Macro-dynamics (long-term level variation)
- PLR (Peak to Loudness Ratio)
- Automatic compression detection and ratio estimation

### Spectrum Analyzer
- Real-time magnitude spectrum with configurable FFT size (1024-16384)
- Exponential averaging and peak hold
- 1/3 octave band display (31 bands)
- Spectral features: Centroid, Spread, Rolloff, Flatness, Crest
- Automatic resonance detection with Q estimation

### Smart Recommendations
Generated from all analysis domains:
- **EQ**: Cut/Boost/Notch/HP/LP with exact frequency, gain, Q
- **Panning**: Spatial separation suggestions
- **Compression**: Ratio, threshold, attack, release, knee
- **Sidechain**: Frequency-specific ducking
- **Saturation**: Drive amount, type (Tube/Tape/Transistor/SoftClip)
- **Transient Shaping**: Attack/sustain control
- **De-Essing**: Frequency-targeted sibilance reduction
- **Multiband Compression**: Band-specific dynamics control

## Building

### Requirements
- CMake 3.22+
- C++20 compatible compiler (MSVC 2022, GCC 11+, Clang 13+)
- JUCE 7+ (included as submodule or external)

### Windows (Visual Studio)
```bash
# Clone JUCE if not present
git submodule update --init --recursive

# Configure
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Release
```

### Using JUCE from system
```bash
cmake -B build -DJUCE_DIR=/path/to/JUCE
cmake --build build --config Release
```

### Output
- `build/MixBalancePro_artefacts/Release/VST3/MixBalancePro.vst3`

## Installation

Copy the built `.vst3` folder to your VST3 directory:
- **Windows**: `%ProgramFiles%\Common Files\VST3\`
- **User**: `%LOCALAPPDATA%\Programs\Common Files\VST3\`

Then rescan plugins in FL Studio.

## Usage

1. **Insert on Master Bus** for overall mix analysis
2. **Insert on Individual Tracks** for track-specific analysis
3. **Use "Capture" mode** to record 30 seconds for detailed offline analysis
4. **Enable Multi-track** by adding track references in the Tracks panel
5. **Review Recommendations** in the bottom panel - filter by priority/type/track

## Analysis Modes

| Mode | Description |
|------|-------------|
| Real-time | Continuous live analysis, low latency |
| Capture | Records audio for 30s then runs full analysis |
| Offline | Processes entire project/buffer at once |

## Presets Included

- **Streaming Master** - Optimized for -14 LUFS streaming targets
- **CD Master** - -9 LUFS for CD/mastering
- **Club Master** - -6 LUFS for club playback
- **Broadcast** - EBU R128 -23 LUFS compliance
- **Mix Analysis** - Comprehensive conflict detection
- **Real-time Monitor** - Low-latency monitoring
- **Mastering QC** - Full offline quality control

## Architecture

```
MixBalancePro/
├── Source/
│   ├── PluginProcessor.h/cpp      # Main processor, parameter handling
│   ├── PluginEditor.h/cpp         # Editor wrapper
│   ├── Analysis/
│   │   ├── LoudnessAnalyzer       # EBU R128 loudness
│   │   ├── ConflictDetector       # Frequency conflicts
│   │   ├── DynamicRangeAnalyzer   # DR, crest factor, compression
│   │   ├── SpectrumAnalyzer       # FFT, spectral features
│   │   └── RecommendationEngine   # Smart recommendations
│   ├── DSP/
│   │   ├── FFTProcessor           # FFT with windowing
│   │   └── AudioBufferUtils       # Buffer utilities
│   ├── UI/
│   │   ├── MainComponent          # Main layout
│   │   ├── Visualization/         # Meters, heatmap, spectrum, DR
│   │   └── Panels/                # Recommendations, track list
│   └── Utils/
│       ├── ParameterManager       # APVTS parameters
│       └── PresetManager          # Preset handling
├── Resources/
│   ├── LookAndFeel.json           # UI theme
│   └── Icon.png                   # Plugin icon
└── CMakeLists.txt                 # Build configuration
```

## License

Proprietary - All rights reserved.

## Support

For issues and feature requests, please contact the developer.