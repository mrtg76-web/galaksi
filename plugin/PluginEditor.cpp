#include "PluginEditor.h"
#include "Analysis.h"
#include "Text.h"

namespace {
const juce::Colour kPanel(0xffdeded9), kInk(0xff1c1c1c), kField(0xfff1f1ed), kDim(0xff4a4a46), kShadow(0xff8a8a85),
    kScrew(0xffbdbdb7), kGreen(0xff2fbf4a), kAmber(0xfff0a500), kRed(0xffe5483b), kLedOff(0xffb3b3ad);

juce::Font uiFont(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}

constexpr int kPanelTop = 64, kPanelH = 168, kPadTop = 242;
} // namespace

// ---------------------------------------------------------------- görünüm
PanelLook::PanelLook()
{
    setColour(juce::ResizableWindow::backgroundColourId, kPanel);
    setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::buttonOnColourId, kInk);
    setColour(juce::TextButton::textColourOffId, kInk);
    setColour(juce::TextButton::textColourOnId, kPanel);
    setColour(juce::ComboBox::backgroundColourId, kField);
    setColour(juce::ComboBox::outlineColourId, kInk);
    setColour(juce::ComboBox::textColourId, kInk);
    setColour(juce::ComboBox::arrowColourId, kInk);
    setColour(juce::PopupMenu::backgroundColourId, kField);
    setColour(juce::PopupMenu::textColourId, kInk);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kInk);
    setColour(juce::PopupMenu::highlightedTextColourId, kPanel);
    setColour(juce::TextEditor::backgroundColourId, kField);
    setColour(juce::TextEditor::outlineColourId, kInk);
    setColour(juce::TextEditor::focusedOutlineColourId, kInk);
    setColour(juce::TextEditor::textColourId, kInk);
    setColour(juce::TextEditor::highlightColourId, kInk.withAlpha(0.2f));
    setColour(juce::CaretComponent::caretColourId, kInk);
    setColour(juce::Label::textColourId, kInk);
    setColour(juce::Slider::textBoxTextColourId, kDim);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId, kInk.withAlpha(0.2f));
    setColour(juce::TooltipWindow::backgroundColourId, kInk);
    setColour(juce::TooltipWindow::textColourId, kPanel);
    setColour(juce::TooltipWindow::outlineColourId, kInk);
}

void PanelLook::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(0.75f);
    const float alpha = b.isEnabled() ? 1.f : 0.35f;
    if (b.getToggleState()) {
        g.setColour(kInk.withAlpha(alpha));
        g.fillRoundedRectangle(r, 2.5f);
    } else if (over || down) {
        g.setColour(kInk.withAlpha(down ? 0.16f : 0.07f));
        g.fillRoundedRectangle(r, 2.5f);
    }
    g.setColour(kInk.withAlpha(alpha));
    g.drawRoundedRectangle(r, 2.5f, 1.3f);
}

void PanelLook::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont(getTextButtonFont(b, b.getHeight()));
    g.setColour((b.getToggleState() ? kPanel : kInk).withAlpha(b.isEnabled() ? 1.f : 0.35f));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(3, 1), juce::Justification::centred, 1);
}

juce::Font PanelLook::getTextButtonFont(juce::TextButton&, int h)
{
    return uiFont(juce::jlimit(11.f, 14.f, h * 0.55f), true);
}

juce::Font PanelLook::getComboBoxFont(juce::ComboBox&) { return uiFont(14.f); }
juce::Font PanelLook::getPopupMenuFont() { return uiFont(14.f); }

void PanelLook::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&)
{
    const auto c = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).getCentre();
    const float R = juce::jmin(w, h) * 0.5f - 1.f;
    const float knobR = R * 0.72f;

    // ölçek çentikleri
    g.setColour(kInk);
    for (int i = 0; i <= 10; ++i) {
        const float a = start + (end - start) * (float) i / 10.f;
        const float s = std::sin(a), co = -std::cos(a);
        g.drawLine(c.x + s * R * 0.86f, c.y + co * R * 0.86f, c.x + s * R, c.y + co * R, 1.4f);
    }
    // gölge + siyah gövde
    g.setColour(kShadow);
    g.fillEllipse(c.x - knobR, c.y - knobR + 3.f, knobR * 2, knobR * 2);
    g.setColour(kInk);
    g.fillEllipse(c.x - knobR, c.y - knobR, knobR * 2, knobR * 2);
    g.setColour(juce::Colours::black);
    g.drawEllipse(c.x - knobR, c.y - knobR, knobR * 2, knobR * 2, 1.2f);
    // beyaz gösterge çizgisi
    const float a = start + pos * (end - start);
    const float s = std::sin(a), co = -std::cos(a);
    g.setColour(juce::Colour(0xfff6f6f2));
    juce::Path p;
    p.startNewSubPath(c.x + s * knobR * 0.2f, c.y + co * knobR * 0.2f);
    p.lineTo(c.x + s * knobR * 0.86f, c.y + co * knobR * 0.86f);
    g.strokePath(p, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void PanelLook::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float>(0, 0, (float) w, (float) h).reduced(0.75f);
    g.setColour(kField);
    g.fillRoundedRectangle(r, 3.f);
    g.setColour(kInk.withAlpha(box.isEnabled() ? 1.f : 0.4f));
    g.drawRoundedRectangle(r, 3.f, 1.5f);
    juce::Path arrow;
    const float ax = (float) w - 14.f, ay = h * 0.5f;
    arrow.addTriangle(ax - 4.f, ay - 2.f, ax + 4.f, ay - 2.f, ax, ay + 3.f);
    g.fillPath(arrow);
}

void PanelLook::fillTextEditorBackground(juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour(kField);
    g.fillRoundedRectangle(0.f, 0.f, (float) w, (float) h, 3.f);
}

void PanelLook::drawTextEditorOutline(juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour(kInk);
    g.drawRoundedRectangle(0.75f, 0.75f, w - 1.5f, h - 1.5f, 3.f, e.hasKeyboardFocus(true) ? 2.2f : 1.5f);
}

// ---------------------------------------------------------------- küçük parçalar
void Led::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto c = r.getCentre();
    const float d = juce::jmin(r.getWidth(), r.getHeight()) * 0.55f;
    if (colour != kLedOff) {
        g.setColour(colour.withAlpha(0.25f));
        g.fillEllipse(c.x - d, c.y - d, d * 2, d * 2);
    }
    g.setColour(colour);
    g.fillEllipse(c.x - d * 0.55f, c.y - d * 0.55f, d * 1.1f, d * 1.1f);
}

void RoundButton::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds();
    const float d = isBig ? 40.f : 28.f;
    const float x = isBig ? 0.f : 6.f, y = (r.getHeight() - d) * 0.5f;
    const float press = down ? 2.f : 0.f;
    g.setColour(kShadow);
    g.fillEllipse(x, y + 3.f, d, d);
    g.setColour(over ? kInk.brighter(0.15f) : kInk);
    g.fillEllipse(x, y + press, d, d);
    g.setColour(juce::Colours::black);
    g.drawEllipse(x, y + press, d, d, 1.2f);
    const auto c = juce::Point<float>(x + d * 0.5f, y + press + d * 0.5f);
    if (isBig) {
        const auto led = ledColour.isTransparent() ? kLedOff : ledColour;
        if (led != kLedOff) {
            g.setColour(led.withAlpha(0.3f));
            g.fillEllipse(c.x - 9.f, c.y - 9.f, 18.f, 18.f);
        }
        g.setColour(led);
        g.fillEllipse(c.x - 5.f, c.y - 5.f, 10.f, 10.f);
    } else {
        g.setColour(juce::Colour(0xfff6f6f2));
        g.fillRoundedRectangle(c.x - 5.f, c.y - 1.f, 10.f, 2.f, 1.f);
    }
    g.setColour(kInk);
    g.setFont(uiFont(isBig ? 16.f : 15.f, true));
    g.drawText(getButtonText(), r.withTrimmedLeft((int) (x + d + 10.f)), juce::Justification::centredLeft, true);
}

void DragPill::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(0.75f);
    const float rad = r.getHeight() * 0.5f;
    const float a = active ? 1.f : 0.35f;
    const bool over = isMouseOver() && active;
    if (dark) {
        g.setColour(kInk.withAlpha(a).brighter(over ? 0.15f : 0.f));
        g.fillRoundedRectangle(r, rad);
        // jak simgesi
        const float jd = r.getHeight() - 10.f;
        const auto jc = juce::Point<float>(r.getX() + 5.f + jd * 0.5f, r.getCentreY());
        g.setColour(juce::Colour(0xff444440));
        g.fillEllipse(jc.x - jd * 0.5f, jc.y - jd * 0.5f, jd, jd);
        g.setColour(kPanel);
        g.drawEllipse(jc.x - jd * 0.5f, jc.y - jd * 0.5f, jd, jd, 2.f);
        g.setColour(juce::Colour(0xff0c0c0b));
        g.fillEllipse(jc.x - 3.f, jc.y - 3.f, 6.f, 6.f);
        g.setColour(kPanel.withAlpha(active ? 1.f : 0.6f));
        g.setFont(uiFont(13.f, true));
        g.drawText(text, r.withTrimmedLeft(jd + 12.f).withTrimmedRight(10.f), juce::Justification::centred, true);
    } else {
        if (over) {
            g.setColour(kInk.withAlpha(0.08f));
            g.fillRoundedRectangle(r, rad);
        }
        g.setColour(kInk.withAlpha(a));
        g.drawRoundedRectangle(r, rad, 1.5f);
        g.setFont(uiFont(12.f, true));
        g.drawText(text, r, juce::Justification::centred, true);
    }
}

void DragPill::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging || !active || e.getDistanceFromDragStart() < 5 || !filesToDrag) return;
    const auto files = filesToDrag();
    if (files.isEmpty()) return;
    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false, this);
}

// ---------------------------------------------------------------- pad
PadComponent::PadComponent(GalaksiProcessor& p, int i) : proc(p), index(i)
{
    lockBtn.setButtonText(TR("kilit"));
    lockBtn.setClickingTogglesState(true);
    lockBtn.setTooltip(TR("Kilitli yuvalar yenilenmez"));
    lockBtn.onClick = [this] { proc.engine.setLocked(index, lockBtn.getToggleState()); };
    refreshBtn.setButtonText(TR("yeni"));
    refreshBtn.setTooltip(TR("Sadece bu yuvayı yenile"));
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
    refreshBtn.setButtonText(info.state == gk::SlotInfo::State::Searching ? TR("dur") : TR("yeni"));
    refreshBtn.setEnabled(!info.locked);
    repaint();
}

juce::Rectangle<int> PadComponent::waveArea() const
{
    auto r = getLocalBounds().reduced(8, 6);
    r.removeFromTop(24);
    return r.removeFromTop(juce::jmax(20, r.getHeight() - 36));
}

void PadComponent::resized()
{
    auto top = getLocalBounds().reduced(8, 6).removeFromTop(19);
    refreshBtn.setBounds(top.removeFromRight(38));
    top.removeFromRight(4);
    lockBtn.setBounds(top.removeFromRight(36));
}

void PadComponent::paint(juce::Graphics& g)
{
    using S = gk::SlotInfo::State;
    const bool searching = info.state == S::Searching;
    const double t = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    auto r = getLocalBounds().toFloat().reduced(1.f);

    if (searching) {
        g.setColour(kInk.withAlpha(0.035f));
        g.fillRoundedRectangle(r, 4.f);
    }
    g.setColour(kInk);
    g.drawRoundedRectangle(r, 4.f, info.locked ? 2.5f : (playing ? 2.f : 1.4f));

    // üst satır: numara, Drum Rack notası, ışık
    auto top = getLocalBounds().reduced(8, 6).removeFromTop(19);
    g.setFont(uiFont(15.f, true));
    g.drawText(juce::String(index + 1), top.removeFromLeft(20), juce::Justification::centredLeft);
    const juce::String note = juce::MidiMessage::getMidiNoteName(GalaksiProcessor::FirstMidiNote + index, true, true, 3);
    auto chip = top.removeFromLeft(juce::jmax(24, (int) (note.length() * 7 + 8))).toFloat().reduced(0, 2);
    g.drawRoundedRectangle(chip, 2.f, 1.f);
    g.setFont(uiFont(11.f, true));
    g.drawText(note, chip, juce::Justification::centred);
    top.removeFromLeft(7);
    juce::Colour led = kLedOff;
    if (searching) led = kAmber.withAlpha(0.55f + 0.45f * (float) (0.5 + 0.5 * std::sin(t * 6.0 + index)));
    else if (info.state == S::Ready) led = playing ? kGreen.brighter(0.4f) : kGreen;
    else if (info.state == S::Failed) led = kRed;
    const auto lc = top.removeFromLeft(10).toFloat().getCentre();
    if (led != kLedOff) {
        g.setColour(led.withAlpha(0.3f));
        g.fillEllipse(lc.x - 6.f, lc.y - 6.f, 12.f, 12.f);
    }
    g.setColour(led);
    g.fillEllipse(lc.x - 3.5f, lc.y - 3.5f, 7.f, 7.f);

    // dalga formu, iki ince çizgi arasında
    auto wa = waveArea().toFloat();
    g.setColour(kInk);
    g.drawLine(wa.getX(), wa.getY(), wa.getRight(), wa.getY(), 1.f);
    g.drawLine(wa.getX(), wa.getBottom(), wa.getRight(), wa.getBottom(), 1.f);
    auto inner = wa.reduced(0, 2);
    if (!info.peaks.empty() && info.state != S::Empty && !searching) {
        const int n = (int) info.peaks.size();
        const float mid = inner.getCentreY(), half = inner.getHeight() * 0.48f;
        juce::Path p;
        p.startNewSubPath(inner.getX(), mid);
        for (int i = 0; i < n; ++i)
            p.lineTo(inner.getX() + inner.getWidth() * (i + 0.5f) / n, mid - juce::jmax(0.02f, info.peaks[(size_t) i]) * half);
        for (int i = n - 1; i >= 0; --i)
            p.lineTo(inner.getX() + inner.getWidth() * (i + 0.5f) / n, mid + juce::jmax(0.02f, info.peaks[(size_t) i]) * half);
        p.closeSubPath();
        g.setColour(kInk);
        g.fillPath(p);
        if (playing) {
            const float x = inner.getX() + inner.getWidth() * juce::jlimit(0.f, 1.f, playPos);
            g.setColour(kGreen.darker(0.3f));
            g.drawLine(x, inner.getY(), x, inner.getBottom(), 2.f);
        }
    } else {
        g.setColour(info.state == S::Failed ? kRed.darker(0.3f) : kDim);
        g.setFont(uiFont(12.f));
        juce::String s = info.status;
        if (searching) {
            s += juce::String::repeatedString(".", (int) (t * 3) % 4);
            if (info.attempts > 1) s << "  (" << info.attempts << ")";
        }
        g.drawFittedText(s, wa.toNearestInt().reduced(4, 1), juce::Justification::centred, 2);
    }

    // alt bilgi
    auto bottom = getLocalBounds().reduced(8, 6).removeFromBottom(32);
    if (info.audio) {
        g.setColour(kInk);
        g.setFont(uiFont(13.f, true));
        juce::String name = info.station;
        if (!info.percussion && info.country.isNotEmpty()) name << " (" << info.country << ")";
        g.drawText(name, bottom.removeFromTop(16), juce::Justification::centredLeft, true);
        g.setColour(kDim);
        g.setFont(uiFont(11.f));
        juce::String m;
        if (info.percussion) {
            m << info.tag << TR("  ·  ") << info.license << TR("  ·  ") << juce::String(info.durationSec, 2) << " s";
            if (info.country.isNotEmpty()) m << TR("  ·  ") << info.country;
        } else {
            m << info.origKey;
            m << (info.shift == 0 ? TR("  ·  kaydırma yok") : TR("  →  ") + juce::String(info.shift > 0 ? "+" : "") + juce::String(info.shift) + " st");
            m << TR("  ·  uyum %") << juce::roundToInt(info.fit * 100);
        }
        g.drawText(m, bottom, juce::Justification::centredLeft, true);
    } else if (!searching) {
        g.setColour(kDim);
        g.setFont(uiFont(12.f));
        g.drawText(TR("—"), bottom.removeFromTop(16), juce::Justification::centredLeft);
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

    auto setupLabel = [this](juce::Label& l, const juce::String& text, float size = 12.f, bool bold = true) {
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, kInk);
        l.setFont(uiFont(size, bold));
        l.setBorderSize({0, 1, 0, 1});
        addAndMakeVisible(l);
    };
    auto setupEditor = [this](juce::TextEditor& e) {
        e.setFont(uiFont(14.f));
        e.setIndents(7, 4);
        e.setJustification(juce::Justification::centredLeft);
        addAndMakeVisible(e);
    };
    auto segment = [this](juce::TextButton& b, const juce::String& text, const char* param, int index) {
        b.setButtonText(text);
        b.setClickingTogglesState(false);
        b.onClick = [this, param, index] { setChoice(param, index); };
        addAndMakeVisible(b);
    };

    // başlık
    segment(srcRadio, TR("radyo"), "source", 0);
    segment(srcPerc, TR("perküsyon"), "source", 1);
    srcRadio.setTooltip(TR("Canlı internet radyolarından, hepsi tek scale'e taşınmış sample'lar"));
    srcPerc.setTooltip(TR("Freesound'dan tek vuruşluk perküsif sesler: etnik davullar, metal, kapı, tahta…"));
    addAndMakeVisible(busyLed);
    setupLabel(busyLabel, {}, 13.f, false);

    // perküsyon
    const auto es = proc.engine.getSettings();
    setupLabel(termsLabel, TR("arama kelimeleri (İngilizce, virgülle)"));
    setupLabel(exclLabel, TR("hariç tut"));
    setupLabel(licenseLabel, TR("lisans"));
    setupLabel(keyLabel, TR("freesound api anahtarı"));
    setupEditor(termsEditor);
    termsEditor.setText(es.percTerms.joinIntoString(", "), false);
    termsEditor.setTooltip(TR("Her yuva bu kelimelerden birini rastgele seçip Freesound'da arar"));
    setupEditor(exclEditor);
    exclEditor.setText(es.percExclude.joinIntoString(", "), false);
    exclEditor.setTooltip(TR("Adında ya da etiketlerinde bu kelimeler geçen sesler alınmaz (modern davul sesleri)"));
    setupEditor(keyEditor);
    keyEditor.setPasswordCharacter((juce::juce_wchar) 0x2022);
    keyEditor.setText(proc.apiKey(), false);
    keyEditor.setTextToShowWhenEmpty(TR("freesound.org/apiv2/apply"), kDim);
    keyEditor.setTooltip(TR("Ücretsiz: freesound.org'da hesap aç, freesound.org/apiv2/apply adresinden anahtar al. "
                            "Anahtar bu bilgisayarda saklanır, her projede tekrar girmen gerekmez."));
    segment(licAll, TR("hepsi"), "license", 0);
    segment(licCc0, TR("sadece cc0"), "license", 1);
    licCc0.setTooltip(TR("CC0: yazar adı belirtmeden her yerde kullanılabilir"));

    // radyo
    setupLabel(tagsLabel, TR("türler"));
    setupLabel(scaleLabel, TR("hedef scale"));
    setupLabel(shiftLabel, TR("transpoz"));
    setupEditor(tagsEditor);
    tagsEditor.setText(es.tags.joinIntoString(", "), false);
    tagsEditor.setTextToShowWhenEmpty("jazz, world music", kDim);
    for (int i = 0; i < 12; ++i) rootBox.addItem(gk::noteName(i), i + 1);
    for (int i = 0; i < gk::NumModes; ++i) modeBox.addItem(TR(gk::modeNameTr(i)), i + 1);
    addAndMakeVisible(rootBox);
    addAndMakeVisible(modeBox);
    rootAtt = std::make_unique<CA>(proc.params, "root", rootBox);
    modeAtt = std::make_unique<CA>(proc.params, "mode", modeBox);
    segment(shiftQuality, TR("kaliteli"), "shiftmode", 0);
    segment(shiftTape, TR("bant (hız değişir)"), "shiftmode", 1);

    for (auto* e : {&termsEditor, &exclEditor, &keyEditor, &tagsEditor}) {
        e->onReturnKey = [this, e] { commitTexts(); e->unfocusAllComponents(); };
        e->onFocusLost = [this] { commitTexts(); };
    }

    auto setupKnob = [&](Knob& k, const char* id, const juce::String& name, int decimals, const juce::String& suffix) {
        setupLabel(k.label, name, 13.f, true);
        k.label.setJustificationType(juce::Justification::centred);
        k.slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 18);
        k.slider.setNumDecimalPlacesToDisplay(decimals);
        k.slider.setTextValueSuffix(suffix);
        k.slider.setColour(juce::Slider::textBoxTextColourId, kDim);
        k.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(k.slider);
        k.att = std::make_unique<SA>(proc.params, id, k.slider);
    };
    setupKnob(count, "count", TR("sample sayısı"), 0, "");
    setupKnob(length, "length", TR("uzunluk"), 2, " s");
    setupKnob(record, "record", TR("dinleme"), 0, " s");
    setupKnob(minconf, "minconf", TR("ton güveni"), 2, "");
    setupKnob(maxshift, "maxshift", TR("kaydırma"), 0, " st");
    setupKnob(gain, "gain", TR("önizleme"), 1, " dB");
    minconf.slider.setTooltip(TR("Seçicilik: yüksek = sadece tonu çok net kayıtlar kabul edilir"));
    record.slider.setTooltip(TR("Her istasyondan kaç saniye kaydedilip analiz edilir"));

    refreshAllBtn.onClick = [this] { commitTexts(); proc.refreshAllUnlocked(); };
    stopAllBtn.onClick = [this] {
        for (int i = 0; i < gk::SlotEngine::MaxSlots; ++i) proc.engine.stop(i);
        proc.stopPreview();
    };
    folderBtn.onClick = [this] { chooseFolder(); };
    openFolderBtn.onClick = [this] {
        auto f = proc.engine.getSettings().folder;
        f.createDirectory();
        f.startAsProcess();
    };
    for (auto* b : {(juce::Button*) &refreshAllBtn, (juce::Button*) &stopAllBtn, (juce::Button*) &folderBtn, (juce::Button*) &openFolderBtn})
        addAndMakeVisible(*b);

    adgPill.text = TR("drum rack'e sürükle (.adg)");
    adgPill.setTooltip(TR("Hazır sample'lardan bir Drum Rack preset'i oluşturur. Ableton'da boş bir MIDI track'e "
                          "ya da track listesinin altına bırak. Yuva 1 = C1, yuva 2 = C#1…"));
    adgPill.filesToDrag = [this]() -> juce::StringArray {
        commitTexts();
        juce::String err;
        const auto f = proc.writeDrumRack(err);
        if (!f.existsAsFile()) {
            statusLabel.setText(TR("Drum Rack oluşturulamadı: ") + err, juce::dontSendNotification);
            return {};
        }
        statusLabel.setText(TR("Drum Rack hazır: ") + f.getFileName(), juce::dontSendNotification);
        return {f.getFullPathName()};
    };
    wavPill.text = TR("wav'lar");
    wavPill.setTooltip(TR("Bütün WAV'ları birden sürükle: Drum Rack'in C1 pad'ine bırakırsan art arda pad'lere dizilir"));
    wavPill.filesToDrag = [this] { return proc.readyFilesInOrder(); };
    addAndMakeVisible(adgPill);
    addAndMakeVisible(wavPill);

    setupLabel(statusLabel, {}, 13.f, false);
    setupLabel(hintLabel, TR("pad'e tıkla: dinle  ·  sürükle: ableton'a bırak"), 12.f, false);
    hintLabel.setColour(juce::Label::textColourId, kDim);
    hintLabel.setJustificationType(juce::Justification::centredRight);

    for (int i = 0; i < gk::SlotEngine::MaxSlots; ++i) {
        pads.push_back(std::make_unique<PadComponent>(proc, i));
        addChildComponent(*pads.back());
    }

    for (auto* e : {&termsEditor, &exclEditor, &keyEditor, &tagsEditor}) e->setCaretPosition(0);

    setSize(1020, 720);
    updateSourceMode();
    syncSegments();
    startTimerHz(20);
}

GalaksiEditor::~GalaksiEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void GalaksiEditor::commitTexts()
{
    proc.setTags(tagsEditor.getText());
    proc.setPercTerms(termsEditor.getText());
    proc.setPercExclude(exclEditor.getText());
    proc.setApiKey(keyEditor.getText());
}

void GalaksiEditor::setChoice(const char* id, int index)
{
    if (auto* p = proc.params.getParameter(id)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1((float) index));
        p->endChangeGesture();
    }
    syncSegments();
}

void GalaksiEditor::syncSegments()
{
    auto val = [this](const char* id) { return (int) proc.params.getRawParameterValue(id)->load(); };
    const int src = val("source"), lic = val("license"), sm = val("shiftmode");
    srcRadio.setToggleState(src == 0, juce::dontSendNotification);
    srcPerc.setToggleState(src == 1, juce::dontSendNotification);
    licAll.setToggleState(lic == 0, juce::dontSendNotification);
    licCc0.setToggleState(lic == 1, juce::dontSendNotification);
    shiftQuality.setToggleState(sm == 0, juce::dontSendNotification);
    shiftTape.setToggleState(sm == 1, juce::dontSendNotification);
}

void GalaksiEditor::updateSourceMode()
{
    lastSource = proc.sourceMode();
    proc.pushSettingsToEngine();
    const bool perc = lastSource == GalaksiProcessor::SourcePercussion;
    for (juce::Component* c : std::initializer_list<juce::Component*> {&termsLabel, &exclLabel, &licenseLabel, &keyLabel,
                                                                       &termsEditor, &exclEditor, &keyEditor, &licAll, &licCc0})
        c->setVisible(perc);
    for (juce::Component* c : std::initializer_list<juce::Component*> {&tagsLabel, &scaleLabel, &shiftLabel, &tagsEditor, &rootBox,
                                                                       &modeBox, &shiftQuality, &shiftTape, &record.label,
                                                                       &record.slider, &minconf.label, &minconf.slider,
                                                                       &maxshift.label, &maxshift.slider})
        c->setVisible(!perc);
    length.label.setText(perc ? TR("maks uzunluk") : TR("uzunluk"), juce::dontSendNotification);
    count.label.setText(perc ? TR("sample sayısı") : TR("sayı"), juce::dontSendNotification);
    length.slider.setTooltip(perc ? TR("Uzun kuyruklu sesler (gong, zil) bu noktada yumuşakça kesilir")
                                  : TR("Sample süresi"));
    resized();
    repaint();
}

void GalaksiEditor::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser>(TR("Sample'ların kaydedileceği klasör"), proc.engine.getSettings().folder);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                         [this](const juce::FileChooser& fc) {
                             auto f = fc.getResult();
                             if (f != juce::File()) {
                                 proc.setFolder(f);
                                 statusLabel.setText(TR("WAV klasörü: ") + f.getFullPathName(), juce::dontSendNotification);
                             }
                         });
}

void GalaksiEditor::paint(juce::Graphics& g)
{
    g.fillAll(kPanel);

    // vida delikleri
    for (auto pt : {juce::Point<float>(12.5f, 12.5f), juce::Point<float>(getWidth() - 12.5f, 12.5f),
                    juce::Point<float>(12.5f, getHeight() - 12.5f), juce::Point<float>(getWidth() - 12.5f, getHeight() - 12.5f)}) {
        g.setColour(kScrew);
        g.fillEllipse(pt.x - 4.5f, pt.y - 4.5f, 9.f, 9.f);
        g.setColour(juce::Colour(0xff8f8f8a));
        g.drawEllipse(pt.x - 4.5f, pt.y - 4.5f, 9.f, 9.f, 1.f);
    }

    // başlık
    g.setColour(kInk);
    g.setFont(uiFont(32.f, true));
    g.drawText("galaksi", 30, 14, 140, 40, juce::Justification::centredLeft);
    g.setColour(kDim);
    g.setFont(uiFont(14.f));
    g.drawText(TR("rastgele sample toplayıcı"), 158, 22, 220, 30, juce::Justification::centredLeft);
    g.setFont(uiFont(13.f, true));
    g.drawText(TR("kaynak"), srcRadio.getX() - 60, srcRadio.getY(), 52, srcRadio.getHeight(), juce::Justification::centredRight);
    g.setColour(kInk.withAlpha(0.35f));
    g.fillRect(srcPerc.getRight() + 14, 21, 1, 26);

    // kontrol paneli çerçevesi
    const auto box = juce::Rectangle<float>(16.f, (float) kPanelTop, getWidth() - 32.f, (float) kPanelH);
    g.setColour(kInk);
    g.drawRoundedRectangle(box.reduced(0.75f), 5.f, 1.5f);
    g.fillRect((float) sep1, box.getY(), 1.5f, box.getHeight());
    g.fillRect((float) sep2, box.getY(), 1.5f, box.getHeight());
}

void GalaksiEditor::resized()
{
    const int W = getWidth();
    // başlık satırı
    {
        busyLed.setBounds(W - 220, 27, 14, 14);
        busyLabel.setBounds(W - 202, 22, 180, 24);
        const int segR = W - 250;
        srcPerc.setBounds(segR - 104, 19, 104, 30);
        srcRadio.setBounds(segR - 104 - 74, 19, 75, 30);
    }

    const bool perc = proc.sourceMode() == GalaksiProcessor::SourcePercussion;
    const int top = kPanelTop, x0 = 16;
    const int leftW = perc ? 400 : 330, actW = 200;
    sep1 = x0 + leftW;
    sep2 = W - 16 - actW;

    // sol sütun
    {
        auto c = juce::Rectangle<int>(x0 + 16, top + 11, leftW - 32, kPanelH - 22);
        auto field = [&](juce::Label& l, juce::Component& e) {
            l.setBounds(c.removeFromTop(16));
            c.removeFromTop(2);
            e.setBounds(c.removeFromTop(26));
            c.removeFromTop(7);
        };
        if (perc) {
            field(termsLabel, termsEditor);
            field(exclLabel, exclEditor);
            auto row = c;
            auto lic = row.removeFromLeft(172);
            licenseLabel.setBounds(lic.removeFromTop(16));
            lic.removeFromTop(2);
            lic = lic.removeFromTop(26);
            licAll.setBounds(lic.removeFromLeft(62));
            licCc0.setBounds(lic.removeFromLeft(98).withTrimmedLeft(-1));
            row.removeFromLeft(14);
            keyLabel.setBounds(row.removeFromTop(16));
            row.removeFromTop(2);
            keyEditor.setBounds(row.removeFromTop(26));
        } else {
            field(tagsLabel, tagsEditor);
            scaleLabel.setBounds(c.removeFromTop(16));
            c.removeFromTop(2);
            auto sr = c.removeFromTop(26);
            rootBox.setBounds(sr.removeFromLeft(72));
            sr.removeFromLeft(6);
            modeBox.setBounds(sr);
            c.removeFromTop(7);
            shiftLabel.setBounds(c.removeFromTop(16));
            c.removeFromTop(2);
            auto tr = c.removeFromTop(26);
            shiftQuality.setBounds(tr.removeFromLeft(80));
            shiftTape.setBounds(tr.removeFromLeft(140).withTrimmedLeft(-1));
        }
    }

    // düğmeler
    {
        std::vector<Knob*> knobs = perc ? std::vector<Knob*> {&count, &length, &gain}
                                        : std::vector<Knob*> {&count, &length, &record, &minconf, &maxshift, &gain};
        auto area = juce::Rectangle<int>(sep1 + 2, top + 10, sep2 - sep1 - 4, kPanelH - 20);
        const int cw = area.getWidth() / (int) knobs.size();
        const int knobSize = perc ? 92 : 74;
        for (auto* k : knobs) {
            auto col = area.removeFromLeft(cw);
            k->label.setBounds(col.removeFromTop(18));
            col.removeFromTop(2);
            const int ks = juce::jmin(knobSize, col.getHeight() - 20, col.getWidth() - 6);
            k->slider.setBounds(col.withSizeKeepingCentre(juce::jmax(ks, 80), ks + 20).withY(col.getY()));
        }
    }

    // eylem tuşları
    {
        auto c = juce::Rectangle<int>(sep2 + 16, top + 10, actW - 26, kPanelH - 20);
        refreshAllBtn.setBounds(c.removeFromTop(48));
        c.removeFromTop(4);
        const int h = c.getHeight() / 3;
        stopAllBtn.setBounds(c.removeFromTop(h));
        folderBtn.setBounds(c.removeFromTop(h));
        openFolderBtn.setBounds(c);
    }

    // alt şerit
    {
        auto bar = juce::Rectangle<int>(28, getHeight() - 40, W - 56, 30);
        adgPill.setBounds(bar.removeFromRight(226).reduced(0, 1));
        bar.removeFromRight(10);
        wavPill.setBounds(bar.removeFromRight(76).reduced(0, 3));
        bar.removeFromRight(12);
        hintLabel.setBounds(bar.removeFromRight(260));
        statusLabel.setBounds(bar);
    }
    layoutPads();
}

void GalaksiEditor::layoutPads()
{
    const int n = proc.slotCount();
    lastCount = n;
    const int cols = n <= 4 ? n : 4;
    const int rows = (n + cols - 1) / cols;
    auto area = juce::Rectangle<int>(16, kPadTop, getWidth() - 32, getHeight() - kPadTop - 48);
    const int gap = 8;
    const int pw = (area.getWidth() - (cols - 1) * gap) / cols;
    const int ph = juce::jmin(150, (area.getHeight() - (rows - 1) * gap) / rows);
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
    if (proc.sourceMode() != lastSource) updateSourceMode();
    if (tick % 5 == 0) syncSegments(); // otomasyon / preset değişimleri

    const int v = proc.engine.version();
    const bool changed = v != lastVersion;
    lastVersion = v;
    for (int i = 0; i < lastCount; ++i) {
        const bool playing = proc.isPlaying(i);
        auto info = proc.engine.getSlot(i);
        if (changed || playing || info.state == gk::SlotInfo::State::Searching)
            pads[(size_t) i]->update(info, playing, proc.playPosition(i));
    }

    const int busy = proc.engine.busyCount();
    const double t = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    const auto led = busy > 0 ? kGreen.withAlpha(0.5f + 0.5f * (float) (0.5 + 0.5 * std::sin(t * 5.0))) : kLedOff;
    busyLed.setColour(led);
    refreshAllBtn.ledColour = busy > 0 ? led : kGreen;
    if (tick % 4 == 0) refreshAllBtn.repaint();
    busyLabel.setText(busy > 0 ? juce::String(busy) + (lastSource == GalaksiProcessor::SourcePercussion ? TR(" yuva arıyor") : TR(" yuva radyo dinliyor"))
                               : TR("hazır"),
                      juce::dontSendNotification);

    if (changed) {
        const int ready = proc.readyFilesInOrder().size();
        adgPill.active = wavPill.active = ready > 0;
        adgPill.repaint();
        wavPill.repaint();
        statusLabel.setText(proc.engine.lastMessage(), juce::dontSendNotification);
    }
}
