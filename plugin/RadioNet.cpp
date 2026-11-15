#include "RadioNet.h"
#include "Text.h"

namespace gk {

static const char* kUserAgent = "Galaksi/0.1 (radio sampler plugin)";
static const char* kServers[] = {"de1", "de2", "fi1", "nl1", "at1"};

// ---------------------------------------------------------------- istasyon listesi
juce::Array<Station> fetchStations(const juce::String& tag, int limit, juce::String& error)
{
    juce::Array<Station> result;
    juce::Array<int> order;
    for (int i = 0; i < (int) juce::numElementsInArray(kServers); ++i) order.add(i);
    juce::Random rng;
    for (int i = order.size() - 1; i > 0; --i) order.swap(i, rng.nextInt(i + 1));

    for (auto scheme : {"https", "http"}) {
        for (int idx : order) {
            juce::URL url(juce::String(scheme) + "://" + kServers[idx] + ".api.radio-browser.info/json/stations/search");
            url = url.withParameter("tag", tag)
                      .withParameter("codec", "MP3")
                      .withParameter("hidebroken", "true")
                      .withParameter("lastcheckok", "1")
                      .withParameter("order", "random")
                      .withParameter("limit", juce::String(limit));
            auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                            .withConnectionTimeoutMs(8000)
                            .withExtraHeaders(juce::String("User-Agent: ") + kUserAgent);
            std::unique_ptr<juce::InputStream> in(url.createInputStream(opts));
            if (!in) { error = TR("Radio-Browser sunucusuna bağlanılamadı"); continue; }
            const auto json = juce::JSON::parse(in->readEntireStreamAsString());
            auto* arr = json.getArray();
            if (!arr) { error = TR("Radio-Browser yanıtı okunamadı"); continue; }
            for (auto& v : *arr) {
                Station s;
                s.url = v.getProperty("url_resolved", "").toString().trim();
                if (s.url.isEmpty()) s.url = v.getProperty("url", "").toString().trim();
                const bool hls = (int) v.getProperty("hls", 0) != 0;
                if (s.url.isEmpty() || hls || s.url.containsIgnoreCase(".m3u8") || s.url.endsWithIgnoreCase(".pls")
                    || s.url.endsWithIgnoreCase(".m3u"))
                    continue;
                s.name = v.getProperty("name", "?").toString().trim();
                s.country = v.getProperty("countrycode", "").toString();
                s.codec = v.getProperty("codec", "").toString();
                s.uuid = v.getProperty("stationuuid", "").toString();
                s.bitrate = (int) v.getProperty("bitrate", 0);
                s.tag = tag;
                result.add(s);
            }
            error.clear();
            return result;
        }
    }
    return result;
}

// ---------------------------------------------------------------- yayın yakalama
namespace {

struct ParsedUrl {
    bool https = false;
    juce::String host, path;
    int port = 80;
};

bool parseUrl(const juce::String& u, ParsedUrl& p)
{
    juce::String rest;
    if (u.startsWithIgnoreCase("http://")) { p.https = false; rest = u.substring(7); p.port = 80; }
    else if (u.startsWithIgnoreCase("https://")) { p.https = true; rest = u.substring(8); p.port = 443; }
    else return false;
    const int slash = rest.indexOfChar('/');
    juce::String hostPort = slash >= 0 ? rest.substring(0, slash) : rest;
    p.path = slash >= 0 ? rest.substring(slash) : "/";
    if (hostPort.containsChar('@')) hostPort = hostPort.fromLastOccurrenceOf("@", false, false);
    const int colon = hostPort.lastIndexOfChar(':');
    if (colon > 0 && !hostPort.endsWithChar(']')) {
        p.port = hostPort.substring(colon + 1).getIntValue();
        hostPort = hostPort.substring(0, colon);
    }
    p.host = hostPort.removeCharacters("[]");
    return p.host.isNotEmpty() && p.port > 0;
}

struct Limits {
    size_t targetBytes = 0;     // 0 = sadece süreye göre
    double softSeconds = 15;    // targetBytes yoksa bu süre kadar oku
    double hardSeconds = 25;    // her durumda en fazla
};

bool readBody(const std::function<int(char*, int)>& readSome, juce::MemoryBlock& out, const Limits& lim,
              const std::function<bool()>& shouldStop)
{
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    std::vector<char> buf(16384);
    for (;;) {
        if (shouldStop && shouldStop()) return false;
        const double el = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        if (lim.targetBytes > 0 && out.getSize() >= lim.targetBytes) break;
        if (lim.targetBytes == 0 && el >= lim.softSeconds) break;
        if (el >= lim.hardSeconds) break;
        const int n = readSome(buf.data(), (int) buf.size());
        if (n < 0) break;   // bağlantı kapandı
        if (n > 0) out.append(buf.data(), (size_t) n);
    }
    return out.getSize() > 16000;
}

bool captureHttpSocket(const ParsedUrl& p, const Limits& lim, juce::MemoryBlock& out, juce::String& redirect,
                       const std::function<bool()>& shouldStop, juce::String& error)
{
    juce::StreamingSocket sock;
    if (!sock.connect(p.host, p.port, 7000)) { error = TR("bağlanılamadı"); return false; }

    juce::String req;
    req << "GET " << p.path << " HTTP/1.0\r\n"
        << "Host: " << p.host << (p.port != 80 ? ":" + juce::String(p.port) : juce::String()) << "\r\n"
        << "User-Agent: " << kUserAgent << "\r\n"
        << "Accept: */*\r\n"
        << "Icy-MetaData: 0\r\n"
        << "Connection: close\r\n\r\n";
    const auto r8 = req.toRawUTF8();
    if (sock.write(r8, (int) strlen(r8)) < 0) { error = TR("istek gönderilemedi"); return false; }

    // başlıkları oku
    juce::String header;
    {
        juce::MemoryBlock hb;
        const auto t0 = juce::Time::getMillisecondCounter();
        for (;;) {
            if (shouldStop && shouldStop()) return false;
            if (juce::Time::getMillisecondCounter() - t0 > 8000 || hb.getSize() > 16384) { error = TR("yanıt yok"); return false; }
            const int ready = sock.waitUntilReady(true, 500);
            if (ready < 0) { error = TR("bağlantı koptu"); return false; }
            if (ready == 0) continue;
            char c;
            if (sock.read(&c, 1, false) <= 0) { error = TR("bağlantı kapandı"); return false; }
            hb.append(&c, 1);
            const auto* d = static_cast<const char*>(hb.getData());
            const size_t n = hb.getSize();
            if ((n >= 4 && memcmp(d + n - 4, "\r\n\r\n", 4) == 0) || (n >= 2 && memcmp(d + n - 2, "\n\n", 2) == 0))
                break;
        }
        header = juce::String::fromUTF8(static_cast<const char*>(hb.getData()), (int) hb.getSize());
    }

    const auto lines = juce::StringArray::fromLines(header);
    const juce::String status = lines[0].trim();
    const int code = status.fromFirstOccurrenceOf(" ", false, false).getIntValue();
    juce::String contentType, location;
    for (auto& l : lines) {
        if (l.startsWithIgnoreCase("content-type:")) contentType = l.fromFirstOccurrenceOf(":", false, false).trim().toLowerCase();
        if (l.startsWithIgnoreCase("location:")) location = l.fromFirstOccurrenceOf(":", false, false).trim();
    }
    if (code >= 300 && code < 400 && location.isNotEmpty()) {
        if (location.startsWithChar('/')) location = "http://" + p.host + ":" + juce::String(p.port) + location;
        redirect = location;
        return false;
    }
    if (code != 200) { error = "HTTP " + juce::String(code); return false; }
    if (contentType.contains("html") || contentType.contains("mpegurl") || contentType.contains("scpls")
        || contentType.contains("ogg") || contentType.contains("aac") || contentType.contains("mp4")) {
        error = TR("desteklenmeyen biçim (") + contentType + ")";
        return false;
    }

    auto readSome = [&sock](char* b, int max) -> int {
        const int ready = sock.waitUntilReady(true, 1000);
        if (ready < 0) return -1;
        if (ready == 0) return 0;
        const int n = sock.read(b, max, false);
        return n <= 0 ? -1 : n;
    };
    if (!readBody(readSome, out, lim, shouldStop)) { if (error.isEmpty()) error = "yeterli veri gelmedi"; return false; }
    return true;
}

bool captureHttps(const juce::String& url, const Limits& lim, juce::MemoryBlock& out,
                  const std::function<bool()>& shouldStop, juce::String& error)
{
    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(8000)
                    .withNumRedirectsToFollow(5)
                    .withExtraHeaders(juce::String("User-Agent: ") + kUserAgent + "\r\nIcy-MetaData: 0");
    std::unique_ptr<juce::InputStream> in(juce::URL(url).createInputStream(opts));
    if (!in) { error = TR("bağlanılamadı"); return false; }
    auto readSome = [&in](char* b, int max) -> int {
        if (in->isExhausted()) return -1;
        const int n = in->read(b, max);
        return n <= 0 ? -1 : n;
    };
    if (!readBody(readSome, out, lim, shouldStop)) { error = "yeterli veri gelmedi"; return false; }
    return true;
}

} // namespace

bool captureStream(const juce::String& startUrl, double seconds, int bitrateKbps, juce::MemoryBlock& out,
                   const std::function<bool()>& shouldStop, juce::String& error)
{
    Limits lim;
    // Sunucular bağlanınca birkaç saniyelik veriyi bir anda gönderebiliyor; bitrate biliniyorsa bayt sayısıyla dur.
    if (bitrateKbps >= 24 && bitrateKbps <= 512)
        lim.targetBytes = (size_t) (bitrateKbps * 125.0 * (seconds + 0.5));
    lim.softSeconds = seconds + 1.0;
    lim.hardSeconds = seconds + 8.0;

    juce::String url = startUrl;
    for (int hop = 0; hop < 6; ++hop) {
        out.reset();
        ParsedUrl p;
        if (!parseUrl(url, p)) { error = TR("geçersiz adres"); return false; }
        if (p.https) return captureHttps(url, lim, out, shouldStop, error);
        juce::String redirect;
        if (captureHttpSocket(p, lim, out, redirect, shouldStop, error)) return true;
        if (redirect.isEmpty()) return false;
        url = redirect;
    }
    error = TR("çok fazla yönlendirme");
    return false;
}

} // namespace gk
