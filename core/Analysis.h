// Galaksi core: ton / akort tespiti ve scale yardımcıları (JUCE'den bağımsız)
#pragma once
#include <array>
#include <string>
#include <vector>

namespace gk {

enum Mode { Major = 0, Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, HarmonicMinor, NumModes };

const char* modeName(int mode);            // "major", "minor", ...
const char* modeNameTr(int mode);          // arayüz için
const char* noteName(int pc);              // "C", "Db", ...
std::array<int, 7> modeSteps(int mode);
unsigned scaleMask(int root, int mode);    // 12 bitlik nota kümesi
std::string keyName(int root, bool minor); // "Eb major"

struct KeyResult {
    int root = 0;
    bool minor = false;
    double confidence = 0;   // en iyi tonun Pearson korelasyonu (0..1)
    double secondBest = 0;   // ikinci en iyi aday (göreli ton dahil değil)
    double tuningCents = 0;  // A=440'tan sapma, -50..+50
    double flatness = 0;     // spektral düzlük (gürültü/konuşma göstergesi)
};

struct Analysis {
    KeyResult key;
    int hop = 2048;
    std::vector<std::array<float, 12>> chroma; // kare başına
};

// mono, herhangi bir örnekleme frekansı
Analysis analyze(const float* x, int n, double sampleRate);

// Kaydın, tespit edilen tona en çok uyan (ve sessiz olmayan) bölümünün başlangıcı
int bestWindow(const float* x, int n, const Analysis& a, int windowSamples);

// Tespit edilen tonun nota kümesini hedef scale'e taşıyan en küçük kaydırma (yarım ton, -6..+5)
// overlapOut: 7 notadan kaçının örtüştüğü
int transposeShift(int detRoot, bool detMinor, int tgtRoot, int tgtMode, int* overlapOut = nullptr);

// Hedef scale notalarına düşen chroma enerjisinin oranı (0..1) — doğrulama için
double scaleFit(const Analysis& a, int tgtRoot, int tgtMode);

} // namespace gk
