#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

// Modüler synth paneli görünümü: açık gri alüminyum, ince siyah çizgiler, siyah düğmeler
class PanelLook : public juce::LookAndFeel_V4 {
public:
    PanelLook();
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void fillTextEditorBackground(juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int w, int h, juce::TextEditor&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int h) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

// Küçük durum ışığı
class Led : public juce::Component {
public:
    void setColour(juce::Colour c) { if (c != colour) { colour = c; repaint(); } }
    void paint(juce::Graphics&) override;

private:
    juce::Colour colour {juce::Colours::grey};
};

// Panel tuşu: yuvarlak siyah tuş + yanında etiket. big = LED'li büyük tuş
class RoundButton : public juce::Button {
public:
    RoundButton(const juce::String& text, bool big) : juce::Button(text), isBig(big) { setButtonText(text); }
    void paintButton(juce::Graphics&, bool over, bool down) override;
    juce::Colour ledColour;

private:
    const bool isBig;
};

// Dışarı sürükleme tutamağı (.adg ya da WAV'lar)
class DragPill : public juce::Component, public juce::SettableTooltipClient {
public:
    DragPill(bool darkStyle) : dark(darkStyle) { setMouseCursor(juce::MouseCursor::DraggingHandCursor); }
    void paint(juce::Graphics&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override { dragging = false; }
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    std::function<juce::StringArray()> filesToDrag; // sürükleme başlarken çağrılır
    juce::String text;
    bool active = false;

private:
    const bool dark;
    bool dragging = false;
};

class PadComponent : public juce::Component {
public:
    PadComponent(GalaksiProcessor& p, int index);
    void update(const gk::SlotInfo& info, bool playing, float playPos);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> waveArea() const;
    GalaksiProcessor& proc;
    const int index;
    gk::SlotInfo info;
    bool playing = false, dragging = false;
    float playPos = 0;
    juce::TextButton lockBtn, refreshBtn;
};

class GalaksiEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit GalaksiEditor(GalaksiProcessor&);
    ~GalaksiEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshNow() { timerCallback(); } // ekransız görüntü aracı için

private:
    void timerCallback() override;
    void layoutPads();
    void chooseFolder();
    void updateSourceMode();
    void setChoice(const char* paramId, int index);
    void syncSegments();
    void commitTexts();

    GalaksiProcessor& proc;
    PanelLook look;
    juce::TooltipWindow tooltips {this, 500};

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // başlık
    juce::TextButton srcRadio {"radyo"}, srcPerc;
    Led busyLed;
    juce::Label busyLabel;

    // perküsyon alanları
    juce::Label termsLabel, exclLabel, licenseLabel, keyLabel;
    juce::TextEditor termsEditor, exclEditor, keyEditor;
    juce::TextButton licAll, licCc0;

    // radyo alanları
    juce::Label tagsLabel, scaleLabel, shiftLabel;
    juce::TextEditor tagsEditor;
    juce::ComboBox rootBox, modeBox;
    std::unique_ptr<CA> rootAtt, modeAtt;
    juce::TextButton shiftQuality, shiftTape;

    struct Knob {
        juce::Label label;
        juce::Slider slider {juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
        std::unique_ptr<SA> att;
    };
    Knob count, length, record, minconf, maxshift, gain;

    RoundButton refreshAllBtn {"kilitsizleri yenile", true}, stopAllBtn {"durdur", false},
        folderBtn {juce::String::fromUTF8("klasör…"), false}, openFolderBtn {juce::String::fromUTF8("klasörü aç"), false};

    DragPill adgPill {true}, wavPill {false};
    juce::Label statusLabel, hintLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    std::vector<std::unique_ptr<PadComponent>> pads;
    int lastVersion = -1, lastCount = -1, lastSource = -1, tick = 0;
    int sep1 = 0, sep2 = 0; // kontrol panelindeki dikey çizgiler

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GalaksiEditor)
};
