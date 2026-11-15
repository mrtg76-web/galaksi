// Galaksi core testleri (JUCE'siz). Çalıştır: ctest --test-dir build
#include "Analysis.h"
#include "Audio.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <random>
#include <vector>

static int failures = 0;
#define CHECK(cond, ...)                                                 \
    do {                                                                 \
        if (!(cond)) {                                                   \
            ++failures;                                                  \
            std::printf("FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond);  \
            std::printf(__VA_ARGS__);                                    \
            std::printf("\n");                                           \
        }                                                                \
    } while (0)

static const double SR = 44100;
static const double PI = 3.14159265358979323846;

static void addNote(std::vector<float>& x, int start, double dur, int midi, double detuneCents, float amp)
{
    const double f = 440.0 * std::pow(2.0, (midi - 69 + detuneCents / 100.0) / 12.0);
    const int n = (int) (dur * SR);
    if ((int) x.size() < start + n) x.resize(start + n, 0.f);
    for (int i = 0; i < n; ++i) {
        const double t = i / SR;
        double v = 0;
        for (int k = 1; k <= 5; ++k) v += std::sin(2 * PI * f * k * t) / k;
        x[start + i] += (float) (amp * v * std::exp(-t * 1.5));
    }
}

static std::vector<float> progression(const std::vector<std::vector<int>>& chords, double detune = 0, double dur = 2.0)
{
    std::vector<float> x;
    int pos = 0;
    for (auto& ch : chords) {
        for (int m : ch) addNote(x, pos, dur, m, detune, 0.2f);
        addNote(x, pos, dur, ch[0] - 12, detune, 0.12f);
        pos += (int) (dur * SR);
    }
    return x;
}

static gk::Pcm toPcm(const std::vector<float>& x, int channels = 2)
{
    gk::Pcm p;
    p.sampleRate = SR;
    p.channels = channels;
    for (int c = 0; c < channels; ++c) p.data.push_back(x);
    return p;
}

static const std::vector<std::vector<int>> C_MINOR = {{60, 63, 67}, {65, 68, 72}, {67, 71, 74}, {60, 63, 67}, {56, 60, 63}, {67, 70, 74}, {60, 63, 67}};
static const std::vector<std::vector<int>> E_MAJOR = {{64, 68, 71}, {69, 73, 76}, {71, 75, 78}, {64, 68, 71}, {61, 64, 68}, {71, 75, 78}, {64, 68, 71}};
static const std::vector<std::vector<int>> A_MINOR = {{57, 60, 64}, {62, 65, 69}, {64, 67, 71}, {57, 60, 64}, {53, 57, 60}, {64, 67, 71}, {57, 60, 64}};

static unsigned keyMask(int root, bool minor) { return gk::scaleMask(root, minor ? gk::Minor : gk::Major); }

int main()
{
    const unsigned cMinorSet = gk::scaleMask(0, gk::Minor);

    { // 1. C minör tespiti
        auto x = progression(C_MINOR);
        auto a = gk::analyze(x.data(), (int) x.size(), SR);
        CHECK(a.key.root == 0 && a.key.minor, "tespit: %s", gk::keyName(a.key.root, a.key.minor).c_str());
        CHECK(a.key.confidence > 0.7, "güven %.2f", a.key.confidence);
        CHECK(std::abs(a.key.tuningCents) < 5, "akort %.1f", a.key.tuningCents);
    }
    { // 2. E majör tespiti + kaydırma hesabı
        auto x = progression(E_MAJOR);
        auto a = gk::analyze(x.data(), (int) x.size(), SR);
        CHECK(a.key.root == 4 && !a.key.minor, "tespit: %s", gk::keyName(a.key.root, a.key.minor).c_str());
        int ov = 0;
        int s = gk::transposeShift(4, false, 0, gk::Minor, &ov);
        CHECK(s == -1 && ov == 7, "kaydırma %d örtüşme %d", s, ov);
    }
    { // 3. akort sapması (+30 cent)
        auto x = progression(A_MINOR, 30);
        auto a = gk::analyze(x.data(), (int) x.size(), SR);
        CHECK(std::abs(a.key.tuningCents - 30) < 6, "akort %.1f", a.key.tuningCents);
        CHECK(keyMask(a.key.root, a.key.minor) == keyMask(9, true) || a.key.root == 9 || a.key.root == 4,
              "tespit: %s", gk::keyName(a.key.root, a.key.minor).c_str());
    }
    { // 4. gürültü tonal sayılmamalı
        std::mt19937 rng(1);
        std::normal_distribution<float> nd(0, 0.2f);
        std::vector<float> x((size_t) (SR * 12));
        for (auto& v : x) v = nd(rng);
        auto a = gk::analyze(x.data(), (int) x.size(), SR);
        CHECK(a.key.confidence < 0.55, "gürültü güveni %.2f", a.key.confidence);
        gk::Settings st;
        auto r = gk::processCapture(toPcm(x), st);
        CHECK(!r.ok, "gürültü kabul edildi");
    }
    { // 5. her diatonik ton C minör kümesine 7/7 taşınabilmeli
        for (int mi = 0; mi < 2; ++mi)
            for (int r = 0; r < 12; ++r) {
                int ov = 0;
                int s = gk::transposeShift(r, mi == 1, 0, gk::Minor, &ov);
                unsigned moved = 0;
                for (int p = 0; p < 12; ++p)
                    if (keyMask(r, mi == 1) & (1u << p)) moved |= 1u << (((p + s) % 12 + 12) % 12);
                CHECK(ov == 7 && moved == cMinorSet && s >= -6 && s <= 5, "%s -> %d (%d)", gk::keyName(r, mi == 1).c_str(), s, ov);
            }
    }
    for (int mode = 0; mode < 2; ++mode) { // 6. tam işleme hattı (Kaliteli / Bant)
        auto x = progression(E_MAJOR, 18);
        gk::Settings st;
        st.lengthSec = 2.5;
        st.shiftMode = mode == 0 ? gk::ShiftMode::Quality : gk::ShiftMode::Tape;
        auto r = gk::processCapture(toPcm(x), st);
        CHECK(r.ok, "mod %d reddedildi: %s", mode, r.reason.c_str());
        if (r.ok) {
            CHECK(std::abs(r.audio.length() - (int) (2.5 * SR)) <= 2, "uzunluk %d", r.audio.length());
            CHECK(r.audio.channels == 2, "kanal %d", r.audio.channels);
            CHECK(r.shift == -1, "kaydırma %d", r.shift);
            auto m = r.audio.mono();
            auto a = gk::analyze(m.data(), (int) m.size(), SR);
            CHECK(keyMask(a.key.root, a.key.minor) == cMinorSet, "mod %d sonuç tonu %s", mode, gk::keyName(a.key.root, a.key.minor).c_str());
            CHECK(std::abs(a.key.tuningCents) < 8, "mod %d sonuç akort %.1f", mode, a.key.tuningCents);
            CHECK(r.fitAfter > 0.75, "mod %d scale uyumu %.2f", mode, r.fitAfter);
            std::printf("mod %d: %s -> %s, uyum %.2f -> %.2f\n", mode, gk::keyName(r.root, r.minor).c_str(),
                        gk::keyName(a.key.root, a.key.minor).c_str(), r.fitBefore, r.fitAfter);
        }
    }
    { // 7. maks kaydırma sınırı
        auto x = progression(E_MAJOR);
        gk::Settings st;
        st.targetRoot = 7; // G minör: E majör'den +/-? kaydırma gerekir
        st.maxShift = 0;
        auto r = gk::processCapture(toPcm(x), st);
        CHECK(!r.ok, "sınır uygulanmadı");
    }
    { // 8. ortasından kesilmiş MP3 akışını çözme
        std::ifstream f(std::string(GK_TEST_DATA) + "/cminor_64k.mp3", std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), {});
        CHECK(bytes.size() > 10000, "test dosyası yok");
        if (bytes.size() > 10000) {
            auto p = gk::decodeMp3(bytes.data() + 1337, bytes.size() - 1337); // kare ortasından başla
            CHECK(p.length() > SR * 5, "çözülen uzunluk %d", p.length());
            CHECK(p.sampleRate == 44100 && p.channels == 2, "format %g/%d", p.sampleRate, p.channels);
            auto m = p.mono();
            auto a = gk::analyze(m.data(), (int) m.size(), p.sampleRate);
            CHECK(a.key.root == 0 && a.key.minor, "mp3 tonu %s", gk::keyName(a.key.root, a.key.minor).c_str());
        }
        uint8_t junk[4096] = {};
        CHECK(gk::decodeMp3(junk, sizeof junk).length() == 0, "çöp veri çözüldü");
    }

    if (failures) std::printf("\n%d test BAŞARISIZ\n", failures);
    else std::printf("\nTüm testler geçti\n");
    return failures ? 1 : 0;
}
