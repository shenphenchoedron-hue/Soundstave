// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/NoteModel.h"
#include "TimelineRuler.h"
#include "LookAndFeel.h"

namespace Soundstave {

enum class EditTool { Select, Draw, Split };

// Transparent layer above the spectrogram that shows and edits the notes.
class NoteOverlay : public juce::Component,
                    public NoteModel::Listener
{
public:
    explicit NoteOverlay(NoteModel& model);
    ~NoteOverlay() override;

    void setViewState(const ViewState& vs);
    void setTool(EditTool tool);
    EditTool getTool() const noexcept { return currentTool; }

    // Snap function (identity = free placement) and the grid length used as
    // default length for newly drawn notes.
    void setSnap(std::function<double(double)> snapFn, double gridSeconds);

    void notesChanged()     override { repaint(); }
    void selectionChanged() override { repaint(); }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

    std::function<void(const juce::MouseEvent&, const juce::MouseWheelDetails&)> onWheel;
    std::function<void(int midi, int velocity)> onNotePreview;
    std::function<void(double)> onSeek;

private:
    enum class DragMode { None, Move, ResizeLeft, ResizeRight, Draw, RubberBand };

    juce::Rectangle<float> noteRect(const Note& n) const;
    juce::Uuid noteAt(juce::Point<float> p) const;
    DragMode   edgeModeFor(const Note& n, float x) const;
    float      timeToX(double t) const noexcept { return (float)vs.timeToX(t, getWidth()); }
    double     xToTime(float x)  const noexcept { return vs.xToTime((double)x, getWidth()); }
    int        yToMidi(float y)  const noexcept;
    double     snap(double t) const { return snapFn ? snapFn(t) : t; }
    void       updateCursor(juce::Point<float> p);

    NoteModel&  model;
    ViewState   vs;
    EditTool    currentTool = EditTool::Select;
    std::function<double(double)> snapFn;
    double      gridSec = 0.0;

    DragMode           dragMode = DragMode::None;
    juce::Point<float> dragStart;
    juce::Uuid         dragNoteId;
    Note               dragPrimaryOrig;
    std::vector<Note>  dragOrig;
    int                lastPreviewMidi = -1;
    Note               draftNote;
    juce::Rectangle<float> rubberBand;
    juce::Uuid         hoverId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoteOverlay)
};

} // namespace Soundstave
