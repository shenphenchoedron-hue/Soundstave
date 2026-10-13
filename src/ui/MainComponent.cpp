// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainComponent.h"
#include "ExportDialog.h"
#include "../export/MidiExporter.h"
#include "../export/MusicXmlExporter.h"
#include "../analysis/TempoEstimator.h"
#include <bitset>

namespace Soundstave {

static const char* kAudioPatterns = "*.wav;*.flac;*.aif;*.aiff;*.mp3";

static bool isAudioFile(const juce::File& f)
{
    return f.hasFileExtension("wav;flac;aif;aiff;mp3");
}

// =========================================================================
// Construction
// =========================================================================
MainComponent::MainComponent()
{
    setLookAndFeel(&laf);
    juce::LookAndFeel::setDefaultLookAndFeel(&laf);
    tooltips = std::make_unique<juce::TooltipWindow>(this, 600);

    juce::PropertiesFile::Options opts;
    opts.applicationName     = "Soundstave";
    opts.filenameSuffix      = ".settings";
    opts.folderName          = "Soundstave";
    opts.osxLibrarySubFolder = "Application Support";
    props = std::make_unique<juce::PropertiesFile>(opts);

    noteModel.addListener(this);
    noteModel.getUndoManager().addChangeListener(this);

    // ---- Audio ----
    if (!audioEngine.initAudioDevice())
        juce::Logger::writeToLog("Soundstave: no audio output device: " + audioEngine.getDeviceError());
    audioEngine.onPositionChanged = [this](double p){ onPositionChanged(p); };

    // ---- Children (z-order: spectrogram < grid < notes < progress) ----
    addAndMakeVisible(toolbar);
    addAndMakeVisible(transportBar);
    addAndMakeVisible(ruler);
    addAndMakeVisible(waveform);
    addAndMakeVisible(divider);
    addAndMakeVisible(keyboard);
    addAndMakeVisible(spectrogram);
    addAndMakeVisible(pianoRoll);
    addAndMakeVisible(noteOverlay);
    addAndMakeVisible(hScrollBar);
    addAndMakeVisible(vScrollBar);
    addChildComponent(progressOverlay);

    hScrollBar.addListener(this);
    vScrollBar.addListener(this);
    hScrollBar.setAutoHide(false);
    vScrollBar.setAutoHide(false);

    // ---- Toolbar ----
    toolbar.onOpen            = [this]{ openWithChooser(); };
    toolbar.onSave            = [this]{ save(); };
    toolbar.onAnalyse         = [this]{ runAnalysis(); };
    toolbar.onToolChanged     = [this](EditTool t){ setTool(t); };
    toolbar.onMerge           = [this]{ noteModel.mergeSelected(); };
    toolbar.onDelete          = [this]{ noteModel.deleteSelected(); };
    toolbar.onUndo            = [this]{ noteModel.getUndoManager().undo(); };
    toolbar.onRedo            = [this]{ noteModel.getUndoManager().redo(); };
    toolbar.onTogglePianoRoll = [this]{ setPianoRollVisible(!pianoRollVisible); };
    toolbar.onSettings        = [this]{ showSettings(); };
    toolbar.onExportMidi      = [this]{ exportMidi(); };
    toolbar.onExportMusicXml  = [this]{ exportMusicXml(); };

    // ---- Transport ----
    transportBar.onPlay  = [this]{ audioEngine.play(); };
    transportBar.onPause = [this]{ audioEngine.pause(); };
    transportBar.onStop  = [this]{ audioEngine.stop(); onPositionChanged(audioEngine.getPosition()); };
    transportBar.onTempoChanged = [this](double bpm)
    {
        auto t = noteModel.getTempo(); t.bpm = bpm;
        tempoUserSet = true;
        noteModel.setTempo(t);
    };
    transportBar.onTimeSignatureChanged = [this](int num, int den)
    {
        auto t = noteModel.getTempo(); t.numerator = num; t.denominator = den;
        noteModel.setTempo(t);
        grabKeyboardFocus();
    };
    transportBar.onGridChanged   = [this](int id){ setGrid(id); grabKeyboardFocus(); };
    transportBar.onEstimateTempo = [this]{ estimateTempo(false); };
    transportBar.onAudioVolumeChanged  = [this](float v){ audioVol = v; audioEngine.setAudioVolume(v); };
    transportBar.onMidiVolumeChanged   = [this](float v){ midiVol = v; audioEngine.setMidiVolume(v); };
    transportBar.onAudioEnabledChanged = [this](bool b){ audioEngine.setAudioEnabled(b); };
    transportBar.onMidiEnabledChanged  = [this](bool b){ audioEngine.setMidiEnabled(b); };

    // ---- Views ----
    auto wheel = [this](const juce::MouseEvent& e, const juce::MouseWheelDetails& w){ handleWheel(e, w); };
    waveform.onWheel    = wheel;
    spectrogram.onWheel = wheel;
    noteOverlay.onWheel = wheel;
    keyboard.onWheel    = wheel;

    ruler.onSeek       = [this](double t){ seek(t); };
    waveform.onSeek    = [this](double t){ seek(t); };
    spectrogram.onSeek = [this](double t){ seek(t); };
    noteOverlay.onSeek = [this](double t){ seek(t); };
    // Dragging a section in the ruler or waveform defines the loop and turns it on
    auto loopDragged = [this](double s, double e, bool finished)
    {
        if (s < 0.0) { setLoop(-1.0, -1.0, false); return; }
        setLoop(s, e, true);
        if (finished && loopOn && audioEngine.isFileLoaded() && !audioEngine.isPlaying())
            seek(loopStart);
    };
    waveform.onLoopDragged = loopDragged;
    ruler.onLoopDragged    = loopDragged;
    waveform.setSnap([this](double t){ return snapLoopEdge(t); });
    ruler.setSnap([this](double t){ return snapLoopEdge(t); });
    transportBar.onToggleLoop = [this]{ toggleLoop(); };

    keyboard.onKeyPressed      = [this](int midi){ audioEngine.previewNote(midi, 90, 0.6); };
    noteOverlay.onNotePreview  = [this](int midi, int vel){ audioEngine.previewNote(midi, vel, 0.35); };

    divider.onDrag = [this](int dy)
    {
        waveformHeight = juce::jlimit(40, 400, waveformHeight + dy);
        resized();
    };

    progressOverlay.onCancel = [this]{ cancelAnalysis(); };

    // ---- Commands ----
    commandManager.registerAllCommandsForTarget(this);
    commandManager.setFirstCommandTarget(this);
    addKeyListener(commandManager.getKeyMappings());
    setWantsKeyboardFocus(true);
    addMouseListener(this, true);

    loadAppState();
    spectrogram.setDynamicRange(settings.displayRangeDb);
    spectrogram.setHintText(U("Træk en lydfil hertil, eller klik Åbn\n(WAV, FLAC, AIFF eller MP3)"));
    audioEngine.setAudioVolume(audioVol);
    audioEngine.setMidiVolume(midiVol);
    transportBar.setMixer(audioVol, midiVol, true, true);
    transportBar.setGrid(gridId);
    transportBar.setTempo(noteModel.getTempo());
    setPianoRollVisible(pianoRollVisible);
    applyGrid();
    setTool(EditTool::Select);

    viewState.startTimeSec = 0.0;
    viewState.endTimeSec   = 10.0;
    viewState.midiLow  = 28;
    viewState.midiHigh = 96;
    updateViewState();
    updateToolbarState();

    setSize(1280, 820);
}

MainComponent::~MainComponent()
{
    saveAppState();
    stopTimer();
    spectralAnalyzer.cancel();
    pitchDetector.cancel();
    hScrollBar.removeListener(this);
    vScrollBar.removeListener(this);
    noteModel.getUndoManager().removeChangeListener(this);
    noteModel.removeListener(this);
    audioEngine.shutdownAudio();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

// =========================================================================
// Layout & painting
// =========================================================================
void MainComponent::resized()
{
    const int w = getWidth(), h = getHeight();
    const int tb = Toolbar::kHeight, tr = TransportBar::kHeight;
    const int kb = PianoKeyboard::kWidth;
    const int sb = 14;
    const int rulerH = TimelineRuler::kHeight;

    toolbar.setBounds(0, 0, w, tb);
    transportBar.setBounds(0, h - tr, w, tr);

    const int right = w - sb;
    int y = tb;
    ruler.setBounds(kb, y, right - kb, rulerH);
    y += rulerH;

    const int maxWf = std::max(40, h - tb - tr - rulerH - 6 - sb - 140);
    const int wfH = juce::jlimit(40, maxWf, waveformHeight);
    waveform.setBounds(kb, y, right - kb, wfH);
    y += wfH;

    divider.setBounds(0, y, w, 6);
    y += 6;

    const int specH = std::max(60, h - tr - sb - y);
    const juce::Rectangle<int> area(kb, y, right - kb, specH);
    keyboard.setBounds(0, y, kb, specH);
    spectrogram.setBounds(area);
    pianoRoll.setBounds(area);
    noteOverlay.setBounds(area);
    progressOverlay.setBounds(area);
    vScrollBar.setBounds(right, y, sb, specH);
    hScrollBar.setBounds(kb, y + specH, right - kb, sb);

    updateViewState();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Colours::SurfaceRaised));

    // Caption beside the waveform
    auto cap = juce::Rectangle<int>(0, Toolbar::kHeight, PianoKeyboard::kWidth,
                                    TimelineRuler::kHeight + waveform.getHeight());
    g.setColour(juce::Colour(Colours::TextSecondary));
    g.setFont(SoundstaveLookAndFeel::getBoldFont(11.0f));
    g.drawFittedText(U("Lyd"), cap.reduced(4), juce::Justification::centred, 1);

    // Corner between scrollbars
    g.setColour(juce::Colour(Colours::SurfaceRaised));
    g.fillRect(getWidth() - 14, hScrollBar.getY(), 14, 14);
}

void MainComponent::Divider::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Colours::SurfaceRaised));
    g.setColour(juce::Colour(Colours::Border));
    g.drawHorizontalLine(0, 0.0f, (float)getWidth());
    g.drawHorizontalLine(getHeight() - 1, 0.0f, (float)getWidth());
    const float cx = (float)getWidth() * 0.5f;
    g.setColour(juce::Colour(Colours::TextSecondary).withAlpha(0.6f));
    for (int i = -1; i <= 1; ++i)
        g.fillEllipse(cx + (float)i * 8.0f - 1.5f, (float)getHeight() * 0.5f - 1.5f, 3.0f, 3.0f);
}

void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    // Keep keyboard shortcuts working after clicking around, but don't steal
    // focus from text entry fields.
    auto* c = e.eventComponent;
    if (c == nullptr) return;
    if (dynamic_cast<juce::TextEditor*>(c) != nullptr) return;
    if (dynamic_cast<juce::Slider*>(c) != nullptr || c->findParentComponentOfClass<juce::Slider>() != nullptr) return;
    if (dynamic_cast<juce::ComboBox*>(c) != nullptr) return;
    if (!hasKeyboardFocus(false)) grabKeyboardFocus();
}

// =========================================================================
// View state
// =========================================================================
void MainComponent::updateViewState()
{
    ruler.setViewState(viewState, totalDuration);
    waveform.setViewState(viewState);
    spectrogram.setViewState(viewState);
    pianoRoll.setViewState(viewState);
    noteOverlay.setViewState(viewState);
    keyboard.setRange(viewState.midiLow, viewState.midiHigh);

    const double limit = std::max(totalDuration, viewState.endTimeSec);
    hScrollBar.setRangeLimits(0.0, std::max(0.1, limit), juce::dontSendNotification);
    hScrollBar.setCurrentRange(viewState.startTimeSec, viewState.visibleDuration(), juce::dontSendNotification);

    // Vertical scrollbar: top of the bar = highest pitch
    const int span = viewState.midiHigh - viewState.midiLow + 1;
    vScrollBar.setRangeLimits(0.0, 128.0, juce::dontSendNotification);
    vScrollBar.setCurrentRange(127.0 - viewState.midiHigh, (double)span, juce::dontSendNotification);
}

void MainComponent::scrollBarMoved(juce::ScrollBar* bar, double start)
{
    if (bar == &hScrollBar)
    {
        const double dur = viewState.visibleDuration();
        viewState.startTimeSec = start;
        viewState.endTimeSec   = start + dur;
        viewState.clamp(std::max(totalDuration, dur));
    }
    else
    {
        const int range = viewState.midiRange();
        viewState.midiHigh = juce::jlimit(range, 127, 127 - juce::roundToInt(start));
        viewState.midiLow  = viewState.midiHigh - range;
    }
    updateViewState();
}

void MainComponent::handleWheel(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto local = e.getEventRelativeTo(&spectrogram).position;
    const double dy = w.deltaY, dx = w.deltaX;
    const double dur = viewState.visibleDuration();
    const double total = std::max(totalDuration, dur);

    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        if (std::abs(dy) < 1.0e-9) return;
        const double focus = viewState.xToTime(local.x, spectrogram.getWidth());
        viewState.zoom(dy > 0 ? 0.8 : 1.25, focus, totalDuration);
    }
    else if (e.mods.isAltDown())
    {
        if (std::abs(dy) < 1.0e-9) return;
        const float centre = yToPitch(local.y, viewState.midiLow, viewState.midiHigh, spectrogram.getHeight());
        viewState.zoomMidi(dy > 0 ? 0.85 : 1.18, centre);
    }
    else if (e.mods.isShiftDown())
    {
        const double d = std::abs(dy) > 1.0e-9 ? dy : dx;
        if (std::abs(d) < 1.0e-9) return;
        const int steps = std::max(1, juce::roundToInt(std::abs(d) * 16.0));
        viewState.scrollMidi(d > 0 ? steps : -steps);
    }
    else if (std::abs(dx) > std::abs(dy))
    {
        viewState.scroll(-dx * dur * 0.5, total);
    }
    else
    {
        viewState.scroll(-dy * dur * 0.3, total);
    }
    updateViewState();
}

void MainComponent::onPositionChanged(double pos)
{
    const bool playing = audioEngine.isPlaying();
    transportBar.setPlaying(playing);
    transportBar.setPosition(pos, totalDuration);

    if (playing)
    {
        const double dur = viewState.visibleDuration();
        if (pos >= viewState.endTimeSec - dur * 0.08 || pos < viewState.startTimeSec)
        {
            viewState.startTimeSec = std::max(0.0, pos - dur * 0.08);
            viewState.endTimeSec   = viewState.startTimeSec + dur;
            viewState.clamp(std::max(totalDuration, dur));
            updateViewState();
        }

        std::bitset<128> active;
        for (auto& n : noteModel.getNotes())
            if (n.startSec <= pos && n.endSec > pos) active.set((size_t)n.midiNote);
        keyboard.setActiveNotes(active);
    }
    else
    {
        keyboard.setActiveNotes({});
    }

    waveform.setPlayheadPosition(pos);
    spectrogram.setPlayheadPosition(pos);
}

void MainComponent::seek(double t)
{
    if (!audioEngine.isFileLoaded()) return;
    audioEngine.setPosition(t);
    onPositionChanged(audioEngine.getPosition());
}

// =========================================================================
// Loop
// =========================================================================
double MainComponent::snapLoopEdge(double t) const
{
    return gridSeconds() > 0.0 ? snapTime(t) : t;
}

void MainComponent::setLoop(double startSec, double endSec, bool enabled)
{
    if (startSec < 0.0 || endSec - startSec < 0.05 || !audioEngine.isFileLoaded())
    {
        startSec = endSec = -1.0;
        enabled = false;
    }
    else
    {
        startSec = juce::jlimit(0.0, totalDuration, startSec);
        endSec   = juce::jlimit(0.0, totalDuration, endSec);
    }
    loopStart = startSec;
    loopEnd   = endSec;
    loopOn    = enabled;

    if (loopStart >= 0.0) audioEngine.setLoop(loopOn, loopStart, loopEnd);
    else                  audioEngine.clearLoopRegion();
    ruler.setLoopRegion(loopStart, loopEnd, loopOn);
    waveform.setLoopRegion(loopStart, loopEnd, loopOn);
    spectrogram.setLoopRegion(loopStart, loopEnd, loopOn);
    transportBar.setLoopState(loopOn);
}

void MainComponent::toggleLoop()
{
    if (!audioEngine.isFileLoaded()) return;

    if (loopOn)
    {
        setLoop(loopStart, loopEnd, false);
        return;
    }

    if (loopStart < 0.0)
    {
        // No section yet: loop the selected notes, otherwise what is on screen
        if (noteModel.getNumSelected() > 0) { loopSelection(); return; }
        setLoop(snapLoopEdge(viewState.startTimeSec),
                snapLoopEdge(std::min(totalDuration, viewState.endTimeSec)), true);
    }
    else
    {
        setLoop(loopStart, loopEnd, true);
    }

    const double pos = audioEngine.getPosition();
    if (loopOn && (pos < loopStart || pos >= loopEnd)) seek(loopStart);
}

void MainComponent::loopSelection()
{
    if (!audioEngine.isFileLoaded()) return;
    const auto sel = noteModel.getSelectedNotes();
    if (sel.empty())
    {
        alert(false, U("Loop"), U("Markér de noder, der skal gentages, eller træk et stykke i tidslinjen."));
        return;
    }
    double s = 1e18, e = 0.0;
    for (auto& n : sel) { s = std::min(s, n.startSec); e = std::max(e, n.endSec); }

    // Widen to the grid so the loop keeps the rhythm
    const double g = gridSeconds();
    if (g > 0.0)
    {
        const double origin = noteModel.getTempo().offsetSec;
        s = origin + std::floor((s - origin) / g + 1e-6) * g;
        e = origin + std::ceil ((e - origin) / g - 1e-6) * g;
    }
    setLoop(std::max(0.0, s), e, true);
    seek(loopStart);
}

void MainComponent::autoRangeMidi()
{
    const auto& notes = noteModel.getNotes();
    if (notes.empty()) return;

    std::vector<int> pitches;
    for (auto& n : notes) pitches.push_back(n.midiNote);
    std::sort(pitches.begin(), pitches.end());
    // Ignore the extreme 1 % so a few outliers don't flatten the view
    const size_t cut = pitches.size() / 100;
    int lo = pitches[cut] - 5;
    int hi = pitches[pitches.size() - 1 - cut] + 5;
    if (hi - lo < 24) { const int c = (lo + hi) / 2; lo = c - 12; hi = c + 12; }
    lo = juce::jlimit(0, 103, lo);
    hi = juce::jlimit(lo + 24, 127, hi);
    viewState.midiLow = lo;
    viewState.midiHigh = hi;
    updateViewState();
}

void MainComponent::zoomToFit()
{
    if (totalDuration <= 0.0) return;
    viewState.startTimeSec = 0.0;
    viewState.endTimeSec = totalDuration;
    updateViewState();
}

void MainComponent::setTool(EditTool tool)
{
    noteOverlay.setTool(tool);
    toolbar.setTool(tool);
}

void MainComponent::setPianoRollVisible(bool visible)
{
    pianoRollVisible = visible;
    pianoRoll.setVisible(visible);
    noteOverlay.setVisible(visible);
    toolbar.setPianoRollVisible(visible);
}

void MainComponent::setGrid(int id)
{
    gridId = juce::jlimit((int)GridFree, (int)GridEighthTriplet, id);
    transportBar.setGrid(gridId);
    applyGrid();
}

double MainComponent::gridSeconds() const
{
    const double q = noteModel.getTempo().quarterSec();
    switch (gridId)
    {
        case GridQuarter:       return q;
        case GridEighth:        return q / 2.0;
        case GridSixteenth:     return q / 4.0;
        case GridThirtySecond:  return q / 8.0;
        case GridEighthTriplet: return q / 3.0;
        default:                return 0.0;
    }
}

double MainComponent::snapTime(double t) const
{
    const double g = gridSeconds();
    if (g <= 0.0) return std::max(0.0, t);
    const double origin = noteModel.getTempo().offsetSec;
    double s = origin + std::round((t - origin) / g) * g;
    while (s < 0.0) s += g;
    return s;
}

void MainComponent::applyGrid()
{
    const double g = gridSeconds();
    noteOverlay.setSnap([this](double t){ return snapTime(t); }, g);
    pianoRoll.setTempo(noteModel.getTempo());
    pianoRoll.setGridSeconds(g);
}

// =========================================================================
// Model listener
// =========================================================================
void MainComponent::notesChanged()
{
    syncPlaybackNotes();
    updateTitle();
    updateToolbarState();
}

void MainComponent::selectionChanged()
{
    updateToolbarState();
}

void MainComponent::tempoChanged()
{
    transportBar.setTempo(noteModel.getTempo());
    applyGrid();
    updateTitle();
}

void MainComponent::syncPlaybackNotes()
{
    std::vector<PlaybackNote> pn;
    pn.reserve(noteModel.getNotes().size());
    for (auto& n : noteModel.getNotes())
        pn.push_back({ n.startSec, n.endSec, n.midiNote, n.velocity });
    audioEngine.setPlaybackNotes(std::move(pn));
}

void MainComponent::updateTitle()
{
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        window->setName(projectManager.getWindowTitle());
}

void MainComponent::updateToolbarState()
{
    auto& um = noteModel.getUndoManager();
    toolbar.setEditState(!noteModel.getNotes().empty(), noteModel.getNumSelected() > 0,
                         um.canUndo(), um.canRedo());
}

// =========================================================================
// Files
// =========================================================================
void MainComponent::openWithChooser()
{
    const auto start = lastDirectory.isDirectory() ? lastDirectory
                     : juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    chooser = std::make_unique<juce::FileChooser>(U("Åbn lydfil eller projekt"), start,
                                                  juce::String(kAudioPatterns) + ";*.soundstave");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this](const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile()) openFile(f);
    });
}

void MainComponent::openFileFromCommandLine(const juce::File& f, bool analyse)
{
    if (!f.existsAsFile()) return;
    openFile(f);
    if (analyse && audioEngine.isFileLoaded()) runAnalysis();
}

void MainComponent::openFile(const juce::File& file)
{
    confirmUnsaved([this, file]
    {
        lastDirectory = file.getParentDirectory();
        if (file.hasFileExtension("soundstave"))
            loadProject(file);
        else if (isAudioFile(file))
            loadAudio(file, true);
        else
            alert(true, U("Kan ikke åbne filen"), U("Filtypen understøttes ikke: ") + file.getFileName());
    });
}

bool MainComponent::loadAudio(const juce::File& file, bool startNewProject)
{
    cancelAnalysis();
    audioEngine.stop();

    juce::String error;
    if (!audioEngine.loadFile(file, error))
    {
        alert(true, U("Kan ikke åbne lydfilen"), file.getFileName() + "\n\n" + error);
        return false;
    }

    if (startNewProject)
    {
        projectManager.newProject();
        tempoUserSet = false;
    }
    projectManager.setAudioFile(file);

    totalDuration = audioEngine.getTotalLengthSeconds();
    spectrogramData.reset();
    spectrogram.clearData();
    waveform.setAudioBuffer(audioEngine.getAudioBuffer(), audioEngine.getSampleRate());
    setLoop(-1.0, -1.0, false);

    viewState.startTimeSec = 0.0;
    viewState.endTimeSec   = std::min(totalDuration, 30.0);
    if (startNewProject) { viewState.midiLow = 28; viewState.midiHigh = 96; }
    updateViewState();
    onPositionChanged(0.0);

    transportBar.setHasAudio(true);
    toolbar.setHasAudio(true);
    syncPlaybackNotes();
    updateTitle();
    updateToolbarState();

    startSpectrogram(false);
    return true;
}

void MainComponent::loadProject(const juce::File& file)
{
    juce::String error;
    cancelAnalysis();
    if (!projectManager.openProject(file, error))
    {
        alert(true, U("Kan ikke åbne projektet"), error);
        return;
    }
    const auto view  = projectManager.getViewSettings();
    const auto audio = projectManager.getAudioFile();

    // Analysis/display settings must be in place before the spectrogram starts
    auto savedSettings = view.getChildWithName("Settings");
    if (savedSettings.isValid())
    {
        settings.fromValueTree(savedSettings);
        spectrogram.setDynamicRange(settings.displayRangeDb);
    }

    if (!audio.existsAsFile())
    {
        clearAudioState(U("Lydfilen blev ikke fundet.\nNoderne kan stadig redigeres og eksporteres."));
        alert(true, U("Lydfilen mangler"),
              U("Den lydfil, projektet henviser til, blev ikke fundet:\n") + audio.getFullPathName());
    }
    else if (!loadAudio(audio, false))   // loadAudio has shown the error
    {
        clearAudioState(U("Lydfilen kunne ikke indlæses.\nNoderne kan stadig redigeres og eksporteres."));
    }

    tempoUserSet = true;
    applyViewSettings(view);
    transportBar.setTempo(noteModel.getTempo());
    applyGrid();
    syncPlaybackNotes();
    updateTitle();
    updateToolbarState();
}

void MainComponent::clearAudioState(const juce::String& hint)
{
    audioEngine.closeFile();
    waveform.clearAudio();
    spectrogram.clearData();
    setLoop(-1.0, -1.0, false);
    spectrogramData.reset();
    totalDuration = 0.0;
    for (auto& n : noteModel.getNotes()) totalDuration = std::max(totalDuration, n.endSec + 1.0);
    transportBar.setHasAudio(false);
    toolbar.setHasAudio(false);
    spectrogram.setHintText(hint);
    onPositionChanged(0.0);
    updateViewState();
}

// =========================================================================
// Analysis
// =========================================================================
AnalysisParams MainComponent::effectiveAnalysisParams() const
{
    // Store bins up to the highest note the user wants detected (+ headroom
    // for its upper partials), never below 6 kHz so the picture stays useful.
    AnalysisParams p = settings.analysis;
    const double top = Note::midiToFreq((double)settings.detection.maxMidiNote + 0.5) * 1.5;
    p.maxFreqHz = (float)juce::jlimit(6000.0, audioEngine.getSampleRate() * 0.5, top);
    return p;
}

void MainComponent::startSpectrogram(bool thenDetect)
{
    auto* buf = audioEngine.getAudioBuffer();
    if (buf == nullptr) return;

    pendingDetection = thenDetect;
    spectrogram.setHintText({});
    showProgress(U("Beregner spektrogram…"));
    const auto params = effectiveAnalysisParams();
    spectralAnalyzer.analyse(*buf, audioEngine.getSampleRate(), params,
                             [this, params](SpectrogramPtr data)
    {
        spectrogramParams = params;
        onSpectrogramReady(std::move(data));
    });
}

void MainComponent::onSpectrogramReady(SpectrogramPtr data)
{
    if (data == nullptr)
    {
        pendingDetection = false;
        hideProgress();
        spectrogram.setHintText(U("Lyden kunne ikke analyseres."));
        alert(true, U("Analyse mislykkedes"), U("Lydfilen kunne ikke analyseres (ugyldig samplerate eller tom fil)."));
        return;
    }
    spectrogramData = std::move(data);
    spectrogram.setData(spectrogramData);
    if (!tempoUserSet) estimateTempo(true);
    if (pendingDetection)
        startDetection(true);
    else
        hideProgress();
}

void MainComponent::runAnalysis()
{
    if (!audioEngine.isFileLoaded())
    {
        alert(false, U("Ingen lydfil"), U("Åbn en lydfil, før du analyserer."));
        return;
    }
    if (spectralAnalyzer.isRunning())
    {
        pendingDetection = true;    // detection follows when the STFT finishes
        showProgress(U("Beregner spektrogram…"));
        return;
    }
    const auto wanted = effectiveAnalysisParams();
    if (spectrogramData == nullptr || spectrogramParams.fftOrder != wanted.fftOrder
        || std::abs(spectrogramParams.maxFreqHz - wanted.maxFreqHz) > 1.0f)
        startSpectrogram(true);
    else
        startDetection(false);
}

void MainComponent::startDetection(bool chained)
{
    pendingDetection = false;
    detectionBase = chained ? 0.5f : 0.0f;
    showProgress(U("Finder toner…"));
    pitchDetector.detect(spectrogramData, settings.detection,
                         [this](std::vector<Note> notes){ onDetectionComplete(std::move(notes)); });
}

void MainComponent::onDetectionComplete(std::vector<Note> notes)
{
    hideProgress();
    const bool none = notes.empty();
    noteModel.replaceAnalysisNotes(std::move(notes));
    if (!tempoUserSet) estimateTempo(true);
    autoRangeMidi();

    if (none)
        alert(false, U("Ingen toner fundet"),
              U("Der blev ikke fundet nogen toner. Prøv at øge følsomheden under Indstillinger."));
}

void MainComponent::estimateTempo(bool quiet)
{
    // Prefer the audio itself (works before analysis and with loose timing);
    // fall back to the detected notes, e.g. when the audio file is missing.
    TempoEstimator::Result r;
    if (spectrogramData != nullptr)
        r = TempoEstimator::estimate(*spectrogramData);
    if (!r.valid && !noteModel.getNotes().empty())
        r = TempoEstimator::estimate(noteModel.getNotes());

    if (!r.valid)
    {
        if (!quiet)
        {
            if (spectrogramData == nullptr && spectralAnalyzer.isRunning())
                alert(false, U("Tempo"), U("Spektrogrammet beregnes stadig. Prøv igen om et øjeblik."));
            else if (spectrogramData == nullptr && noteModel.getNotes().empty())
                alert(false, U("Tempo"), U("Åbn en lydfil først."));
            else
                alert(false, U("Tempo"), U("Optagelsen har ingen tydelig fast puls, så tempoet kunne ikke "
                                           "findes automatisk. Indtast tempoet manuelt i feltet Tempo."));
        }
        return;
    }
    auto t = noteModel.getTempo();
    t.bpm = r.bpm;
    t.offsetSec = r.offsetSec;
    noteModel.setTempo(t);
    if (!quiet) tempoUserSet = true;
}

void MainComponent::cancelAnalysis()
{
    spectralAnalyzer.cancel();
    pitchDetector.cancel();
    pendingDetection = false;
    hideProgress();
    if (spectrogramData == nullptr && audioEngine.isFileLoaded())
        spectrogram.setHintText(U("Spektrogrammet blev annulleret. Klik Analysér for at fortsætte."));
}

void MainComponent::showProgress(const juce::String& message)
{
    progressMessage = message;
    progressOverlay.setProgress(0.0f, message);
    progressOverlay.setVisible(true);
    toolbar.setAnalysisRunning(true);
    startTimerHz(20);
}

void MainComponent::hideProgress()
{
    stopTimer();
    progressOverlay.setVisible(false);
    toolbar.setAnalysisRunning(false);
    toolbar.setHasAudio(audioEngine.isFileLoaded());
}

void MainComponent::timerCallback()
{
    // Poll worker progress (the workers never touch UI objects)
    const bool chained = pendingDetection;
    if (spectralAnalyzer.isRunning())
        progressOverlay.setProgress(spectralAnalyzer.getProgress() * (chained ? 0.5f : 1.0f), progressMessage);
    else if (pitchDetector.isRunning())
        progressOverlay.setProgress(detectionBase + pitchDetector.getProgress() * (1.0f - detectionBase), progressMessage);
}

// =========================================================================
// Save / close
// =========================================================================
void MainComponent::save(std::function<void(bool)> onDone)
{
    if (!projectManager.hasProjectFile())
    {
        saveAs(std::move(onDone));
        return;
    }
    projectManager.setViewSettings(makeViewSettings());
    const bool ok = projectManager.saveProject();
    if (!ok) alert(true, U("Kunne ikke gemme"), projectManager.getProjectFile().getFullPathName());
    updateTitle();
    if (onDone) onDone(ok);
}

void MainComponent::saveAs(std::function<void(bool)> onDone)
{
    juce::File suggestion;
    if (projectManager.hasProjectFile())
        suggestion = projectManager.getProjectFile();
    else if (projectManager.getAudioFile() != juce::File())
        suggestion = projectManager.getAudioFile().withFileExtension(ProjectManager::kExtension);
    else
        suggestion = (lastDirectory.isDirectory() ? lastDirectory
                      : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory))
                     .getChildFile(U("Unavngivet.soundstave"));

    chooser = std::make_unique<juce::FileChooser>(U("Gem projekt"), suggestion, "*.soundstave");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::warnAboutOverwriting,
                         [this, onDone](const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f == juce::File()) { if (onDone) onDone(false); return; }
        projectManager.setViewSettings(makeViewSettings());
        const bool ok = projectManager.saveProjectAs(f);
        if (!ok) alert(true, U("Kunne ikke gemme"), f.getFullPathName());
        else lastDirectory = f.getParentDirectory();
        updateTitle();
        if (onDone) onDone(ok);
    });
}

void MainComponent::confirmUnsaved(std::function<void()> proceed)
{
    if (!projectManager.hasUnsavedChanges())
    {
        proceed();
        return;
    }

    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle(U("Ikke-gemte ændringer"))
        .withMessage(U("Projektet har ændringer, der ikke er gemt. Vil du gemme dem først?"))
        .withButton(U("Gem"))
        .withButton(U("Gem ikke"))
        .withButton(U("Annullér"))
        .withAssociatedComponent(this);

    juce::Component::SafePointer<MainComponent> safe(this);
    juce::AlertWindow::showAsync(options, [safe, proceed](int result)
    {
        if (safe == nullptr) return;
        if (result == 1)        safe->save([proceed](bool ok){ if (ok) proceed(); });
        else if (result == 2)   proceed();
    });
}

void MainComponent::requestClose(std::function<void()> onProceed)
{
    confirmUnsaved([this, onProceed]
    {
        saveAppState();
        onProceed();
    });
}

void MainComponent::alert(bool warning, const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync(warning ? juce::MessageBoxIconType::WarningIcon
                                                   : juce::MessageBoxIconType::InfoIcon,
                                           title, message, "OK", this);
}

// =========================================================================
// Export
// =========================================================================
void MainComponent::exportMidi()
{
    if (noteModel.getNotes().empty())
    {
        alert(false, U("Ingen noder"), U("Der er ingen noder at eksportere. Analysér lyden eller tegn noder først."));
        return;
    }
    const auto base = projectManager.getAudioFile() != juce::File()
                        ? projectManager.getAudioFile().withFileExtension(".mid")
                        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Soundstave.mid");
    chooser = std::make_unique<juce::FileChooser>(U("Eksportér MIDI"), base, "*.mid");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::warnAboutOverwriting,
                         [this](const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        if (!f.hasFileExtension("mid;midi")) f = f.withFileExtension(".mid");
        if (MidiExporter::exportToFile(noteModel.getNotes(), noteModel.getTempo(), f))
            alert(false, U("MIDI eksporteret"), f.getFullPathName());
        else
            alert(true, U("Eksport mislykkedes"), U("MIDI-filen kunne ikke skrives."));
    });
}

void MainComponent::exportMusicXml()
{
    if (noteModel.getNotes().empty())
    {
        alert(false, U("Ingen noder"), U("Der er ingen noder at eksportere. Analysér lyden eller tegn noder først."));
        return;
    }

    MusicXmlExportPanel::show(this, noteModel.getNotes(), noteModel.getTempo(), lastXmlResolution,
                              [this](QuantizeResolution res)
    {
        lastXmlResolution = res;
        const auto audio = projectManager.getAudioFile();
        const auto title = audio != juce::File() ? audio.getFileNameWithoutExtension() : juce::String("Soundstave");
        const auto base  = audio != juce::File() ? audio.withFileExtension(".musicxml")
                         : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Soundstave.musicxml");

        chooser = std::make_unique<juce::FileChooser>(U("Eksportér MusicXML"), base, "*.musicxml");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting,
                             [this, res, title](const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (!f.hasFileExtension("musicxml;xml")) f = f.withFileExtension(".musicxml");
            if (MusicXmlExporter::exportToFile(noteModel.getNotes(), noteModel.getTempo(), res, f, title))
                alert(false, U("MusicXML eksporteret"), f.getFullPathName());
            else
                alert(true, U("Eksport mislykkedes"), U("MusicXML-filen kunne ikke skrives."));
        });
    });
}

void MainComponent::showSettings()
{
    juce::Component::SafePointer<MainComponent> safe(this);
    SettingsPanel::show(this, settings, [safe](const AppSettings& s)
    {
        if (safe == nullptr) return;
        safe->settings = s;
        safe->spectrogram.setDynamicRange(s.displayRangeDb);
        // A different FFT size gives a different picture: recompute it now
        if (safe->spectrogramData != nullptr && safe->spectrogramParams.fftOrder != s.analysis.fftOrder
            && !safe->spectralAnalyzer.isRunning())
            safe->startSpectrogram(false);
    });
}

// =========================================================================
// Persistence of view/app state
// =========================================================================
juce::ValueTree MainComponent::makeViewSettings() const
{
    juce::ValueTree v("View");
    v.setProperty("start",     viewState.startTimeSec, nullptr);
    v.setProperty("end",       viewState.endTimeSec,   nullptr);
    v.setProperty("midiLow",   viewState.midiLow,      nullptr);
    v.setProperty("midiHigh",  viewState.midiHigh,     nullptr);
    v.setProperty("pianoRoll", pianoRollVisible,       nullptr);
    v.setProperty("grid",      gridId,                 nullptr);
    v.setProperty("waveformHeight", waveformHeight,    nullptr);
    v.setProperty("xmlResolution", (int)lastXmlResolution, nullptr);
    v.setProperty("loopStart", loopStart, nullptr);
    v.setProperty("loopEnd",   loopEnd,   nullptr);
    v.setProperty("loopOn",    loopOn,    nullptr);
    v.addChild(settings.toValueTree(), -1, nullptr);
    return v;
}

void MainComponent::applyViewSettings(const juce::ValueTree& v)
{
    if (!v.isValid()) return;
    if (v.hasProperty("start"))
    {
        viewState.startTimeSec = (double)v.getProperty("start");
        viewState.endTimeSec   = std::max(viewState.startTimeSec + 0.1, (double)v.getProperty("end"));
        viewState.clamp(std::max(totalDuration, viewState.visibleDuration()));
    }
    if (v.hasProperty("midiLow"))
    {
        viewState.midiLow  = juce::jlimit(0, 115, (int)v.getProperty("midiLow"));
        viewState.midiHigh = juce::jlimit(viewState.midiLow + 12, 127, (int)v.getProperty("midiHigh"));
    }
    if (v.hasProperty("pianoRoll"))      setPianoRollVisible((bool)v.getProperty("pianoRoll"));
    if (v.hasProperty("grid"))           setGrid((int)v.getProperty("grid"));
    if (v.hasProperty("waveformHeight")) waveformHeight = (int)v.getProperty("waveformHeight");
    if (v.hasProperty("xmlResolution"))
        lastXmlResolution = (QuantizeResolution)juce::jlimit(0, 4, (int)v.getProperty("xmlResolution"));
    if (v.hasProperty("loopStart"))
    {
        const double ls = (double)v.getProperty("loopStart"), le = (double)v.getProperty("loopEnd");
        if (std::isfinite(ls) && std::isfinite(le))
            setLoop(ls, le, (bool)v.getProperty("loopOn"));
    }
    auto s = v.getChildWithName("Settings");
    if (s.isValid())
    {
        settings.fromValueTree(s);
        spectrogram.setDynamicRange(settings.displayRangeDb);
    }
    resized();
}

void MainComponent::loadAppState()
{
    waveformHeight   = props->getIntValue("waveformHeight", 90);
    pianoRollVisible = props->getBoolValue("pianoRoll", true);
    gridId           = props->getIntValue("grid", GridSixteenth);
    audioVol         = (float)props->getDoubleValue("audioVolume", 0.8);
    midiVol          = (float)props->getDoubleValue("midiVolume", 0.6);
    lastDirectory    = juce::File(props->getValue("lastDirectory"));
    if (auto xml = props->getXmlValue("settings"))
        settings.fromValueTree(juce::ValueTree::fromXml(*xml));
}

void MainComponent::saveAppState()
{
    if (props == nullptr) return;
    props->setValue("waveformHeight", waveformHeight);
    props->setValue("pianoRoll",      pianoRollVisible);
    props->setValue("grid",           gridId);
    props->setValue("audioVolume",    (double)audioVol);
    props->setValue("midiVolume",     (double)midiVol);
    props->setValue("lastDirectory",  lastDirectory.getFullPathName());
    if (auto xml = settings.toValueTree().createXml())
        props->setValue("settings", xml.get());
    props->saveIfNeeded();
}

// =========================================================================
// Drag & drop
// =========================================================================
bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& f : files)
    {
        juce::File file(f);
        if (isAudioFile(file) || file.hasFileExtension("soundstave")) return true;
    }
    return false;
}

void MainComponent::filesDropped(const juce::StringArray& files, int, int)
{
    for (auto& f : files)
    {
        juce::File file(f);
        if (isAudioFile(file) || file.hasFileExtension("soundstave"))
        {
            openFile(file);
            return;
        }
    }
}

// =========================================================================
// Commands
// =========================================================================
void MainComponent::getAllCommands(juce::Array<juce::CommandID>& c)
{
    c.addArray({ cmdOpen, cmdSave, cmdSaveAs, cmdAnalyse, cmdPlay, cmdStop, cmdGoToStart,
                 cmdUndo, cmdRedo, cmdSelectAll, cmdDeselect, cmdDelete, cmdCopy, cmdCut, cmdPaste,
                 cmdMerge, cmdTransposeUp, cmdTransposeDown, cmdOctaveUp, cmdOctaveDown,
                 cmdNudgeLeft, cmdNudgeRight, cmdZoomIn, cmdZoomOut, cmdZoomFit,
                 cmdToolSelect, cmdToolDraw, cmdToolSplit, cmdTogglePianoRoll,
                 cmdExportMidi, cmdExportMusicXml, cmdSettings, cmdToggleLoop, cmdLoopSelection });
}

void MainComponent::getCommandInfo(juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    using MK = juce::ModifierKeys;
    using KP = juce::KeyPress;
    const int cmd = MK::commandModifier, shift = MK::shiftModifier;
    auto set = [&](const char* name, const char* cat) { info.setInfo(U(name), U(name), U(cat), 0); };

    switch (id)
    {
        case cmdOpen:      set("Åbn", "Fil");            info.addDefaultKeypress('o', cmd); break;
        case cmdSave:      set("Gem", "Fil");            info.addDefaultKeypress('s', cmd); break;
        case cmdSaveAs:    set("Gem som", "Fil");        info.addDefaultKeypress('s', cmd | shift); break;
        case cmdAnalyse:   set("Analysér", "Analyse");   info.addDefaultKeypress('r', cmd); break;
        case cmdPlay:      set("Afspil/pause", "Afspilning"); info.addDefaultKeypress(KP::spaceKey, 0); break;
        case cmdStop:      set("Stop", "Afspilning");    info.addDefaultKeypress(KP::spaceKey, shift); break;
        case cmdGoToStart: set("Til start", "Afspilning"); info.addDefaultKeypress(KP::homeKey, 0); break;
        case cmdUndo:      set("Fortryd", "Rediger");    info.addDefaultKeypress('z', cmd); break;
        case cmdRedo:      set("Gentag", "Rediger");     info.addDefaultKeypress('z', cmd | shift);
                                                         info.addDefaultKeypress('y', cmd); break;
        case cmdSelectAll: set("Markér alle", "Rediger"); info.addDefaultKeypress('a', cmd); break;
        case cmdDeselect:  set("Fjern markering", "Rediger"); info.addDefaultKeypress(KP::escapeKey, 0); break;
        case cmdDelete:    set("Slet", "Rediger");       info.addDefaultKeypress(KP::deleteKey, 0);
                                                         info.addDefaultKeypress(KP::backspaceKey, 0); break;
        case cmdCopy:      set("Kopiér", "Rediger");     info.addDefaultKeypress('c', cmd); break;
        case cmdCut:       set("Klip", "Rediger");       info.addDefaultKeypress('x', cmd); break;
        case cmdPaste:     set("Indsæt", "Rediger");     info.addDefaultKeypress('v', cmd); break;
        case cmdMerge:     set("Saml noder", "Rediger"); info.addDefaultKeypress('j', cmd); break;
        case cmdTransposeUp:   set("Halvtone op", "Rediger");  info.addDefaultKeypress(KP::upKey, 0); break;
        case cmdTransposeDown: set("Halvtone ned", "Rediger"); info.addDefaultKeypress(KP::downKey, 0); break;
        case cmdOctaveUp:      set("Oktav op", "Rediger");     info.addDefaultKeypress(KP::upKey, shift); break;
        case cmdOctaveDown:    set("Oktav ned", "Rediger");    info.addDefaultKeypress(KP::downKey, shift); break;
        case cmdNudgeLeft:     set("Flyt til venstre", "Rediger"); info.addDefaultKeypress(KP::leftKey, 0); break;
        case cmdNudgeRight:    set("Flyt til højre", "Rediger");   info.addDefaultKeypress(KP::rightKey, 0); break;
        case cmdZoomIn:    set("Zoom ind", "Visning");   info.addDefaultKeypress('=', cmd);
                                                         info.addDefaultKeypress('+', cmd); break;
        case cmdZoomOut:   set("Zoom ud", "Visning");    info.addDefaultKeypress('-', cmd); break;
        case cmdZoomFit:   set("Vis hele", "Visning");   info.addDefaultKeypress('0', cmd); break;
        case cmdToolSelect: set("Markér-værktøj", "Værktøj"); info.addDefaultKeypress('s', 0); break;
        case cmdToolDraw:   set("Tegne-værktøj", "Værktøj");  info.addDefaultKeypress('d', 0); break;
        case cmdToolSplit:  set("Dele-værktøj", "Værktøj");   info.addDefaultKeypress('c', 0); break;
        case cmdTogglePianoRoll: set("Pianoroll", "Visning"); info.addDefaultKeypress('p', 0); break;
        case cmdExportMidi:     set("Eksportér MIDI", "Fil");     info.addDefaultKeypress('e', cmd); break;
        case cmdExportMusicXml: set("Eksportér MusicXML", "Fil"); info.addDefaultKeypress('e', cmd | shift); break;
        case cmdSettings:  set("Indstillinger", "Fil");  info.addDefaultKeypress(',', cmd); break;
        case cmdToggleLoop:    set("Loop til/fra", "Afspilning");        info.addDefaultKeypress('l', 0); break;
        case cmdLoopSelection: set("Loop markerede noder", "Afspilning"); info.addDefaultKeypress('l', cmd); break;
        default: break;
    }
}

bool MainComponent::perform(const juce::ApplicationCommandTarget::InvocationInfo& info)
{
    const double nudge = gridSeconds() > 0.0 ? gridSeconds() : 0.05;
    switch (info.commandID)
    {
        case cmdOpen:      openWithChooser(); return true;
        case cmdSave:      save(); return true;
        case cmdSaveAs:    saveAs(); return true;
        case cmdAnalyse:   runAnalysis(); return true;
        case cmdPlay:      if (audioEngine.isPlaying()) audioEngine.pause(); else audioEngine.play(); return true;
        case cmdStop:      audioEngine.stop(); onPositionChanged(audioEngine.getPosition()); return true;
        case cmdGoToStart: seek(0.0); return true;
        case cmdUndo:      noteModel.getUndoManager().undo(); return true;
        case cmdRedo:      noteModel.getUndoManager().redo(); return true;
        case cmdSelectAll: noteModel.selectAll(); return true;
        case cmdDeselect:  noteModel.deselectAll(); return true;
        case cmdDelete:    noteModel.deleteSelected(); return true;
        case cmdCopy:      noteModel.copySelected(); return true;
        case cmdCut:       noteModel.copySelected(); noteModel.deleteSelected(); return true;
        case cmdPaste:     noteModel.paste(snapTime(audioEngine.getPosition())); return true;
        case cmdMerge:     noteModel.mergeSelected(); return true;
        case cmdTransposeUp:   noteModel.transposeSelected(1);   return true;
        case cmdTransposeDown: noteModel.transposeSelected(-1);  return true;
        case cmdOctaveUp:      noteModel.transposeSelected(12);  return true;
        case cmdOctaveDown:    noteModel.transposeSelected(-12); return true;
        case cmdNudgeLeft:     noteModel.nudgeSelected(-nudge);  return true;
        case cmdNudgeRight:    noteModel.nudgeSelected(nudge);   return true;
        case cmdZoomIn:    viewState.zoom(0.7, audioEngine.getPosition(), totalDuration); updateViewState(); return true;
        case cmdZoomOut:   viewState.zoom(1.4, audioEngine.getPosition(), totalDuration); updateViewState(); return true;
        case cmdZoomFit:   zoomToFit(); return true;
        case cmdToolSelect: setTool(EditTool::Select); return true;
        case cmdToolDraw:   setTool(EditTool::Draw);   return true;
        case cmdToolSplit:  setTool(EditTool::Split);  return true;
        case cmdTogglePianoRoll: setPianoRollVisible(!pianoRollVisible); return true;
        case cmdExportMidi:     exportMidi(); return true;
        case cmdExportMusicXml: exportMusicXml(); return true;
        case cmdSettings:  showSettings(); return true;
        case cmdToggleLoop:    toggleLoop(); return true;
        case cmdLoopSelection: loopSelection(); return true;
        default: return false;
    }
}

} // namespace Soundstave
