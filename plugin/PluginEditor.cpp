#include "PluginEditor.h"
#include "Analysis.h"
#include "Text.h"

namespace {
const juce::Colour kBg(0xff0a0d1a), kPanel(0xff141a2e), kPanel2(0xff1b2340), kLine(0xff2a3358);
const juce::Colour kText(0xffe6e9f5), kDim(0xff8a92b2), kAccent(0xff8b7cff), kCyan(0xff4fd1c5), kWarn(0xffff6b6b),
    kGold(0xffffc857);
} // namespace

// ---------------------------------------------------------------- görünüm
GalaksiLook::GalaksiLook()
{
    setColour(juce::ResizableWindow::backgroundColourId, kBg);
    setColour(juce::TextButton::buttonColourId, kPanel2);
    setColour(juce::TextButton::buttonOnColourId, kGold.darker(0.2f));
    setColour(juce::TextButton::textColourOffId, kText);
    setColour(juce::TextButton::textColourOnId, kBg);
    setColour(juce::ComboBox::backgroundColourId, kPanel2);
    setColour(juce::ComboBox::outlineColourId, kLine);
    setColour(juce::ComboBox::textColourId, kText);
    setColour(juce::ComboBox::arrowColourId, kDim);
    setColour(juce::PopupMenu::backgroundColourId, kPanel);
    setColour(juce::PopupMenu::textColourId, kText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kAccent);
    setColour(juce::TextEditor::backgroundColourId, kPanel2);
    setColour(juce::TextEditor::outlineColourId, kLine);
    setColour(juce::TextEditor::focusedOutlineColourId, kAccent);
    setColour(juce::TextEditor::textColourId, kText);
    setColour(juce::Label::textColourId, kText);
    setColour(juce::Slider::textBoxTextColourId, kText);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::trackColourId, kAccent);
}

void GalaksiLook::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& bgColour, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    auto c = b.getToggleState() ? b.findColour(juce::TextButton::buttonOnColourId) : bgColour;
    if (down) c = c.brighter(0.25f);
    else if (over) c = c.brighter(0.1f);
    g.setColour(c);
    g.fillRoundedRectangle(r, 5.f);
    g.setColour(kLine.brighter(over ? 0.4f : 0.f));
    g.drawRoundedRectangle(r, 5.f, 1.f);
}

void GalaksiLook::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                   juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearBar) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    auto r = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h);
    g.setColour(kPanel2);
    g.fillRoundedRectangle(r, 5.f);
    auto fill = r.withRight(pos);
    juce::ColourGradient grad(kAccent.withAlpha(0.85f), r.getX(), 0, kCyan.withAlpha(0.85f), r.getRight(), 0, false);
    g.setGradientFill(grad);
    g.saveState();
    g.reduceClipRegion(fill.toNearestInt());
    g.fillRoundedRectangle(r, 5.f);
    g.restoreState();
    g.setColour(kLine);
    g.drawRoundedRectangle(r.reduced(0.5f), 5.f, 1.f);
}

juce::Font GalaksiLook::getTextButtonFont(juce::TextButton&, int h)
{
    return juce::Font(juce::FontOptions((float) juce::jmin(14, (int) (h * 0.55f)), juce::Font::bold));
}

// ---------------------------------------------------------------- pad
PadComponent::PadComponent(GalaksiProcessor& p, int i) : proc(p), index(i)
{
    lockBtn.setButtonText(TR("KİLİT"));
    lockBtn.setClickingTogglesState(true);
    lockBtn.setTooltip(TR("Kilitli yuvalar yenilenmez"));
    lockBtn.onClick = [this] { proc.engine.setLocked(index, lockBtn.getToggleState()); };
    refreshBtn.setButtonText(TR("YENİ"));
    refreshBtn.onClick = [this] {
        if (info.state == gk::SlotInfo::State::Searching) proc.engine.stop(index);
        else proc.refreshSlot(index);
    };
    addAndMakeVisible(lockBtn);
    addAndMakeVisible(refreshBtn);
}

void PadComponent::update(const gk::SlotInfo& i, bool pl, float pos)
{
    info = i;
    playing = pl;
    playPos = pos;
    lockBtn.setToggleState(info.locked, juce::dontSendNotification);
    refreshBtn.setButtonText(info.state == gk::SlotInfo::State::Searching ? TR("DUR") : TR("YENİ"));
    refreshBtn.setEnabled(!info.locked);
    repaint();
}

juce::Rectangle<int> PadComponent::waveArea() const
{
    return getLocalBounds().reduced(8).withTrimmedTop(30).withTrimmedBottom(38);
}

void PadComponent::resized()
{
    auto top = getLocalBounds().reduced(8).removeFromTop(22);
    refreshBtn.setBounds(top.removeFromRight(48));
    top.removeFromRight(4);
    lockBtn.setBounds(top.removeFromRight(52));
}

void PadComponent::paint(juce::Graphics& g)
{
    using S = gk::SlotInfo::State;
    auto r = getLocalBounds().toFloat().reduced(2.f);
    const bool searching = info.state == S::Searching;
    const double t = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    g.setColour(kPanel);
    g.fillRoundedRectangle(r, 8.f);
    juce::Colour border = info.locked ? kGold
                          : info.state == S::Ready ? kAccent.withAlpha(0.7f)
                          : info.state == S::Failed ? kWarn.withAlpha(0.8f)
                          : kLine;
    if (searching) border = kCyan.withAlpha(0.35f + 0.35f * (float) std::sin(t * 4.0 + index));
    if (playing) border = kCyan;
    g.setColour(border);
    g.drawRoundedRectangle(r, 8.f, playing || info.locked ? 2.f : 1.2f);

    // numara + MIDI notası
    auto inner = getLocalBounds().reduced(10);
    auto top = inner.removeFromTop(20);
    g.setColour(kText);
    g.setFont(juce::FontOptions(17.f, juce::Font::bold));
    g.drawText(juce::String(index + 1), top.removeFromLeft(26), juce::Justification::centredLeft);
    g.setColour(kDim);
    g.setFont(juce::FontOptions(11.f));
    g.drawText(juce::MidiMessage::getMidiNoteName(GalaksiProcessor::FirstMidiNote + index, true, true, 3),
               top.removeFromLeft(40), juce::Justification::centredLeft);

    // dalga formu
    auto wa = waveArea().toFloat();
    g.setColour(kBg.withAlpha(0.6f));
    g.fillRoundedRectangle(wa, 5.f);
    if (!info.peaks.empty() && info.state != S::Empty) {
        const int n = (int) info.peaks.size();
        const float mid = wa.getCentreY(), half = wa.getHeight() * 0.45f;
        juce::Path p;
        for (int i = 0; i < n; ++i) {
            const float x = wa.getX() + wa.getWidth() * (i + 0.5f) / n;
            const float a = juce::jmax(0.01f, info.peaks[(size_t) i]) * half;
            p.addRectangle(x - 0.8f, mid - a, 1.6f, a * 2.f);
        }
        juce::ColourGradient grad(kAccent, wa.getX(), 0, kCyan, wa.getRight(), 0, false);
        g.setGradientFill(grad);
        g.setOpacity(searching ? 0.35f : 1.f);
        g.fillPath(p);
        g.setOpacity(1.f);
        if (playing) {
            const float x = wa.getX() + wa.getWidth() * juce::jlimit(0.f, 1.f, playPos);
            g.setColour(kText);
            g.drawLine(x, wa.getY(), x, wa.getBottom(), 1.5f);
        }
    }
    if (searching || info.state == S::Empty || info.state == S::Failed) {
        g.setColour(info.state == S::Failed ? kWarn : kDim);
        g.setFont(juce::FontOptions(12.f));
        juce::String s = info.status;
        if (searching) {
            const int dots = (int) (t * 3) % 4;
            s = info.status + juce::String::repeatedString(".", dots);
            if (info.attempts > 1) s << "  (" << info.attempts << ". deneme)";
        }
        g.drawFittedText(s, waveArea().reduced(6), juce::Justification::centred, 3);
    }

    // alt bilgi
    auto bottom = getLocalBounds().reduced(10).removeFromBottom(34);
    if (info.audio) {
        g.setColour(kText);
        g.setFont(juce::FontOptions(12.f, juce::Font::bold));
        juce::String st = info.station;
        if (info.country.isNotEmpty()) st << " (" << info.country << ")";
        g.drawText(st, bottom.removeFromTop(17), juce::Justification::centredLeft, true);
        g.setColour(kDim);
        g.setFont(juce::FontOptions(11.f));
        juce::String k = info.origKey;
        k << (info.shift == 0 ? TR("  ·  kaydırma yok") : TR("  →  ") + juce::String(info.shift > 0 ? "+" : "") + juce::String(info.shift) + " st");
        k << TR("  ·  uyum %") << juce::roundToInt(info.fit * 100);
        g.drawText(k, bottom, juce::Justification::centredLeft, true);
    }
}

void PadComponent::mouseDown(const juce::MouseEvent&) { dragging = false; }

void PadComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6 || !info.file.existsAsFile()) return;
    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles({info.file.getFullPathName()}, false, this);
}

void PadComponent::mouseUp(const juce::MouseEvent& e)
{
    if (!dragging && info.audio && e.mouseWasClicked()) proc.preview(index);
    dragging = false;
}

// ---------------------------------------------------------------- editör
GalaksiEditor::GalaksiEditor(GalaksiProcessor& p) : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&look);

    juce::Random rng(76);
    for (int i = 0; i < 140; ++i) stars.push_back({rng.nextFloat(), rng.nextFloat()});

    auto setupLabel = [this](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, kDim);
        l.setFont(juce::FontOptions(12.f));
        addAndMakeVisible(l);
    };

    setupLabel(tagsLabel, TR("Türler (virgülle)"));
    setupLabel(scaleLabel, TR("Hedef scale"));
    setupLabel(shiftLabel, TR("Transpoz"));

    tagsEditor.setText(proc.engine.getSettings().tags.joinIntoString(", "), false);
    tagsEditor.setFont(juce::FontOptions(14.f));
    tagsEditor.setTextToShowWhenEmpty("jazz, world music", kDim);
    tagsEditor.onReturnKey = [this] { proc.setTags(tagsEditor.getText()); tagsEditor.unfocusAllComponents(); };
    tagsEditor.onFocusLost = [this] { proc.setTags(tagsEditor.getText()); };
    addAndMakeVisible(tagsEditor);

    for (int i = 0; i < 12; ++i) rootBox.addItem(gk::noteName(i), i + 1);
    for (int i = 0; i < gk::NumModes; ++i) modeBox.addItem(TR(gk::modeNameTr(i)), i + 1);
    shiftModeBox.addItem(TR("Kaliteli"), 1);
    shiftModeBox.addItem(TR("Bant (hız değişir)"), 2);
    for (auto* b : {&rootBox, &modeBox, &shiftModeBox}) addAndMakeVisible(*b);
    rootAtt = std::make_unique<CA>(proc.params, "root", rootBox);
    modeAtt = std::make_unique<CA>(proc.params, "mode", modeBox);
    shiftModeAtt = std::make_unique<CA>(proc.params, "shiftmode", shiftModeBox);

    auto setupKnob = [&](Knob& k, const char* id, const juce::String& name, int decimals, const juce::String& suffix) {
        setupLabel(k.label, name);
        k.slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 0, 0);
        k.slider.setNumDecimalPlacesToDisplay(decimals);
        k.slider.setTextValueSuffix(suffix);
        k.slider.setColour(juce::Slider::textBoxTextColourId, kText);
        addAndMakeVisible(k.slider);
        k.att = std::make_unique<SA>(proc.params, id, k.slider);
    };
    setupKnob(count, "count", TR("Sample sayısı"), 0, "");
    setupKnob(length, "length", TR("Uzunluk"), 2, " s");
    setupKnob(record, "record", TR("Dinleme süresi"), 0, " s");
    setupKnob(minconf, "minconf", TR("Ton güveni (seçicilik)"), 2, "");
    setupKnob(maxshift, "maxshift", TR("Maks kaydırma"), 0, " st");
    setupKnob(gain, "gain", TR("Önizleme sesi"), 1, " dB");

    refreshAllBtn.setButtonText(TR("KİLİTSİZLERİ YENİLE"));
    refreshAllBtn.setColour(juce::TextButton::buttonColourId, kAccent.darker(0.35f));
    refreshAllBtn.onClick = [this] { proc.setTags(tagsEditor.getText()); proc.refreshAllUnlocked(); };
    stopAllBtn.setButtonText(TR("DURDUR"));
    stopAllBtn.onClick = [this] {
        for (int i = 0; i < gk::SlotEngine::MaxSlots; ++i) proc.engine.stop(i);
        proc.stopPreview();
    };
    folderBtn.setButtonText(TR("KLASÖR…"));
    folderBtn.onClick = [this] { chooseFolder(); };
    openFolderBtn.setButtonText(TR("KLASÖRÜ AÇ"));
    openFolderBtn.onClick = [this] {
        auto f = proc.engine.getSettings().folder;
        f.createDirectory();
        f.startAsProcess();
    };
    for (auto* b : {&refreshAllBtn, &stopAllBtn, &folderBtn, &openFolderBtn}) addAndMakeVisible(*b);

    folderLabel.setColour(juce::Label::textColourId, kDim);
    folderLabel.setFont(juce::FontOptions(12.f));
    addAndMakeVisible(folderLabel);
    updateFolderLabel();

    statusLabel.setFont(juce::FontOptions(13.f));
    statusLabel.setColour(juce::Label::textColourId, kText);
    addAndMakeVisible(statusLabel);
    hintLabel.setFont(juce::FontOptions(11.f));
    hintLabel.setColour(juce::Label::textColourId, kDim);
    hintLabel.setJustificationType(juce::Justification::centredRight);
    hintLabel.setText(TR("Pad'e tıkla: dinle  ·  sürükle: Ableton'a bırak  ·  MIDI C1–D#2 ile çal"), juce::dontSendNotification);
    addAndMakeVisible(hintLabel);

    for (int i = 0; i < gk::SlotEngine::MaxSlots; ++i) {
        pads.push_back(std::make_unique<PadComponent>(proc, i));
        addChildComponent(*pads.back());
    }

    setSize(1020, 720);
    startTimerHz(20);
}

GalaksiEditor::~GalaksiEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void GalaksiEditor::updateFolderLabel()
{
    folderLabel.setText(TR("WAV klasörü: ") + proc.engine.getSettings().folder.getFullPathName(), juce::dontSendNotification);
}

void GalaksiEditor::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser>(TR("Sample'ların kaydedileceği klasör"), proc.engine.getSettings().folder);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                         [this](const juce::FileChooser& fc) {
                             auto f = fc.getResult();
                             if (f != juce::File()) {
                                 proc.setFolder(f);
                                 updateFolderLabel();
                             }
                         });
}

void GalaksiEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
    // yıldızlar
    for (size_t i = 0; i < stars.size(); ++i) {
        const float a = 0.15f + 0.35f * (float) ((i * 37) % 10) / 10.f;
        g.setColour(juce::Colours::white.withAlpha(a));
        const float s = (i % 7 == 0) ? 2.f : 1.2f;
        g.fillEllipse(stars[i].x * getWidth(), stars[i].y * getHeight(), s, s);
    }
    // başlık
    g.setColour(kText);
    g.setFont(juce::FontOptions(28.f, juce::Font::bold));
    g.drawText(TR("GALAKSİ"), 20, 12, 200, 34, juce::Justification::centredLeft);
    g.setColour(kDim);
    g.setFont(juce::FontOptions(13.f));
    g.drawText(TR("canlı radyo sampler  ·  her şey tek scale'de"), 170, 18, 400, 24, juce::Justification::centredLeft);

    const int busy = proc.engine.busyCount();
    if (busy > 0) {
        g.setColour(kCyan);
        g.drawText(juce::String(busy) + TR(" yuva radyo dinliyor"), getWidth() - 320, 18, 300, 24, juce::Justification::centredRight);
    }

    // kontrol paneli
    g.setColour(kPanel.withAlpha(0.85f));
    g.fillRoundedRectangle(14.f, 56.f, (float) getWidth() - 28.f, 156.f, 10.f);
    g.setColour(kLine);
    g.drawRoundedRectangle(14.f, 56.f, (float) getWidth() - 28.f, 156.f, 10.f, 1.f);
}

void GalaksiEditor::resized()
{
    const int x0 = 28, w = getWidth() - 56;
    // satır 1
    {
        auto row = juce::Rectangle<int>(x0, 64, w, 50);
        auto col = [&](int width) { auto c = row.removeFromLeft(width); row.removeFromLeft(14); return c; };
        auto c1 = col(380);
        tagsLabel.setBounds(c1.removeFromTop(18));
        tagsEditor.setBounds(c1.reduced(0, 2));
        auto c2 = col(250);
        scaleLabel.setBounds(c2.removeFromTop(18));
        rootBox.setBounds(c2.removeFromLeft(70).reduced(0, 2));
        c2.removeFromLeft(6);
        modeBox.setBounds(c2.reduced(0, 2));
        auto c3 = col(200);
        shiftLabel.setBounds(c3.removeFromTop(18));
        shiftModeBox.setBounds(c3.reduced(0, 2));
    }
    // satır 2
    {
        auto row = juce::Rectangle<int>(x0, 120, w, 46);
        const int kw = (w - 5 * 12) / 6;
        for (auto* k : {&count, &length, &record, &minconf, &maxshift, &gain}) {
            auto c = row.removeFromLeft(kw);
            row.removeFromLeft(12);
            k->label.setBounds(c.removeFromTop(18));
            k->slider.setBounds(c.reduced(0, 2));
        }
    }
    // satır 3
    {
        auto row = juce::Rectangle<int>(x0, 172, w, 30);
        refreshAllBtn.setBounds(row.removeFromLeft(200));
        row.removeFromLeft(8);
        stopAllBtn.setBounds(row.removeFromLeft(90));
        row.removeFromLeft(8);
        folderBtn.setBounds(row.removeFromLeft(90));
        row.removeFromLeft(8);
        openFolderBtn.setBounds(row.removeFromLeft(110));
        row.removeFromLeft(12);
        folderLabel.setBounds(row);
    }
    auto status = getLocalBounds().removeFromBottom(30).reduced(20, 4);
    hintLabel.setBounds(status.removeFromRight(440));
    statusLabel.setBounds(status);
    layoutPads();
}

void GalaksiEditor::layoutPads()
{
    const int n = proc.slotCount();
    lastCount = n;
    const int cols = n <= 4 ? n : (n <= 8 ? 4 : 4);
    const int rows = (n + cols - 1) / cols;
    auto area = juce::Rectangle<int>(14, 222, getWidth() - 28, getHeight() - 222 - 34);
    const int gap = 10;
    const int pw = (area.getWidth() - (cols - 1) * gap) / cols;
    const int ph = juce::jmin(170, (area.getHeight() - (rows - 1) * gap) / rows);
    for (int i = 0; i < (int) pads.size(); ++i) {
        auto& p = *pads[(size_t) i];
        if (i < n) {
            const int r = i / cols, c = i % cols;
            p.setBounds(area.getX() + c * (pw + gap), area.getY() + r * (ph + gap), pw, ph);
            p.setVisible(true);
        } else {
            p.setVisible(false);
        }
    }
}

void GalaksiEditor::timerCallback()
{
    ++tick;
    if (proc.slotCount() != lastCount) layoutPads();
    const int v = proc.engine.version();
    const bool changed = v != lastVersion;
    lastVersion = v;
    for (int i = 0; i < lastCount; ++i) {
        const bool playing = proc.isPlaying(i);
        auto info = proc.engine.getSlot(i);
        if (changed || playing || info.state == gk::SlotInfo::State::Searching)
            pads[(size_t) i]->update(info, playing, proc.playPosition(i));
    }
    if (changed) {
        statusLabel.setText(proc.engine.lastMessage(), juce::dontSendNotification);
        repaint(getWidth() - 340, 10, 330, 36);
    }
    if (tick % 10 == 0) repaint(getWidth() - 340, 10, 330, 36);
}
