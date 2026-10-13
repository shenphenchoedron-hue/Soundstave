// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteModel.h"
#include "NoteEditCommands.h"
#include <cmath>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <cstring>

namespace Soundstave {

// =========================================================================
// Note helpers
// =========================================================================
int Note::freqToMidi(double freqHz)
{
    if (freqHz <= 0.0) return 0;
    return (int)std::round(69.0 + 12.0 * std::log2(freqHz / 440.0));
}

double Note::midiToFreq(int midi)    { return 440.0 * std::pow(2.0, (midi - 69) / 12.0); }
double Note::midiToFreq(double midi) { return 440.0 * std::pow(2.0, (midi - 69.0) / 12.0); }

juce::String Note::midiToName(int midi)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F",
                                   "F#", "G", "G#", "A", "A#", "B" };
    const int pc = ((midi % 12) + 12) % 12;
    return juce::String(names[pc]) + juce::String(midi / 12 - 1);
}

juce::ValueTree Note::toValueTree() const
{
    juce::ValueTree vt("Note");
    vt.setProperty("id",       id.toString(), nullptr);
    vt.setProperty("midi",     midiNote,      nullptr);
    vt.setProperty("start",    startSec,      nullptr);
    vt.setProperty("end",      endSec,        nullptr);
    vt.setProperty("velocity", velocity,      nullptr);
    vt.setProperty("channel",  channel,       nullptr);
    vt.setProperty("manual",   !fromAnalysis, nullptr);
    return vt;
}

Note Note::fromValueTree(const juce::ValueTree& vt)
{
    Note n;
    n.id           = juce::Uuid(vt.getProperty("id").toString());
    n.midiNote     = juce::jlimit(0, 127, (int)vt.getProperty("midi", 60));
    // Values come from a file: reject NaN/inf and keep times in a sane range
    auto time = [&vt](const char* key)
    {
        const double v = (double)vt.getProperty(key, 0.0);
        return std::isfinite(v) ? juce::jlimit(0.0, kMaxTimeSec, v) : 0.0;
    };
    n.startSec     = time("start");
    n.endSec       = time("end");
    n.velocity     = juce::jlimit(1, 127, (int)vt.getProperty("velocity", 80));
    n.channel      = juce::jlimit(1, 16, (int)vt.getProperty("channel", 1));
    n.fromAnalysis = !(bool)vt.getProperty("manual", false);
    if (n.endSec < n.startSec + 0.01) n.endSec = n.startSec + 0.01;
    return n;
}

// =========================================================================
// NoteModel
// =========================================================================
NoteModel::NoteModel() = default;

std::vector<Note> NoteModel::getSelectedNotes() const
{
    std::vector<Note> sel;
    for (auto& n : notes)
        if (n.selected) sel.push_back(n);
    return sel;
}

int NoteModel::getNumSelected() const noexcept
{
    int c = 0;
    for (auto& n : notes) if (n.selected) ++c;
    return c;
}

std::optional<Note> NoteModel::findNote(const juce::Uuid& id) const
{
    for (auto& n : notes)
        if (n.id == id) return n;
    return std::nullopt;
}

Note* NoteModel::findNotePtr(const juce::Uuid& id)
{
    for (auto& n : notes)
        if (n.id == id) return &n;
    return nullptr;
}

void NoteModel::setTempo(const TempoInfo& t)
{
    TempoInfo clean = t;
    clean.bpm = std::isfinite(clean.bpm) ? juce::jlimit(kMinBpm, kMaxBpm, clean.bpm) : 120.0;
    // Grid origin lies within one bar before 0
    clean.offsetSec = std::isfinite(clean.offsetSec) ? juce::jlimit(-16.0, 0.0, clean.offsetSec) : 0.0;
    clean.numerator = juce::jlimit(1, 32, clean.numerator);
    if (clean.denominator != 2 && clean.denominator != 4 && clean.denominator != 8 && clean.denominator != 16)
        clean.denominator = 4;
    tempo = clean;
    dirty = true;
    listeners.call([](Listener& l){ l.tempoChanged(); });
}

// ---- Gestures -----------------------------------------------------------
void NoteModel::beginGesture()
{
    if (gestureActive) return;
    gestureActive   = true;
    gestureSnapshot = notes;
}

static bool sameNoteLists(const std::vector<Note>& a, const std::vector<Note>& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (!a[i].sameContent(b[i])) return false;
    return true;
}

void NoteModel::endGesture(const juce::String& name)
{
    if (!gestureActive) return;
    gestureActive = false;

    if (sameNoteLists(gestureSnapshot, notes))
    {
        gestureSnapshot.clear();
        return;
    }

    undoMgr.beginNewTransaction(name);
    undoMgr.perform(new NoteSnapshotAction(*this, std::move(gestureSnapshot), notes));
    gestureSnapshot.clear();
}

void NoteModel::performEdit(const juce::String& name, const std::function<void()>& fn)
{
    const bool nested = gestureActive;
    if (!nested) beginGesture();
    fn();
    if (!nested) endGesture(name);
}

void NoteModel::restoreSnapshot(const std::vector<Note>& snapshot)
{
    notes = snapshot;
    dirty = true;
    notifyListeners();
}

// ---- Raw edits ------------------------------------------------------------
void NoteModel::addNote(const Note& note)
{
    Note n = note;
    if (n.id.isNull()) n.id = juce::Uuid();
    n.endSec = std::max(n.endSec, n.startSec + 0.01);
    notes.push_back(n);
    dirty = true;
    notifyListeners();
}

void NoteModel::removeNote(const juce::Uuid& id)
{
    auto it = std::find_if(notes.begin(), notes.end(), [&id](const Note& n){ return n.id == id; });
    if (it != notes.end())
    {
        notes.erase(it);
        dirty = true;
        notifyListeners();
    }
}

void NoteModel::updateNote(const Note& note)
{
    updateNotes({ note });
}

namespace {
struct UuidHash
{
    size_t operator()(const juce::Uuid& u) const noexcept
    {
        juce::uint64 a, b;
        std::memcpy(&a, u.getRawData(), 8);
        std::memcpy(&b, u.getRawData() + 8, 8);
        return (size_t)(a ^ (b * 0x9E3779B97F4A7C15ULL));
    }
};
}

void NoteModel::updateNotes(const std::vector<Note>& changed)
{
    // Called on every mouse-drag event with the whole selection: index by id
    // once so this stays O(n + m) instead of O(n × m).
    Note* single = nullptr;
    std::unordered_map<juce::Uuid, size_t, UuidHash> index;
    if (changed.size() == 1)
        single = findNotePtr(changed[0].id);
    else
    {
        index.reserve(notes.size());
        for (size_t i = 0; i < notes.size(); ++i) index.emplace(notes[i].id, i);
    }

    bool any = false;
    for (auto& c : changed)
    {
        Note* n = single;
        if (n == nullptr && !index.empty())
        {
            auto it = index.find(c.id);
            if (it != index.end()) n = &notes[it->second];
        }
        if (n != nullptr)
        {
            if (!n->sameContent(c)) { *n = c; n->fromAnalysis = false; any = true; }
            else n->selected = c.selected;
        }
    }
    if (any) { dirty = true; notifyListeners(); }
}

void NoteModel::setNotes(std::vector<Note> newNotes)
{
    notes = std::move(newNotes);
    dirty = true;
    notifyListeners();
}

void NoteModel::clearNotes()
{
    notes.clear();
    clipboard.clear();
    undoMgr.clearUndoHistory();
    notifyListeners();
}

// ---- Selection ------------------------------------------------------------
void NoteModel::selectNote(const juce::Uuid& id, bool addToSelection)
{
    for (auto& n : notes)
    {
        if (n.id == id) n.selected = addToSelection ? !n.selected : true;
        else if (!addToSelection) n.selected = false;
    }
    notifySelectionListeners();
}

void NoteModel::setSelected(const juce::Uuid& id, bool sel)
{
    if (auto* n = findNotePtr(id))
        if (n->selected != sel) { n->selected = sel; notifySelectionListeners(); }
}

void NoteModel::selectWhere(const std::function<bool(const Note&)>& pred, bool addToSelection)
{
    bool changed = false;
    for (auto& n : notes)
    {
        const bool want = pred(n) || (addToSelection && n.selected);
        if (n.selected != want) { n.selected = want; changed = true; }
    }
    if (changed) notifySelectionListeners();
}

void NoteModel::selectAll()
{
    for (auto& n : notes) n.selected = true;
    notifySelectionListeners();
}

void NoteModel::deselectAll()
{
    bool changed = false;
    for (auto& n : notes) if (n.selected) { n.selected = false; changed = true; }
    if (changed) notifySelectionListeners();
}

// ---- High-level edits ------------------------------------------------------
void NoteModel::deleteSelected()
{
    if (getNumSelected() == 0) return;
    performEdit("Slet noder", [this]
    {
        notes.erase(std::remove_if(notes.begin(), notes.end(),
                                   [](const Note& n){ return n.selected; }), notes.end());
        dirty = true;
        notifyListeners();
    });
}

void NoteModel::transposeSelected(int semitones)
{
    if (getNumSelected() == 0 || semitones == 0) return;
    performEdit("Transponer", [this, semitones]
    {
        for (auto& n : notes)
            if (n.selected) { n.midiNote = juce::jlimit(0, 127, n.midiNote + semitones); n.fromAnalysis = false; }
        dirty = true;
        notifyListeners();
    });
}

void NoteModel::nudgeSelected(double deltaSec)
{
    if (getNumSelected() == 0) return;
    performEdit("Flyt noder", [this, deltaSec]
    {
        double minStart = 1e9;
        for (auto& n : notes) if (n.selected) minStart = std::min(minStart, n.startSec);
        const double d = std::max(deltaSec, -minStart);
        for (auto& n : notes)
            if (n.selected) { n.startSec += d; n.endSec += d; n.fromAnalysis = false; }
        dirty = true;
        notifyListeners();
    });
}

void NoteModel::mergeSelected()
{
    // Groups the selected notes by pitch; every group of 2+ notes becomes one
    // note spanning from the earliest start to the latest end.
    std::map<int, std::vector<size_t>> byPitch;
    for (size_t i = 0; i < notes.size(); ++i)
        if (notes[i].selected) byPitch[notes[i].midiNote].push_back(i);

    bool anything = false;
    for (auto& [p, idx] : byPitch) if (idx.size() >= 2) anything = true;
    if (!anything) return;

    performEdit("Saml noder", [this, byPitch]
    {
        std::vector<juce::Uuid> toRemove;
        for (auto& [pitch, idx] : byPitch)
        {
            if (idx.size() < 2) continue;
            Note& keep = notes[idx[0]];
            for (size_t k = 1; k < idx.size(); ++k)
            {
                const Note& o = notes[idx[k]];
                keep.startSec = std::min(keep.startSec, o.startSec);
                keep.endSec   = std::max(keep.endSec,   o.endSec);
                keep.velocity = std::max(keep.velocity, o.velocity);
                toRemove.push_back(o.id);
            }
            keep.fromAnalysis = false;
        }
        notes.erase(std::remove_if(notes.begin(), notes.end(), [&](const Note& n)
        {
            return std::find(toRemove.begin(), toRemove.end(), n.id) != toRemove.end();
        }), notes.end());
        dirty = true;
        notifyListeners();
    });
}

bool NoteModel::splitNote(const juce::Uuid& id, double atSec)
{
    auto* n = findNotePtr(id);
    if (n == nullptr || atSec <= n->startSec + 0.02 || atSec >= n->endSec - 0.02)
        return false;

    performEdit("Del node", [this, id, atSec]
    {
        auto* src = findNotePtr(id);
        Note right = *src;
        right.id = juce::Uuid();
        right.startSec = atSec;
        right.fromAnalysis = false;
        src->endSec = atSec;
        src->fromAnalysis = false;
        notes.push_back(right);
        dirty = true;
        notifyListeners();
    });
    return true;
}

void NoteModel::copySelected()
{
    clipboard = getSelectedNotes();
}

void NoteModel::paste(double atTime)
{
    if (clipboard.empty()) return;
    double earliest = clipboard[0].startSec;
    for (auto& n : clipboard) earliest = std::min(earliest, n.startSec);
    const double offset = std::max(0.0, atTime) - earliest;

    performEdit(juce::String::fromUTF8("Indsæt"), [this, offset]
    {
        for (auto& n : notes) n.selected = false;
        for (auto n : clipboard)
        {
            n.id = juce::Uuid();
            n.startSec += offset;
            n.endSec   += offset;
            n.selected  = true;
            n.fromAnalysis = false;
            notes.push_back(n);
        }
        dirty = true;
        notifyListeners();
    });
}

void NoteModel::replaceAnalysisNotes(std::vector<Note> analysed)
{
    performEdit("Analyse", [this, &analysed]
    {
        std::vector<Note> result;
        for (auto& n : notes)
            if (!n.fromAnalysis) result.push_back(n);
        for (auto& n : analysed)
        {
            n.fromAnalysis = true;
            n.selected = false;
            result.push_back(n);
        }
        notes = std::move(result);
        dirty = true;
        notifyListeners();
    });
}

// ---- Persistence ------------------------------------------------------------
juce::ValueTree NoteModel::toValueTree() const
{
    juce::ValueTree root("NoteModel");
    root.setProperty("bpm",         tempo.bpm,         nullptr);
    root.setProperty("numerator",   tempo.numerator,   nullptr);
    root.setProperty("denominator", tempo.denominator, nullptr);
    root.setProperty("offset",      tempo.offsetSec,   nullptr);

    for (auto& n : notes)
        root.addChild(n.toValueTree(), -1, nullptr);

    return root;
}

void NoteModel::fromValueTree(const juce::ValueTree& vt)
{
    notes.clear();
    clipboard.clear();
    TempoInfo t;
    t.bpm         = (double)vt.getProperty("bpm",         120.0);
    t.numerator   = (int)   vt.getProperty("numerator",   4);
    t.denominator = (int)   vt.getProperty("denominator", 4);
    t.offsetSec   = (double)vt.getProperty("offset",      0.0);

    for (int i = 0; i < vt.getNumChildren(); ++i)
    {
        auto child = vt.getChild(i);
        if (child.hasType("Note"))
            notes.push_back(Note::fromValueTree(child));
    }
    undoMgr.clearUndoHistory();
    setTempo(t);
    dirty = false;
    notifyListeners();
}

void NoteModel::notifyListeners()
{
    listeners.call([](Listener& l){ l.notesChanged(); });
}

void NoteModel::notifySelectionListeners()
{
    listeners.call([](Listener& l){ l.selectionChanged(); });
}

} // namespace Soundstave
