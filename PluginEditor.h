#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/MainComponent.h"

//==============================================================================
class MixBalanceProAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    MixBalanceProAudioProcessorEditor (MixBalanceProAudioProcessor&);
    ~MixBalanceProAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MixBalanceProAudioProcessor& processorRef;
    std::unique_ptr<MainComponent> mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixBalanceProAudioProcessorEditor)
};