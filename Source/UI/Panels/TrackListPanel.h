#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
    Track List Panel

    Manages track references for multi-track conflict analysis.
    Shows track levels, allows adding/removing track references.
*/
class TrackListPanel  : public juce::Component
{
public:
    TrackListPanel (MixBalanceProAudioProcessor& processor);
    ~TrackListPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MixBalanceProAudioProcessor& processor;

    class TrackTable  : public juce::TableListBox,
                        public juce::TableListBoxModel
    {
    public:
        TrackTable (TrackListPanel& panel);
        ~TrackTable() = default;

        int getNumRows() override;
        void paintRowBackground (juce::Graphics&, int rowNumber, int width, int height, bool rowIsSelected) override;
        void paintCell (juce::Graphics&, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
        void cellClicked (int rowNumber, int columnId, const juce::MouseEvent&) override;
        void sortOrderChanged (int newSortColumnId, bool isForwards) override;
        juce::Component* refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
                                                   juce::Component* existingComponentToUpdate) override;

        void refresh() { updateContent(); }

    private:
        TrackListPanel& panel;
        juce::Array<juce::String> columnNames = { "Track", "Level", "Peak", "Conflicts", "Actions" };
        int sortColumn = 0;
        bool sortForwards = true;

        void updateContent();
    };

    class TrackHeader  : public juce::Component
    {
    public:
        TrackHeader (TrackListPanel& panel);
        void resized() override;

        juce::TextButton addTrackButton { "+ Add Track" };
        juce::TextButton analyzeButton { "Analyze Group" };
        juce::TextButton clearButton { "Clear All" };
        juce::Label trackCountLabel { {}, "Tracks: 0" };

    private:
        TrackListPanel& panel;
    };

    std::unique_ptr<TrackHeader> header;
    std::unique_ptr<TrackTable> table;

    //==============================================================================
    void addTrackReference (const juce::String& name);
    void removeTrackReference (int index);
    void analyzeSelectedTracks();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackListPanel)
};