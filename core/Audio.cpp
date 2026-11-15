#include "Audio.h"
#include "Analysis.h"

#include <algorithm>
#include <cmath>

#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#include <rubberband/RubberBandStretcher.h>

namespace gk {

std::vector<float> Pcm::mono() const
{
    std::vector<float> m;
    if (data.empty()) return m;
    const int n = length();
    m.assign(n, 0.f);
    for (auto& ch : data)
        for (int i = 0; i < n; ++i) m[i] += ch[i];
    const float g = 1.f / (float) data.size();
    for (auto& v : m) v *= g;
    return m;
}

// ---------------------------------------------------------------- MP3
Pcm decodeMp3(const uint8_t* buf, size_t size)
{
    Pcm out;
    if (!buf || size < 1024) return out;

    mp3dec_t dec;
    mp3dec_init(&dec);
    mp3dec_frame_info_t info;
    std::vector<float> pcm(MINIMP3_MAX_SAMPLES_PER_FRAME);

    int hz = 0, ch = 0;
    int goodFrames = 0;
    size_t pos = 0;
    while (pos < size) {
        const int remaining = (int) std::min<size_t>(size - pos, 1 << 20);
        const int samples = mp3dec_decode_frame(&dec, buf + pos, remaining, pcm.data(), &info);
        if (info.frame_bytes <= 0) break; // veri bitti
        pos += (size_t) info.frame_bytes;
        if (samples <= 0) continue;       // çöp atlandı / ilk kare
        if (hz == 0) {
            hz = info.hz;
            ch = std::min(info.channels, 2);
            out.sampleRate = hz;
            out.channels = ch;
            out.data.assign(ch, {});
        }
        if (info.hz != hz || std::min(info.channels, 2) != ch) continue; // akış ortasında format değişimi
        ++goodFrames;
        for (int c = 0; c < ch; ++c) {
            auto& d = out.data[c];
            const size_t base = d.size();
            d.resize(base + samples);
            for (int i = 0; i < samples; ++i) d[base + i] = pcm[(size_t) i * info.channels + c];
        }
    }
    if (goodFrames < 10) return Pcm{};
    return out;
}

// ---------------------------------------------------------------- pitch shift
static float hermite(const std::vector<float>& x, double pos)
{
    const int n = (int) x.size();
    const int i = (int) std::floor(pos);
    const float t = (float) (pos - i);
    auto at = [&](int k) { return x[std::clamp(k, 0, n - 1)]; };
    const float xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
    const float c0 = x0, c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + c0;
}

Pcm pitchShift(const Pcm& in, double semitones, ShiftMode mode)
{
    if (std::abs(semitones) < 1e-4 || in.length() == 0) return in;
    const double ratio = std::pow(2.0, semitones / 12.0);
    const int n = in.length();

    Pcm out;
    out.sampleRate = in.sampleRate;
    out.channels = in.channels;

    if (mode == ShiftMode::Tape) {
        const int outLen = std::max(1, (int) std::floor((n - 1) / ratio));
        out.data.assign(in.channels, std::vector<float>(outLen));
        // yukarı kaydırırken basit bir yumuşatma (aliasing azaltma) uygulanmaz; kısa sample'lar için yeterli
        for (int c = 0; c < in.channels; ++c)
            for (int i = 0; i < outLen; ++i) out.data[c][i] = hermite(in.data[c], i * ratio);
        return out;
    }

    using RB = RubberBand::RubberBandStretcher;
    RB st((size_t) in.sampleRate, (size_t) in.channels,
          RB::OptionProcessOffline | RB::OptionEngineFiner | RB::OptionChannelsTogether,
          1.0, ratio);
    st.setExpectedInputDuration((size_t) n);

    const int block = 4096;
    std::vector<const float*> ptrs(in.channels);
    for (int pos = 0; pos < n; pos += block) {
        const int cnt = std::min(block, n - pos);
        for (int c = 0; c < in.channels; ++c) ptrs[c] = in.data[c].data() + pos;
        st.study(ptrs.data(), (size_t) cnt, pos + cnt >= n);
    }

    out.data.assign(in.channels, {});
    std::vector<std::vector<float>> tmp(in.channels, std::vector<float>(block * 4));
    std::vector<float*> optrs(in.channels);
    auto drain = [&]() {
        int av;
        while ((av = st.available()) > 0) {
            const int take = std::min(av, (int) tmp[0].size());
            for (int c = 0; c < in.channels; ++c) optrs[c] = tmp[c].data();
            const size_t got = st.retrieve(optrs.data(), (size_t) take);
            for (int c = 0; c < in.channels; ++c)
                out.data[c].insert(out.data[c].end(), tmp[c].begin(), tmp[c].begin() + (long) got);
        }
        return av;
    };
    for (int pos = 0; pos < n; pos += block) {
        const int cnt = std::min(block, n - pos);
        for (int c = 0; c < in.channels; ++c) ptrs[c] = in.data[c].data() + pos;
        st.process(ptrs.data(), (size_t) cnt, pos + cnt >= n);
        drain();
    }
    while (drain() >= 0) {}

    for (auto& d : out.data) d.resize((size_t) n, 0.f); // offline modda uzunluk korunur
    return out;
}

// ---------------------------------------------------------------- işleme hattı
static Pcm slice(const Pcm& p, int start, int len)
{
    Pcm s;
    s.sampleRate = p.sampleRate;
    s.channels = p.channels;
    start = std::clamp(start, 0, p.length());
    len = std::clamp(len, 0, p.length() - start);
    for (auto& ch : p.data) s.data.emplace_back(ch.begin() + start, ch.begin() + start + len);
    return s;
}

CaptureResult processCapture(const Pcm& rec, const Settings& s)
{
    CaptureResult r;
    const int n = rec.length();
    if (n < rec.sampleRate * 1.0) { r.reason = "kayıt çok kısa"; return r; }

    const std::vector<float> mono = rec.mono();
    double e = 0;
    for (float v : mono) e += (double) v * v;
    if (std::sqrt(e / n) < 0.003) { r.reason = "sessiz"; return r; }

    const Analysis a = analyze(mono.data(), n, rec.sampleRate);
    r.root = a.key.root;
    r.minor = a.key.minor;
    r.confidence = a.key.confidence;
    r.tuningCents = a.key.tuningCents;
    r.fitBefore = scaleFit(a, s.targetRoot, s.targetMode);
    if (a.key.confidence < s.minConfidence) { r.reason = "ton net değil (konuşma/gürültü?)"; return r; }

    r.shift = transposeShift(a.key.root, a.key.minor, s.targetRoot, s.targetMode, &r.overlap);
    if (std::abs(r.shift) > s.maxShift) { r.reason = "gereken kaydırma çok büyük"; return r; }

    const double total = r.shift - a.key.tuningCents / 100.0;
    const double ratio = std::pow(2.0, total / 12.0);
    const int outLen = std::max(1, (int) std::lround(s.lengthSec * rec.sampleRate));
    const bool tape = s.shiftMode == ShiftMode::Tape;
    const int srcWin = std::min(n, (int) std::lround(tape ? outLen * ratio : outLen));

    const int start = bestWindow(mono.data(), n, a, srcWin);

    // kenarlarda bozulma olmaması için pencerenin iki yanından pay alınır
    const int margin = (int) (0.35 * rec.sampleRate);
    const int a0 = std::max(0, start - margin);
    const int a1 = std::min(n, start + srcWin + margin);
    const Pcm shifted = pitchShift(slice(rec, a0, a1 - a0), total, s.shiftMode);

    const int off = tape ? (int) std::lround((start - a0) / ratio) : (start - a0);
    const int len = std::min(outLen, shifted.length() - off);
    r.audio = slice(shifted, off, len);
    if (r.audio.length() < rec.sampleRate * 0.05) { r.reason = "işleme hatası"; return r; }

    // normalize (-1 dBFS) + kısa fade
    float peak = 0;
    for (auto& ch : r.audio.data)
        for (float v : ch) peak = std::max(peak, std::abs(v));
    const float g = peak > 1e-6f ? 0.891f / peak : 1.f;
    const int L = r.audio.length();
    const int fade = std::min(L / 2, (int) (s.fadeMs * 0.001 * rec.sampleRate));
    for (auto& ch : r.audio.data) {
        for (auto& v : ch) v *= g;
        for (int i = 0; i < fade; ++i) {
            const float w = (float) i / (float) fade;
            ch[i] *= w;
            ch[L - 1 - i] *= w;
        }
    }

    const std::vector<float> outMono = r.audio.mono();
    const Analysis b = analyze(outMono.data(), L, rec.sampleRate);
    r.fitAfter = scaleFit(b, s.targetRoot, s.targetMode);
    r.ok = true;
    return r;
}

} // namespace gk
