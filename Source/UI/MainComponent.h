#pragma once

#include <JuceHeader.h>
#include "Visualization/LoudnessMeter.h"
#include "Visualization/ConflictHeatmap.h"
#include "Visualization/SpectrumDisplay.h"
#include "Visualization/DynamicRangeDisplay.h"
#include "Panels/RecommendationPanel.h"
#include "Panels/TrackListPanel.h"

//==============================================================================
class MainComponent  : public juce::Component,
                       public juce::Timer
{
public:
    MainComponent (MixBalanceProAudioProcessor& processor);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    MixBalanceProAudioProcessor& processor;

    // Layout sections
    enum class Section
    {
        Toolbar,
        MainView,
        BottomPanel
    };

    // Toolbar
    class ToolbarComponent : public juce::Component
    {
    public:
        ToolbarComponent (MixBalanceProAudioProcessor& p);
        void resized() override;

        juce::ComboBox analysisModeBox;
        juce::TextButton captureButton { "Capture" };
        juce::TextButton clearButton { "Clear" };
        juce::ComboBox loudnessTargetBox;
        juce::Slider conflictThresholdSlider;
        juce::Label thresholdLabel { {}, "Conflict Threshold:" };
        juce::ToggleButton autoScrollButton { "Auto-scroll" };

    private:
        MixBalanceProAudioProcessor& processor;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAttachment;
    };

    // Main visualization area
    class MainViewComponent : public juce::Component
    {
    public:
        MainViewComponent (MixBalanceProAudioProcessor& p);
        void resized() override;

        LoudnessMeter loudnessMeter;
        ConflictHeatmap conflictHeatmap;
        SpectrumDisplay spectrumDisplay;
        DynamicRangeDisplay dynamicRangeDisplay;

    private:
        MixBalanceProAudioProcessor& processor;
    };

    // Bottom panel with recommendations and track list
    class BottomPanelComponent : public juce::Component
    {
    public:
        BottomPanelComponent (MixBalanceProAudioProcessor& p);
        void resized() override;

        RecommendationPanel recommendationPanel;
        TrackListPanel trackListPanel;
        juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    private:
        MixBalanceProAudioProcessor& processor;
    };

    // Components
    std::unique_ptr<ToolbarComponent> toolbar;
    std::unique_ptr<MainViewComponent> mainView;
    std::unique_ptr<BottomPanelComponent> bottomPanel;

    // Layout proportions
    static constexpr float toolbarHeight = 50.0f;
    static constexpr float bottomPanelHeight = 300.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};