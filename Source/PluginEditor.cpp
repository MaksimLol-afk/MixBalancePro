#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
MixBalanceProAudioProcessorEditor::MixBalanceProAudioProcessorEditor (MixBalanceProAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Set up the main component
    mainComponent = std::make_unique<MainComponent> (processorRef);
    addAndMakeVisible (mainComponent.get());

    // Set initial size - large enough for all panels
    setSize (1200, 800);
    setResizable (true, true);
    setResizeLimits (1000, 700, 1920, 1200);
}

MixBalanceProAudioProcessorEditor::~MixBalanceProAudioProcessorEditor() = default;

//==============================================================================
void MixBalanceProAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Dark professional background
    g.fillAll (juce::Colour (0xFF1a1a2e));
}

void MixBalanceProAudioProcessorEditor::resized()
{
    if (mainComponent)
        mainComponent->setBounds (getLocalBounds());
}