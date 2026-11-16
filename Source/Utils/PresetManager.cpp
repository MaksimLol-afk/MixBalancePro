#include "PresetManager.h"
#include "ParameterManager.h"
#include <ctime>

//==============================================================================
PresetManager::PresetManager (juce::AudioProcessor& p)
    : processor (p)
{
}

void PresetManager::initialize()
{
    loadFactoryPresets();

    // Load user presets
    auto userDir = getUserPresetDirectory();
    if (userDir.exists())
        importPresetsFromDirectory (userDir);

    // Set default preset
    if (presets.size() > 0)
        setCurrentPreset (presets[0]->name);
}

//==============================================================================
void PresetManager::addPreset (const Preset& preset)
{
    // Check if already exists
    for (auto* p : presets)
    {
        if (p->name == preset.name)
        {
            *p = preset;
            return;
        }
    }

    auto* newPreset = new Preset (preset);
    presets.add (newPreset);

    if (!currentPreset)
        setCurrentPreset (preset.name);
}

void PresetManager::removePreset (const juce::String& name)
{
    for (int i = 0; i < presets.size(); ++i)
    {
        if (presets[i]->name == name)
        {
            bool wasCurrent = (presets[i] == currentPreset);
            presets.remove (i);

            if (wasCurrent && presets.size() > 0)
                setCurrentPreset (presets[0]->name);
            else if (wasCurrent)
                currentPreset = nullptr;

            break;
        }
    }
}

void PresetManager::renamePreset (const juce::String& oldName, const juce::String& newName)
{
    for (auto* p : presets)
    {
        if (p->name == oldName)
        {
            p->name = newName;
            break;
        }
    }
}

//==============================================================================
void PresetManager::setCurrentPreset (const juce::String& name)
{
    for (auto* p : presets)
    {
        if (p->name == name)
        {
            currentPreset = p;
            processor.setStateInformation (p->state.createXml().get(), 0); // This needs proper binary conversion

            if (onPresetChanged)
                onPresetChanged (name);

            break;
        }
    }
}

juce::String PresetManager::getCurrentPresetName() const
{
    return currentPreset ? currentPreset->name : juce::String();
}

//==============================================================================
juce::Array<PresetManager::Preset> PresetManager::getAllPresets() const
{
    juce::Array<Preset> result;
    for (auto* p : presets)
        result.add (*p);
    return result;
}

juce::Array<PresetManager::Preset> PresetManager::getPresetsByCategory (const juce::String& category) const
{
    juce::Array<Preset> result;
    for (auto* p : presets)
    {
        if (p->category == category)
            result.add (*p);
    }
    return result;
}

juce::StringArray PresetManager::getCategories() const
{
    juce::StringArray cats;
    for (auto* p : presets)
    {
        if (!p->category.isEmpty() && !cats.contains (p->category))
            cats.add (p->category);
    }
    return cats;
}

//==============================================================================
void PresetManager::loadFactoryPresets()
{
    createDefaultPresets();
}

void PresetManager::createDefaultPresets()
{
    auto createPreset = [this] (const juce::String& name, const juce::String& category,
                                const juce::String& desc, const juce::ValueTree& state)
    {
        Preset p;
        p.name = name;
        p.category = category;
        p.description = desc;
        p.state = state;
        p.isFactory = true;
        p.author = "MixBalancePro";
        p.version = "1.0.0";

        // Current date
        time_t now = time (nullptr);
        char buf[64];
        strftime (buf, sizeof (buf), "%Y-%m-%d", localtime (&now));
        p.dateCreated = buf;

        addPreset (p);
    };

    // Get default parameter state
    auto& paramManager = dynamic_cast<MixBalanceProAudioProcessor&> (processor).getParameterManager();
    juce::ValueTree defaultState = paramManager.getState();

    // Streaming Master preset
    {
        auto state = defaultState.createCopy();
        state.setProperty ("loudnessTarget", 0, nullptr);  // Streaming -14 LUFS
        state.setProperty ("conflictThreshold", 0.3f, nullptr);
        state.setProperty ("averagingTime", 0.5f, nullptr);
        state.setProperty ("peakHoldTime", 2.0f, nullptr);
        createPreset ("Streaming Master", "Mastering",
                      "Optimized for streaming platforms (-14 LUFS)", state);
    }

    // CD Master preset
    {
        auto state = defaultState.createCopy();
        state.setProperty ("loudnessTarget", 1, nullptr);  // CD -9 LUFS
        state.setProperty ("conflictThreshold", 0.25f, nullptr);
        createPreset ("CD Master", "Mastering",
                      "Optimized for CD/mastering (-9 LUFS)", state);
    }

    // Club Master preset
    {
        auto state = defaultState.createCopy();
        state.setProperty ("loudnessTarget", 2, nullptr);  // Club -6 LUFS
        state.setProperty ("conflictThreshold", 0.35f, nullptr);
        createPreset ("Club Master", "Mastering",
                      "Optimized for club playback (-6 LUFS)", state);
    }

    // Broadcast preset
    {
        auto state = defaultState.createCopy();
        state.setProperty ("loudnessTarget", 3, nullptr);  // Broadcast -23 LUFS
        state.setProperty ("conflictThreshold", 0.2f, nullptr);
        state.setProperty ("meterRange", 2, nullptr);  // 120 dB range
        createPreset ("Broadcast", "Mastering",
                      "EBU R128 broadcast standard (-23 LUFS)", state);
    }

    // Mix Analysis preset
    {
        auto state = defaultState.createCopy();
        state.setProperty ("analysisMode", 1, nullptr);  // Capture mode
        state.setProperty ("captureLength", 1.0f, nullptr);
        state.setProperty ("conflictThreshold", 0.25f, nullptr);
        state.setProperty ("conflictSensitivity", 1.2f, nullptr);
        state.setProperty ("psychoacousticModel", true, nullptr);
        createPreset ("Mix Analysis", "Analysis",
                      "Comprehensive mix analysis with conflict detection", state);
    }

    // Real-time Monitoring preset
    {
        auto state = defaultState.createCopy();
        state.setProperty ("analysisMode", 0, nullptr);  // Real-time
        state.setProperty ("averagingTime", 0.3f, nullptr);
        state.setProperty ("peakHoldTime", 1.0f, nullptr);
        state.setProperty ("fftSize", 2, nullptr);  // 4096
        createPreset ("Real-time Monitor", "Monitoring",
                      "Low-latency real-time monitoring", state);
    }

    // Mastering QC preset
    {
        auto state = defaultState.createCopy();
        state.setProperty ("analysisMode", 2, nullptr);  // Offline
        state.setProperty ("captureLength", 5.0f, nullptr);
        state.setProperty ("conflictThreshold", 0.15f, nullptr);
        state.setProperty ("conflictSensitivity", 1.5f, nullptr);
        state.setProperty ("psychoacousticModel", true, nullptr);
        state.setProperty ("averagingTime", 1.0f, nullptr);
        state.setProperty ("peakHoldTime", 5.0f, nullptr);
        state.setProperty ("fftSize", 3, nullptr);  // 8192
        createPreset ("Mastering QC", "Quality Control",
                      "Full offline analysis for mastering quality control", state);
    }
}

//==============================================================================
bool PresetManager::exportPreset (const juce::String& name, const juce::File& file)
{
    for (auto* p : presets)
    {
        if (p->name == name)
        {
            auto xml = p->state.createXml();
            if (xml)
            {
                // Add metadata
                xml->setAttribute ("presetName", p->name);
                xml->setAttribute ("category", p->category);
                xml->setAttribute ("description", p->description);
                xml->setAttribute ("author", p->author);
                xml->setAttribute ("version", p->version);
                xml->setAttribute ("dateCreated", p->dateCreated);
                xml->setAttribute ("isFactory", p->isFactory);

                return xml->writeToFile (file, {});
            }
            break;
        }
    }
    return false;
}

bool PresetManager::importPreset (const juce::File& file)
{
    std::unique_ptr<juce::XmlElement> xml (juce::XmlElement::parse (file));

    if (xml && xml->hasTagName ("PARAMETERS"))
    {
        Preset p;
        p.name = xml->getStringAttribute ("presetName", file.getFileNameWithoutExtension());
        p.category = xml->getStringAttribute ("category", "User");
        p.description = xml->getStringAttribute ("description", "");
        p.author = xml->getStringAttribute ("author", "User");
        p.version = xml->getStringAttribute ("version", "1.0.0");
        p.dateCreated = xml->getStringAttribute ("dateCreated", "");
        p.isFactory = xml->getBoolAttribute ("isFactory", false);
        p.state = juce::ValueTree::fromXml (*xml);

        addPreset (p);
        return true;
    }
    return false;
}

bool PresetManager::exportAllPresets (const juce::File& directory)
{
    if (!directory.exists())
        directory.createDirectory();

    bool allSuccess = true;
    for (auto* p : presets)
    {
        if (!p->isFactory)  // Only export user presets by default
        {
            juce::File file = directory.getChildFile (p->name + ".mbp");
            if (!exportPreset (p->name, file))
                allSuccess = false;
        }
    }
    return allSuccess;
}

bool PresetManager::importPresetsFromDirectory (const juce::File& directory)
{
    if (!directory.exists() || !directory.isDirectory())
        return false;

    juce::Array<juce::File> files;
    directory.findChildFiles (files, juce::File::findFiles, false, "*.mbp");

    bool anySuccess = false;
    for (auto& file : files)
    {
        if (importPreset (file))
            anySuccess = true;
    }
    return anySuccess;
}

//==============================================================================
juce::ValueTree PresetManager::getState() const
{
    if (currentPreset)
        return currentPreset->state.createCopy();
    return {};
}

void PresetManager::setState (const juce::ValueTree& state)
{
    // Create a temporary preset with current state
    Preset temp;
    temp.name = "Current State";
    temp.category = "Session";
    temp.state = state.createCopy();
    temp.isFactory = false;
    temp.dateCreated = juce::Time::getCurrentTime().toISO8601 (true);

    addPreset (temp);
    setCurrentPreset (temp.name);
}

//==============================================================================
juce::File PresetManager::getUserPresetDirectory() const
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    return dir.getChildFile ("MixBalancePro/Presets");
}

juce::File PresetManager::getFactoryPresetDirectory() const
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    return dir.getSiblingFile ("Resources/FactoryPresets");
}