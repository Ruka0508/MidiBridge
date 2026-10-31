#pragma once
#include "PluginProcessor.h"

class RowComponent : public juce::Component
{
public:
    RowComponent (BridgeProcessor& p, int idx, std::function<void()> onRemove);
    void resized() override;

    static constexpr int widths[8] = { 170, 70, 84, 84, 84, 104, 56, 30 };
    static constexpr int gap = 4;

private:
    void apply();

    BridgeProcessor& proc;
    int index;
    juce::TextEditor memo;
    juce::ComboBox type;
    juce::Slider ch, num, val, trig;
    juce::TextButton send { "Send" }, remove { "X" };
};

class BridgeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BridgeEditor (BridgeProcessor&);
    ~BridgeEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void rebuildRows();
    void fillPorts();
    void timerCallback() override;

    BridgeProcessor& proc;
    juce::ComboBox portBox;
    juce::TextButton refreshBtn { "Refresh" }, addBtn { "Add Row" };
    juce::Label status, lastEvent;
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<RowComponent> rowComps;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeEditor)
};
