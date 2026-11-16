#pragma once

#include <JuceHeader.h>
#include "Analysis/RecommendationEngine.h"

//==============================================================================
/**
    Recommendation Panel

    Displays actionable mixing recommendations with priority,
    filtering, and detailed parameter views.
*/
class RecommendationPanel  : public juce::Component
{
public:
    RecommendationPanel (const RecommendationEngine& engine);
    ~RecommendationPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void addRecommendation (const RecommendationEngine::Recommendation& rec);
    void clearRecommendations();

    // Filtering
    void setPriorityFilter (RecommendationEngine::Recommendation::Priority minPriority);
    void setTypeFilter (RecommendationEngine::Recommendation::Type type);
    void setTrackFilter (const juce::String& trackName);

private:
    const RecommendationEngine& engine;

    // UI Components
    class RecommendationList  : public juce::ListBox,
                                public juce::ListBoxModel
    {
    public:
        RecommendationList (RecommendationPanel& panel);
        ~RecommendationList() = default;

        int getNumRows() override;
        void paintListBoxItem (int rowNumber, juce::Graphics&, int width, int height, bool rowIsSelected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        void selectedRowsChanged (int lastRowSelected) override;

        void refresh() { updateContent(); }

    private:
        RecommendationPanel& panel;
        juce::Array<RecommendationEngine::Recommendation> filteredRecommendations;

        void updateContent();
        juce::Colour getPriorityColour (RecommendationEngine::Recommendation::Priority p) const;
        juce::String getTypeIcon (RecommendationEngine::Recommendation::Type t) const;
    };

    class FilterBar  : public juce::Component
    {
    public:
        FilterBar (RecommendationPanel& panel);
        void resized() override;

        juce::ComboBox priorityFilter;
        juce::ComboBox typeFilter;
        juce::ComboBox trackFilter;
        juce::TextButton clearButton { "Clear All" };
        juce::TextButton exportButton { "Export" };

    private:
        RecommendationPanel& panel;
    };

    std::unique_ptr<FilterBar> filterBar;
    std::unique_ptr<RecommendationList> listBox;

    // Filter state
    RecommendationEngine::Recommendation::Priority minPriority = RecommendationEngine::Recommendation::Priority::Info;
    std::optional<RecommendationEngine::Recommendation::Type> typeFilter;
    juce::String trackFilter;

    // All recommendations
    juce::Array<RecommendationEngine::Recommendation> allRecommendations;

    // Detail view
    class DetailView  : public juce::Component
    {
    public:
        DetailView();
        void setRecommendation (const RecommendationEngine::Recommendation* rec);
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        const RecommendationEngine::Recommendation* currentRec = nullptr;
    };

    std::unique_ptr<DetailView> detailView;
    int selectedRow = -1;

    //==============================================================================
    juce::Array<RecommendationEngine::Recommendation> getFilteredRecommendations() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecommendationPanel)
};