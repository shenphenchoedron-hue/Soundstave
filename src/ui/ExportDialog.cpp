// SPDX-License-Identifier: GPL-3.0-or-later
#include "ExportDialog.h"
#include "TimelineRuler.h"

namespace Soundstave {

static const QuantizeResolution kResolutions[] = {
    QuantizeResolution::Quarter, QuantizeResolution::Eighth, QuantizeResolution::Sixteenth,
    QuantizeResolution::ThirtySecond, QuantizeResolution::EighthTriplet,
};

MusicXmlExportPanel::MusicXmlExportPanel(std::vector<Note> n, TempoInfo t, QuantizeResolution initial)
    : notes(std::move(n)), tempo(t)
{
    lblRes.setText(U("Kvantisering"), juce::dontSendNotification);
    lblRes.setFont(SoundstaveLookAndFeel::getBoldFont(13.5f));
    lblRes.setColour(juce::Label::textColourId, juce::Colour(Colours::TextPrimary));
    addAndMakeVisible(lblRes);

    for (int i = 0; i < 5; ++i)
    {
        cmbRes.addItem(QuantizationEngine::name(kResolutions[i]), i + 1);
        if (kResolutions[i] == initial) cmbRes.setSelectedId(i + 1, juce::dontSendNotification);
    }
    cmbRes.onChange = [this]{ updatePreview(); };
    addAndMakeVisible(cmbRes);

    lblInfo.setFont(SoundstaveLookAndFeel::getDefaultFont(13.0f));
    lblInfo.setColour(juce::Label::textColourId, juce::Colour(Colours::TextSecondary));
    addAndMakeVisible(lblInfo);

    lblLegend.setText(U("Lyse felter = optagelsens timing   ·   Farvede noder = det der skrives i noderne"),
                      juce::dontSendNotification);
    lblLegend.setFont(SoundstaveLookAndFeel::getDefaultFont(12.0f));
    lblLegend.setColour(juce::Label::textColourId, juce::Colour(Colours::TextSecondary));
    addAndMakeVisible(lblLegend);

    viewport.setViewedComponent(&preview, false);
    viewport.setScrollBarsShown(false, true);
    addAndMakeVisible(viewport);

    btnExport.setButtonText(U("Eksportér…"));
    btnCancel.setButtonText(U("Annullér"));
    SoundstaveLookAndFeel::setButtonRole(btnExport, ButtonRole::Export);
    btnExport.onClick = [this]
    {
        const auto res = selected();
        auto cb = onExport;
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>()) dw->exitModalState(1);
        if (cb) juce::MessageManager::callAsync([cb, res]{ cb(res); });
    };
    btnCancel.onClick = [this]
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>()) dw->exitModalState(0);
    };
    addAndMakeVisible(btnExport);
    addAndMakeVisible(btnCancel);

    setSize(860, 520);
    updatePreview();
}

QuantizeResolution MusicXmlExportPanel::selected() const
{
    const int i = juce::jlimit(1, 5, cmbRes.getSelectedId()) - 1;
    return kResolutions[i];
}

void MusicXmlExportPanel::updatePreview()
{
    score = QuantizationEngine::quantize(notes, tempo, selected());
    preview.setData(&notes, &tempo, &score);

    const double secPerMeasure = tempo.barSec();
    lblInfo.setText(juce::String((int)score.notes.size()) + U(" noder  ·  ")
                    + juce::String(score.numMeasures) + U(" takter  ·  ")
                    + juce::String(tempo.bpm, 1) + U(" BPM  ·  ")
                    + juce::String(tempo.numerator) + "/" + juce::String(tempo.denominator)
                    + U("  ·  ") + juce::String(secPerMeasure, 2) + U(" s pr. takt"),
                    juce::dontSendNotification);

    const int h = std::max(200, viewport.getHeight() - viewport.getScrollBarThickness());
    const int w = std::max(viewport.getWidth(),
                           score.numMeasures * score.ticksPerMeasure / kTicksPerQuarter * Preview::kPxPerQuarter + 40);
    preview.setSize(w, h);
    preview.repaint();
}

void MusicXmlExportPanel::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto top = area.removeFromTop(30);
    lblRes.setBounds(top.removeFromLeft(110));
    cmbRes.setBounds(top.removeFromLeft(170).reduced(0, 2));
    top.removeFromLeft(16);
    lblInfo.setBounds(top);

    area.removeFromTop(6);
    lblLegend.setBounds(area.removeFromTop(20));
    area.removeFromTop(4);

    auto buttons = area.removeFromBottom(34);
    btnExport.setBounds(buttons.removeFromRight(120));
    buttons.removeFromRight(8);
    btnCancel.setBounds(buttons.removeFromRight(100));
    area.removeFromBottom(10);

    viewport.setBounds(area);
    updatePreview();
}

void MusicXmlExportPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Colours::Background));
}

// ---- Preview -----------------------------------------------------------------
void MusicXmlExportPanel::Preview::setData(const std::vector<Note>* n, const TempoInfo* t, const QuantizedScore* s)
{
    notes = n; tempo = t; score = s;
}

void MusicXmlExportPanel::Preview::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF151A33));
    if (score == nullptr || tempo == nullptr || notes == nullptr) return;

    const int w = getWidth(), h = getHeight();
    int lo = 127, hi = 0;
    for (auto& n : score->notes) { lo = std::min(lo, n.midi); hi = std::max(hi, n.midi); }
    if (lo > hi) { lo = 60; hi = 72; }
    lo = std::max(0, lo - 2); hi = std::min(127, hi + 2);

    const float topPad = 18.0f;
    const int ph = h - (int)topPad;
    auto yFor = [&](int midi) { return topPad + pitchToY((float)midi, lo, hi, ph); };
    const float lane = laneHeight(lo, hi, ph);
    const float pxPerTick = (float)kPxPerQuarter / (float)kTicksPerQuarter;
    auto xFor = [&](double tick) { return 20.0f + (float)tick * pxPerTick; };

    for (int m = lo; m <= hi; ++m)
        if (isBlackKey(m))
        {
            g.setColour(juce::Colours::black.withAlpha(0.25f));
            g.fillRect(0.0f, yFor(m) - lane * 0.5f, (float)w, lane);
        }

    // Beats and bars
    const int tpm = score->ticksPerMeasure;
    const int beatTicks = kTicksPerQuarter * 4 / tempo->denominator;
    g.setFont(SoundstaveLookAndFeel::getBoldFont(11.0f));
    for (int t = 0; t <= score->numMeasures * tpm; t += beatTicks)
    {
        const bool bar = (t % tpm) == 0;
        g.setColour(juce::Colours::white.withAlpha(bar ? 0.35f : 0.08f));
        g.drawVerticalLine((int)xFor(t), bar ? 0.0f : topPad, (float)h);
        if (bar && t < score->numMeasures * tpm)
        {
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.drawText(juce::String(t / tpm + 1), (int)xFor(t) + 4, 2, 40, 14, juce::Justification::centredLeft);
        }
    }

    // Original timing (ghost)
    for (auto& n : *notes)
    {
        const double s = std::max(0.0, QuantizationEngine::secondsToTicks(n.startSec, *tempo));
        const double e = QuantizationEngine::secondsToTicks(n.endSec, *tempo);
        auto r = juce::Rectangle<float>(xFor(s), yFor(n.midiNote) - lane * 0.4f,
                                        std::max(2.0f, (float)(e - s) * pxPerTick), lane * 0.8f);
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.fillRoundedRectangle(r, 2.0f);
    }

    // Quantized result
    for (auto& qn : score->notes)
    {
        auto r = juce::Rectangle<float>(xFor(qn.startTick) + 0.5f, yFor(qn.midi) - lane * 0.4f,
                                        std::max(2.0f, (float)qn.durTicks * pxPerTick - 1.0f), lane * 0.8f);
        g.setColour(juce::Colour(Colours::NoteColour).withAlpha(0.9f));
        g.fillRoundedRectangle(r, 2.0f);
        g.setColour(juce::Colour(Colours::NoteBorder));
        g.drawRoundedRectangle(r, 2.0f, 1.0f);
    }
}

// ---- Launch --------------------------------------------------------------------
void MusicXmlExportPanel::show(juce::Component* parent, std::vector<Note> notes, const TempoInfo& tempo,
                               QuantizeResolution initial, std::function<void(QuantizeResolution)> onExport)
{
    auto* panel = new MusicXmlExportPanel(std::move(notes), tempo, initial);
    panel->onExport = std::move(onExport);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned(panel);
    o.dialogTitle = U("Eksportér MusicXML");
    o.dialogBackgroundColour = juce::Colour(Colours::Background);
    o.componentToCentreAround = parent;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

} // namespace Soundstave
