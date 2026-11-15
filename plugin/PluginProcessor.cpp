#include "PluginProcessor.h"
#include "Analysis.h"
#include "PluginEditor.h"
#include "Text.h"

GalaksiProcessor::GalaksiProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "PARAMS", createLayout())
{
    for (auto& t : uiTrigger) t = 0;
    engine.onSlotChanged = [this](int s) { slotChanged(s); };
    pushSettingsToEngine();
}

GalaksiProcessor::~GalaksiProcessor()
{
    engine.onSlotChanged = nullptr;
}

juce::AudioProcessorValueTreeState::ParameterLayout GalaksiProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    StringArray notes, modes;
    for (int i = 0; i < 12; ++i) notes.add(gk::noteName(i));
    for (int i = 0; i < gk::NumModes; ++i) modes.add(TR(gk::modeNameTr(i)));

    l.add(std::make_unique<AudioParameterInt>(ParameterID {"count", 1}, TR("Sample sayısı"), 1, 16, 8));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID {"length", 1}, TR("Uzunluk"),
                                                NormalisableRange<float>(0.1f, 16.f, 0.01f, 0.4f), 2.f,
                                                AudioParameterFloatAttributes().withLabel("s")));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID {"root", 1}, TR("Kök nota"), notes, 0));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID {"mode", 1}, TR("Mod"), modes, 1));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID {"minconf", 1}, TR("Ton güveni"),
                                                NormalisableRange<float>(0.3f, 0.9f, 0.01f), 0.55f));
    l.add(std::make_unique<AudioParameterInt>(ParameterID {"maxshift", 1}, TR("Maks kaydırma"), 0, 6, 6));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID {"shiftmode", 1}, TR("Transpoz modu"),
                                                 StringArray {TR("Kaliteli"), TR("Bant (hız değişir)")}, 0));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID {"record", 1}, TR("Dinleme süresi"),
                                                NormalisableRange<float>(6.f, 30.f, 1.f), 12.f,
                                                AudioParameterFloatAttributes().withLabel("s")));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID {"gain", 1}, TR("Ses"),
                                                NormalisableRange<float>(-30.f, 6.f, 0.1f), 0.f,
                                                AudioParameterFloatAttributes().withLabel("dB")));
    return l;
}

int GalaksiProcessor::slotCount() const
{
    return juce::jlimit(1, 16, (int) params.getRawParameterValue("count")->load());
}

void GalaksiProcessor::pushSettingsToEngine()
{
    auto es = engine.getSettings();
    es.core.targetRoot = (int) params.getRawParameterValue("root")->load();
    es.core.targetMode = (int) params.getRawParameterValue("mode")->load();
    es.core.lengthSec = params.getRawParameterValue("length")->load();
    es.core.minConfidence = params.getRawParameterValue("minconf")->load();
    es.core.maxShift = (int) params.getRawParameterValue("maxshift")->load();
    es.core.shiftMode = params.getRawParameterValue("shiftmode")->load() > 0.5f ? gk::ShiftMode::Tape : gk::ShiftMode::Quality;
    es.recordSeconds = juce::jmax((double) params.getRawParameterValue("record")->load(), es.core.lengthSec + 3.0);
    engine.setSettings(es);
}

void GalaksiProcessor::setTags(const juce::String& s)
{
    auto es = engine.getSettings();
    es.tags = juce::StringArray::fromTokens(s, ",", "");
    es.tags.trim();
    es.tags.removeEmptyStrings();
    engine.setSettings(es);
}

void GalaksiProcessor::setFolder(const juce::File& f)
{
    auto es = engine.getSettings();
    es.folder = f;
    engine.setSettings(es);
}

void GalaksiProcessor::slotChanged(int slot)
{
    double sr = 44100;
    auto buf = engine.getAudio(slot, sr);
    const juce::SpinLock::ScopedLockType sl(bufferLock);
    slotBuffers[(size_t) slot] = std::move(buf);
    slotRates[(size_t) slot] = sr;
}

void GalaksiProcessor::preview(int slot)
{
    if (slot < 0 || slot >= gk::SlotEngine::MaxSlots) return;
    uiTrigger[(size_t) slot] = voices[(size_t) slot].playingFlag.load() ? 2 : 1;
}

void GalaksiProcessor::stopPreview() { stopAll = true; }

void GalaksiProcessor::prepareToPlay(double sampleRate, int)
{
    hostRate = sampleRate > 0 ? sampleRate : 44100;
}

bool GalaksiProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void GalaksiProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();
    const int outCh = buffer.getNumChannels();
    const float master = juce::Decibels::decibelsToGain(params.getRawParameterValue("gain")->load());

    auto start = [this](int slot, float vel) {
        const juce::SpinLock::ScopedTryLockType tl(bufferLock);
        if (!tl.isLocked()) return false;
        auto& v = voices[(size_t) slot];
        v.buffer = slotBuffers[(size_t) slot];
        if (!v.buffer || v.buffer->getNumSamples() == 0) { v.active = false; return true; }
        v.pos = 0;
        v.step = slotRates[(size_t) slot] / hostRate;
        v.gain = vel;
        v.active = true;
        return true;
    };

    if (stopAll.exchange(false))
        for (auto& v : voices) v.active = false;

    for (int s = 0; s < gk::SlotEngine::MaxSlots; ++s) {
        const int t = uiTrigger[(size_t) s].load();
        if (t == 0) continue;
        if (t == 2) { voices[(size_t) s].active = false; uiTrigger[(size_t) s] = 0; }
        else if (start(s, 1.f)) uiTrigger[(size_t) s] = 0;
    }

    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) {
            const int slot = m.getNoteNumber() - FirstMidiNote;
            if (slot >= 0 && slot < gk::SlotEngine::MaxSlots) start(slot, m.getFloatVelocity());
        }
    }
    midi.clear();

    for (auto& v : voices) {
        if (!v.active || !v.buffer) { v.playingFlag = false; continue; }
        const auto& b = *v.buffer;
        const int len = b.getNumSamples();
        const int srcCh = b.getNumChannels();
        const float g = v.gain * master;
        for (int i = 0; i < numSamples; ++i) {
            const int i0 = (int) v.pos;
            if (i0 >= len - 1) { v.active = false; break; }
            const float frac = (float) (v.pos - i0);
            for (int c = 0; c < outCh; ++c) {
                const int sc = juce::jmin(c, srcCh - 1);
                const float a = b.getSample(sc, i0), bb = b.getSample(sc, i0 + 1);
                float val = a + frac * (bb - a);
                if (outCh == 1 && srcCh == 2) val = 0.5f * (val + (b.getSample(1, i0) + frac * (b.getSample(1, i0 + 1) - b.getSample(1, i0))));
                buffer.addSample(c, i, val * g);
            }
            v.pos += v.step;
        }
        v.playingFlag = v.active;
        v.displayPos = len > 0 ? (float) (v.pos / len) : 0.f;
    }
}

juce::AudioProcessorEditor* GalaksiProcessor::createEditor() { return new GalaksiEditor(*this); }

void GalaksiProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree root("GALAKSI");
    root.appendChild(params.copyState(), nullptr);
    root.appendChild(engine.saveState(), nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary(*xml, destData);
}

void GalaksiProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (!xml) return;
    auto root = juce::ValueTree::fromXml(*xml);
    if (!root.hasType("GALAKSI")) return;
    auto p = root.getChildWithName(params.state.getType());
    if (p.isValid()) params.replaceState(p);
    engine.loadState(root.getChildWithName("ENGINE"));
    pushSettingsToEngine();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GalaksiProcessor(); }
