// Geliştirme aracı: arayüzü ekransız çizip PNG'ye kaydeder (cmake -DGK_BUILD_SNAPSHOT=ON)
#include "PluginEditor.h"
#include "PluginProcessor.h"

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String outDir = argc > 1 ? argv[1] : "/tmp";
    GalaksiProcessor proc;
    // örnek yuvalar: argv[2] klasöründeki WAV'lar (varsa)
    if (argc > 2) {
        juce::ValueTree eng("ENGINE");
        auto wavs = juce::File(argv[2]).findChildFiles(juce::File::findFiles, false, "*.wav");
        wavs.sort();
        const char* terms[] = {"darbuka", "bendir", "gong", "anvil", "door slam", "tabla", "pipe hit", "pot lid"};
        for (int i = 0; i < wavs.size() && i < 16; ++i) {
            if (i == 2 || i == 11) continue; // boş bırak
            juce::ValueTree c("SLOT");
            c.setProperty("index", i, nullptr);
            c.setProperty("locked", i == 6, nullptr);
            c.setProperty("file", wavs[i].getFullPathName(), nullptr);
            c.setProperty("station", wavs[i].getFileNameWithoutExtension(), nullptr);
            c.setProperty("country", "user" + juce::String(i), nullptr);
            c.setProperty("tag", terms[i % 8], nullptr);
            c.setProperty("percussion", true, nullptr);
            c.setProperty("license", i % 2 ? "CC-BY" : "CC0", nullptr);
            eng.appendChild(c, nullptr);
        }
        proc.engine.loadState(eng);
    }
    for (int mode = 0; mode < 2; ++mode) {
        if (auto* p = proc.params.getParameter("source")) p->setValueNotifyingHost(p->convertTo0to1((float) mode));
        if (auto* p = proc.params.getParameter("count")) p->setValueNotifyingHost(p->convertTo0to1(16.f));
        std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
        if (auto* ge = dynamic_cast<GalaksiEditor*>(ed.get())) ge->refreshNow();
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        juce::File f(outDir + (mode ? "/galaksi_perc.png" : "/galaksi_radyo.png"));
        f.deleteFile();
        juce::FileOutputStream os(f);
        juce::PNGImageFormat().writeImageToStream(img, os);
        std::printf("%s\n", f.getFullPathName().toRawUTF8());
    }
    juce::String err;
    const auto adg = proc.writeDrumRack(err);
    std::printf("adg: %s %s\n", adg.getFullPathName().toRawUTF8(), err.toRawUTF8());
    return 0;
}
