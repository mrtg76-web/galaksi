#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

class GalaksiLook : public juce::LookAndFeel_V4 {
public:
    GalaksiLook();
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int h) override;
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

// Bütün hazır sample'ları tek seferde dışarı sürüklemek için tutamak
class DragAllComponent : public juce::Component {
public:
    explicit DragAllComponent(GalaksiProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override { dragging = false; }
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    int count = 0;

private:
    GalaksiProcessor& proc;
    bool dragging = false;
};

class GalaksiEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit GalaksiEditor(GalaksiProcessor&);
    ~GalaksiEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void layoutPads();
    void chooseFolder();
    void updateFolderLabel();

    GalaksiProcessor& proc;
    GalaksiLook look;
    juce::TooltipWindow tooltips {this, 500};

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    juce::Label tagsLabel, scaleLabel, shiftLabel, outputLabel;
    juce::TextEditor tagsEditor;
    juce::ComboBox rootBox, modeBox, shiftModeBox, outputBox;
    std::unique_ptr<CA> rootAtt, modeAtt, shiftModeAtt, outputAtt;

    struct Knob {
        juce::Label label;
        juce::Slider slider {juce::Slider::LinearBar, juce::Slider::TextBoxLeft};
        std::unique_ptr<SA> att;
    };
    Knob count, length, record, minconf, maxshift, gain;

    juce::TextButton refreshAllBtn, stopAllBtn, folderBtn, openFolderBtn;
    DragAllComponent dragAll {proc};
    juce::Label folderLabel, statusLabel, hintLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    std::vector<std::unique_ptr<PadComponent>> pads;
    int lastVersion = -1, lastCount = -1, lastOutput = -1, tick = 0;
    void updateOutputMode();
    std::vector<juce::Point<float>> stars;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GalaksiEditor)
};
