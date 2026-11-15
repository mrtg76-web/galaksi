#include "SlotEngine.h"
#include "Adg.h"
#include "Analysis.h"
#include "Text.h"

namespace gk {

class SlotEngine::Job : public juce::ThreadPoolJob {
public:
    Job(SlotEngine& e, int s, int g) : juce::ThreadPoolJob("galaksi slot " + juce::String(s)), engine(e), slot(s), gen(g) {}
    JobStatus runJob() override
    {
        engine.runJob(slot, gen);
        return jobHasFinished;
    }
    SlotEngine& engine;
    const int slot, gen;
};

juce::File SlotEngine::defaultCacheFolder()
{
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    return base.getChildFile("Application Support/Galaksi/Cache");
#else
    return base.getChildFile("Galaksi/Cache");
#endif
}

SlotEngine::SlotEngine() : threads(6)
{
    settings.folder = juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Galaksi");
    settings.cacheFolder = defaultCacheFolder();
    settings.percTerms = juce::StringArray::fromTokens(defaultPercTerms(), ",", "");
    settings.percTerms.trim();
    settings.percExclude = juce::StringArray::fromTokens(defaultPercExclude(), ",", "");
    settings.percExclude.trim();
    for (auto& s : slots) s.status = TR("boş — yenile'ye bas");
}

SlotEngine::~SlotEngine()
{
    shuttingDown = true;
    threads.removeAllJobs(true, 15000);
}

void SlotEngine::setSettings(const EngineSettings& s)
{
    const juce::ScopedLock sl(lock);
    const bool tagsChanged = s.tags != settings.tags;
    const bool percChanged = s.percTerms != settings.percTerms || s.percExclude != settings.percExclude
                             || s.cc0Only != settings.cc0Only || s.apiKey != settings.apiKey
                             || s.perc.maxLengthSec != settings.perc.maxLengthSec;
    settings = s;
    if (tagsChanged) {
        const juce::ScopedLock pl(poolLock);
        pool.clear();
    }
    if (percChanged) {
        const juce::ScopedLock pl(poolLock);
        soundPool.clear();
        termCount.clear();
        termEmptyFetches.clear();
        exhaustedTerms.clear();
    }
}

EngineSettings SlotEngine::getSettings() const
{
    const juce::ScopedLock sl(lock);
    return settings;
}

void SlotEngine::refresh(int slot)
{
    if (slot < 0 || slot >= MaxSlots) return;
    int gen;
    {
        const juce::ScopedLock sl(lock);
        if (slots[slot].locked) return;
        gen = ++generations[slot];
        slots[slot].state = SlotInfo::State::Searching;
        slots[slot].status = TR("sırada…");
        slots[slot].attempts = 0;
    }
    publish(slot);
    threads.addJob(new Job(*this, slot, gen), true);
}

void SlotEngine::refreshAllUnlocked(int count)
{
    for (int i = 0; i < std::min(count, (int) MaxSlots); ++i) refresh(i);
}

void SlotEngine::stop(int slot)
{
    {
        const juce::ScopedLock sl(lock);
        ++generations[slot];
        auto& s = slots[slot];
        if (s.state == SlotInfo::State::Searching) {
            s.state = s.audio ? SlotInfo::State::Ready : SlotInfo::State::Empty;
            s.status = TR("durduruldu");
        }
    }
    publish(slot);
}

void SlotEngine::setLocked(int slot, bool locked)
{
    if (slot < 0 || slot >= MaxSlots) return;
    {
        const juce::ScopedLock sl(lock);
        slots[slot].locked = locked;
    }
    publish(slot);
}

SlotInfo SlotEngine::getSlot(int slot) const
{
    const juce::ScopedLock sl(lock);
    return slots[juce::jlimit(0, MaxSlots - 1, slot)];
}

SharedBuffer SlotEngine::getAudio(int slot, double& sr) const
{
    const juce::ScopedLock sl(lock);
    sr = slots[slot].sampleRate;
    return slots[slot].audio;
}

juce::String SlotEngine::lastMessage() const
{
    const juce::ScopedLock sl(lock);
    return message;
}

int SlotEngine::busyCount() const
{
    const juce::ScopedLock sl(lock);
    int n = 0;
    for (auto& s : slots) n += s.state == SlotInfo::State::Searching;
    return n;
}

void SlotEngine::publish(int slot)
{
    ++versionCounter;
    if (onSlotChanged) onSlotChanged(slot);
}

void SlotEngine::setStatus(int slot, int gen, const juce::String& s)
{
    {
        const juce::ScopedLock sl(lock);
        if (generations[slot] != gen) return;
        slots[slot].status = s;
    }
    ++versionCounter;
}

void SlotEngine::setMessage(const juce::String& s)
{
    {
        const juce::ScopedLock sl(lock);
        message = s;
    }
    ++versionCounter;
}

bool SlotEngine::pickStation(Station& out, juce::String& err)
{
    juce::StringArray tags;
    {
        const juce::ScopedLock sl(lock);
        tags = settings.tags;
    }
    tags.removeEmptyStrings();
    if (tags.isEmpty()) tags.add("jazz");

    for (int tries = 0; tries < (int) tags.size() * 2; ++tries) {
        juce::String tag;
        {
            const juce::ScopedLock pl(poolLock);
            tag = tags[rng.nextInt(tags.size())];
        }
        bool empty;
        {
            const juce::ScopedLock pl(poolLock);
            empty = pool[tag].isEmpty();
        }
        if (empty) {
            const juce::ScopedLock fl(fetchLock); // aynı anda tek liste isteği
            {
                const juce::ScopedLock pl(poolLock);
                empty = pool[tag].isEmpty();
            }
            if (empty) {
                juce::String e;
                auto list = fetchStations(tag, 300, e);
                const juce::ScopedLock pl(poolLock);
                for (auto& s : list)
                    if (badUrls.count(s.url) == 0) pool[tag].add(s);
                if (pool[tag].isEmpty()) {
                    err = e.isNotEmpty() ? e : TR("\"") + tag + TR("\" türünde istasyon bulunamadı");
                    continue;
                }
            }
        }
        const juce::ScopedLock pl(poolLock);
        auto& arr = pool[tag];
        for (int k = 0; k < 8 && arr.size() > 0; ++k) {
            const int i = rng.nextInt(arr.size());
            Station s = arr[i];
            arr.remove(i);
            if (inUse.count(s.url) || badUrls.count(s.url)) continue;
            inUse.insert(s.url);
            out = s;
            return true;
        }
    }
    if (err.isEmpty()) err = TR("istasyon bulunamadı");
    return false;
}

std::vector<float> SlotEngine::computePeaks(const juce::AudioBuffer<float>& b, int points)
{
    std::vector<float> p((size_t) points, 0.f);
    const int n = b.getNumSamples();
    if (n == 0) return p;
    for (int i = 0; i < points; ++i) {
        const int a0 = (int) ((int64_t) n * i / points), a1 = std::max(a0 + 1, (int) ((int64_t) n * (i + 1) / points));
        float m = 0;
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int j = a0; j < std::min(a1, n); ++j) m = std::max(m, std::abs(b.getSample(c, j)));
        p[(size_t) i] = m;
    }
    return p;
}

juce::File SlotEngine::writeWav(int slot, const CaptureResult& r, const Station& st, const EngineSettings& es)
{
    const juce::File folder = es.exportToFolder ? es.folder : es.cacheFolder;
    if (!folder.createDirectory()) return {};
    auto clean = [](juce::String s) {
        s = s.retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_");
        return s.trim().replaceCharacter(' ', '-').substring(0, 28);
    };
    const juce::String target = juce::String(noteName(es.core.targetRoot)) + modeName(es.core.targetMode);
    const juce::String orig = juce::String(keyName(r.root, r.minor)).removeCharacters(" ");
    const juce::String stamp = juce::Time::getCurrentTime().formatted("%H%M%S");
    // Asla üzerine yazılmaz: Ableton'a sürüklenmiş eski dosyalar bozulmasın
    const juce::String base = juce::String(slot + 1).paddedLeft('0', 2) + "_" + target.removeCharacters(" ") + "_" + orig
                              + "_" + (r.shift >= 0 ? "+" : "") + juce::String(r.shift) + "st_" + clean(st.name) + "_" + stamp;
    return writeBuffer(folder.getNonexistentChildFile(base, ".wav", false), r.audio);
}

juce::File SlotEngine::writeBuffer(const juce::File& f, const Pcm& audio)
{
    juce::AudioBuffer<float> buf(audio.channels, audio.length());
    for (int c = 0; c < audio.channels; ++c) buf.copyFrom(c, 0, audio.data[(size_t) c].data(), audio.length());

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os(f.createOutputStream());
    if (!os) return {};
    std::unique_ptr<juce::AudioFormatWriter> w(
        wav.createWriterFor(os.get(), audio.sampleRate, (unsigned) audio.channels, 24, {}, 0));
    if (!w) return {};
    os.release(); // writer artık akışın sahibi
    w->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
    return f;
}

void SlotEngine::runJob(int slot, int gen)
{
    if (getSettings().source == Source::Percussion) {
        runPercJob(slot, gen);
        return;
    }
    auto stale = [this, slot, gen]() {
        if (shuttingDown.load()) return true;
        const juce::ScopedLock sl(lock);
        return generations[slot] != gen;
    };

    const int maxAttempts = 40;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        if (stale()) return;
        const EngineSettings es = getSettings();

        Station st;
        juce::String err;
        if (!pickStation(st, err)) {
            setStatus(slot, gen, err);
            setMessage(err);
            juce::Thread::sleep(1500);
            continue;
        }
        auto release = [this, &st]() {
            const juce::ScopedLock pl(poolLock);
            inUse.erase(st.url);
        };

        {
            const juce::ScopedLock sl(lock);
            if (generations[slot] != gen) { release(); return; }
            slots[slot].attempts = attempt;
            slots[slot].status = TR("dinleniyor: ") + st.name;
        }
        ++versionCounter;

        juce::MemoryBlock mp3;
        const bool got = captureStream(st.url, es.recordSeconds, st.bitrate, mp3, stale, err);
        release();
        if (stale()) return;
        if (!got) {
            const juce::ScopedLock pl(poolLock);
            badUrls.insert(st.url);
            continue;
        }

        setStatus(slot, gen, TR("analiz: ") + st.name);
        const Pcm pcm = decodeMp3(static_cast<const uint8_t*>(mp3.getData()), mp3.getSize());
        if (pcm.length() < pcm.sampleRate * 2) {
            const juce::ScopedLock pl(poolLock);
            badUrls.insert(st.url);
            continue;
        }

        const CaptureResult r = processCapture(pcm, es.core);
        if (stale()) return;
        if (!r.ok) {
            setStatus(slot, gen, TR("geçildi (") + TR(r.reason.c_str()) + "): " + st.name);
            continue;
        }

        const juce::File file = writeWav(slot, r, st, es);
        auto buf = std::make_shared<juce::AudioBuffer<float>>(r.audio.channels, r.audio.length());
        for (int c = 0; c < r.audio.channels; ++c) buf->copyFrom(c, 0, r.audio.data[(size_t) c].data(), r.audio.length());

        {
            const juce::ScopedLock sl(lock);
            if (generations[slot] != gen) return;
            auto& s = slots[slot];
            s.state = SlotInfo::State::Ready;
            s.percussion = false;
            s.license.clear();
            s.link.clear();
            s.durationSec = r.audio.length() / juce::jmax(1.0, r.audio.sampleRate);
            s.station = st.name;
            s.country = st.country;
            s.tag = st.tag;
            s.origKey = keyName(r.root, r.minor);
            s.targetKey = juce::String(noteName(es.core.targetRoot)) + " " + modeName(es.core.targetMode);
            s.shift = r.shift;
            s.tuning = r.tuningCents;
            s.confidence = r.confidence;
            s.fit = r.fitAfter;
            s.file = file;
            s.audio = buf;
            s.sampleRate = r.audio.sampleRate;
            s.peaks = computePeaks(*buf, 160);
            s.status = file.existsAsFile() ? TR("hazır") : TR("hazır (WAV yazılamadı!)");
        }
        setMessage(TR("Yuva ") + juce::String(slot + 1) + ": " + st.name + "  " + keyName(r.root, r.minor)
                   + (r.shift ? TR(" → ") + (r.shift > 0 ? "+" : "") + juce::String(r.shift) + TR(" yarım ton") : TR(" (kaydırma yok)")));
        publish(slot);
        return;
    }

    {
        const juce::ScopedLock sl(lock);
        if (generations[slot] != gen) return;
        auto& s = slots[slot];
        s.state = s.audio ? SlotInfo::State::Ready : SlotInfo::State::Failed;
        s.status = TR("uygun sample bulunamadı — tekrar dene");
    }
    publish(slot);
}


// ---------------------------------------------------------------- perküsyon (Freesound)
SlotEngine::PickResult SlotEngine::pickSound(SoundRef& out, juce::String& err)
{
    EngineSettings es = getSettings();
    if (es.apiKey.trim().isEmpty()) {
        err = TR("Freesound API anahtarını gir (freesound.org/apiv2/apply)");
        return PickResult::Fatal;
    }
    juce::StringArray terms = es.percTerms;
    terms.trim();
    terms.removeEmptyStrings();
    if (terms.isEmpty()) {
        terms = juce::StringArray::fromTokens(defaultPercTerms(), ",", "");
        terms.trim();
    }

    for (int tries = 0; tries < terms.size() * 2 + 2; ++tries) {
        // terim seç: tükenmişleri ve son kullanılanları atla, çeşitlilik için
        juce::String term;
        {
            const juce::ScopedLock pl(poolLock);
            juce::StringArray candidates;
            for (auto& t : terms)
                if (exhaustedTerms.count(t) == 0) candidates.add(t);
            if (candidates.isEmpty()) {
                err = TR("bütün arama kelimeleri tükendi — yeni kelime ekle");
                return PickResult::Fatal;
            }
            const int avoid = std::min((int) recentTerms.size(), std::min(6, candidates.size() - 1));
            for (int k = 0; k < 8; ++k) {
                term = candidates[rng.nextInt(candidates.size())];
                bool recent = false;
                for (int r = 0; r < avoid; ++r)
                    if (recentTerms[recentTerms.size() - 1 - (size_t) r] == term) recent = true;
                if (!recent) break;
            }
            recentTerms.push_back(term);
            while (recentTerms.size() > 16) recentTerms.pop_front();
        }

        bool empty;
        {
            const juce::ScopedLock pl(poolLock);
            empty = soundPool[term].isEmpty();
        }
        if (empty) {
            const juce::ScopedLock fl(fetchLock); // aynı anda tek arama isteği
            {
                const juce::ScopedLock pl(poolLock);
                empty = soundPool[term].isEmpty();
            }
            if (empty) {
                SearchQuery q;
                q.term = term;
                q.apiKey = es.apiKey.trim();
                q.cc0Only = es.cc0Only;
                q.maxDuration = juce::jlimit(1.0, 8.0, es.perc.maxLengthSec + 2.0);
                q.pageSize = 150;
                static const char* kSingleEvent[] = {"single_event", "ac_single_event", ""};
                int known, variant;
                {
                    const juce::ScopedLock pl(poolLock);
                    known = termCount.count(term) ? termCount[term] : -1;
                    variant = singleEventVariant;
                }
                q.singleEventField = kSingleEvent[variant];
                // her seferinde farklı sonuçlar: toplam sayı biliniyorsa rastgele bir sayfa
                if (known > q.pageSize) q.page = 1 + rng.nextInt(std::min(40, (known + q.pageSize - 1) / q.pageSize));
                SearchResult res = searchFreesound(q);
                // tek olay filtresi bu API'de tanınmıyorsa (400) sıradaki adı dene
                while (res.httpCode == 400 && variant < 2) {
                    ++variant;
                    {
                        const juce::ScopedLock pl(poolLock);
                        singleEventVariant = std::max(singleEventVariant, variant);
                    }
                    q.singleEventField = kSingleEvent[variant];
                    res = searchFreesound(q);
                }
                // filtreyle hiç sonuç yoksa bu kelime için filtresiz dene (analizi yapılmamış sesler)
                if (res.httpCode == 200 && res.count == 0 && q.singleEventField.isNotEmpty()) {
                    q.singleEventField.clear();
                    res = searchFreesound(q);
                }
                if (res.httpCode == 401 || res.httpCode == 403) { err = res.error; return PickResult::Fatal; }
                if (res.httpCode == 429) { err = res.error; juce::Thread::sleep(5000); return PickResult::Retry; }
                if (res.error.isNotEmpty() && res.httpCode != 404) { err = res.error; return PickResult::Retry; }

                const juce::ScopedLock pl(poolLock);
                termCount[term] = res.count;
                int added = 0;
                for (auto& snd : res.sounds) {
                    if (usedSoundIds.count(snd.id) || inUseSoundIds.count(snd.id)) continue;
                    if (isExcluded(snd, es.percExclude)) continue;
                    soundPool[term].add(snd);
                    ++added;
                }
                if (added == 0) {
                    // küçük sonuç kümesi bittiyse ya da iki denemede de yeni ses çıkmadıysa terimi bırak
                    if (res.count <= q.pageSize || ++termEmptyFetches[term] >= 3) exhaustedTerms.insert(term);
                    err = TR("\"") + term + TR("\" için yeni ses yok");
                    continue;
                }
            }
        }

        const juce::ScopedLock pl(poolLock);
        auto& arr = soundPool[term];
        while (arr.size() > 0) {
            const int i = rng.nextInt(arr.size());
            SoundRef snd = arr[i];
            arr.remove(i);
            if (usedSoundIds.count(snd.id) || inUseSoundIds.count(snd.id)) continue;
            inUseSoundIds.insert(snd.id);
            out = snd;
            return PickResult::Ok;
        }
    }
    if (err.isEmpty()) err = TR("uygun ses bulunamadı");
    return PickResult::Retry;
}

juce::File SlotEngine::writePercWav(int slot, const PercResult& r, const SoundRef& snd, const EngineSettings& es)
{
    if (!es.folder.createDirectory()) return {};
    auto clean = [](juce::String s, int maxLen) {
        if (s.containsChar('.')) s = s.upToLastOccurrenceOf(".", false, false);
        s = s.retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_");
        return s.trim().replaceCharacter(' ', '-').substring(0, maxLen);
    };
    const juce::String base = juce::String(slot + 1).paddedLeft('0', 2) + "_" + clean(snd.term, 16) + "_"
                              + clean(snd.name, 28) + "_fs" + juce::String(snd.id);
    return writeBuffer(es.folder.getNonexistentChildFile(base, ".wav", false), r.audio);
}

void SlotEngine::appendCredit(const juce::File& wav, const SoundRef& snd)
{
    // Freesound lisansları (özellikle CC-BY) yazar adının belirtilmesini istiyor
    const juce::File credits = wav.getParentDirectory().getChildFile("KAYNAKLAR.txt");
    juce::String line;
    line << wav.getFileName() << "  |  \"" << snd.name << "\"  |  " << snd.username << "  |  " << snd.license
         << "  |  " << snd.pageUrl << "\n";
    const juce::ScopedLock pl(poolLock);
    if (!credits.existsAsFile())
        credits.appendText(TR("Galaksi — perküsyon sample kaynakları (freesound.org). CC-BY sesleri yayınlarken yazar adını belirt.\n\n"),
                           false, false, "\n");
    credits.appendText(line, false, false, "\n");
}

void SlotEngine::runPercJob(int slot, int gen)
{
    auto stale = [this, slot, gen]() {
        if (shuttingDown.load()) return true;
        const juce::ScopedLock sl(lock);
        return generations[slot] != gen;
    };
    auto fail = [this, slot, gen](const juce::String& why) {
        {
            const juce::ScopedLock sl(lock);
            if (generations[slot] != gen) return;
            auto& s = slots[slot];
            s.state = s.audio ? SlotInfo::State::Ready : SlotInfo::State::Failed;
            s.status = why;
        }
        setMessage(why);
        publish(slot);
    };

    const int maxAttempts = 30;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        if (stale()) return;
        const EngineSettings es = getSettings();

        SoundRef snd;
        juce::String err;
        const auto pr = pickSound(snd, err);
        if (pr == PickResult::Fatal) { fail(err); return; }
        if (pr == PickResult::Retry) {
            setStatus(slot, gen, err);
            juce::Thread::sleep(800);
            continue;
        }
        auto release = [this, &snd]() {
            const juce::ScopedLock pl(poolLock);
            inUseSoundIds.erase(snd.id);
        };
        {
            const juce::ScopedLock sl(lock);
            if (generations[slot] != gen) { release(); return; }
            slots[slot].attempts = attempt;
            slots[slot].status = TR("indiriliyor: ") + snd.name;
        }
        ++versionCounter;

        juce::MemoryBlock mp3;
        const bool got = downloadBytes(snd.previewUrl, mp3, stale, err);
        if (stale()) { release(); return; }
        if (!got) { release(); continue; }

        setStatus(slot, gen, TR("analiz: ") + snd.name);
        const Pcm pcm = decodeMp3(static_cast<const uint8_t*>(mp3.getData()), mp3.getSize(), 1);
        const PercResult r = processPercussion(pcm, es.perc);
        {
            const juce::ScopedLock pl(poolLock);
            inUseSoundIds.erase(snd.id);
            usedSoundIds.insert(snd.id); // aynı ses bu oturumda bir daha gelmesin
        }
        if (stale()) return;
        if (!r.ok) {
            setStatus(slot, gen, TR("geçildi (") + TR(r.reason.c_str()) + "): " + snd.name);
            continue;
        }

        const juce::File file = writePercWav(slot, r, snd, es);
        if (file.existsAsFile()) appendCredit(file, snd);
        auto buf = std::make_shared<juce::AudioBuffer<float>>(r.audio.channels, r.audio.length());
        for (int c = 0; c < r.audio.channels; ++c) buf->copyFrom(c, 0, r.audio.data[(size_t) c].data(), r.audio.length());

        {
            const juce::ScopedLock sl(lock);
            if (generations[slot] != gen) return;
            auto& s = slots[slot];
            const bool wasLocked = s.locked;
            s = SlotInfo {};
            s.locked = wasLocked;
            s.state = SlotInfo::State::Ready;
            s.percussion = true;
            s.station = snd.name;
            s.country = snd.username;
            s.tag = snd.term;
            s.license = snd.license;
            s.link = snd.pageUrl;
            s.durationSec = r.durationSec;
            s.file = file;
            s.audio = buf;
            s.sampleRate = r.audio.sampleRate;
            s.peaks = computePeaks(*buf, 160);
            s.attempts = attempt;
            s.status = file.existsAsFile() ? TR("hazır") : TR("hazır (WAV yazılamadı!)");
        }
        setMessage(TR("Yuva ") + juce::String(slot + 1) + ": \"" + snd.name + "\"  ·  " + snd.term + "  ·  "
                   + juce::String(r.durationSec, 2) + " s  ·  " + snd.license);
        publish(slot);
        return;
    }
    fail(TR("uygun ses bulunamadı — tekrar dene"));
}

juce::File SlotEngine::writeDrumRack(int slotCount, juce::String& error)
{
    const EngineSettings es = getSettings();
    std::vector<AdgPad> pads;
    for (int i = 0; i < std::min(slotCount, (int) MaxSlots); ++i) {
        const SlotInfo s = getSlot(i);
        if (!s.audio || !s.file.existsAsFile()) continue;
        AdgPad p;
        p.midiNote = 36 + i;
        p.name = (juce::String(i + 1) + " " + (s.station.isNotEmpty() ? s.station : s.file.getFileNameWithoutExtension())).toStdString();
        const juce::File f = s.file;
        p.fileName = f.getFileName().toStdString();
        p.absoluteDir = f.getParentDirectory().getFullPathName().toStdString();
        p.frames = s.audio->getNumSamples();
        p.sampleRate = s.sampleRate;
        p.fileSize = f.getSize();
        pads.push_back(p);
    }
    if (pads.empty()) { error = TR("henüz sample yok"); return {}; }

    // Live 10 yolları .adg'ye göre göreli okur: bütün WAV'lar .adg ile aynı klasörde olmalı,
    // başka klasörde kalmış olanlar (örn. klasör değiştirildiyse) kopyalanır
    const juce::File dir = es.folder;
    if (!dir.createDirectory()) { error = TR("klasör oluşturulamadı"); return {}; }
    for (auto& p : pads) {
        const juce::File src = juce::File(juce::String(p.absoluteDir)).getChildFile(juce::String(p.fileName));
        if (src.getParentDirectory() != dir) {
            const juce::File dst = dir.getChildFile(src.getFileName());
            if (!dst.existsAsFile()) src.copyFileTo(dst);
            p.absoluteDir = dir.getFullPathName().toStdString();
        }
    }

    const std::string xml = drumRackXml(pads);
    const juce::String name = TR(es.source == Source::Percussion ? "Galaksi_Perkusyon_" : "Galaksi_Radyo_")
                              + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    const juce::File adg = dir.getNonexistentChildFile(name, ".adg", false);
    {
        std::unique_ptr<juce::FileOutputStream> fos(adg.createOutputStream());
        if (!fos) { error = TR(".adg yazılamadı"); return {}; }
        juce::GZIPCompressorOutputStream gz(*fos, 9, juce::GZIPCompressorOutputStream::windowBitsGZIP);
        gz.write(xml.data(), xml.size());
        gz.flush();
    }
    return adg;
}

// ---------------------------------------------------------------- durum kaydı
juce::ValueTree SlotEngine::saveState() const
{
    const juce::ScopedLock sl(lock);
    juce::ValueTree v("ENGINE");
    v.setProperty("tags", settings.tags.joinIntoString(","), nullptr);
    v.setProperty("folder", settings.folder.getFullPathName(), nullptr);
    v.setProperty("percTerms", settings.percTerms.joinIntoString(", "), nullptr);
    v.setProperty("percExclude", settings.percExclude.joinIntoString(", "), nullptr);
    for (int i = 0; i < MaxSlots; ++i) {
        const auto& s = slots[i];
        juce::ValueTree c("SLOT");
        c.setProperty("index", i, nullptr);
        c.setProperty("locked", s.locked, nullptr);
        if (s.audio && s.file.existsAsFile()) {
            c.setProperty("file", s.file.getFullPathName(), nullptr);
            c.setProperty("station", s.station, nullptr);
            c.setProperty("country", s.country, nullptr);
            c.setProperty("tag", s.tag, nullptr);
            c.setProperty("origKey", s.origKey, nullptr);
            c.setProperty("targetKey", s.targetKey, nullptr);
            c.setProperty("shift", s.shift, nullptr);
            c.setProperty("tuning", s.tuning, nullptr);
            c.setProperty("confidence", s.confidence, nullptr);
            c.setProperty("fit", s.fit, nullptr);
            c.setProperty("percussion", s.percussion, nullptr);
            c.setProperty("license", s.license, nullptr);
            c.setProperty("link", s.link, nullptr);
            c.setProperty("duration", s.durationSec, nullptr);
        }
        v.appendChild(c, nullptr);
    }
    return v;
}

void SlotEngine::loadState(const juce::ValueTree& v)
{
    if (!v.hasType("ENGINE")) return;
    {
        const juce::ScopedLock sl(lock);
        settings.tags = juce::StringArray::fromTokens(v.getProperty("tags", "jazz,world music").toString(), ",", "");
        settings.tags.trim();
        settings.tags.removeEmptyStrings();
        const juce::String folder = v.getProperty("folder", "").toString();
        if (folder.isNotEmpty()) settings.folder = juce::File(folder);
        if (v.hasProperty("percTerms")) {
            settings.percTerms = juce::StringArray::fromTokens(v.getProperty("percTerms").toString(), ",", "");
            settings.percTerms.trim();
            settings.percTerms.removeEmptyStrings();
        }
        if (v.hasProperty("percExclude")) {
            settings.percExclude = juce::StringArray::fromTokens(v.getProperty("percExclude").toString(), ",", "");
            settings.percExclude.trim();
            settings.percExclude.removeEmptyStrings();
        }
    }
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    for (auto c : v) {
        const int i = c.getProperty("index", -1);
        if (i < 0 || i >= MaxSlots) continue;
        SlotInfo s;
        s.locked = c.getProperty("locked", false);
        const juce::File f(c.getProperty("file", "").toString());
        if (f.existsAsFile()) {
            std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(f));
            if (rd && rd->lengthInSamples > 0) {
                auto buf = std::make_shared<juce::AudioBuffer<float>>((int) rd->numChannels, (int) rd->lengthInSamples);
                rd->read(buf.get(), 0, (int) rd->lengthInSamples, 0, true, true);
                s.audio = buf;
                s.sampleRate = rd->sampleRate;
                s.peaks = computePeaks(*buf, 160);
                s.file = f;
                s.state = SlotInfo::State::Ready;
                s.station = c.getProperty("station").toString();
                s.country = c.getProperty("country").toString();
                s.tag = c.getProperty("tag").toString();
                s.origKey = c.getProperty("origKey").toString();
                s.targetKey = c.getProperty("targetKey").toString();
                s.shift = c.getProperty("shift", 0);
                s.tuning = c.getProperty("tuning", 0.0);
                s.confidence = c.getProperty("confidence", 0.0);
                s.fit = c.getProperty("fit", 0.0);
                s.percussion = c.getProperty("percussion", false);
                s.license = c.getProperty("license").toString();
                s.link = c.getProperty("link").toString();
                s.durationSec = c.getProperty("duration", (double) rd->lengthInSamples / juce::jmax(1.0, rd->sampleRate));
                s.status = TR("hazır");
            }
        }
        if (!s.audio) s.status = TR("boş — yenile'ye bas");
        {
            const juce::ScopedLock sl(lock);
            ++generations[i];
            slots[i] = s;
        }
        publish(i);
    }
}

} // namespace gk
