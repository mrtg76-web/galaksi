// Tek vuruş tespiti testleri (JUCE'siz). Çalıştır: ctest --test-dir build
#include "Adg.h"
#include "Percussion.h"

#include <fstream>
#include <string>

#include <cmath>
#include <cstdlib>
#include <cstdio>
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
static std::mt19937 rng(7);

// gürültü + tonal karışımı bir vuruş: anlık atak, üstel sönüm
static void addHit(std::vector<float>& x, double at, double tau, float amp, double freq = 180)
{
    std::normal_distribution<float> nd(0, 1);
    const int s0 = (int) (at * SR);
    const int n = (int) (tau * 8 * SR);
    if ((int) x.size() < s0 + n) x.resize((size_t) (s0 + n), 0.f);
    for (int i = 0; i < n; ++i) {
        const double t = i / SR;
        const double env = std::exp(-t / tau);
        x[(size_t) (s0 + i)] += (float) (amp * env * (0.5 * nd(rng) * std::exp(-t / (tau * 0.3)) + std::sin(2 * M_PI * freq * t)));
    }
}

static gk::Pcm pcm(std::vector<float> x, double totalSec)
{
    x.resize((size_t) (totalSec * SR), 0.f);
    gk::Pcm p;
    p.sampleRate = SR;
    p.channels = 2;
    p.data = {x, x};
    return p;
}

int main()
{
    gk::PercSettings st;

    { // 1. sessizlikten sonra tek vuruş -> kabul, baş kırpılır
        std::vector<float> x;
        addHit(x, 0.30, 0.06, 0.6);
        auto r = gk::processPercussion(pcm(x, 2.0), st);
        CHECK(r.ok, "tek vuruş reddedildi: %s", r.reason.c_str());
        CHECK(r.attackMs < 20, "atak %.1f ms", r.attackMs);
        CHECK(r.durationSec > 0.15 && r.durationSec < 1.0, "süre %.2f", r.durationSec);
        CHECK(r.audio.channels == 2, "kanal %d", r.audio.channels);
        float pk = 0;
        for (auto& c : r.audio.data) for (float v : c) pk = std::max(pk, std::abs(v));
        CHECK(std::abs(pk - 0.944f) < 0.01f, "normalize %.3f", pk);
        // ilk 5 ms içinde ses olmalı (baştaki sessizlik gitmiş)
        float early = 0;
        for (int i = 0; i < (int) (0.005 * SR); ++i) early = std::max(early, std::abs(r.audio.data[0][(size_t) i]));
        CHECK(early > 0.2f, "baş kırpılmamış %.3f", early);
        std::printf("tek vuruş: atak %.1f ms, süre %.2f s\n", r.attackMs, r.durationSec);
    }
    { // 2. iki vuruş, ilki hızlı sönüyor -> ilk vuruş kesilip kullanılır
        std::vector<float> x;
        addHit(x, 0.10, 0.025, 0.6);
        addHit(x, 0.60, 0.025, 0.6);
        auto r = gk::processPercussion(pcm(x, 1.5), st);
        CHECK(r.ok, "kesilebilir çift vuruş reddedildi: %s", r.reason.c_str());
        CHECK(r.cutBeforeSecondHit, "ikinci vuruştan önce kesilmedi");
        CHECK(r.durationSec < 0.52, "süre %.2f", r.durationSec);
    }
    { // 3. sık vuruşlar, sönmeden üst üste -> ritim, reddedilir
        std::vector<float> x;
        for (int k = 0; k < 6; ++k) addHit(x, 0.05 + k * 0.18, 0.3, 0.5, 120 + 40 * k);
        auto r = gk::processPercussion(pcm(x, 2.0), st);
        CHECK(!r.ok, "ritim kabul edildi");
        std::printf("ritim -> %s\n", r.reason.c_str());
    }
    { // 4. yavaş atak (pad) -> reddedilir
        std::vector<float> x((size_t) (2 * SR));
        for (size_t i = 0; i < x.size(); ++i) {
            const double t = i / SR;
            x[i] = (float) (0.5 * std::min(1.0, t / 0.6) * std::exp(-std::max(0.0, t - 0.6) * 3) * std::sin(2 * M_PI * 220 * t));
        }
        auto r = gk::processPercussion(pcm(x, 2.0), st);
        CHECK(!r.ok, "pad kabul edildi");
        std::printf("pad -> %s\n", r.reason.c_str());
    }
    { // 5. anlık başlayan ama sönmeyen ses -> reddedilir
        std::vector<float> x((size_t) (2 * SR));
        std::normal_distribution<float> nd(0, 0.2f);
        for (auto& v : x) v = nd(rng);
        auto r = gk::processPercussion(pcm(x, 2.0), st);
        CHECK(!r.ok, "sürekli gürültü kabul edildi");
        std::printf("sürekli -> %s\n", r.reason.c_str());
    }
    { // 6. uzun gong: maks uzunlukta kesilir, sonu fade'li
        std::vector<float> x;
        addHit(x, 0.02, 1.2, 0.5, 300);
        auto r = gk::processPercussion(pcm(x, 4.0), st);
        CHECK(r.ok, "gong reddedildi: %s", r.reason.c_str());
        CHECK(std::abs(r.durationSec - st.maxLengthSec) < 0.02, "süre %.2f", r.durationSec);
        if (r.ok) {
            const auto& c = r.audio.data[0];
            CHECK(std::abs(c.back()) < 1e-3f, "son örnek %.4f", c.back());
        }
    }
    { // 7. sessizlik
        std::vector<float> x((size_t) SR, 0.0005f);
        auto r = gk::processPercussion(pcm(x, 1.0), st);
        CHECK(!r.ok, "sessizlik kabul edildi");
    }

    { // 8. Drum Rack .adg XML'i
        std::vector<gk::AdgPad> pads(2);
        pads[0].midiNote = 36; pads[0].name = "darbuka <dum> & tek"; pads[0].fileName = "01_darbuka.wav";
        pads[0].absoluteDir = "/Users/mert/Music/Galaksi"; pads[0].frames = 22050;
        pads[1].midiNote = 37; pads[1].name = "kapı"; pads[1].fileName = "02_kapi.wav"; pads[1].frames = 1000;
        const std::string x = gk::drumRackXml(pads);
        CHECK(x.find("<ReceivingNote Value=\"92\" />") != std::string::npos, "C1 notası yok");
        CHECK(x.find("<ReceivingNote Value=\"91\" />") != std::string::npos, "C#1 notası yok");
        CHECK(x.find("darbuka &lt;dum&gt; &amp; tek") != std::string::npos, "kaçış hatalı");
        CHECK(x.find("<SampleEnd Value=\"22049\" />") != std::string::npos, "sample sonu yok");
        CHECK(x.find("Dir=\"Galaksi\"") != std::string::npos, "yol ipucu yok");
        if (const char* out = std::getenv("GK_ADG_OUT")) { std::ofstream f(out); f << x; }
    }

    if (failures) std::printf("\n%d test BAŞARISIZ\n", failures);
    else std::printf("\nTüm perküsyon testleri geçti\n");
    return failures ? 1 : 0;
}
