#include "RecommendationPanel.h"

//==============================================================================
RecommendationPanel::RecommendationPanel (const RecommendationEngine& e)
    : engine (e)
{
    filterBar = std::make_unique<FilterBar> (*this);
    listBox = std::make_unique<RecommendationList> (*this);
    detailView = std::make_unique<DetailView>();

    addAndMakeVisible (filterBar.get());
    addAndMakeVisible (listBox.get());
    addAndMakeVisible (detailView.get());

    // Initialize filter bar
    filterBar->priorityFilter.addItem ("All Priorities", 1);
    filterBar->priorityFilter.addItem ("Critical Only", 2);
    filterBar->priorityFilter.addItem ("High & Above", 3);
    filterBar->priorityFilter.addItem ("Medium & Above", 4);
    filterBar->priorityFilter.addItem ("Low & Above", 5);
    filterBar->priorityFilter.setSelectedId (1);
    filterBar->priorityFilter.onChange = [this]()
    {
        int id = filterBar->priorityFilter.getSelectedId();
        minPriority = static_cast<RecommendationEngine::Recommendation::Priority> (juce::jlimit (0, 4, 4 - (id - 1)));
        listBox->refresh();
    };

    filterBar->typeFilter.addItem ("All Types", 1);
    filterBar->typeFilter.addItem ("EQ", 2);
    filterBar->typeFilter.addItem ("Compression", 3);
    filterBar->typeFilter.addItem ("Panning", 4);
    filterBar->typeFilter.addItem ("Saturation", 5);
    filterBar->typeFilter.addItem ("Level", 6);
    filterBar->typeFilter.addItem ("Sidechain", 7);
    filterBar->typeFilter.addItem ("Transient", 8);
    filterBar->typeFilter.addItem ("De-Essing", 9);
    filterBar->typeFilter.addItem ("Multiband", 10);
    filterBar->typeFilter.setSelectedId (1);
    filterBar->typeFilter.onChange = [this]()
    {
        int id = filterBar->typeFilter.getSelectedId();
        if (id == 1) typeFilter.reset();
        else typeFilter = static_cast<RecommendationEngine::Recommendation::Type> (id - 2);
        listBox->refresh();
    };

    filterBar->clearButton.onClick = [this]() { clearRecommendations(); };
    filterBar->exportButton.onClick = [this]()
    {
        // Export to file
        juce::FileChooser fc ("Export Recommendations", {}, "*.json");
        fc.launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                        [this] (const juce::FileChooser& chooser)
                        {
                            auto file = chooser.getResult();
                            if (file != juce::File())
                                file.replaceWithText ("[]");
                        });
    };
}

RecommendationPanel::~RecommendationPanel() = default;

void RecommendationPanel::resized()
{
    auto bounds = getLocalBounds().reduced (5);
    filterBar->setBounds (bounds.removeFromTop (40));
    auto mainArea = bounds;

    // Split between list and detail
    int listWidth = mainArea.getWidth() * 3 / 5;
    listBox->setBounds (mainArea.removeFromLeft (listWidth).withTrimmedTop (5));
    detailView->setBounds (mainArea.withTrimmedLeft (5));
}

void RecommendationPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0d1117));
    g.setColour (juce::Colour (0xFF30363d));
    g.drawRect (getLocalBounds(), 1);
}

void RecommendationPanel::addRecommendation (const RecommendationEngine::Recommendation& rec)
{
    allRecommendations.add (rec);
    listBox->refresh();

    // Auto-scroll to newest if it matches filter
    auto filtered = getFilteredRecommendations();
    if (!filtered.isEmpty())
    {
        // Check if the new recommendation is in filtered list
        for (int i = 0; i < filtered.size(); ++i)
        {
            if (&filtered.getReference(i) == &allRecommendations.getReference(allRecommendations.size() - 1))
            {
                listBox->selectRow (i);
                break;
            }
        }
    }
}

void RecommendationPanel::clearRecommendations()
{
    allRecommendations.clear();
    listBox->refresh();
    detailView->setRecommendation (nullptr);
}

void RecommendationPanel::setPriorityFilter (RecommendationEngine::Recommendation::Priority minP)
{
    minPriority = minP;
    listBox->refresh();
}

void RecommendationPanel::setTypeFilter (RecommendationEngine::Recommendation::Type type)
{
    typeFilter = type;
    listBox->refresh();
}

void RecommendationPanel::setTrackFilter (const juce::String& track)
{
    trackFilter = track;
    filterBar->trackFilter.setText (track);
    listBox->refresh();
}

juce::Array<RecommendationEngine::Recommendation> RecommendationPanel::getFilteredRecommendations() const
{
    juce::Array<RecommendationEngine::Recommendation> result;

    for (auto& rec : allRecommendations)
    {
        // Priority filter
        if (static_cast<int> (rec.priority) > static_cast<int> (minPriority))
            continue;

        // Type filter
        if (typeFilter.has_value() && rec.type != typeFilter.value())
            continue;

        // Track filter
        if (trackFilter.isNotEmpty() && rec.targetTrack != trackFilter)
            continue;

        result.add (rec);
    }

    // Sort by priority then confidence
    auto recommendationComparator = [] (const RecommendationEngine::Recommendation& a,
                      const RecommendationEngine::Recommendation& b)
    {
        int pa = static_cast<int> (a.priority);
        int pb = static_cast<int> (b.priority);
        if (pa != pb) return pa < pb;
        return a.confidence > b.confidence;
    };
    result.sort (recommendationComparator);

    return result;
}

//==============================================================================
// FilterBar Implementation
RecommendationPanel::FilterBar::FilterBar (RecommendationPanel& p)
    : panel (p)
{
    addAndMakeVisible (priorityFilter);
    addAndMakeVisible (typeFilter);
    addAndMakeVisible (trackFilter);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (exportButton);

    trackFilter.setTextWhenNothingSelected ("All Tracks");
    trackFilter.addItem ("All Tracks", 1);
    trackFilter.setSelectedId (1);
    trackFilter.onChange = [this]()
    {
        panel.setTrackFilter (trackFilter.getText());
    };
}

void RecommendationPanel::FilterBar::resized()
{
    auto bounds = getLocalBounds().reduced (5);
    priorityFilter.setBounds (bounds.removeFromLeft (150));
    bounds.removeFromLeft (5);
    typeFilter.setBounds (bounds.removeFromLeft (150));
    bounds.removeFromLeft (5);
    trackFilter.setBounds (bounds.removeFromLeft (150));
    bounds.removeFromLeft (10);
    clearButton.setBounds (bounds.removeFromLeft (80));
    bounds.removeFromLeft (5);
    exportButton.setBounds (bounds.removeFromLeft (80));
}

//==============================================================================
// RecommendationList Implementation
RecommendationPanel::RecommendationList::RecommendationList (RecommendationPanel& p)
    : panel (p), ListBox (nullptr, nullptr)
{
    setModel (this);
    setRowHeight (45);
    setMultipleSelectionEnabled (false);
    setColour (ListBox::backgroundColourId, juce::Colour (0xFF0d1117));
    setColour (ListBox::outlineColourId, juce::Colour (0xFF30363d));
}

int RecommendationPanel::RecommendationList::getNumRows()
{
    return panel.getFilteredRecommendations().size();
}

void RecommendationPanel::RecommendationList::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                                                 int width, int height, bool rowIsSelected)
{
    auto& recs = panel.getFilteredRecommendations();
    if (rowNumber >= recs.size()) return;

    auto& rec = recs.getReference (rowNumber);
    auto bounds = juce::Rectangle<int> (0, 0, width, height).reduced (5);

    // Selection background
    if (rowIsSelected)
    {
        g.setColour (juce::Colour (0xFF1f6feb).withAlpha (0.3f));
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
        g.setColour (juce::Colour (0xFF1f6feb));
        g.drawRoundedRectangle (bounds.toFloat(), 4.0f, 1.0f);
    }
    else
    {
        g.setColour (juce::Colour (0xFF161b22));
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
        g.setColour (juce::Colour (0xFF30363d));
        g.drawRoundedRectangle (bounds.toFloat(), 4.0f, 0.5f);
    }

    // Priority indicator (left bar)
    juce::Colour priorityCol = getPriorityColour (rec.priority);
    g.setColour (priorityCol);
    g.fillRoundedRectangle (bounds.getX(), bounds.getY() + 2, 4, bounds.getHeight() - 4, 2.0f);

    // Type icon
    g.setColour (priorityCol);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (getTypeIcon (rec.type),
                bounds.getX() + 10, bounds.getY(), 24, bounds.getHeight(),
                juce::Justification::centredLeft);

    // Description
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (rec.description,
                bounds.getX() + 38, bounds.getY() + 2,
                bounds.getWidth() - 48, 20,
                juce::Justification::centredLeft);

    // Technical details
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (juce::FontOptions (9.0f));
    g.drawText (rec.technicalDetails,
                bounds.getX() + 38, bounds.getY() + 22,
                bounds.getWidth() - 48, 16,
                juce::Justification::centredLeft);

    // Source badge
    g.setColour (juce::Colour (0xFF58a6ff).withAlpha (0.2f));
    g.fillRoundedRectangle (bounds.getRight() - 70, bounds.getY() + 4, 65, 18, 3.0f);
    g.setColour (juce::Colour (0xFF58a6ff));
    g.setFont (juce::FontOptions (8.0f));
    g.drawText (rec.source,
                bounds.getRight() - 70, bounds.getY() + 4, 65, 18,
                juce::Justification::centred);
}

void RecommendationPanel::RecommendationList::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    panel.selectedRow = row;
    auto& recs = panel.getFilteredRecommendations();
    if (row >= 0 && row < recs.size())
    {
        panel.detailView->setRecommendation (&recs.getReference (row));
    }
}

void RecommendationPanel::RecommendationList::selectedRowsChanged (int lastRowSelected)
{
    panel.selectedRow = getSelectedRow();
    auto& recs = panel.getFilteredRecommendations();
    if (panel.selectedRow >= 0 && panel.selectedRow < recs.size())
        panel.detailView->setRecommendation (&recs.getReference (panel.selectedRow));
}

void RecommendationPanel::RecommendationList::updateContent()
{
    ListBox::updateContent();
}

juce::Colour RecommendationPanel::RecommendationList::getPriorityColour (
    RecommendationEngine::Recommendation::Priority p) const
{
    switch (p)
    {
        case RecommendationEngine::Recommendation::Priority::Critical: return juce::Colour (0xFFf85149);
        case RecommendationEngine::Recommendation::Priority::High: return juce::Colour (0xFFf85149).darker();
        case RecommendationEngine::Recommendation::Priority::Medium: return juce::Colour (0xFFd29922);
        case RecommendationEngine::Recommendation::Priority::Low: return juce::Colour (0xFF58a6ff);
        case RecommendationEngine::Recommendation::Priority::Info: return juce::Colour (0xFF3fb950);
    }
    return juce::Colours::white;
}

juce::String RecommendationPanel::RecommendationList::getTypeIcon (
    RecommendationEngine::Recommendation::Type t) const
{
    switch (t)
    {
        case RecommendationEngine::Recommendation::Type::EQ_Cut: return "✂";
        case RecommendationEngine::Recommendation::Type::EQ_Boost: return "⬆";
        case RecommendationEngine::Recommendation::Type::EQ_Notch: return "🎯";
        case RecommendationEngine::Recommendation::Type::EQ_HighPass: return "⤴";
        case RecommendationEngine::Recommendation::Type::EQ_LowPass: return "⤵";
        case RecommendationEngine::Recommendation::Type::Panning: return "↔";
        case RecommendationEngine::Recommendation::Type::Compression: return "📊";
        case RecommendationEngine::Recommendation::Type::Saturation: return "🔥";
        case RecommendationEngine::Recommendation::Type::LevelAdjust: return "🔊";
        case RecommendationEngine::Recommendation::Type::StereoWidth: return "⤢";
        case RecommendationEngine::Recommendation::Type::Sidechain: return "🔗";
        case RecommendationEngine::Recommendation::Type::TransientShaping: return "⚡";
        case RecommendationEngine::Recommendation::Type::DeEssing: return "🎙";
        case RecommendationEngine::Recommendation::Type::MultiBandComp: return "📈";
    }
    return "•";
}

//==============================================================================
// DetailView Implementation
RecommendationPanel::DetailView::DetailView()
{
    setOpaque (true);
}

void RecommendationPanel::DetailView::setRecommendation (const RecommendationEngine::Recommendation* rec)
{
    currentRec = rec;
    repaint();
}

void RecommendationPanel::DetailView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced (10);

    g.fillAll (juce::Colour (0xFF0d1117));
    g.setColour (juce::Colour (0xFF30363d));
    g.drawRect (bounds, 1);

    if (!currentRec)
    {
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("Select a recommendation to see details",
                    bounds, juce::Justification::centred);
        return;
    }

    auto& rec = *currentRec;

    // Priority banner
    juce::Colour priorityCol = RecommendationList().getPriorityColour (rec.priority);
    g.setColour (priorityCol.withAlpha (0.2f));
    g.fillRoundedRectangle (bounds.removeFromTop (60).toFloat(), 4.0f);
    g.setColour (priorityCol);
    g.setFont (juce::FontOptions (16.0f).withStyle ("Bold"));
    g.drawText (rec.toString(), bounds.removeFromTop (60).reduced (10), juce::Justification::centredLeft);

    // Technical details
    auto detailBounds = bounds.reduced (10);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (11.0f));

    y = detailBounds.getY();

    auto drawDetail = [&] (const juce::String& label, const juce::String& value)
    {
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawText (label, detailBounds.getX(), y, 120, 22, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white);
        g.drawText (value, detailBounds.getX() + 130, y, detailBounds.getWidth() - 130, 22, juce::Justification::centredLeft);
        y += 24;
    };

    drawDetail ("Type:", juce::String (static_cast<int> (rec.type)));
    drawDetail ("Priority:", juce::String (static_cast<int> (rec.priority)));
    drawDetail ("Target Track:", rec.targetTrack.isEmpty() ? "Master/All" : rec.targetTrack);
    drawDetail ("Source:", rec.source);
    drawDetail ("Confidence:", juce::String (rec.confidence * 100, 0) + "%");

    if (rec.frequency > 0)
        drawDetail ("Frequency:", juce::String (rec.frequency, 1) + " Hz");
    if (rec.gain != 0)
        drawDetail ("Gain:", juce::String (rec.gain, 1) + " dB");
    if (rec.q != 1.0f)
        drawDetail ("Q Factor:", juce::String (rec.q, 2));
    if (rec.ratio > 1.0f)
        drawDetail ("Ratio:", juce::String (rec.ratio, 1) + ":1");
    if (rec.threshold != 0)
        drawDetail ("Threshold:", juce::String (rec.threshold, 1) + " dB");
    if (rec.attack > 0)
        drawDetail ("Attack:", juce::String (rec.attack, 1) + " ms");
    if (rec.release > 0)
        drawDetail ("Release:", juce::String (rec.release, 1) + " ms");
    if (rec.panPosition != 0)
        drawDetail ("Pan:", juce::String (rec.panPosition * 100, 0) + "%");
    if (rec.drive > 0)
        drawDetail ("Drive:", juce::String (rec.drive, 1) + " dB");
    if (rec.width != 1.0f)
        drawDetail ("Width:", juce::String (rec.width, 2) + "x");

    // Description
    y += 10;
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (juce::FontOptions (10.0f));
    g.drawMultiLineText (rec.description, detailBounds.getX(), y, detailBounds.getWidth());
}

void RecommendationPanel::DetailView::resized()
{
    // Handled in paint
}