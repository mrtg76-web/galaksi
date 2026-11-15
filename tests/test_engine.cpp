// Ağ + yuva motoru entegrasyon testi. Yerel sahte radyo sunucusu gerektirir (tests/fake_radio.py).
// CI'da çalışmaz; geliştirme sırasında elle derlenir.
#include "SlotEngine.h"
#include "Analysis.h"

#include <cstdio>

using namespace gk;

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s ", __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

int main()
{
    const juce::String base = "http://127.0.0.1:8770";

    // 1. ham yakalama: ICY yanıtı, yönlendirme, HTML, ölü bağlantı
    for (auto path : {"/icy", "/redir", "/plain"}) {
        juce::MemoryBlock mb;
        juce::String err;
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        bool ok = captureStream(base + path, 6.0, 128, mb, nullptr, err);
        const double el = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        CHECK(ok, "%s: %s", path, err.toRawUTF8());
        auto pcm = decodeMp3((const uint8_t*) mb.getData(), mb.getSize());
        std::printf("%s: %zu bayt, %.1f s sürdü, %.1f s ses\n", path, mb.getSize(), el, pcm.length() / std::max(1.0, pcm.sampleRate));
        CHECK(pcm.length() > pcm.sampleRate * 5.5, "%s ses kısa", path);
        CHECK(el < 9, "%s çok uzun sürdü", path);
    }
    {
        juce::MemoryBlock mb;
        juce::String err;
        CHECK(!captureStream(base + "/html", 4.0, 128, mb, nullptr, err), "html kabul edildi");
        std::printf("/html -> %s\n", err.toRawUTF8());
        CHECK(!captureStream(base + "/dead", 4.0, 128, mb, nullptr, err), "ölü kabul edildi");
        std::printf("/dead -> %s\n", err.toRawUTF8());
        CHECK(!captureStream("http://127.0.0.1:1/x", 4.0, 128, mb, nullptr, err), "kapalı port kabul edildi");
        std::printf("kapalı port -> %s\n", err.toRawUTF8());
    }

    // 2. motor: 3 yuva, biri kilitli; bozuk istasyonlar atlanmalı
    juce::File folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("galaksi_test");
    folder.deleteRecursively();
    {
        SlotEngine eng;
        EngineSettings es;
        es.tags = {"test"};
        es.folder = folder;
        es.recordSeconds = 6;
        es.core.lengthSec = 2.0;
        eng.setSettings(es);
        juce::Array<Station> list;
        for (auto path : {"/dead", "/html", "/icy", "/redir", "/plain", "/icy?2", "/plain?2"}) {
            Station s;
            s.name = juce::String("Test ") + path;
            s.url = base + path;
            s.bitrate = 128;
            s.tag = "test";
            list.add(s);
        }
        eng.addStationsForTesting("test", list);
        eng.setLocked(2, true);
        eng.refreshAllUnlocked(3);
        for (int i = 0; i < 300 && eng.busyCount() > 0; ++i) juce::Thread::sleep(100);
        for (int i = 0; i < 3; ++i) {
            auto s = eng.getSlot(i);
            std::printf("yuva %d: durum %d  %s  %s -> %+d  uyum %.2f  dosya %s\n", i + 1, (int) s.state, s.station.toRawUTF8(),
                        s.origKey.toRawUTF8(), s.shift, s.fit, s.file.getFileName().toRawUTF8());
        }
        auto a = eng.getSlot(0), b = eng.getSlot(1), c = eng.getSlot(2);
        CHECK(a.state == SlotInfo::State::Ready && b.state == SlotInfo::State::Ready, "yuvalar dolmadı");
        CHECK(c.state == SlotInfo::State::Empty, "kilitli yuva değişti");
        CHECK(a.file.existsAsFile() && b.file.existsAsFile(), "WAV yok");
        CHECK(a.shift == -1, "kaydırma %d", a.shift);
        CHECK(a.fit > 0.8, "uyum %.2f", a.fit);
        if (a.audio) CHECK(std::abs(a.audio->getNumSamples() - 2.0 * a.sampleRate) < 4, "uzunluk %d", a.audio->getNumSamples());

        // WAV'ı geri oku, tonu kontrol et
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(a.file));
        CHECK(rd != nullptr && rd->bitsPerSample == 24, "wav okunamadı");
        if (rd) {
            juce::AudioBuffer<float> buf((int) rd->numChannels, (int) rd->lengthInSamples);
            rd->read(&buf, 0, buf.getNumSamples(), 0, true, true);
            std::vector<float> m((size_t) buf.getNumSamples());
            for (int i = 0; i < buf.getNumSamples(); ++i) m[(size_t) i] = 0.5f * (buf.getSample(0, i) + buf.getSample(buf.getNumChannels() - 1, i));
            auto an = analyze(m.data(), (int) m.size(), rd->sampleRate);
            CHECK(scaleMask(an.key.root, an.key.minor) == scaleMask(0, Minor), "wav tonu %s", keyName(an.key.root, an.key.minor).c_str());
        }

        // 3. durum kaydet/yükle
        auto state = eng.saveState();
        SlotEngine eng2;
        eng2.loadState(state);
        auto a2 = eng2.getSlot(0);
        CHECK(a2.state == SlotInfo::State::Ready && a2.audio && a2.origKey == a.origKey, "durum geri yüklenmedi");
        CHECK(eng2.getSlot(2).locked, "kilit geri yüklenmedi");
        CHECK(eng2.getSettings().folder == folder, "klasör geri yüklenmedi");

        // 4. arama sırasında yenile -> eski iş iptal edilir, yeni sonuç gelir
        eng.setLocked(2, false);
        eng.refresh(2);
        juce::Thread::sleep(500);
        eng.refresh(2);
        for (int i = 0; i < 300 && eng.busyCount() > 0; ++i) juce::Thread::sleep(100);
        CHECK(eng.getSlot(2).state == SlotInfo::State::Ready, "yeniden yenileme başarısız");
        int wavs = folder.getNumberOfChildFiles(juce::File::findFiles, "*.wav");
        std::printf("klasördeki wav: %d\n", wavs);
        CHECK(wavs == 3, "beklenmeyen wav sayısı %d (iptal edilen iş dosya yazmamalı)", wavs);

        // 5. MIDI pad modu: WAV kullanıcı klasörüne değil önbelleğe yazılır
        juce::File cache = folder.getSiblingFile("galaksi_test_cache");
        cache.deleteRecursively();
        auto es2 = eng.getSettings();
        es2.exportToFolder = false;
        es2.cacheFolder = cache;
        eng.setSettings(es2);
        eng.refresh(0);
        for (int i = 0; i < 300 && eng.busyCount() > 0; ++i) juce::Thread::sleep(100);
        CHECK(eng.getSlot(0).state == SlotInfo::State::Ready, "MIDI modunda yuva dolmadı");
        CHECK(folder.getNumberOfChildFiles(juce::File::findFiles, "*.wav") == 3, "MIDI modunda klasöre yazıldı");
        CHECK(cache.getNumberOfChildFiles(juce::File::findFiles, "*.wav") == 1, "önbelleğe yazılmadı");
        CHECK(eng.getSlot(0).file.isAChildOf(cache), "dosya önbellekte değil");
        cache.deleteRecursively();
    }
    folder.deleteRecursively();
    std::printf(failures ? "\n%d BAŞARISIZ\n" : "\nTüm entegrasyon testleri geçti\n", failures);
    return failures ? 1 : 0;
}
