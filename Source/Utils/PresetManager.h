#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Preset Manager for MixBalancePro

    Handles factory presets, user presets, and preset import/export
*/
class PresetManager
{
public:
    struct Preset
    {
        juce::String name;
        juce::String category;
        juce::String description;
        juce::ValueTree state;
        bool isFactory = false;
        juce::String author;
        juce::String version;
        juce::String dateCreated;

        juce::String toString() const
        {
            return name + (category.isEmpty() ? "" : " [" + category + "]");
        }
    };

    //==============================================================================
    PresetManager (juce::AudioProcessor& processor);
    ~PresetManager() = default;

    //==============================================================================
    void initialize();

    // Preset management
    void addPreset (const Preset& preset);
    void removePreset (const juce::String& name);
    void renamePreset (const juce::String& oldName, const juce::String& newName);

    // Current preset
    void setCurrentPreset (const juce::String& name);
    const Preset* getCurrentPreset() const noexcept { return currentPreset; }
    juce::String getCurrentPresetName() const;

    // Preset lists
    juce::Array<Preset> getAllPresets() const;
    juce::Array<Preset> getPresetsByCategory (const juce::String& category) const;
    juce::StringArray getCategories() const;

    // Factory presets
    void loadFactoryPresets();

    // Import/Export
    bool exportPreset (const juce::String& name, const juce::File& file);
    bool importPreset (const juce::File& file);
    bool exportAllPresets (const juce::File& directory);
    bool importPresetsFromDirectory (const juce::File& directory);

    // State access
    juce::ValueTree getState() const;
    void setState (const juce::ValueTree& state);

    // Callbacks
    std::function<void(const juce::String&)> onPresetChanged;

private:
    //==============================================================================
    juce::AudioProcessor& processor;
    juce::OwnedArray<Preset> presets;
    Preset* currentPreset = nullptr;

    // File paths
    juce::File getUserPresetDirectory() const;
    juce::File getFactoryPresetDirectory() const;

    // Default presets
    void createDefaultPresets();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};