#include "MainComponent.h"
#include "PluginProcessor.h"

//==============================================================================
// ToolbarComponent Implementation
MainComponent::ToolbarComponent::ToolbarComponent (MixBalanceProAudioProcessor& p)
    : processor (p)
{
    addAndMakeVisible (analysisModeBox);
    analysisModeBox.addItem ("Real-time", 1);
    analysisModeBox.addItem ("Capture (30s)", 2);
    analysisModeBox.addItem ("Offline Analysis", 3);
    analysisModeBox.setSelectedId (1);
    analysisModeBox.onChange = [this]()
    {
        processor.setAnalysisMode (static_cast<MixBalanceProAudioProcessor::AnalysisMode> (analysisModeBox.getSelectedId() - 1));
    };

    addAndMakeVisible (captureButton);
    captureButton.onClick = [this]()
    {
        processor.requestFullAnalysis();
        captureButton.setButtonText (processor.getParameterManager().getParameterValue ("analysisMode") == 1 ? "Capturing..." : "Capture");
    };

    addAndMakeVisible (clearButton);
    clearButton.onClick = [this]() { processor.clearAnalysisHistory(); };

    addAndMakeVisible (loudnessTargetBox);
    loudnessTargetBox.addItem ("Streaming (-14 LUFS)", 1);
    loudnessTargetBox.addItem ("CD (-9 LUFS)", 2);
    loudnessTargetBox.addItem ("Club (-6 LUFS)", 3);
    loudnessTargetBox.addItem ("Broadcast (-23 LUFS)", 4);
    loudnessTargetBox.addItem ("Custom", 5);
    loudnessTargetBox.setSelectedId (1);

    addAndMakeVisible (conflictThresholdSlider);
    conflictThresholdSlider.setRange (0.1, 1.0, 0.01);
    conflictThresholdSlider.setValue (0.3);
    conflictThresholdSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
    addAndMakeVisible (thresholdLabel);

    addAndMakeVisible (autoScrollButton);
    autoScrollButton.setToggleState (true, juce::dontSendNotification);
}

void MainComponent::ToolbarComponent::resized()
{
    auto bounds = getLocalBounds().reduced (10);
    auto row = bounds.removeFromTop (30);

    analysisModeBox.setBounds (row.removeFromLeft (150));
    row.removeFromLeft (10);
    captureButton.setBounds (row.removeFromLeft (100));
    row.removeFromLeft (10);
    clearButton.setBounds (row.removeFromLeft (80));
    row.removeFromLeft (20);
    loudnessTargetBox.setBounds (row.removeFromLeft (180));
    row.removeFromLeft (20);
    thresholdLabel.setBounds (row.removeFromLeft (120));
    conflictThresholdSlider.setBounds (row.removeFromLeft (100));
    row.removeFromLeft (20);
    autoScrollButton.setBounds (row.removeFromLeft (100));
}

//==============================================================================
// MainViewComponent Implementation
MainComponent::MainViewComponent::MainViewComponent (MixBalanceProAudioProcessor& p)
    : processor (p),
      loudnessMeter (processor.getLoudnessAnalyzer()),
      conflictHeatmap (processor.getConflictDetector()),
      spectrumDisplay (processor.getSpectrumAnalyzer()),
      dynamicRangeDisplay (processor.getDynamicRangeAnalyzer())
{
    addAndMakeVisible (loudnessMeter);
    addAndMakeVisible (conflictHeatmap);
    addAndMakeVisible (spectrumDisplay);
    addAndMakeVisible (dynamicRangeDisplay);
}

void MainComponent::MainViewComponent::resized()
{
    auto bounds = getLocalBounds().reduced (5);

    // Top row: Loudness meter (left) + Conflict heatmap (right)
    auto topRow = bounds.removeFromTop (bounds.getHeight() / 2);
    loudnessMeter.setBounds (topRow.removeFromLeft (topRow.getWidth() * 0.4));
    topRow.removeFromLeft (5);
    conflictHeatmap.setBounds (topRow);

    // Bottom row: Spectrum (left) + Dynamic range (right)
    auto bottomRow = bounds;
    spectrumDisplay.setBounds (bottomRow.removeFromLeft (bottomRow.getWidth() * 0.6));
    bottomRow.removeFromLeft (5);
    dynamicRangeDisplay.setBounds (bottomRow);
}

//==============================================================================
// BottomPanelComponent Implementation
MainComponent::BottomPanelComponent::BottomPanelComponent (MixBalanceProAudioProcessor& p)
    : processor (p),
      recommendationPanel (processor.getRecommendationEngine()),
      trackListPanel (processor)
{
    addAndMakeVisible (tabs);
    tabs.addTab ("Recommendations", juce::Colours::transparentBlack, &recommendationPanel, true);
    tabs.addTab ("Tracks", juce::Colours::transparentBlack, &trackListPanel, true);
    tabs.setOutline (0);
    tabs.getTabbedButtonBar().setColour (juce::TabbedButtonBar::backgroundColourId, juce::Colour (0xFF16213e));
    tabs.getTabbedButtonBar().setColour (juce::TabbedButtonBar::tabTextColourId, juce::Colours::white);
}

void MainComponent::BottomPanelComponent::resized()
{
    tabs.setBounds (getLocalBounds().reduced (5));
}

//==============================================================================
// MainComponent Implementation
MainComponent::MainComponent (MixBalanceProAudioProcessor& p)
    : processor (p)
{
    toolbar = std::make_unique<ToolbarComponent> (processor);
    mainView = std::make_unique<MainViewComponent> (processor);
    bottomPanel = std::make_unique<BottomPanelComponent> (processor);

    addAndMakeVisible (toolbar.get());
    addAndMakeVisible (mainView.get());
    addAndMakeVisible (bottomPanel.get());

    // Set up analysis update callback
    processor.onAnalysisUpdate = [this]()
    {
        // Trigger repaint on message thread
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        {
            mainView->loudnessMeter.repaint();
            mainView->conflictHeatmap.repaint();
            mainView->spectrumDisplay.repaint();
            mainView->dynamicRangeDisplay.repaint();
            bottomPanel->recommendationPanel.repaint();
        }
        else
        {
            juce::MessageManager::callAsync ([this]()
            {
                mainView->loudnessMeter.repaint();
                mainView->conflictHeatmap.repaint();
                mainView->spectrumDisplay.repaint();
                mainView->dynamicRangeDisplay.repaint();
                bottomPanel->recommendationPanel.repaint();
            });
        }
    };

    processor.onNewRecommendation = [this] (const Recommendation& rec)
    {
        juce::MessageManager::callAsync ([this, rec]()
        {
            bottomPanel->recommendationPanel.addRecommendation (rec);
        });
    };

    startTimerHz (30); // 30 FPS UI updates
}

MainComponent::~MainComponent()
{
    stopTimer();
}

void MainComponent::paint (juce::Graphics& g)
{
    // Background gradient
    juce::ColourGradient gradient (
        juce::Colour (0xFF0f0f1a), 0, 0,
        juce::Colour (0xFF1a1a2e), 0, (float)getHeight(),
        false
    );
    g.setGradientFill (gradient);
    g.fillAll();

    // Subtle grid lines
    g.setColour (juce::Colour (0xFF00ffff).withAlpha (0.03f));
    for (int x = 0; x < getWidth(); x += 50)
        g.drawVerticalLine (x, 0.0f, (float)getHeight());
    for (int y = 0; y < getHeight(); y += 50)
        g.drawHorizontalLine (y, 0.0f, (float)getWidth());
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    toolbar->setBounds (bounds.removeFromTop (toolbarHeight));
    bottomPanel->setBounds (bounds.removeFromBottom (bottomPanelHeight));
    mainView->setBounds (bounds);
}

void MainComponent::timerCallback()
{
    // Update smoothed meter values
    // The actual values are updated in processBlock via smoothed values
    // This timer just triggers repaints for smooth animation
}