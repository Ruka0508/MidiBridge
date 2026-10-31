#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <vector>

struct BridgeRow
{
    juce::String memo;
    int type = 0;          // 0 = CC, 1 = PC
    int channel = 1;       // 1-16
    int number = 0;        // CC number or program number
    int value = 0;         // CC value (unused for PC)
    int triggerNote = -1;  // -1 = none
};

class BridgeProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    BridgeProcessor();
    ~BridgeProcessor() override;

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "MIDI Bridge"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // rows (thread-safe)
    int getNumRows() const;
    BridgeRow getRow (int i) const;
    void setRow (int i, const BridgeRow& r);
    void addRow();
    void removeRow (int i);

    // output port (message thread only)
    void openPort (const juce::String& name);
    juce::String getPortName() const { return portName; }
    juce::String getStatus() const;
    void sendRowNow (int i);
    juce::String getLastEvent() const;

private:
    struct Raw { juce::uint8 d[3] {}; int n = 0; };

    static Raw makeRaw (const BridgeRow& r);
    void pushFromAudio (const Raw& r);
    void sendRaw (const Raw& r);
    void timerCallback() override;

    mutable juce::CriticalSection rowsLock;
    std::vector<BridgeRow> rows;

    std::unique_ptr<juce::MidiOutput> output;
    juce::String portName;

    juce::AbstractFifo fifo { 512 };
    Raw fifoData[512];

    mutable juce::CriticalSection eventLock;
    juce::String lastEvent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeProcessor)
};
