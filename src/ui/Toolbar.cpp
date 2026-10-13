// SPDX-License-Identifier: GPL-3.0-or-later
#include "Toolbar.h"

namespace Soundstave {

Toolbar::Toolbar()
{
    auto setup = [this](juce::TextButton& b, const char* text, const char* tip, ButtonRole role, bool toggle = false)
    {
        b.setButtonText(U(text));
        b.setTooltip(U(tip));
        b.setClickingTogglesState(toggle);
        b.setWantsKeyboardFocus(false);   // keep Space/arrows for the editor
        SoundstaveLookAndFeel::setButtonRole(b, role);
        addAndMakeVisible(b);
    };

    setup(btnOpen,    "Åbn",        "Åbn lydfil eller projekt (Ctrl+O)",           ButtonRole::File);
    setup(btnSave,    "Gem",        "Gem projekt (Ctrl+S)",                        ButtonRole::File);
    setup(btnAnalyse, "Analysér",   "Find toner automatisk (Ctrl+R)",              ButtonRole::Analyse);
    setup(btnSelect,  "Markér",     "Markér, flyt og ændr længde på noder (S)",    ButtonRole::Tool, true);
    setup(btnDraw,    "Tegn",       "Tegn nye noder med musen (D)",                ButtonRole::Tool, true);
    setup(btnSplit,   "Del",        "Klik på en node for at dele den (C)",         ButtonRole::Tool, true);
    setup(btnMerge,   "Saml",       "Saml markerede noder med samme tonehøjde (Ctrl+J)", ButtonRole::Default);
    setup(btnDelete,  "Slet",       "Slet markerede noder (Delete)",               ButtonRole::Delete);
    setup(btnUndo,    "Fortryd",    "Fortryd (Ctrl+Z)",                            ButtonRole::Default);
    setup(btnRedo,    "Gentag",     "Gentag (Ctrl+Shift+Z)",                       ButtonRole::Default);
    setup(btnPR,      "Pianoroll",  "Vis/skjul pianoroll-laget over spektrogrammet (P)", ButtonRole::View, true);
    setup(btnSettings,"Indstillinger", "Avancerede indstillinger for analyse og visning", ButtonRole::Default);
    setup(btnMidi,    "MIDI",       "Eksportér til Standard MIDI File (Ctrl+E)",  ButtonRole::Export);
    setup(btnXml,     "MusicXML",   "Eksportér noder til MusicXML (Ctrl+Shift+E)", ButtonRole::Export);

    for (auto* b : { &btnSelect, &btnDraw, &btnSplit }) b->setRadioGroupId(1001);
    btnSelect.setToggleState(true, juce::dontSendNotification);
    btnPR.setToggleState(true, juce::dontSendNotification);

    btnOpen.onClick     = [this]{ if (onOpen) onOpen(); };
    btnSave.onClick     = [this]{ if (onSave) onSave(); };
    btnAnalyse.onClick  = [this]{ if (onAnalyse) onAnalyse(); };
    btnSelect.onClick   = [this]{ if (onToolChanged) onToolChanged(EditTool::Select); };
    btnDraw.onClick     = [this]{ if (onToolChanged) onToolChanged(EditTool::Draw); };
    btnSplit.onClick    = [this]{ if (onToolChanged) onToolChanged(EditTool::Split); };
    btnMerge.onClick    = [this]{ if (onMerge) onMerge(); };
    btnDelete.onClick   = [this]{ if (onDelete) onDelete(); };
    btnUndo.onClick     = [this]{ if (onUndo) onUndo(); };
    btnRedo.onClick     = [this]{ if (onRedo) onRedo(); };
    btnPR.onClick       = [this]{ if (onTogglePianoRoll) onTogglePianoRoll(); };
    btnSettings.onClick = [this]{ if (onSettings) onSettings(); };
    btnMidi.onClick     = [this]{ if (onExportMidi) onExportMidi(); };
    btnXml.onClick      = [this]{ if (onExportMusicXml) onExportMusicXml(); };

    setEditState(false, false, false, false);
    setHasAudio(false);
}

void Toolbar::setAnalysisRunning(bool running)
{
    btnAnalyse.setEnabled(!running);
    btnAnalyse.setButtonText(running ? U("Analyserer…") : U("Analysér"));
}

void Toolbar::setTool(EditTool tool)
{
    btnSelect.setToggleState(tool == EditTool::Select, juce::dontSendNotification);
    btnDraw.setToggleState  (tool == EditTool::Draw,   juce::dontSendNotification);
    btnSplit.setToggleState (tool == EditTool::Split,  juce::dontSendNotification);
}

void Toolbar::setPianoRollVisible(bool visible)
{
    btnPR.setToggleState(visible, juce::dontSendNotification);
}

void Toolbar::setEditState(bool hasNotes, bool hasSelection, bool canUndo, bool canRedo)
{
    btnMerge.setEnabled(hasSelection);
    btnDelete.setEnabled(hasSelection);
    btnUndo.setEnabled(canUndo);
    btnRedo.setEnabled(canRedo);
    btnMidi.setEnabled(hasNotes);
    btnXml.setEnabled(hasNotes);
}

void Toolbar::setHasAudio(bool hasAudio)
{
    if (btnAnalyse.getButtonText() == U("Analysér"))
        btnAnalyse.setEnabled(hasAudio);
}

void Toolbar::resized()
{
    const int h  = getHeight();
    const int bH = 34;
    const int y  = (h - bH) / 2;
    int x = 12;
    separators.clear();

    auto place = [&](juce::Component& c, int w) { c.setBounds(x, y, w, bH); x += w + 4; };
    auto gap   = [&]{ x += 6; separators.push_back(x - 1); x += 7; };

    place(btnOpen, 64);   place(btnSave, 58);               gap();
    place(btnAnalyse, 92);                                  gap();
    place(btnSelect, 74); place(btnDraw, 62); place(btnSplit, 52); gap();
    place(btnMerge, 60);  place(btnDelete, 58);             gap();
    place(btnUndo, 78);   place(btnRedo, 70);               gap();
    place(btnPR, 92);

    int rx = getWidth() - 12;
    auto placeRight = [&](juce::Component& c, int w) { rx -= w; c.setBounds(rx, y, w, bH); rx -= 4; };
    placeRight(btnXml, 92);
    placeRight(btnMidi, 62);
    rx -= 9; separators.push_back(rx + 4);
    placeRight(btnSettings, 112);

    // Hide the right-hand group rather than overlap on very narrow windows
    const bool fits = rx > x;
    btnSettings.setVisible(fits);
}

void Toolbar::paint(juce::Graphics& g)
{
    g.setGradientFill(juce::ColourGradient(juce::Colours::white, 0.0f, 0.0f,
                                           juce::Colour(0xFFE6EBF3), 0.0f, (float)getHeight(), false));
    g.fillAll();

    g.setColour(juce::Colour(Colours::Border));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, (float)getWidth());

    g.setColour(juce::Colour(Colours::Border).withAlpha(0.8f));
    for (int sx : separators)
        g.drawVerticalLine(sx, 12.0f, (float)getHeight() - 12.0f);
}

} // namespace Soundstave
