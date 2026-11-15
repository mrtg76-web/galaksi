#include "FreesoundNet.h"
#include "Text.h"

namespace gk {

static const char* kUserAgent = "Galaksi/0.2 (percussion sampler plugin)";

juce::String defaultPercTerms()
{
    return "darbuka, djembe, tabla, bendir, frame drum, daf, taiko, cajon, udu, talking drum, "
           "gong, anvil, metal hit, pipe hit, bell hit, pot lid, pan hit, door slam, door knock, "
           "wood block, wood hit, bucket hit, glass hit, stone hit";
}

juce::String defaultPercExclude()
{
    return "808, 909, 707, drum machine, electronic, synth, edm, trap, dubstep, techno, kick, snare, "
           "hihat, hi-hat, cymbal, drum kit, drumkit, drum set, loop, voice, speech";
}

juce::String shortLicense(const juce::String& url)
{
    const auto u = url.toLowerCase();
    if (u.contains("zero") || u.contains("publicdomain")) return "CC0";
    if (u.contains("sampling")) return "Sampling+";
    if (u.contains("by-nc")) return "CC-BY-NC";
    if (u.contains("/by/") || u.contains("attribution")) return "CC-BY";
    return url.isNotEmpty() ? url.fromLastOccurrenceOf("/licenses/", false, false).upToFirstOccurrenceOf("/", false, false).toUpperCase() : "?";
}

static juce::String normalise(const juce::String& s)
{
    return s.toLowerCase().replaceCharacters("-_", "  ");
}

bool isExcluded(const SoundRef& s, const juce::StringArray& excludeWords)
{
    const juce::String name = normalise(s.name);
    juce::StringArray tags;
    for (auto& t : s.tags) tags.add(normalise(t));
    for (auto w : excludeWords) {
        w = normalise(w).trim();
        if (w.length() < 2) continue;
        if (name.contains(w)) return true;
        for (auto& t : tags)
            if (t == w || t.contains(w)) return true;
    }
    return false;
}

static SearchResult searchAt(const juce::String& endpoint, const SearchQuery& q)
{
    SearchResult res;
    juce::String filter = "duration:[0.03 TO " + juce::String(q.maxDuration, 2) + "]";
    if (q.cc0Only) filter << " license:\"Creative Commons 0\"";
    if (q.singleEventField.isNotEmpty()) filter << " " << q.singleEventField << ":true";

    juce::URL url(endpoint);
    url = url.withParameter("query", q.term)
              .withParameter("filter", filter)
              .withParameter("fields", "id,name,username,license,previews,tags,duration,url")
              .withParameter("page_size", juce::String(q.pageSize))
              .withParameter("page", juce::String(q.page))
              .withParameter("token", q.apiKey);

    int status = 0;
    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(10000)
                    .withStatusCode(&status)
                    .withExtraHeaders(juce::String("User-Agent: ") + kUserAgent);
    std::unique_ptr<juce::InputStream> in(url.createInputStream(opts));
    res.httpCode = status;
    if (!in) {
        res.error = TR("Freesound'a bağlanılamadı");
        return res;
    }
    const juce::String body = in->readEntireStreamAsString();
    if (status == 401 || status == 403) { res.error = TR("Freesound API anahtarı geçersiz"); return res; }
    if (status == 429) { res.error = TR("Freesound istek sınırı doldu, biraz bekle"); return res; }
    if (status == 404) { res.error = TR("sayfa yok"); return res; }
    if (status != 200) {
        res.error = "Freesound HTTP " + juce::String(status);
        return res;
    }

    const auto json = juce::JSON::parse(body);
    res.count = (int) json.getProperty("count", 0);
    if (auto* arr = json.getProperty("results", {}).getArray()) {
        for (auto& v : *arr) {
            SoundRef s;
            s.id = (int) v.getProperty("id", 0);
            s.name = v.getProperty("name", "").toString().trim();
            s.username = v.getProperty("username", "").toString();
            s.license = shortLicense(v.getProperty("license", "").toString());
            s.pageUrl = v.getProperty("url", "").toString();
            s.duration = (double) v.getProperty("duration", 0.0);
            const auto previews = v.getProperty("previews", {});
            s.previewUrl = previews.getProperty("preview-hq-mp3", "").toString();
            if (s.previewUrl.isEmpty()) s.previewUrl = previews.getProperty("preview-lq-mp3", "").toString();
            if (auto* t = v.getProperty("tags", {}).getArray())
                for (auto& tag : *t) s.tags.add(tag.toString());
            s.term = q.term;
            if (s.id > 0 && s.previewUrl.isNotEmpty()) res.sounds.add(s);
        }
    } else {
        res.error = TR("Freesound yanıtı okunamadı");
    }
    return res;
}

SearchResult searchFreesound(const SearchQuery& q)
{
    // Güncel uç nokta /apiv2/search/ (Kasım 2025'te /search/text/ yerine geldi); yoksa eskisi denenir
    auto res = searchAt("https://freesound.org/apiv2/search/", q);
    if (res.httpCode == 404 && q.page == 1) res = searchAt("https://freesound.org/apiv2/search/text/", q);
    return res;
}

bool downloadBytes(const juce::String& url, juce::MemoryBlock& out, const std::function<bool()>& shouldStop,
                   juce::String& error)
{
    out.reset();
    int status = 0;
    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(10000)
                    .withNumRedirectsToFollow(5)
                    .withStatusCode(&status)
                    .withExtraHeaders(juce::String("User-Agent: ") + kUserAgent);
    std::unique_ptr<juce::InputStream> in(juce::URL(url).createInputStream(opts));
    if (!in) { error = TR("indirilemedi"); return false; }
    if (status != 0 && status != 200) { error = "HTTP " + juce::String(status); return false; }
    char buf[16384];
    const auto t0 = juce::Time::getMillisecondCounter();
    while (!in->isExhausted()) {
        if (shouldStop && shouldStop()) return false;
        if (juce::Time::getMillisecondCounter() - t0 > 30000 || out.getSize() > 8 * 1024 * 1024) break;
        const int n = in->read(buf, (int) sizeof buf);
        if (n <= 0) break;
        out.append(buf, (size_t) n);
    }
    if (out.getSize() < 256) { error = TR("dosya boş"); return false; }
    return true;
}

} // namespace gk
