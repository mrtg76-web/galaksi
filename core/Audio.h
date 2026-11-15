// Galaksi core: MP3 çözme, pitch-shift ve yakalama işleme hattı
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace gk {

struct Pcm {
    double sampleRate = 0;
    int channels = 0;                       // 1 veya 2
    std::vector<std::vector<float>> data;   // [kanal][örnek]
    int length() const { return data.empty() ? 0 : (int) data[0].size(); }
    std::vector<float> mono() const;
};

// Bir yayından ham olarak kaydedilmiş MP3 baytlarını çözer (ortadan başlayan akışlar dahil).
// En fazla 2 kanal döner. Başarısızsa length()==0.
Pcm decodeMp3(const uint8_t* data, size_t size);

enum class ShiftMode { Quality = 0, Tape = 1 };

// Perdeyi 'semitones' kadar kaydırır. Quality: süre korunur (Rubber Band R3).
// Tape: plak/bant gibi hız değişir (süre 1/oran katına çıkar).
Pcm pitchShift(const Pcm& in, double semitones, ShiftMode mode);

struct Settings {
    int targetRoot = 0;          // C
    int targetMode = 1;          // minor
    double lengthSec = 2.0;
    double minConfidence = 0.55;
    int maxShift = 6;            // izin verilen en büyük kaydırma (yarım ton)
    ShiftMode shiftMode = ShiftMode::Quality;
    double fadeMs = 4.0;
};

struct CaptureResult {
    bool ok = false;
    std::string reason;          // ok değilse neden (Türkçe)
    int root = 0;
    bool minor = false;
    double confidence = 0;
    double tuningCents = 0;
    int shift = 0;               // yarım ton (akort düzeltmesi hariç)
    int overlap = 0;             // 7 notadan kaçı örtüşüyor
    double fitBefore = 0, fitAfter = 0; // hedef scale'e düşen enerji oranı
    Pcm audio;                   // hazır sample (normalize edilmiş)
};

// Yakalanan bir kaydı analiz eder; uygunsa hedef scale'e taşınmış sample'ı döner.
CaptureResult processCapture(const Pcm& recording, const Settings& s);

} // namespace gk
