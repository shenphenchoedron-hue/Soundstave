// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteOverlay.h"
#include <cmath>

namespace Soundstave {

static constexpr float  kEdgeW      = 6.0f;
static constexpr double kMinLenSec  = 0.03;

NoteOverlay::NoteOverlay(NoteModel& m) : model(m)
{
    setOpaque(false);
    setInterceptsMouseClicks(true, false);
    model.addListener(this);
}

NoteOverlay::~NoteOverlay()
{
    model.removeListener(this);
}

void NoteOverlay::setViewState(const ViewState& v) { vs = v; repaint(); }

void NoteOverlay::setTool(EditTool tool)
{
    currentTool = tool;
    setMouseCursor(tool == EditTool::Draw  ? juce::MouseCursor::CrosshairCursor
                 : tool == EditTool::Split ? juce::MouseCursor::IBeamCursor
                                           : juce::MouseCursor::NormalCursor);
}

void NoteOverlay::setSnap(std::function<double(double)> fn, double gridSeconds)
{
    snapFn  = std::move(fn);
    gridSec = gridSeconds;
}

int NoteOverlay::yToMidi(float y) const noexcept
{
    return juce::jlimit(0, 127, juce::roundToInt(yToPitch(y, vs.midiLow, vs.midiHigh, getHeight())));
}

juce::Rectangle<float> NoteOverlay::noteRect(const Note& n) const
{
    const float lane = laneHeight(vs.midiLow, vs.midiHigh, getHeight());
    const float h  = std::max(4.0f, lane * 0.82f);
    const float yc = pitchToY((float)n.midiNote, vs.midiLow, vs.midiHigh, getHeight());
    const float x1 = timeToX(n.startSec);
    const float x2 = timeToX(n.endSec);
    return { x1, yc - h * 0.5f, std::max(3.0f, x2 - x1), h };
}

juce::Uuid NoteOverlay::noteAt(juce::Point<float> p) const
{
    const auto& notes = model.getNotes();
    for (int i = (int)notes.size() - 1; i >= 0; --i)
        if (noteRect(notes[(size_t)i]).expanded(1.5f, 1.0f).contains(p))
            return notes[(size_t)i].id;
    return {};
}

NoteOverlay::DragMode NoteOverlay::edgeModeFor(const Note& n, float x) const
{
    const auto r = noteRect(n);
    const float edge = std::min(kEdgeW, r.getWidth() * 0.3f);
    if (x <= r.getX() + edge)     return DragMode::ResizeLeft;
    if (x >= r.getRight() - edge) return DragMode::ResizeRight;
    return DragMode::Move;
}

// ---- Paint -----------------------------------------------------------------
void NoteOverlay::paint(juce::Graphics& g)
{
    const int w = getWidth();
    auto font = SoundstaveLookAndFeel::getBoldFont(10.5f);
    g.setFont(font);

    auto drawNote = [&](const Note& n, bool draft)
    {
        auto r = noteRect(n);
        if (r.getRight() < 0.0f || r.getX() > (float)w) return;
        if (r.getBottom() < 0.0f || r.getY() > (float)getHeight()) return;

        const bool sel = n.selected && !draft;
        const bool hover = (n.id == hoverId) && !draft;
        juce::Colour base = sel ? juce::Colour(Colours::NoteSelected) : juce::Colour(Colours::NoteColour);
        if (!n.fromAnalysis && !sel) base = base.withRotatedHue(-0.04f);   // manual notes slightly warmer
        const float velAlpha = 0.62f + 0.38f * (float)n.velocity / 127.0f;
        const float radius = std::min(3.0f, r.getHeight() * 0.35f);

        // Dark outline keeps notes separated from the spectrogram behind them
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(r.expanded(1.0f), radius + 1.0f);

        juce::ColourGradient grad(base.brighter(0.25f).withAlpha(velAlpha), 0.0f, r.getY(),
                                  base.darker(0.15f).withAlpha(velAlpha),   0.0f, r.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(r, radius);

        g.setColour(sel ? juce::Colours::white : (hover ? juce::Colours::white.withAlpha(0.8f)
                                                        : base.darker(0.4f)));
        g.drawRoundedRectangle(r, radius, sel ? 1.6f : 1.0f);

        if (draft)
        {
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.drawRoundedRectangle(r.expanded(1.0f), radius, 1.0f);
        }

        if (r.getWidth() > 30.0f && r.getHeight() >= 10.0f)
        {
            g.setColour(juce::Colours::black.withAlpha(0.75f));
            g.drawText(Note::midiToName(n.midiNote), r.reduced(4.0f, 0.0f).toNearestInt(),
                       juce::Justification::centredLeft, false);
        }
    };

    for (auto& n : model.getNotes())
        if (!n.selected) drawNote(n, false);
    for (auto& n : model.getNotes())
        if (n.selected) drawNote(n, false);

    if (dragMode == DragMode::Draw)
        drawNote(draftNote, true);

    if (dragMode == DragMode::RubberBand)
    {
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.fillRect(rubberBand);
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.drawRect(rubberBand, 1.0f);
    }
}

// ---- Mouse --------------------------------------------------------------------
void NoteOverlay::mouseDown(const juce::MouseEvent& e)
{
    dragStart = e.position;
    dragMode  = DragMode::None;
    lastPreviewMidi = -1;
    const auto id = noteAt(e.position);

    // Right click: delete note (or the whole selection if it belongs to it)
    if (e.mods.isPopupMenu())
    {
        if (!id.isNull())
        {
            auto n = model.findNote(id);
            if (n && n->selected && model.getNumSelected() > 1)
                model.deleteSelected();
            else
                model.performEdit("Slet node", [&]{ model.removeNote(id); });
        }
        return;
    }

    if (currentTool == EditTool::Split)
    {
        if (!id.isNull())
            model.splitNote(id, snap(xToTime(e.position.x)));
        return;
    }

    if (currentTool == EditTool::Draw)
    {
        dragMode = DragMode::Draw;
        draftNote = Note{};
        draftNote.id = juce::Uuid();
        draftNote.midiNote = yToMidi(e.position.y);
        draftNote.startSec = std::max(0.0, snap(xToTime(e.position.x)));
        const double defLen = gridSec > 0.0 ? gridSec : 0.25;
        draftNote.endSec   = draftNote.startSec + defLen;
        draftNote.velocity = 90;
        draftNote.fromAnalysis = false;
        if (onNotePreview) onNotePreview(draftNote.midiNote, draftNote.velocity);
        repaint();
        return;
    }

    // ---- Select tool ----
    if (!id.isNull())
    {
        auto n = model.findNote(id);
        if (!n) return;

        if (e.mods.isShiftDown() || e.mods.isCommandDown())
            model.selectNote(id, true);          // toggle
        else if (!n->selected)
            model.selectNote(id, false);         // single select
        // (clicking a note that is already selected keeps the selection, so
        //  several selected notes can be dragged together)

        n = model.findNote(id);
        if (!n || !n->selected) return;

        dragMode        = edgeModeFor(*n, e.position.x);
        dragNoteId      = id;
        dragPrimaryOrig = *n;
        dragOrig        = model.getSelectedNotes();
        lastPreviewMidi = n->midiNote;
        model.beginGesture();
        if (onNotePreview) onNotePreview(n->midiNote, n->velocity);
    }
    else
    {
        if (!e.mods.isShiftDown() && !e.mods.isCommandDown()) model.deselectAll();
        dragMode   = DragMode::RubberBand;
        rubberBand = { e.position, e.position };
    }
}

void NoteOverlay::mouseDrag(const juce::MouseEvent& e)
{
    switch (dragMode)
    {
        case DragMode::Draw:
        {
            const double t = snap(xToTime(e.position.x));
            const double minLen = gridSec > 0.0 ? gridSec : kMinLenSec;
            draftNote.endSec = std::max(draftNote.startSec + minLen, t);
            repaint();
            return;
        }

        case DragMode::RubberBand:
        {
            rubberBand = juce::Rectangle<float>(dragStart, e.position);
            const bool add = e.mods.isShiftDown() || e.mods.isCommandDown();
            const auto band = rubberBand;
            model.selectWhere([this, band](const Note& n){ return noteRect(n).intersects(band); }, add);
            repaint();
            return;
        }

        case DragMode::Move:
        {
            double dt = xToTime(e.position.x) - xToTime(dragStart.x);
            const int dMidi = juce::roundToInt(yToPitch(e.position.y, vs.midiLow, vs.midiHigh, getHeight())
                                             - yToPitch(dragStart.y,  vs.midiLow, vs.midiHigh, getHeight()));
            if (!e.mods.isAltDown())   // Alt = temporarily free placement
                dt = snap(dragPrimaryOrig.startSec + dt) - dragPrimaryOrig.startSec;

            double minStart = 1e9;
            for (auto& o : dragOrig) minStart = std::min(minStart, o.startSec);
            dt = std::max(dt, -minStart);

            std::vector<Note> moved = dragOrig;
            for (auto& n : moved)
            {
                n.startSec += dt;
                n.endSec   += dt;
                n.midiNote  = juce::jlimit(0, 127, n.midiNote + dMidi);
            }
            model.updateNotes(moved);

            const int newMidi = juce::jlimit(0, 127, dragPrimaryOrig.midiNote + dMidi);
            if (newMidi != lastPreviewMidi && onNotePreview)
            {
                lastPreviewMidi = newMidi;
                onNotePreview(newMidi, dragPrimaryOrig.velocity);
            }
            return;
        }

        case DragMode::ResizeRight:
        case DragMode::ResizeLeft:
        {
            const double dtRaw = xToTime(e.position.x) - xToTime(dragStart.x);
            const bool right = dragMode == DragMode::ResizeRight;
            const double origEdge = right ? dragPrimaryOrig.endSec : dragPrimaryOrig.startSec;
            double edge = origEdge + dtRaw;
            if (!e.mods.isAltDown()) edge = snap(edge);
            const double delta = edge - origEdge;

            std::vector<Note> changed = dragOrig;
            for (auto& n : changed)
            {
                if (right) n.endSec   = std::max(n.startSec + kMinLenSec, n.endSec + delta);
                else       n.startSec = juce::jlimit(0.0, n.endSec - kMinLenSec, n.startSec + delta);
            }
            model.updateNotes(changed);
            return;
        }

        case DragMode::None:
            return;
    }
}

void NoteOverlay::mouseUp(const juce::MouseEvent&)
{
    switch (dragMode)
    {
        case DragMode::Draw:
        {
            Note n = draftNote;
            n.selected = true;
            model.deselectAll();
            model.performEdit("Tegn node", [&]{ model.addNote(n); });
            break;
        }
        case DragMode::Move:        model.endGesture("Flyt noder"); break;
        case DragMode::ResizeLeft:
        case DragMode::ResizeRight: model.endGesture(juce::String::fromUTF8("Ændr længde")); break;
        case DragMode::RubberBand:
        case DragMode::None:        break;
    }
    dragMode = DragMode::None;
    dragOrig.clear();
    repaint();
}

void NoteOverlay::updateCursor(juce::Point<float> p)
{
    if (currentTool != EditTool::Select) return;
    const auto id = noteAt(p);
    if (id != hoverId) { hoverId = id; repaint(); }

    if (!id.isNull())
    {
        if (auto n = model.findNote(id))
        {
            const auto m = edgeModeFor(*n, p.x);
            setMouseCursor(m == DragMode::Move ? juce::MouseCursor::DraggingHandCursor
                                               : juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void NoteOverlay::mouseMove(const juce::MouseEvent& e) { updateCursor(e.position); }

void NoteOverlay::mouseExit(const juce::MouseEvent&)
{
    if (!hoverId.isNull()) { hoverId = {}; repaint(); }
}

void NoteOverlay::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (currentTool != EditTool::Select) return;
    const auto id = noteAt(e.position);
    if (!id.isNull())
        model.performEdit("Slet node", [&]{ model.removeNote(id); });
    else if (onSeek)
        onSeek(std::max(0.0, xToTime(e.position.x)));
}

void NoteOverlay::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (onWheel) onWheel(e, w);
}

} // namespace Soundstave
