#include "PluginProcessor.h"
#include "PluginEditor.h"

BridgeProcessor::BridgeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    BridgeRow r;
    r.memo = "Example (edit me)";
    r.type = 0; r.channel = 1; r.number = 7; r.value = 20; r.triggerNote = 36;
    rows.push_back (r);
    startTimer (2);
}

BridgeProcessor::~BridgeProcessor()
{
    stopTimer();
    output.reset();
}

bool BridgeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

BridgeProcessor::Raw BridgeProcessor::makeRaw (const BridgeRow& r)
{
    Raw raw;
    const int ch = juce::jlimit (1, 16, r.channel) - 1;
    if (r.type == 0)
    {
        raw.d[0] = (juce::uint8) (0xB0 | ch);
        raw.d[1] = (juce::uint8) juce::jlimit (0, 127, r.number);
        raw.d[2] = (juce::uint8) juce::jlimit (0, 127, r.value);
        raw.n = 3;
    }
    else
    {
        raw.d[0] = (juce::uint8) (0xC0 | ch);
        raw.d[1] = (juce::uint8) juce::jlimit (0, 127, r.number);
        raw.n = 2;
    }
    return raw;
}

void BridgeProcessor::pushFromAudio (const Raw& r)
{
    int s1, sz1, s2, sz2;
    fifo.prepareToWrite (1, s1, sz1, s2, sz2);
    if (sz1 > 0)      fifoData[s1] = r;
    else if (sz2 > 0) fifoData[s2] = r;
    fifo.finishedWrite (sz1 + sz2);
}

void BridgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();

    const juce::ScopedTryLock sl (rowsLock);
    if (sl.isLocked())
    {
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            if (! msg.isNoteOn())
                continue;
            for (const auto& r : rows)
                if (r.triggerNote == msg.getNoteNumber())
                    pushFromAudio (makeRaw (r));
        }
    }
    midi.clear();
}

void BridgeProcessor::sendRaw (const Raw& r)
{
    juce::String text;
    const int ch = (r.d[0] & 0x0F) + 1;
    if (r.n == 3) text = "CC ch" + juce::String (ch) + " #" + juce::String (r.d[1]) + " = " + juce::String (r.d[2]);
    else          text = "PC ch" + juce::String (ch) + " #" + juce::String (r.d[1]);

    if (output != nullptr)
    {
        if (r.n == 3) output->sendMessageNow (juce::MidiMessage ((int) r.d[0], (int) r.d[1], (int) r.d[2]));
        else          output->sendMessageNow (juce::MidiMessage ((int) r.d[0], (int) r.d[1]));
    }
    else
        text += "  (no port)";

    const juce::ScopedLock sl (eventLock);
    lastEvent = text;
}

void BridgeProcessor::timerCallback()
{
    const int ready = fifo.getNumReady();
    if (ready <= 0)
        return;

    int s1, sz1, s2, sz2;
    fifo.prepareToRead (ready, s1, sz1, s2, sz2);
    for (int i = 0; i < sz1; ++i) sendRaw (fifoData[s1 + i]);
    for (int i = 0; i < sz2; ++i) sendRaw (fifoData[s2 + i]);
    fifo.finishedRead (sz1 + sz2);
}

void BridgeProcessor::sendRowNow (int i)
{
    BridgeRow r;
    {
        const juce::ScopedLock sl (rowsLock);
        if (i < 0 || i >= (int) rows.size()) return;
        r = rows[(size_t) i];
    }
    sendRaw (makeRaw (r));
}

juce::String BridgeProcessor::getLastEvent() const
{
    const juce::ScopedLock sl (eventLock);
    return lastEvent;
}

int BridgeProcessor::getNumRows() const
{
    const juce::ScopedLock sl (rowsLock);
    return (int) rows.size();
}

BridgeRow BridgeProcessor::getRow (int i) const
{
    const juce::ScopedLock sl (rowsLock);
    if (i < 0 || i >= (int) rows.size()) return {};
    return rows[(size_t) i];
}

void BridgeProcessor::setRow (int i, const BridgeRow& r)
{
    const juce::ScopedLock sl (rowsLock);
    if (i >= 0 && i < (int) rows.size())
        rows[(size_t) i] = r;
}

void BridgeProcessor::addRow()
{
    const juce::ScopedLock sl (rowsLock);
    BridgeRow r;
    r.triggerNote = juce::jmin (127, 36 + (int) rows.size());
    rows.push_back (r);
}

void BridgeProcessor::removeRow (int i)
{
    const juce::ScopedLock sl (rowsLock);
    if (i >= 0 && i < (int) rows.size())
        rows.erase (rows.begin() + i);
}

void BridgeProcessor::openPort (const juce::String& name)
{
    output.reset();
    portName = name;
    if (name.isEmpty())
        return;

    for (auto& d : juce::MidiOutput::getAvailableDevices())
        if (d.name == name)
        {
            output = juce::MidiOutput::openDevice (d.identifier);
            break;
        }
}

juce::String BridgeProcessor::getStatus() const
{
    if (output != nullptr) return "Connected: " + portName;
    if (portName.isEmpty()) return "No port selected";
    return "Port not found: " + portName;
}

void BridgeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement xml ("MidiBridge");
    xml.setAttribute ("port", portName);
    {
        const juce::ScopedLock sl (rowsLock);
        for (auto& r : rows)
        {
            auto* e = xml.createNewChildElement ("Row");
            e->setAttribute ("memo", r.memo);
            e->setAttribute ("type", r.type);
            e->setAttribute ("ch", r.channel);
            e->setAttribute ("num", r.number);
            e->setAttribute ("val", r.value);
            e->setAttribute ("trig", r.triggerNote);
        }
    }
    copyXmlToBinary (xml, dest);
}

void BridgeProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName ("MidiBridge"))
        return;

    {
        const juce::ScopedLock sl (rowsLock);
        rows.clear();
        for (auto* e : xml->getChildWithTagNameIterator ("Row"))
        {
            BridgeRow r;
            r.memo        = e->getStringAttribute ("memo");
            r.type        = e->getIntAttribute ("type", 0);
            r.channel     = e->getIntAttribute ("ch", 1);
            r.number      = e->getIntAttribute ("num", 0);
            r.value       = e->getIntAttribute ("val", 0);
            r.triggerNote = e->getIntAttribute ("trig", -1);
            rows.push_back (r);
        }
    }
    openPort (xml->getStringAttribute ("port"));
}

juce::AudioProcessorEditor* BridgeProcessor::createEditor()
{
    return new BridgeEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BridgeProcessor();
}
