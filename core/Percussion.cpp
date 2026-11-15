#include "Percussion.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace gk {

namespace {

double toDb(double a) { return 20.0 * std::log10(std::max(a, 1e-9)); }

// Tepe zarfı: her 'hop' örneklik karenin en büyük mutlak değeri
std::vector<float> peakEnvelope(const std::vector<float>& x, int hop)
{
    const int n = (int) x.size();
    std::vector<float> env((size_t) ((n + hop - 1) / hop), 0.f);
    for (int i = 0; i < n; ++i) {
        float& e = env[(size_t) (i / hop)];
        e = std::max(e, std::abs(x[(size_t) i]));
    }
    return env;
}

} // namespace

PercResult processPercussion(const Pcm& rec, const PercSettings& s)
{
    PercResult r;
    const int n = rec.length();
    const double sr = rec.sampleRate;
    if (n == 0 || sr <= 0) { r.reason = "ses çözülemedi"; return r; }

    std::vector<float> mono = rec.mono();
    // DC kaydını temizle
    double mean = 0;
    for (float v : mono) mean += v;
    mean /= n;
    if (std::abs(mean) > 1e-4)
        for (auto& v : mono) v -= (float) mean;

    const int hop = std::max(1, (int) std::lround(sr * 0.0025)); // 2.5 ms
    const double hopSec = hop / sr;
    const std::vector<float> env = peakEnvelope(mono, hop);
    const int F = (int) env.size();

    float peak = 0;
    for (float e : env) peak = std::max(peak, e);
    if (peak < 0.01f) { r.reason = "sessiz"; return r; }

    // --- belirgin vuruşlar: son 30 ms'nin en düşüğüne göre 12 dB'den fazla hızlı yükseliş
    const int look = std::max(2, (int) std::lround(0.030 / hopSec));
    const int refractory = std::max(1, (int) std::lround(0.080 / hopSec));
    const float onsetFloor = peak * 0.2f; // tepeden en fazla 14 dB aşağıda
    std::vector<int> onsets;
    int last = -refractory;
    for (int i = 0; i < F; ++i) {
        if (env[(size_t) i] < onsetFloor || i - last < refractory) continue;
        float lo = env[(size_t) i];
        for (int j = std::max(0, i - look); j < i; ++j) lo = std::min(lo, env[(size_t) j]);
        if (toDb(env[(size_t) i]) - toDb(lo) >= 12.0 || (onsets.empty() && i < look)) {
            onsets.push_back(i);
            last = i;
        }
    }
    r.hits = (int) onsets.size();

    // --- başlangıç: tepeden 40 dB aşağıyı geçen ilk kare
    const float startThr = peak * 0.01f;
    int startF = 0;
    while (startF < F && env[(size_t) startF] < startThr) ++startF;
    if (startF >= F) { r.reason = "sessiz"; return r; }

    // İlk belirgin vuruş başlangıçtan uzaksa (öncesinde uğultu var) başlangıç vuruşa çekilir
    int firstHit = onsets.empty() ? startF : onsets.front();
    if ((firstHit - startF) * hopSec > 0.15) startF = std::max(0, firstHit - (int) std::lround(0.01 / hopSec));

    // --- bölge sonu: ikinci vuruştan önce, en fazla maxLength
    int endF = std::min(F, startF + (int) std::lround(s.maxLengthSec / hopSec));
    int secondHit = -1;
    for (int o : onsets)
        if (o > firstHit + refractory / 2) { secondHit = o; break; }
    if (secondHit >= 0 && secondHit < endF) {
        // ilk vuruşun bu bölgedeki tepesi
        float p1 = 0;
        for (int i = startF; i < secondHit; ++i) p1 = std::max(p1, env[(size_t) i]);
        const int cutF = std::max(startF + 1, secondHit - (int) std::lround(0.005 / hopSec));
        float atCut = env[(size_t) std::max(startF, cutF - 2)];
        const double regionSec = (cutF - startF) * hopSec;
        // ilk vuruş ikinci gelmeden yeterince söndüyse kesip kullan, sönmediyse ritim/loop say
        if (regionSec >= std::max(0.08, s.minLengthSec) && toDb(atCut) - toDb(p1) <= -18.0) {
            endF = cutF;
            r.cutBeforeSecondHit = true;
        } else {
            r.reason = "birden fazla vuruş";
            return r;
        }
    }

    // bölgedeki tepe ve atak süresi
    int peakF = startF;
    float rp = 0;
    for (int i = startF; i < endF; ++i)
        if (env[(size_t) i] > rp) { rp = env[(size_t) i]; peakF = i; }
    r.attackMs = (peakF - startF) * hopSec * 1000.0;
    if (r.attackMs > s.maxAttackMs) { r.reason = "yavaş atak (vuruş değil)"; return r; }

    // kuyruk: tepeye göre tailDb altına son kez inen nokta
    const float tailThr = rp * (float) std::pow(10.0, s.tailDb / 20.0);
    int lastLoud = peakF;
    for (int i = peakF; i < endF; ++i)
        if (env[(size_t) i] >= tailThr) lastLoud = i;
    const bool truncated = lastLoud >= endF - 2 && !r.cutBeforeSecondHit;
    endF = std::min(endF, lastLoud + 1 + (int) std::lround(0.010 / hopSec));
    endF = std::min(endF, F);

    const double regionSec = (endF - startF) * hopSec;
    if (regionSec < s.minLengthSec) { r.reason = "çok kısa"; return r; }

    // sönme kontrolü: son çeyreğin gücü ilk çeyreğe yakınsa ses sönmüyor (drone, uğultu, müzik)
    if (regionSec > 0.35) {
        const int q = std::max(1, (endF - startF) / 4);
        double e1 = 0, e4 = 0;
        for (int i = startF; i < startF + q; ++i) e1 += (double) env[(size_t) i] * env[(size_t) i];
        for (int i = endF - q; i < endF; ++i) e4 += (double) env[(size_t) i] * env[(size_t) i];
        if (e4 >= 0.7 * e1) { r.reason = "sönmeyen sürekli ses"; return r; }
    }

    // --- örnek bazında kesim + fade + normalize
    const int s0 = std::max(0, startF * hop - (int) std::lround(0.001 * sr));
    const int s1 = std::min(n, endF * hop);
    const int L = s1 - s0;
    r.audio.sampleRate = sr;
    r.audio.channels = rec.channels;
    for (auto& ch : rec.data) {
        std::vector<float> seg(ch.begin() + s0, ch.begin() + s1);
        if (std::abs(mean) > 1e-4)
            for (auto& v : seg) v -= (float) mean;
        r.audio.data.push_back(std::move(seg));
    }

    float apk = 0;
    for (auto& ch : r.audio.data)
        for (float v : ch) apk = std::max(apk, std::abs(v));
    const float g = apk > 1e-6f ? 0.944f / apk : 1.f; // -0.5 dBFS
    const int fadeIn = std::min(L / 4, (int) std::lround(0.001 * sr));
    int fadeOut = (int) std::lround((truncated ? 0.12 : 0.012) * sr);
    fadeOut = std::min(fadeOut, L / (truncated ? 3 : 4));
    for (auto& ch : r.audio.data) {
        for (auto& v : ch) v *= g;
        for (int i = 0; i < fadeIn; ++i) ch[(size_t) i] *= (float) i / (float) fadeIn;
        for (int i = 0; i < fadeOut; ++i) {
            const float w = (float) i / (float) fadeOut;
            ch[(size_t) (L - 1 - i)] *= w * w;
        }
    }
    r.durationSec = L / sr;
    r.ok = true;
    return r;
}

} // namespace gk
