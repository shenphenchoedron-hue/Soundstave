// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "NoteModel.h"

namespace Soundstave {

// Generic undoable step: stores the full note list before and after an edit.
// Note lists are small (thousands of notes at most), so snapshots are cheap
// and make every kind of edit (move, resize, split, merge, paste, analysis)
// reliably undoable without per-edit command classes.
class NoteSnapshotAction : public juce::UndoableAction
{
public:
    NoteSnapshotAction(NoteModel& m, std::vector<Note> before, std::vector<Note> after)
        : model(m), beforeState(std::move(before)), afterState(std::move(after)) {}

    bool perform() override { model.restoreSnapshot(afterState);  return true; }
    bool undo()    override { model.restoreSnapshot(beforeState); return true; }
    int  getSizeInUnits() override
    {
        return (int)((beforeState.size() + afterState.size()) * sizeof(Note)) + 16;
    }

private:
    NoteModel&        model;
    std::vector<Note> beforeState, afterState;
};

} // namespace Soundstave
