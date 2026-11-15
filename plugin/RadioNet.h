// İnternet radyosu: Radio-Browser istasyon listesi ve yayın yakalama
#pragma once
#include <juce_core/juce_core.h>
#include <functional>

namespace gk {

struct Station {
    juce::String name, url, country, tag, codec, uuid;
    int bitrate = 0; // kbps (bilinmiyorsa 0)
};

// Radio-Browser'dan verilen etikete sahip, çalışan MP3 istasyonlarını rastgele sırayla getirir.
juce::Array<Station> fetchStations(const juce::String& tag, int limit, juce::String& error);

// Yayından yaklaşık 'seconds' saniyelik ham MP3 verisi kaydeder.
// http:// için kendi soket istemcisini kullanır (ICY/Shoutcast yanıtlarını da kabul eder),
// https:// için sistemin URL katmanını kullanır.
bool captureStream(const juce::String& url, double seconds, int bitrateKbps, juce::MemoryBlock& out,
                   const std::function<bool()>& shouldStop, juce::String& error);

} // namespace gk
