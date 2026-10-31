#include "PluginEditor.h"

static constexpr int rowH = 34;
static constexpr int contentW = 760;

// ---------------------------------------------------------------- Palette popup
class PaletteComponent : public juce::Component
{
public:
    explicit PaletteComponent (std::function<void (juce::uint32)> cb) : onPick (std::move (cb))
    {
        for (int row = 0; row < rows; ++row)
            for (int col = 0; col < cols; ++col)
                colours.push_back (makeColour (row, col));

        setSize (pad * 2 + cols * cell, pad * 2 + noneH + 4 + rows * cell);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff2a2f35));

        auto none = noneArea();
        g.setColour (juce::Colour (0xff4a5058));
        g.fillRoundedRectangle (none.toFloat(), 4.0f);
        g.setColour (juce::Colours::white);
        g.setFont (13.0f);
        g.drawText ("No colour", none, juce::Justification::centred);

        for (int i = 0; i < (int) colours.size(); ++i)
        {
            g.setColour (colours[(size_t) i]);
            g.fillRect (cellArea (i).reduced (1));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        juce::uint32 picked = 0;
        bool hit = false;

        if (noneArea().contains (e.getPosition()))
            hit = true;
        else
            for (int i = 0; i < (int) colours.size(); ++i)
                if (cellArea (i).contains (e.getPosition()))
                {
                    picked = colours[(size_t) i].getARGB();
                    hit = true;
                    break;
                }

        if (! hit)
            return;

        auto cb = onPick;
        auto* box = findParentComponentOfClass<juce::CallOutBox>();
        if (cb) cb (picked);
        if (box != nullptr) box->dismiss();
    }

private:
    static constexpr int cols = 12, rows = 5, cell = 22, pad = 8, noneH = 24;

    static juce::Colour makeColour (int row, int col)
    {
        const float h = (float) col / (float) cols;
        switch (row)
        {
            case 0:  return juce::Colour::fromHSV (h, 0.40f, 1.00f, 1.0f);   // pastel
            case 1:  return juce::Colour::fromHSV (h, 0.65f, 0.90f, 1.0f);   // bright
            case 2:  return juce::Colour::fromHSV (h, 0.85f, 0.65f, 1.0f);   // deep
            case 3:  return juce::Colour::fromHSV (h, 0.70f, 0.38f, 1.0f);   // dark
            default: return juce::Colour::fromHSV (0.0f, 0.0f, 1.0f - (float) col * 0.08f, 1.0f); // greys
        }
    }

    juce::Rectangle<int> noneArea() const { return { pad, pad, cols * cell, noneH }; }
    juce::Rectangle<int> cellArea (int i) const
    {
        return { pad + (i % cols) * cell, pad + noneH + 4 + (i / cols) * cell, cell, cell };
    }

    std::function<void (juce::uint32)> onPick;
    std::vector<juce::Colour> colours;
};

// ---------------------------------------------------------------- RowComponent
RowComponent::RowComponent (BridgeProcessor& p, int idx, std::function<void()> onRemove)
    : proc (p), index (idx)
{
    const auto row = proc.getRow (idx);
    colourArgb = row.colour;

    memo.setText (row.memo, false);
    memo.setTextToShowWhenEmpty ("memo", juce::Colours::grey);
    memo.onTextChange = [this] { apply(); };
    addAndMakeVisible (memo);

    type.addItem ("CC", 1);
    type.addItem ("PC", 2);
    type.addItem ("CC %", 3);
    type.setSelectedId (row.type + 1, juce::dontSendNotification);
    type.onChange = [this]
    {
        const int t = type.getSelectedId() - 1;
        val.setEnabled (t != 1);
        val.setRange (0, t == 2 ? 100 : 127, 1);
        apply();
    };
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
    setup (val,  0, row.type == 2 ? 100 : 127, row.value, 44);
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

    val.setEnabled (row.type != 1);

    send.onClick = [this] { apply(); proc.sendRowNow (index); };
    addAndMakeVisible (send);

    remove.onClick = std::move (onRemove);
    addAndMakeVisible (remove);

    swatch.onClick = [this]
    {
        juce::Component::SafePointer<RowComponent> safe (this);
        auto palette = std::make_unique<PaletteComponent> ([safe] (juce::uint32 c)
        {
            if (safe != nullptr)
            {
                safe->colourArgb = c;
                safe->updateColour();
                safe->apply();
            }
        });
        auto* top = getTopLevelComponent();
        juce::CallOutBox::launchAsynchronously (std::move (palette),
                                                top->getLocalArea (&swatch, swatch.getLocalBounds()),
                                                top);
    };
    addAndMakeVisible (swatch);
    updateColour();
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
    r.colour      = colourArgb;
    proc.setRow (index, r);
}

void RowComponent::updateColour()
{
    const bool none = ((colourArgb >> 24) == 0);
    const juce::Colour c (colourArgb);

    swatch.setColour (juce::TextButton::buttonColourId, none ? juce::Colour (0xff555a60) : c);
    swatch.setButtonText (none ? "-" : "");

    const auto fieldBg = c.darker (0.7f);
    memo.setColour (juce::TextEditor::backgroundColourId,
                    none ? getLookAndFeel().findColour (juce::TextEditor::backgroundColourId) : fieldBg);
    type.setColour (juce::ComboBox::backgroundColourId,
                    none ? getLookAndFeel().findColour (juce::ComboBox::backgroundColourId) : fieldBg);
    memo.repaint();
    type.repaint();
    repaint();
}

void RowComponent::paint (juce::Graphics& g)
{
    if ((colourArgb >> 24) == 0)
        return;
    g.setColour (juce::Colour (colourArgb).withAlpha (0.22f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f);
}

void RowComponent::resized()
{
    auto r = getLocalBounds().reduced (2);
    juce::Component* comps[9] = { &swatch, &memo, &type, &ch, &num, &val, &trig, &send, &remove };
    for (int i = 0; i < 9; ++i)
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
    setResizeLimits (640, 240, 1200, 900);
    setSize (800, 420);
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

    const char* names[9] = { "", "Memo", "Type", "Ch", "No.", "Value", "Trigger note (C3=60)", "", "" };
    int x = 8 + 2;
    for (int i = 0; i < 9; ++i)
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
