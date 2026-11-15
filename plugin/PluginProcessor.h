#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

#include "SlotEngine.h"

class GalaksiProcessor : public juce::AudioProcessor {
public:
    GalaksiProcessor();
    ~GalaksiProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Galaksi"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // arayüzden
    void preview(int slot);           // sample'ı çal (tekrar basınca durdur)
    void stopPreview();
    bool isPlaying(int slot) const { return voices[(size_t) slot].playingFlag.load(); }
    float playPosition(int slot) const { return voices[(size_t) slot].displayPos.load(); }
    void pushSettingsToEngine();      // parametre/etiket/klasör değişince
    void refreshSlot(int slot) { pushSettingsToEngine(); engine.refresh(slot); }
    void refreshAllUnlocked() { pushSettingsToEngine(); engine.refreshAllUnlocked(slotCount()); }
    void setTags(const juce::String& commaSeparated);
    void setFolder(const juce::File& f);
    int slotCount() const;

    gk::SlotEngine engine;
    juce::AudioProcessorValueTreeState params;

    static constexpr int FirstMidiNote = 36; // C1 -> yuva 1

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void slotChanged(int slot);

    struct Voice {
        gk::SharedBuffer buffer;      // sadece ses iş parçacığı
        double pos = 0, step = 1;
        float gain = 1;
        bool active = false;
        std::atomic<bool> playingFlag {false};
        std::atomic<float> displayPos {0};
    };
    std::array<Voice, gk::SlotEngine::MaxSlots> voices;

    // yuva -> güncel tampon (işçi iş parçacığı yazar, ses iş parçacığı okur)
    juce::SpinLock bufferLock;
    std::array<gk::SharedBuffer, gk::SlotEngine::MaxSlots> slotBuffers;
    std::array<double, gk::SlotEngine::MaxSlots> slotRates {};

    std::array<std::atomic<int>, gk::SlotEngine::MaxSlots> uiTrigger {}; // 1 = çal, 2 = durdur
    std::atomic<bool> stopAll {false};
    double hostRate = 44100;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GalaksiProcessor)
};
