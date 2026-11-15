#include "Analysis.h"

#include <algorithm>
#include <bitset>
#include <cmath>
#include <complex>
#include <numeric>

namespace gk {

namespace {

const char* kNotes[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

const std::array<int, 7> kModes[NumModes] = {
    {0, 2, 4, 5, 7, 9, 11}, // major
    {0, 2, 3, 5, 7, 8, 10}, // minor
    {0, 2, 3, 5, 7, 9, 10}, // dorian
    {0, 1, 3, 5, 7, 8, 10}, // phrygian
    {0, 2, 4, 6, 7, 9, 11}, // lydian
    {0, 2, 4, 5, 7, 9, 10}, // mixolydian
    {0, 1, 3, 5, 6, 8, 10}, // locrian
    {0, 2, 3, 5, 7, 8, 11}, // harmonic minor
};

// Krumhansl-Kessler ton profilleri
const double kMajor[12] = {6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88};
const double kMinor[12] = {6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17};

constexpr double kPi = 3.14159265358979323846;

void fft(std::vector<std::complex<float>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2 * kPi / (double) len;
        const std::complex<float> wl((float) std::cos(ang), (float) std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.f, 0.f);
            for (size_t k = 0; k < len / 2; ++k) {
                auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

double pearson(const double* a, const double* b, int n)
{
    double ma = 0, mb = 0;
    for (int i = 0; i < n; ++i) { ma += a[i]; mb += b[i]; }
    ma /= n; mb /= n;
    double num = 0, da = 0, db = 0;
    for (int i = 0; i < n; ++i) {
        num += (a[i] - ma) * (b[i] - mb);
        da += (a[i] - ma) * (a[i] - ma);
        db += (b[i] - mb) * (b[i] - mb);
    }
    return num / (std::sqrt(da * db) + 1e-12);
}

double profileCorr(const double* chroma12, int root, bool minor)
{
    double p[12];
    for (int i = 0; i < 12; ++i) p[(i + root) % 12] = (minor ? kMinor : kMajor)[i];
    return pearson(chroma12, p, 12);
}

struct Spectrogram {
    int nFft = 8192, hop = 2048, frames = 0, bins = 0;
    double binHz = 0;
    std::vector<float> mag; // frames * bins
    float at(int f, int b) const { return mag[(size_t) f * bins + b]; }
};

Spectrogram computeSpectrogram(const float* x, int n, double sr)
{
    Spectrogram s;
    // ~186 ms pencere: 44.1/48 kHz'de 8192
    s.nFft = sr > 64000 ? 16384 : (sr < 30000 ? 4096 : 8192);
    s.hop = s.nFft / 4;
    s.bins = s.nFft / 2 + 1;
    s.binHz = sr / s.nFft;
    s.frames = n >= s.nFft ? 1 + (n - s.nFft) / s.hop : 1;
    s.mag.assign((size_t) s.frames * s.bins, 0.f);

    std::vector<float> win(s.nFft);
    for (int i = 0; i < s.nFft; ++i) win[i] = (float) (0.5 - 0.5 * std::cos(2 * kPi * i / (s.nFft - 1)));

    std::vector<std::complex<float>> buf(s.nFft);
    for (int f = 0; f < s.frames; ++f) {
        const int start = f * s.hop;
        for (int i = 0; i < s.nFft; ++i) {
            const int idx = start + i;
            buf[i] = {idx < n ? x[idx] * win[i] : 0.f, 0.f};
        }
        fft(buf);
        float* row = &s.mag[(size_t) f * s.bins];
        for (int b = 0; b < s.bins; ++b) row[b] = std::abs(buf[b]);
    }
    return s;
}

double estimateTuning(const Spectrogram& s)
{
    const int b0 = std::max(1, (int) std::ceil(100.0 / s.binHz));
    const int b1 = std::min(s.bins - 2, (int) std::floor(2000.0 / s.binHz));
    if (b1 <= b0 + 2) return 0;

    std::vector<float> all;
    all.reserve((size_t) s.frames * (b1 - b0));
    for (int f = 0; f < s.frames; ++f)
        for (int b = b0; b < b1; ++b) all.push_back(s.at(f, b));
    if (all.empty()) return 0;
    auto nth = all.begin() + (long) (all.size() * 0.95);
    std::nth_element(all.begin(), nth, all.end());
    const float thr = *nth;

    double sx = 0, sy = 0;
    int count = 0;
    for (int f = 0; f < s.frames; ++f) {
        for (int b = b0 + 1; b < b1; ++b) {
            const float a = s.at(f, b - 1), m = s.at(f, b), c = s.at(f, b + 1);
            if (m > a && m > c && m > thr) {
                const float denom = a - 2 * m + c;
                const double delta = std::abs(denom) > 1e-12f ? 0.5 * (a - c) / denom : 0.0;
                const double hz = (b + delta) * s.binHz;
                const double cents = 1200.0 * std::log2(hz / 440.0);
                double frac = std::fmod(cents, 100.0);
                if (frac < 0) frac += 100.0;
                const double ang = 2 * kPi * frac / 100.0;
                sx += m * std::cos(ang);
                sy += m * std::sin(ang);
                ++count;
            }
        }
    }
    if (count < 20) return 0;
    return std::atan2(sy, sx) / (2 * kPi) * 100.0; // -50..50
}

} // namespace

const char* noteName(int pc) { return kNotes[((pc % 12) + 12) % 12]; }

const char* modeName(int mode)
{
    static const char* n[NumModes] = {"major", "minor", "dorian", "phrygian", "lydian", "mixolydian", "locrian", "harmonic minor"};
    return n[std::clamp(mode, 0, NumModes - 1)];
}

const char* modeNameTr(int mode)
{
    static const char* n[NumModes] = {"Majör", "Minör", "Dorian", "Frigyen", "Lidyen", "Miksolidyen", "Lokriyen", "Armonik minör"};
    return n[std::clamp(mode, 0, NumModes - 1)];
}

std::array<int, 7> modeSteps(int mode) { return kModes[std::clamp(mode, 0, NumModes - 1)]; }

unsigned scaleMask(int root, int mode)
{
    unsigned m = 0;
    for (int d : modeSteps(mode)) m |= 1u << ((root + d) % 12);
    return m;
}

std::string keyName(int root, bool minor) { return std::string(noteName(root)) + (minor ? " minor" : " major"); }

Analysis analyze(const float* x, int n, double sr)
{
    Analysis out;
    if (n <= 0 || sr <= 0) return out;
    const Spectrogram s = computeSpectrogram(x, n, sr);
    out.hop = s.hop;
    out.key.tuningCents = estimateTuning(s);

    // chroma (HPCP benzeri): her karede spektral tepe noktaları bulunur, parabolik
    // interpolasyonla hassas frekansları hesaplanır ve en yakın notaya cos² ağırlıkla eklenir.
    // 65 Hz – 3.5 kHz, log sıkıştırma, akort düzeltmeli.
    float maxMag = 1e-12f;
    for (float v : s.mag) maxMag = std::max(maxMag, v);
    const int b0 = std::max(2, (int) std::ceil(65.0 / s.binHz));
    const int b1 = std::min(s.bins - 2, (int) std::floor(3500.0 / s.binHz));
    out.chroma.assign(s.frames, {});
    double total[12] = {};
    double flatSum = 0;
    for (int f = 0; f < s.frames; ++f) {
        auto& c = out.chroma[f];
        c.fill(0.f);
        float frameMax = 0;
        for (int b = b0; b <= b1; ++b) frameMax = std::max(frameMax, s.at(f, b));
        const float floorMag = std::max(frameMax * 0.003f, maxMag * 1e-4f);
        for (int b = b0; b <= b1; ++b) {
            const float a = s.at(f, b - 1), m = s.at(f, b), cc = s.at(f, b + 1);
            if (!(m > a && m >= cc && m > floorMag)) continue;
            const float denom = a - 2 * m + cc;
            const double delta = std::abs(denom) > 1e-12f ? 0.5 * (a - cc) / denom : 0.0;
            const double hz = (b + delta) * s.binHz;
            const double peakMag = m - 0.25 * (a - cc) * delta;
            const double midi = 69 + 12 * std::log2(hz / 440.0) - out.key.tuningCents / 100.0;
            const double nearest = std::round(midi);
            const double d = midi - nearest; // -0.5..0.5
            const double w = std::cos(kPi * d);
            const int pc = (((int) nearest) % 12 + 12) % 12;
            c[pc] += (float) (w * w * std::log1p(100.0 * peakMag / maxMag));
        }
        for (int k = 0; k < 12; ++k) total[k] += c[k];

        // spektral düzlük
        double lg = 0, ar = 0;
        int cnt = 0;
        for (int b = 10; b < s.bins; ++b) {
            const double v = s.at(f, b) + 1e-10;
            lg += std::log(v);
            ar += v;
            ++cnt;
        }
        flatSum += std::exp(lg / cnt) / (ar / cnt);
    }
    out.key.flatness = flatSum / std::max(1, s.frames);

    double best = -2, second = -2;
    int bestRoot = 0;
    bool bestMinor = false;
    for (int mi = 0; mi < 2; ++mi)
        for (int r = 0; r < 12; ++r) {
            const double c = profileCorr(total, r, mi == 1);
            if (c > best) { second = best; best = c; bestRoot = r; bestMinor = mi == 1; }
            else if (c > second) second = c;
        }
    out.key.root = bestRoot;
    out.key.minor = bestMinor;
    out.key.confidence = best;
    out.key.secondBest = second;
    return out;
}

int bestWindow(const float* x, int n, const Analysis& a, int win)
{
    if (n <= win || a.chroma.empty()) return 0;
    const int framesPerWin = std::max(1, win / a.hop);
    const int nFrames = (int) a.chroma.size();
    double bestScore = -1e9;
    int bestStart = 0;
    for (int i = 0; i < std::max(1, nFrames - framesPerWin); i += 1) {
        double v[12] = {};
        for (int f = i; f < std::min(nFrames, i + framesPerWin); ++f)
            for (int k = 0; k < 12; ++k) v[k] += a.chroma[f][k];
        double s = profileCorr(v, a.key.root, a.key.minor);
        const int start = std::min(i * a.hop, n - win);
        double e = 0;
        for (int j = start; j < start + win; ++j) e += (double) x[j] * x[j];
        const double rms = std::sqrt(e / win + 1e-12);
        s *= std::min(1.0, rms / 0.02);
        if (s > bestScore) { bestScore = s; bestStart = start; }
    }
    return std::clamp(bestStart, 0, n - win);
}

int transposeShift(int detRoot, bool detMinor, int tgtRoot, int tgtMode, int* overlapOut)
{
    const unsigned det = scaleMask(detRoot, detMinor ? Minor : Major);
    const unsigned tgt = scaleMask(tgtRoot, tgtMode);
    int bestShift = 0, bestOverlap = -1;
    for (int s : {0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5, -6}) { // küçük kaydırmalar önce
        unsigned moved = 0;
        for (int p = 0; p < 12; ++p)
            if (det & (1u << p)) moved |= 1u << (((p + s) % 12 + 12) % 12);
        const int ov = (int) std::bitset<12>(moved & tgt).count();
        if (ov > bestOverlap) { bestOverlap = ov; bestShift = s; }
    }
    if (overlapOut) *overlapOut = bestOverlap;
    return bestShift;
}

double scaleFit(const Analysis& a, int tgtRoot, int tgtMode)
{
    const unsigned m = scaleMask(tgtRoot, tgtMode);
    double in = 0, all = 0;
    for (auto& c : a.chroma)
        for (int k = 0; k < 12; ++k) {
            all += c[k];
            if (m & (1u << k)) in += c[k];
        }
    return all > 0 ? in / all : 0;
}

} // namespace gk
