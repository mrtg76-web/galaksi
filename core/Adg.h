// Galaksi core: Ableton Live Drum Rack preset (.adg) XML'i üretir (JUCE'den bağımsız)
//
// .adg dosyası gzip ile sıkıştırılmış XML'dir. Burada sadece XML metni üretilir; sıkıştırma
// eklentide yapılır. Sample yolları .adg dosyasına göre GÖRELİ yazılır (Live 10 biçimi), bu
// yüzden .adg ile WAV'lar aynı klasörde durmalıdır.
#pragma once
#include <string>
#include <vector>

namespace gk {

struct AdgPad {
    int midiNote = 36;          // pad'in notası (C1 = 36)
    std::string name;           // pad/zincir adı
    std::string fileName;       // .adg ile aynı klasördeki WAV dosyasının adı
    std::string absoluteDir;    // WAV'ın bulunduğu klasörün tam yolu (Live'ın arama ipucu için)
    long long frames = 0;       // örnek sayısı
    double sampleRate = 44100;
    long long fileSize = 0;     // bayt
};

std::string drumRackXml(const std::vector<AdgPad>& pads, const std::string& creator = "Ableton Live 10.1.43");

// XML için metin kaçışı (&, <, >, ", ')
std::string xmlEscape(const std::string& s);

} // namespace gk
