// Freesound.org APIv2: metin araması ve önizleme (MP3) indirme
#pragma once
#include <juce_core/juce_core.h>
#include <functional>

namespace gk {

struct SoundRef {
    int id = 0;
    juce::String name, username, license, previewUrl, pageUrl, term;
    juce::StringArray tags;
    double duration = 0;
};

struct SearchQuery {
    juce::String term, apiKey;
    int page = 1, pageSize = 150;
    double maxDuration = 4.0;
    bool cc0Only = false;
    // Freesound'un "tek olay" analiz alanı (boş = filtre yok). Güncel API'de "single_event",
    // eski API'de "ac_single_event".
    juce::String singleEventField = "single_event";
};

struct SearchResult {
    juce::Array<SoundRef> sounds;
    int count = 0;      // aramanın toplam sonuç sayısı
    int httpCode = 0;   // 200, 401 (anahtar), 400 (filtre), 404 (sayfa yok), 429 (sınır)...
    juce::String error;
};

SearchResult searchFreesound(const SearchQuery& q);

// Önizleme dosyasını indirir (CDN, anahtar gerektirmez)
bool downloadBytes(const juce::String& url, juce::MemoryBlock& out, const std::function<bool()>& shouldStop,
                   juce::String& error);

// Lisans adresini kısa etikete çevirir: "CC0", "CC-BY", "CC-BY-NC", "Sampling+"
juce::String shortLicense(const juce::String& url);

// Ses adı ya da etiketlerinden biri hariç tutulan kelimelerden birini içeriyor mu?
bool isExcluded(const SoundRef& s, const juce::StringArray& excludeWords);

// Varsayılan arama ve hariç tutma listeleri (Freesound İngilizce etiketlerle aranıyor)
juce::String defaultPercTerms();
juce::String defaultPercExclude();

} // namespace gk
