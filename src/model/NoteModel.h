// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>
#include <optional>
#include <functional>

namespace Soundstave {

constexpr double kMaxTimeSec = 4.0 * 3600.0;   // longest supported timeline
constexpr double kMinBpm = 20.0;
constexpr double kMaxBpm = 400.0;

// -------------------------------------------------------------------------
// A single detected / edited note
// -------------------------------------------------------------------------
struct Note
{
    juce::Uuid  id;
    int         midiNote  = 60;     // 0-127
    double      startSec  = 0.0;
    double      endSec    = 0.0;
    int         velocity  = 80;     // 1-127
    int         channel   = 1;      // 1-16
    bool        selected  = false;
    bool        fromAnalysis = true; // false = drawn/edited manually

    double duration() const noexcept { return endSec - startSec; }

    // Content equality (ignores selection)
    bool sameContent(const Note& o) const noexcept
    {
        return id == o.id && midiNote == o.midiNote && velocity == o.velocity
            && std::abs(startSec - o.startSec) < 1e-9 && std::abs(endSec - o.endSec) < 1e-9;
    }

    static int          freqToMidi(double freqHz);
    static double       midiToFreq(int midi);
    static double       midiToFreq(double midi);
    static juce::String midiToName(int midi);

    juce::ValueTree toValueTree() const;
    static Note fromValueTree(const juce::ValueTree& vt);
};

// -------------------------------------------------------------------------
// Tempo / meter. bpm is always quarter notes per minute.
// offsetSec places beat 1 of bar 1 on the audio timeline (in (-beat, 0]),
// so the grid can be aligned to a recording that does not start on a beat.
// -------------------------------------------------------------------------
struct TempoInfo
{
    double bpm         = 120.0;
    int    numerator   = 4;
    int    denominator = 4;
    double offsetSec   = 0.0;

    double quarterSec() const noexcept { return 60.0 / bpm; }
    double beatSec()    const noexcept { return quarterSec() * 4.0 / (double)denominator; }
    double barSec()     const noexcept { return beatSec() * (double)numerator; }
};

// -------------------------------------------------------------------------
// The central note container. All edits go through this class (message
// thread only). Every user-visible edit is undoable via gestures:
//     beginGesture();  ...edits...  endGesture("Name");
// -------------------------------------------------------------------------
class NoteModel
{
public:
    NoteModel();
    ~NoteModel() = default;

    // ---- Read ---------------------------------------------------------------
    const std::vector<Note>& getNotes() const noexcept { return notes; }
    std::vector<Note> getSelectedNotes() const;
    int  getNumSelected() const noexcept;
    std::optional<Note> findNote(const juce::Uuid& id) const;
    bool hasUnsavedChanges() const noexcept { return dirty; }

    const TempoInfo& getTempo() const noexcept { return tempo; }
    void setTempo(const TempoInfo& t);

    // ---- Undo gestures ------------------------------------------------------
    void beginGesture();
    void endGesture(const juce::String& name);
    bool isInGesture() const noexcept { return gestureActive; }
    // Convenience: wraps fn in a gesture
    void performEdit(const juce::String& name, const std::function<void()>& fn);

    // ---- Write (raw – wrap in a gesture to make undoable) -------------------
    void addNote(const Note& note);
    void removeNote(const juce::Uuid& id);
    void updateNote(const Note& note);                  // replace by id
    void updateNotes(const std::vector<Note>& changed); // replace many by id
    void setNotes(std::vector<Note> newNotes);
    void clearNotes();

    // ---- Selection (not undoable, not "dirty") ------------------------------
    void selectNote(const juce::Uuid& id, bool addToSelection = false);
    void setSelected(const juce::Uuid& id, bool sel);
    void selectWhere(const std::function<bool(const Note&)>& pred, bool addToSelection);
    void selectAll();
    void deselectAll();

    // ---- High-level edits (each is a complete undoable step) ---------------
    void deleteSelected();
    void transposeSelected(int semitones);
    void nudgeSelected(double deltaSec);
    void mergeSelected();
    bool splitNote(const juce::Uuid& id, double atSec);
    void copySelected();
    void paste(double atTime);
    bool hasClipboard() const noexcept { return !clipboard.empty(); }

    // Replace all analysis notes, keep manually drawn notes (undoable)
    void replaceAnalysisNotes(std::vector<Note> analysed);

    juce::UndoManager& getUndoManager() noexcept { return undoMgr; }

    // ---- Persistence --------------------------------------------------------
    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& vt);
    void markClean() noexcept { dirty = false; }
    void markDirty() noexcept { dirty = true; }

    // ---- Listeners ----------------------------------------------------------
    struct Listener {
        virtual ~Listener() = default;
        virtual void notesChanged() = 0;
        virtual void selectionChanged() {}
        virtual void tempoChanged() {}
    };
    void addListener(Listener* l)    { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    // Used by the snapshot undo action
    void restoreSnapshot(const std::vector<Note>& snapshot);

private:
    void notifyListeners();
    void notifySelectionListeners();
    Note* findNotePtr(const juce::Uuid& id);

    std::vector<Note>  notes;
    std::vector<Note>  clipboard;
    std::vector<Note>  gestureSnapshot;
    bool               gestureActive = false;
    TempoInfo          tempo;
    juce::UndoManager  undoMgr { 2000000, 100 };
    bool               dirty = false;

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoteModel)
};

} // namespace Soundstave
