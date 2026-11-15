// Galaksi core: tek vuruşluk perküsif ses tespiti ve hazırlama (JUCE'den bağımsız)
#pragma once
#include <string>

#include "Audio.h"

namespace gk {

struct PercSettings {
    double maxLengthSec = 1.5;   // en uzun sample (kuyruk bu noktada fade ile kesilir)
    double minLengthSec = 0.04;  // bundan kısa sesler reddedilir
    double maxAttackMs = 60;     // başlangıçtan tepeye en fazla süre: daha yavaşsa vuruş sayılmaz
    double tailDb = -50;         // kuyruk bu seviyenin altına inince ses biter (tepeye göre)
};

struct PercResult {
    bool ok = false;
    std::string reason;          // ok değilse neden (Türkçe)
    double attackMs = 0;         // başlangıçtan tepeye
    double durationSec = 0;      // hazır sample'ın süresi
    int hits = 0;                // kayıtta bulunan belirgin vuruş sayısı
    bool cutBeforeSecondHit = false;
    Pcm audio;                   // hazır sample: başı kırpılmış, normalize edilmiş, kuyruğu fade'li
};

// Bir kaydın tek vuruşluk perküsif bir ses olup olmadığına karar verir; uygunsa hazırlar.
// Reddedilenler: sessizlik, yavaş atak (pad/drone), birden fazla belirgin vuruş (loop/ritim),
// sönmeyen sürekli sesler.
PercResult processPercussion(const Pcm& recording, const PercSettings& s);

} // namespace gk
