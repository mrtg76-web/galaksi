// Sample yuvaları: arka planda radyo yakalama, analiz, transpoz, WAV aktarma
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <set>

#include "Audio.h"
#include "RadioNet.h"

namespace gk {

using SharedBuffer = std::shared_ptr<const juce::AudioBuffer<float>>;

struct SlotInfo {
    enum class State { Empty, Searching, Ready, Failed };
    State state = State::Empty;
    bool locked = false;
    juce::String status;               // arayüzde gösterilen son durum
    juce::String station, country, tag;
    juce::String origKey, targetKey;   // "E major", "C minor"
    int shift = 0;
    double tuning = 0, confidence = 0, fit = 0;
    juce::File file;
    SharedBuffer audio;
    double sampleRate = 44100;
    std::vector<float> peaks;          // dalga formu için
    int attempts = 0;
};

struct EngineSettings {
    juce::StringArray tags {"jazz", "world music"};
    juce::File folder;
    double recordSeconds = 12;
    Settings core;
};

class SlotEngine {
public:
    static constexpr int MaxSlots = 16;

    SlotEngine();
    ~SlotEngine();

    void setSettings(const EngineSettings& s);
    EngineSettings getSettings() const;

    void refresh(int slot);            // kilitliyse dokunmaz
    void refreshAllUnlocked(int count);
    void stop(int slot);
    void setLocked(int slot, bool locked);

    SlotInfo getSlot(int slot) const;
    SharedBuffer getAudio(int slot, double& sampleRate) const;
    int version() const { return versionCounter.load(); }
    juce::String lastMessage() const;
    int busyCount() const;

    // Bir yuva değiştiğinde (işçi iş parçacığından) çağrılır
    std::function<void(int)> onSlotChanged;

    // Test için: istasyon havuzunu ağ olmadan doldurur
    void addStationsForTesting(const juce::String& tag, const juce::Array<Station>& list)
    {
        const juce::ScopedLock pl(poolLock);
        pool[tag].addArray(list);
    }

    juce::ValueTree saveState() const;
    void loadState(const juce::ValueTree& v);

private:
    class Job;
    void runJob(int slot, int generation);
    bool pickStation(Station& out, juce::String& err);
    void setStatus(int slot, int generation, const juce::String& s);
    void setMessage(const juce::String& s);
    void publish(int slot);
    juce::File writeWav(int slot, const CaptureResult& r, const Station& st, const EngineSettings& es);
    static std::vector<float> computePeaks(const juce::AudioBuffer<float>& b, int points);

    mutable juce::CriticalSection lock;
    SlotInfo slots[MaxSlots];
    int generations[MaxSlots] = {};
    EngineSettings settings;
    juce::String message;
    std::atomic<int> versionCounter {0};
    std::atomic<bool> shuttingDown {false};

    // istasyon havuzu
    juce::CriticalSection poolLock, fetchLock;
    std::map<juce::String, juce::Array<Station>> pool;
    std::set<juce::String> inUse, badUrls;
    juce::Random rng;

    juce::ThreadPool threads;
};

} // namespace gk
