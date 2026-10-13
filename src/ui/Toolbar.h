// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include "NoteOverlay.h"

namespace Soundstave {

class Toolbar : public juce::Component
{
public:
    Toolbar();

    std::function<void()>         onOpen;
    std::function<void()>         onSave;
    std::function<void()>         onAnalyse;
    std::function<void(EditTool)> onToolChanged;
    std::function<void()>         onMerge;
    std::function<void()>         onDelete;
    std::function<void()>         onUndo;
    std::function<void()>         onRedo;
    std::function<void()>         onTogglePianoRoll;
    std::function<void()>         onSettings;
    std::function<void()>         onExportMidi;
    std::function<void()>         onExportMusicXml;

    void setAnalysisRunning(bool running);
    void setTool(EditTool tool);
    void setPianoRollVisible(bool visible);
    void setEditState(bool hasNotes, bool hasSelection, bool canUndo, bool canRedo);
    void setHasAudio(bool hasAudio);

    void resized() override;
    void paint(juce::Graphics& g) override;

    static constexpr int kHeight = 52;

private:
    juce::TextButton btnOpen, btnSave, btnAnalyse;
    juce::TextButton btnSelect, btnDraw, btnSplit;
    juce::TextButton btnMerge, btnDelete;
    juce::TextButton btnUndo, btnRedo;
    juce::TextButton btnPR;
    juce::TextButton btnSettings, btnMidi, btnXml;

    std::vector<int> separators;   // x positions of group separators

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Toolbar)
};

} // namespace Soundstave
