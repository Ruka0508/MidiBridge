#include "PluginEditor.h"

static constexpr int rowH = 34;
static constexpr int contentW = 720;

// ---------------------------------------------------------------- RowComponent
RowComponent::RowComponent (BridgeProcessor& p, int idx, std::function<void()> onRemove)
    : proc (p), index (idx)
{
    const auto row = proc.getRow (idx);

    memo.setText (row.memo, false);
    memo.setTextToShowWhenEmpty ("memo", juce::Colours::grey);
    memo.onTextChange = [this] { apply(); };
    addAndMakeVisible (memo);

    type.addItem ("CC", 1);
    type.addItem ("PC", 2);
    type.setSelectedId (row.type + 1, juce::dontSendNotification);
    type.onChange = [this] { val.setEnabled (type.getSelectedId() == 1); apply(); };
    addAndMakeVisible (type);

    auto setup = [this] (juce::Slider& s, double lo, double hi, double v, int textW)
    {
        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, textW, 24);
        s.setRange (lo, hi, 1);
        s.setValue (v, juce::dontSendNotification);
        s.onValueChange = [this] { apply(); };
        addAndMakeVisible (s);
    };

    setup (ch,   1, 16,  row.channel,     44);
    setup (num,  0, 127, row.number,      44);
    setup (val,  0, 127, row.value,       44);
    setup (trig, -1, 127, row.triggerNote, 64);

    trig.textFromValueFunction = [] (double v)
    {
        const int n = (int) v;
        if (n < 0) return juce::String ("none");
        return juce::MidiMessage::getMidiNoteName (n, true, true, 3) + " (" + juce::String (n) + ")";
    };
    trig.valueFromTextFunction = [] (const juce::String& text)
    {
        if (text.containsIgnoreCase ("none")) return -1.0;
        auto t = text.fromFirstOccurrenceOf ("(", false, false).upToFirstOccurrenceOf (")", false, false);
        if (t.isEmpty()) t = text;
        return (double) t.getIntValue();
    };
    trig.setValue (row.triggerNote, juce::dontSendNotification);
    trig.updateText();

    val.setEnabled (row.type == 0);

    send.onClick = [this] { apply(); proc.sendRowNow (index); };
    addAndMakeVisible (send);

    remove.onClick = std::move (onRemove);
    addAndMakeVisible (remove);
}

void RowComponent::apply()
{
    BridgeRow r;
    r.memo        = memo.getText();
    r.type        = type.getSelectedId() - 1;
    r.channel     = (int) ch.getValue();
    r.number      = (int) num.getValue();
    r.value       = (int) val.getValue();
    r.triggerNote = (int) trig.getValue();
    proc.setRow (index, r);
}

void RowComponent::resized()
{
    auto r = getLocalBounds().reduced (2);
    juce::Component* comps[8] = { &memo, &type, &ch, &num, &val, &trig, &send, &remove };
    for (int i = 0; i < 8; ++i)
    {
        comps[i]->setBounds (r.removeFromLeft (widths[i]));
        r.removeFromLeft (gap);
    }
}

// ---------------------------------------------------------------- BridgeEditor
BridgeEditor::BridgeEditor (BridgeProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p)
{
    addAndMakeVisible (portBox);
    portBox.onChange = [this]
    {
        proc.openPort (portBox.getSelectedId() == 1 ? juce::String() : portBox.getText());
    };

    refreshBtn.onClick = [this] { fillPorts(); };
    addAndMakeVisible (refreshBtn);

    addBtn.onClick = [this] { proc.addRow(); rebuildRows(); };
    addAndMakeVisible (addBtn);

    addAndMakeVisible (status);
    addAndMakeVisible (lastEvent);
    status.setJustificationType (juce::Justification::centredLeft);
    lastEvent.setJustificationType (juce::Justification::centredRight);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    fillPorts();
    rebuildRows();

    setResizable (true, true);
    setResizeLimits (600, 240, 1100, 900);
    setSize (760, 420);
    startTimerHz (10);
}

BridgeEditor::~BridgeEditor()
{
    stopTimer();
}

void BridgeEditor::fillPorts()
{
    portBox.clear (juce::dontSendNotification);
    portBox.addItem ("(none)", 1);

    int selected = 1;
    auto devices = juce::MidiOutput::getAvailableDevices();
    for (int i = 0; i < devices.size(); ++i)
    {
        portBox.addItem (devices[i].name, i + 2);
        if (devices[i].name == proc.getPortName())
            selected = i + 2;
    }
    portBox.setSelectedId (selected, juce::dontSendNotification);
}

void BridgeEditor::rebuildRows()
{
    rowComps.clear();
    const int n = proc.getNumRows();
    juce::Component::SafePointer<BridgeEditor> safe (this);

    for (int i = 0; i < n; ++i)
    {
        auto* rc = new RowComponent (proc, i, [safe, i]
        {
            juce::MessageManager::callAsync ([safe, i]
            {
                if (safe != nullptr)
                {
                    safe->proc.removeRow (i);
                    safe->rebuildRows();
                }
            });
        });
        rowComps.add (rc);
        content.addAndMakeVisible (rc);
        rc->setBounds (0, i * rowH, contentW, rowH);
    }
    content.setSize (contentW, juce::jmax (rowH, n * rowH));
}

void BridgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2227));
    g.setColour (juce::Colours::lightgrey);
    g.setFont (13.0f);

    const char* names[8] = { "Memo", "Type", "Ch", "No.", "Value", "Trigger note (C3=60)", "", "" };
    int x = 8 + 2;
    for (int i = 0; i < 8; ++i)
    {
        g.drawText (names[i], x, 46, RowComponent::widths[i] + 40, 18, juce::Justification::centredLeft);
        x += RowComponent::widths[i] + RowComponent::gap;
    }
}

void BridgeEditor::resized()
{
    auto r = getLocalBounds().reduced (8);

    auto top = r.removeFromTop (30);
    portBox.setBounds (top.removeFromLeft (240));
    top.removeFromLeft (6);
    refreshBtn.setBounds (top.removeFromLeft (80));
    top.removeFromLeft (10);
    status.setBounds (top);

    r.removeFromTop (22); // header strip (painted)

    auto bottom = r.removeFromBottom (32);
    addBtn.setBounds (bottom.removeFromLeft (100));
    lastEvent.setBounds (bottom);

    viewport.setBounds (r);
}

void BridgeEditor::timerCallback()
{
    status.setText (proc.getStatus(), juce::dontSendNotification);
    lastEvent.setText ("Last: " + proc.getLastEvent(), juce::dontSendNotification);
}
