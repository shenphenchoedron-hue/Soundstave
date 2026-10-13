// SPDX-License-Identifier: GPL-3.0-or-later
#include "SettingsDialog.h"
#include "../model/NoteModel.h"

namespace Soundstave {

// ---- AppSettings ---------------------------------------------------------
juce::ValueTree AppSettings::toValueTree() const
{
    juce::ValueTree vt("Settings");
    vt.setProperty("fftOrder",      analysis.fftOrder,        nullptr);
    vt.setProperty("displayRange",  displayRangeDb,           nullptr);
    vt.setProperty("sensitivity",   detection.sensitivityDb,  nullptr);
    vt.setProperty("polyphony",     detection.polyphonic ? detection.maxPolyphony : 1, nullptr);
    vt.setProperty("minNoteMs",     detection.minNoteDuration * 1000.0, nullptr);
    vt.setProperty("lowNote",       detection.minMidiNote,    nullptr);
    vt.setProperty("highNote",      detection.maxMidiNote,    nullptr);
    return vt;
}

void AppSettings::fromValueTree(const juce::ValueTree& vt)
{
    if (!vt.isValid()) return;
    analysis.fftOrder           = juce::jlimit(11, 14, (int)vt.getProperty("fftOrder", analysis.fftOrder));
    analysis.hopDivisor         = 1 << std::max(1, analysis.fftOrder - 10);   // hop ≈ 1024 samples
    displayRangeDb              = juce::jlimit(30.0f, 130.0f, (float)vt.getProperty("displayRange", displayRangeDb));
    detection.sensitivityDb     = juce::jlimit(20.0f, 90.0f, (float)vt.getProperty("sensitivity", detection.sensitivityDb));
    const int poly              = juce::jlimit(1, 12, (int)vt.getProperty("polyphony", detection.maxPolyphony));
    detection.polyphonic        = poly > 1;
    detection.maxPolyphony      = poly;
    detection.minNoteDuration   = juce::jlimit(0.02, 0.5, (double)vt.getProperty("minNoteMs", 60.0) / 1000.0);
    detection.minMidiNote       = juce::jlimit(12, 100, (int)vt.getProperty("lowNote",  detection.minMidiNote));
    detection.maxMidiNote       = juce::jlimit(detection.minMidiNote + 12, 120, (int)vt.getProperty("highNote", detection.maxMidiNote));
}

// ---- Panel -----------------------------------------------------------------
SettingsPanel::SettingsPanel(const AppSettings& initial) : settings(initial)
{
    auto header = [this](juce::Label& l, const char* text)
    {
        l.setText(U(text), juce::dontSendNotification);
        l.setFont(SoundstaveLookAndFeel::getBoldFont(15.0f));
        l.setColour(juce::Label::textColourId, juce::Colour(Colours::Accent));
        addAndMakeVisible(l);
    };
    header(hdrDisplay,  "Spektrogram");
    header(hdrAnalysis, "Tonegenkendelse");

    note.setText(U("Ændringer i analyse træder i kraft næste gang du klikker Analysér. "
                   "Standardværdierne passer til de fleste optagelser."), juce::dontSendNotification);
    note.setFont(SoundstaveLookAndFeel::getDefaultFont(12.5f));
    note.setColour(juce::Label::textColourId, juce::Colour(Colours::TextSecondary));
    note.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(note);

    auto slider = [this](juce::Slider& s, double lo, double hi, double step, const char* suffix)
    {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22);
        s.setRange(lo, hi, step);
        s.setTextValueSuffix(U(suffix));
        s.onValueChange = [this]{ readFromControls(); };
    };
    slider(sRange,  30, 130, 1,  " dB");
    slider(sSens,   20, 90,  1,  " dB");
    slider(sPoly,   1,  12,  1,  "");
    slider(sMinLen, 20, 500, 5,  " ms");
    slider(sLow,    12, 100, 1,  "");
    slider(sHigh,   36, 120, 1,  "");
    auto noteName = [](double v) { return Note::midiToName((int)v); };
    sLow.textFromValueFunction  = noteName;
    sHigh.textFromValueFunction = noteName;
    sPoly.textFromValueFunction = [](double v) { return v <= 1.0 ? U("Monofon") : juce::String((int)v) + U(" toner"); };

    cFft.addItem(U("2048 (hurtig, grov)"),     11);
    cFft.addItem(U("4096"),                    12);
    cFft.addItem(U("8192 (anbefalet)"),        13);
    cFft.addItem(U("16384 (fin bas, langsom)"),14);
    cFft.onChange = [this]{ readFromControls(); };

    addRow(lRange,  sRange,  "Dynamikområde", "Hvor mange dB under det kraftigste signal der vises. Lavere = mere kontrast.");
    displayRows.push_back({ &lRange, &sRange });

    addRow(lFft,    cFft,    "FFT-størrelse", "Større giver bedre frekvensopløsning (bas), mindre giver bedre tidsopløsning.");
    addRow(lSens,   sSens,   "Følsomhed",     "Hvor svage toner der medtages (dB under det kraftigste signal).");
    addRow(lPoly,   sPoly,   "Samtidige toner", "Maksimalt antal toner pr. tidspunkt. 1 = monofon (sang, blæsere).");
    addRow(lMinLen, sMinLen, "Korteste node", "Kortere toner ignoreres.");
    addRow(lLow,    sLow,    "Laveste tone",  "Ingen toner under denne findes.");
    addRow(lHigh,   sHigh,   "Højeste tone",  "Ingen toner over denne findes.");
    analysisRows = { { &lFft, &cFft }, { &lSens, &sSens }, { &lPoly, &sPoly },
                     { &lMinLen, &sMinLen }, { &lLow, &sLow }, { &lHigh, &sHigh } };

    btnDefaults.setButtonText(U("Standardværdier"));
    btnClose.setButtonText(U("Luk"));
    SoundstaveLookAndFeel::setButtonRole(btnClose, ButtonRole::File);
    btnDefaults.onClick = [this]
    {
        settings = AppSettings{};
        writeToControls();
        if (onChanged) onChanged(settings);
    };
    btnClose.onClick = [this]
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(btnDefaults);
    addAndMakeVisible(btnClose);

    writeToControls();
    setSize(520, 470);
}

void SettingsPanel::addRow(juce::Label& label, juce::Component& control, const char* text, const char* tip)
{
    label.setText(U(text), juce::dontSendNotification);
    label.setFont(SoundstaveLookAndFeel::getDefaultFont(13.5f));
    label.setColour(juce::Label::textColourId, juce::Colour(Colours::TextPrimary));
    label.setTooltip(U(tip));
    if (auto* tc = dynamic_cast<juce::SettableTooltipClient*>(&control)) tc->setTooltip(U(tip));
    addAndMakeVisible(label);
    addAndMakeVisible(control);
}

void SettingsPanel::writeToControls()
{
    sRange.setValue(settings.displayRangeDb, juce::dontSendNotification);
    cFft.setSelectedId(settings.analysis.fftOrder, juce::dontSendNotification);
    sSens.setValue(settings.detection.sensitivityDb, juce::dontSendNotification);
    sPoly.setValue(settings.detection.polyphonic ? settings.detection.maxPolyphony : 1, juce::dontSendNotification);
    sMinLen.setValue(settings.detection.minNoteDuration * 1000.0, juce::dontSendNotification);
    sLow.setValue(settings.detection.minMidiNote, juce::dontSendNotification);
    sHigh.setValue(settings.detection.maxMidiNote, juce::dontSendNotification);
}

void SettingsPanel::readFromControls()
{
    settings.displayRangeDb           = (float)sRange.getValue();
    settings.analysis.fftOrder        = cFft.getSelectedId() > 0 ? cFft.getSelectedId() : 13;
    settings.analysis.hopDivisor      = 1 << std::max(1, settings.analysis.fftOrder - 10);
    settings.detection.sensitivityDb  = (float)sSens.getValue();
    const int poly = (int)sPoly.getValue();
    settings.detection.polyphonic     = poly > 1;
    settings.detection.maxPolyphony   = poly;
    settings.detection.minNoteDuration = sMinLen.getValue() / 1000.0;
    settings.detection.minMidiNote    = (int)sLow.getValue();
    settings.detection.maxMidiNote    = std::max((int)sHigh.getValue(), settings.detection.minMidiNote + 12);
    if (onChanged) onChanged(settings);
}

void SettingsPanel::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto layoutRows = [&](std::vector<Row>& rows)
    {
        for (auto& r : rows)
        {
            auto row = area.removeFromTop(34);
            r.label->setBounds(row.removeFromLeft(150));
            r.control->setBounds(row.reduced(0, 4));
        }
    };
    hdrDisplay.setBounds(area.removeFromTop(26));
    layoutRows(displayRows);
    area.removeFromTop(12);
    hdrAnalysis.setBounds(area.removeFromTop(26));
    layoutRows(analysisRows);
    area.removeFromTop(8);
    note.setBounds(area.removeFromTop(40));

    auto buttons = area.removeFromBottom(34);
    btnClose.setBounds(buttons.removeFromRight(90));
    buttons.removeFromRight(8);
    btnDefaults.setBounds(buttons.removeFromRight(150));
}

void SettingsPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Colours::Background));
}

void SettingsPanel::show(juce::Component* parent, const AppSettings& current,
                         std::function<void(const AppSettings&)> onChanged)
{
    auto* panel = new SettingsPanel(current);
    panel->onChanged = std::move(onChanged);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned(panel);
    o.dialogTitle = U("Indstillinger");
    o.dialogBackgroundColour = juce::Colour(Colours::Background);
    o.componentToCentreAround = parent;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

} // namespace Soundstave
