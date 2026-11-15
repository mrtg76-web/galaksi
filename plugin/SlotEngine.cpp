#include "SlotEngine.h"
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
    settings = s;
    if (tagsChanged) {
        const juce::ScopedLock pl(poolLock);
        pool.clear();
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
    juce::File f = folder.getNonexistentChildFile(base, ".wav", false);

    juce::AudioBuffer<float> buf(r.audio.channels, r.audio.length());
    for (int c = 0; c < r.audio.channels; ++c) buf.copyFrom(c, 0, r.audio.data[(size_t) c].data(), r.audio.length());

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os(f.createOutputStream());
    if (!os) return {};
    std::unique_ptr<juce::AudioFormatWriter> w(
        wav.createWriterFor(os.get(), r.audio.sampleRate, (unsigned) r.audio.channels, 24, {}, 0));
    if (!w) return {};
    os.release(); // writer artık akışın sahibi
    w->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
    return f;
}

void SlotEngine::runJob(int slot, int gen)
{
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

// ---------------------------------------------------------------- durum kaydı
juce::ValueTree SlotEngine::saveState() const
{
    const juce::ScopedLock sl(lock);
    juce::ValueTree v("ENGINE");
    v.setProperty("tags", settings.tags.joinIntoString(","), nullptr);
    v.setProperty("folder", settings.folder.getFullPathName(), nullptr);
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
