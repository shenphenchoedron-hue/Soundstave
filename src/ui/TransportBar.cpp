// SPDX-License-Identifier: GPL-3.0-or-later
#include "TransportBar.h"

namespace Soundstave {

namespace {
struct Meter { int num, den; const char* text; };
const Meter kMeters[] = {
    { 2, 4, "2/4" }, { 3, 4, "3/4" }, { 4, 4, "4/4" }, { 5, 4, "5/4" },
    { 3, 8, "3/8" }, { 6, 8, "6/8" }, { 9, 8, "9/8" }, { 12, 8, "12/8" }, { 2, 2, "2/2" },
};
}

TransportBar::TransportBar()
{
    auto button = [this](juce::TextButton& b, const char* text, const char* tip, ButtonRole role, bool toggle)
    {
        b.setButtonText(U(text));
        b.setTooltip(U(tip));
        b.setWantsKeyboardFocus(false);
        b.setClickingTogglesState(toggle);
        SoundstaveLookAndFeel::setButtonRole(b, role);
        addAndMakeVisible(b);
    };
    button(btnPlay,     "Afspil",  "Afspil / pause (Mellemrum)",              ButtonRole::File,    false);
    button(btnStop,     "Stop",    "Stop og gå til start (Shift+Mellemrum)",  ButtonRole::Default, false);
    button(btnLoop,     "Loop",    "Gentag et stykke (L). Vælg stykket ved at trække i tidslinjen eller bølgeformen, "
                                   "eller markér noder og tryk Ctrl+L.", ButtonRole::View, false);
    button(btnEstimate, "Find",    "Find tempo og taktslag ud fra noderne",   ButtonRole::Default, false);
    button(btnAudio,    "Lyd",     "Afspil den originale lyd til/fra",        ButtonRole::View,    true);
    button(btnMidi,     "MIDI",    "Afspil MIDI-noderne til/fra",             ButtonRole::View,    true);
    btnAudio.setToggleState(true, juce::dontSendNotification);
    btnMidi.setToggleState(true,  juce::dontSendNotification);

    btnPlay.onClick = [this]
    {
        if (isPlaying) { if (onPause) onPause(); }
        else           { if (onPlay)  onPlay();  }
    };
    btnStop.onClick     = [this]{ if (onStop) onStop(); };
    btnLoop.onClick     = [this]{ if (onToggleLoop) onToggleLoop(); };
    btnEstimate.onClick = [this]{ if (onEstimateTempo) onEstimateTempo(); };
    btnAudio.onClick    = [this]{ if (onAudioEnabledChanged) onAudioEnabledChanged(btnAudio.getToggleState()); };
    btnMidi.onClick     = [this]{ if (onMidiEnabledChanged)  onMidiEnabledChanged (btnMidi.getToggleState()); };

    lblTime.setFont(SoundstaveLookAndFeel::getMonoFont(15.0f));
    lblTime.setColour(juce::Label::textColourId, juce::Colour(Colours::TextPrimary));
    lblTime.setJustificationType(juce::Justification::centredLeft);
    lblTime.setText("0:00.0", juce::dontSendNotification);
    addAndMakeVisible(lblTime);

    auto caption = [this](juce::Label& l, const char* text)
    {
        l.setText(U(text), juce::dontSendNotification);
        l.setFont(SoundstaveLookAndFeel::getBoldFont(12.5f));
        l.setColour(juce::Label::textColourId, juce::Colour(Colours::TextSecondary));
        l.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(l);
    };
    caption(lblTempo, "Tempo");
    caption(lblMeter, "Takt");
    caption(lblGrid,  "Grid");

    sldTempo.setSliderStyle(juce::Slider::IncDecButtons);
    sldTempo.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 58, 26);
    sldTempo.setRange(30.0, 300.0, 0.5);
    sldTempo.setNumDecimalPlacesToDisplay(1);
    sldTempo.setValue(120.0, juce::dontSendNotification);
    sldTempo.setIncDecButtonsMode(juce::Slider::incDecButtonsDraggable_Vertical);
    sldTempo.setTooltip(U("Tempo i fjerdedele pr. minut. Træk lodret eller skriv en værdi."));
    sldTempo.setColour(juce::Slider::textBoxTextColourId,       juce::Colour(Colours::TextPrimary));
    sldTempo.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
    sldTempo.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(Colours::Border));
    sldTempo.setColour(juce::Slider::textBoxHighlightColourId,  juce::Colour(Colours::AccentDim));
    sldTempo.onValueChange = [this]{ if (onTempoChanged) onTempoChanged(sldTempo.getValue()); };
    addAndMakeVisible(sldTempo);

    for (int i = 0; i < (int)(sizeof(kMeters) / sizeof(kMeters[0])); ++i)
        cmbMeter.addItem(kMeters[i].text, i + 1);
    cmbMeter.setSelectedId(3, juce::dontSendNotification);
    cmbMeter.setTooltip(U("Taktart"));
    cmbMeter.onChange = [this]
    {
        const int i = cmbMeter.getSelectedId() - 1;
        if (i >= 0 && onTimeSignatureChanged) onTimeSignatureChanged(kMeters[i].num, kMeters[i].den);
    };
    addAndMakeVisible(cmbMeter);

    cmbGrid.addItem(U("Fri"),     GridFree);
    cmbGrid.addItem("1/4",        GridQuarter);
    cmbGrid.addItem("1/8",        GridEighth);
    cmbGrid.addItem("1/16",       GridSixteenth);
    cmbGrid.addItem("1/32",       GridThirtySecond);
    cmbGrid.addItem(U("1/8 triol"), GridEighthTriplet);
    cmbGrid.setSelectedId(GridSixteenth, juce::dontSendNotification);
    cmbGrid.setTooltip(U("Snap til nodeinddeling ved tegning og flytning (hold Alt for fri placering)"));
    cmbGrid.onChange = [this]{ if (onGridChanged) onGridChanged(cmbGrid.getSelectedId()); };
    addAndMakeVisible(cmbGrid);

    for (auto* s : { &sldAudio, &sldMidi })
    {
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s->setRange(0.0, 1.0);
        s->setWantsKeyboardFocus(false);
        addAndMakeVisible(*s);
    }
    sldAudio.setValue(0.8, juce::dontSendNotification);
    sldMidi.setValue(0.6, juce::dontSendNotification);
    sldAudio.setTooltip(U("Lydstyrke for den originale optagelse"));
    sldMidi.setTooltip(U("Lydstyrke for MIDI-noderne"));
    sldAudio.onValueChange = [this]{ if (onAudioVolumeChanged) onAudioVolumeChanged((float)sldAudio.getValue()); };
    sldMidi.onValueChange  = [this]{ if (onMidiVolumeChanged)  onMidiVolumeChanged ((float)sldMidi.getValue()); };

    setHasAudio(false);
}

void TransportBar::setPlaying(bool playing)
{
    if (playing == isPlaying) return;
    isPlaying = playing;
    btnPlay.setButtonText(playing ? U("Pause") : U("Afspil"));
}

static juce::String fmt(double s)
{
    s = std::max(0.0, s);
    const int m = (int)(s / 60.0);
    const double r = s - m * 60.0;
    return juce::String(m) + ":" + juce::String(r, 1).paddedLeft('0', 4);
}

void TransportBar::setPosition(double seconds, double total)
{
    auto txt = fmt(seconds);
    if (total > 0.0) txt << "  /  " << fmt(total);
    lblTime.setText(txt, juce::dontSendNotification);
}

void TransportBar::setTempo(const TempoInfo& t)
{
    sldTempo.setValue(t.bpm, juce::dontSendNotification);
    for (int i = 0; i < (int)(sizeof(kMeters) / sizeof(kMeters[0])); ++i)
        if (kMeters[i].num == t.numerator && kMeters[i].den == t.denominator)
            cmbMeter.setSelectedId(i + 1, juce::dontSendNotification);
}

void TransportBar::setGrid(int gridId)
{
    cmbGrid.setSelectedId(gridId, juce::dontSendNotification);
}

void TransportBar::setMixer(float audioVol, float midiVol, bool audioOn, bool midiOn)
{
    sldAudio.setValue(audioVol, juce::dontSendNotification);
    sldMidi.setValue(midiVol, juce::dontSendNotification);
    btnAudio.setToggleState(audioOn, juce::dontSendNotification);
    btnMidi.setToggleState(midiOn, juce::dontSendNotification);
}

void TransportBar::setHasAudio(bool hasAudio)
{
    btnPlay.setEnabled(hasAudio);
    btnStop.setEnabled(hasAudio);
    btnLoop.setEnabled(hasAudio);
}

void TransportBar::setLoopState(bool looping)
{
    btnLoop.setToggleState(looping, juce::dontSendNotification);
}

void TransportBar::resized()
{
    const int h  = getHeight();
    const int bH = 32;
    const int y  = (h - bH) / 2;
    int x = 12;

    btnPlay.setBounds(x, y, 86, bH);  x += 90;
    btnStop.setBounds(x, y, 64, bH);  x += 68;
    btnLoop.setBounds(x, y, 60, bH);  x += 70;
    lblTime.setBounds(x, y, 150, bH); x += 156;

    lblTempo.setBounds(x, y, 52, bH);   x += 54;
    sldTempo.setBounds(x, y + 1, 112, bH - 2); x += 116;
    btnEstimate.setBounds(x, y, 52, bH); x += 64;

    lblMeter.setBounds(x, y, 38, bH);   x += 40;
    cmbMeter.setBounds(x, y + 3, 70, bH - 6); x += 82;

    lblGrid.setBounds(x, y, 36, bH);    x += 38;
    cmbGrid.setBounds(x, y + 3, 100, bH - 6); x += 112;

    // Mixer flush right; sliders shrink on narrow windows
    const int avail = getWidth() - 12 - x;
    const int sliderW = juce::jlimit(40, 110, (avail - 2 * 56 - 24) / 2);
    int rx = getWidth() - 12;
    rx -= sliderW; sldMidi.setBounds(rx, y, sliderW, bH);  rx -= 4;
    rx -= 54;      btnMidi.setBounds(rx, y, 54, bH);       rx -= 14;
    rx -= sliderW; sldAudio.setBounds(rx, y, sliderW, bH); rx -= 4;
    rx -= 50;      btnAudio.setBounds(rx, y, 50, bH);

    const bool mixerFits = rx >= x - 8;
    for (juce::Component* c : { (juce::Component*)&sldMidi, (juce::Component*)&sldAudio })
        c->setVisible(mixerFits);
}

void TransportBar::paint(juce::Graphics& g)
{
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFFE6EBF3), 0.0f, 0.0f,
                                           juce::Colours::white, 0.0f, (float)getHeight(), false));
    g.fillAll();
    g.setColour(juce::Colour(Colours::Border));
    g.drawHorizontalLine(0, 0.0f, (float)getWidth());
}

} // namespace Soundstave
