#include "TrackListPanel.h"
#include "DSP/AudioBufferUtils.h"
#include "PluginProcessor.h"

//==============================================================================
TrackListPanel::TrackListPanel (MixBalanceProAudioProcessor& p)
    : processor (p)
{
    header = std::make_unique<TrackHeader> (*this);
    table = std::make_unique<TrackTable> (*this);

    addAndMakeVisible (header.get());
    addAndMakeVisible (table.get());

    // Initialize header buttons
    header->addTrackButton.onClick = [this]()
    {
        // In a real implementation, this would show a dialog to select a track
        // For now, add a placeholder
        static int trackNum = 1;
        addTrackReference ("Track " + juce::String (trackNum++));
    };

    header->analyzeButton.onClick = [this]() { analyzeSelectedTracks(); };

    header->clearButton.onClick = [this]()
    {
        // Clear all track references
        auto& refs = const_cast<juce::Array<MixBalanceProAudioProcessor::TrackReference>&>(
            processor.getTrackReferences());
        refs.clear();
        table->refresh();
    };
}

TrackListPanel::~TrackListPanel() = default;

void TrackListPanel::resized()
{
    auto bounds = getLocalBounds().reduced (5);
    header->setBounds (bounds.removeFromTop (40));
    table->setBounds (bounds);
}

void TrackListPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0d1117));
    g.setColour (juce::Colour (0xFF30363d));
    g.drawRect (getLocalBounds(), 1);
}

//==============================================================================
void TrackListPanel::addTrackReference (const juce::String& name)
{
    // In a real implementation, this would connect to another plugin instance
    // via shared memory or host track audio access
    // For now, we just add a reference entry
    auto& refs = const_cast<juce::Array<MixBalanceProAudioProcessor::TrackReference>&>(
        processor.getTrackReferences());

    MixBalanceProAudioProcessor::TrackReference ref;
    ref.name = name;
    ref.processor = nullptr;  // Would be set via host integration
    refs.add (ref);

    table->refresh();
    header->trackCountLabel.setText ("Tracks: " + juce::String (refs.size()), juce::dontSendNotification);
}

void TrackListPanel::removeTrackReference (int index)
{
    auto& refs = const_cast<juce::Array<MixBalanceProAudioProcessor::TrackReference>&>(
        processor.getTrackReferences());

    if (index >= 0 && index < refs.size())
    {
        refs.remove (index);
        table->refresh();
        header->trackCountLabel.setText ("Tracks: " + juce::String (refs.size()), juce::dontSendNotification);
    }
}

void TrackListPanel::analyzeSelectedTracks()
{
    // Get selected tracks
    juce::Array<juce::String> selectedNames;
    // In a real implementation, get from table selection
    // For now, analyze all
    for (auto& ref : processor.getTrackReferences())
        selectedNames.add (ref.name);

    if (selectedNames.size() >= 2)
    {
        processor.analyzeTrackGroup (selectedNames);
    }
}

//==============================================================================
// TrackHeader Implementation
TrackListPanel::TrackHeader::TrackHeader (TrackListPanel& p)
    : panel (p)
{
    addAndMakeVisible (addTrackButton);
    addAndMakeVisible (analyzeButton);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (trackCountLabel);

    addTrackButton.setButtonText ("+ Add Track");
    analyzeButton.setButtonText ("Analyze Group");
    clearButton.setButtonText ("Clear All");

    trackCountLabel.setColour (Label::textColourId, juce::Colours::white);
    trackCountLabel.setFont (juce::FontOptions (11.0f));
}

void TrackListPanel::TrackHeader::resized()
{
    auto bounds = getLocalBounds().reduced (5);
    addTrackButton.setBounds (bounds.removeFromLeft (100));
    bounds.removeFromLeft (5);
    analyzeButton.setBounds (bounds.removeFromLeft (100));
    bounds.removeFromLeft (5);
    clearButton.setBounds (bounds.removeFromLeft (80));
    bounds.removeFromLeft (10);
    trackCountLabel.setBounds (bounds.removeFromLeft (100));
}

//==============================================================================
// TrackTable Implementation
TrackListPanel::TrackTable::TrackTable (TrackListPanel& p)
    : panel (p), TableListBox (nullptr, nullptr)
{
    setModel (this);
    setColour (TableListBox::backgroundColourId, juce::Colour (0xFF0d1117));
    setColour (TableListBox::outlineColourId, juce::Colour (0xFF30363d));
    setColour (TableListBox::textColourId, juce::Colours::white);
    setHeaderHeight (28);
    setRowHeight (32);
    setMultipleSelectionEnabled (true);

    for (int i = 0; i < columnNames.size(); ++i)
    {
        auto& header = getHeader();
        if (header)
            header->addColumn (columnNames[i], i + 1, (i == 0) ? 150 : 80, 50, 300,
                               TableHeaderComponent::defaultFlags);
    }
}

int TrackListPanel::TrackTable::getNumRows()
{
    return panel.processor.getTrackReferences().size();
}

void TrackListPanel::TrackTable::paintRowBackground (juce::Graphics& g, int rowNumber,
                                                      int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.setColour (juce::Colour (0xFF1f6feb).withAlpha (0.3f));
        g.fillRect (0, 0, width, height);
    }
    else if (rowNumber % 2 == 0)
    {
        g.setColour (juce::Colour (0xFF161b22));
        g.fillRect (0, 0, width, height);
    }
}

void TrackListPanel::TrackTable::paintCell (juce::Graphics& g, int rowNumber, int columnId,
                                             int width, int height, bool rowIsSelected)
{
    auto& refs = panel.processor.getTrackReferences();
    if (rowNumber >= refs.size()) return;

    auto& ref = refs.getReference (rowNumber);
    auto bounds = juce::Rectangle<int> (0, 0, width, height).reduced (8, 0);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (11.0f));

    switch (columnId)
    {
        case 1: // Track name
            g.drawText (ref.name, bounds, juce::Justification::centredLeft);
            break;

        case 2: // Level (RMS)
            if (ref.lastBuffer.getNumSamples() > 0)
            {
                float rms = AudioBufferUtils::getRMS (ref.lastBuffer);
                float rmsDb = AudioBufferUtils::gainToDb (rms);
                g.drawText (juce::String (rmsDb, 1) + " dB", bounds, juce::Justification::centred);
            }
            else
            {
                g.setColour (juce::Colours::white.withAlpha (0.4f));
                g.drawText ("—", bounds, juce::Justification::centred);
            }
            break;

        case 3: // Peak
            if (ref.lastBuffer.getNumSamples() > 0)
            {
                float peak = AudioBufferUtils::getPeak (ref.lastBuffer);
                float peakDb = AudioBufferUtils::gainToDb (peak);
                g.drawText (juce::String (peakDb, 1) + " dB", bounds, juce::Justification::centred);
            }
            else
            {
                g.setColour (juce::Colours::white.withAlpha (0.4f));
                g.drawText ("—", bounds, juce::Justification::centred);
            }
            break;

        case 4: // Conflicts count
            {
                auto conflicts = panel.processor.getConflictDetector().getConflictsForTrack (ref.name);
                if (conflicts.size() > 0)
                {
                    g.setColour (juce::Colour (0xFFf85149));
                    g.drawText (juce::String (conflicts.size()), bounds, juce::Justification::centred);
                }
                else
                {
                    g.setColour (juce::Colour (0xFF3fb950));
                    g.drawText ("0", bounds, juce::Justification::centred);
                }
            }
            break;

        case 5: // Actions
            // Would add remove button here in real implementation
            break;
    }
}

void TrackListPanel::TrackTable::cellClicked (int rowNumber, int columnId, const juce::MouseEvent&)
{
    if (columnId == 5) // Actions column
    {
        // Could show context menu
    }
}

void TrackListPanel::TrackTable::sortOrderChanged (int newSortColumnId, bool isForwards)
{
    sortColumn = newSortColumnId;
    sortForwards = isForwards;
    // Would implement sorting here
}

juce::Component* TrackListPanel::TrackTable::refreshComponentForCell (int rowNumber, int columnId,
                                                                       bool isRowSelected,
                                                                       juce::Component* existingComponentToUpdate)
{
    // For the Actions column, we could return a button component
    if (columnId == 5)
    {
        // Would create a remove button component
    }
    return nullptr;
}

void TrackListPanel::TrackTable::updateContent()
{
    updateContent();
}