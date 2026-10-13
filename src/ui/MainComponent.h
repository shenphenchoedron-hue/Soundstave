// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "LookAndFeel.h"
#include "Toolbar.h"
#include "TransportBar.h"
#include "TimelineRuler.h"
#include "WaveformDisplay.h"
#include "SpectrogramDisplay.h"
#include "NoteOverlay.h"
#include "PianoKeyboard.h"
#include "PianoRoll.h"
#include "AnalysisProgressOverlay.h"
#include "SettingsDialog.h"

#include "../audio/AudioEngine.h"
#include "../analysis/SpectralAnalyzer.h"
#include "../analysis/PitchDetector.h"
#include "../model/NoteModel.h"
#include "../export/QuantizationEngine.h"
#include "../project/ProjectManager.h"

namespace Soundstave {

class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      public juce::ApplicationCommandTarget,
                      public juce::ScrollBar::Listener,
                      public NoteModel::Listener,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    // Asks to save unsaved work, then calls onProceed (not called on cancel)
    void requestClose(std::function<void()> onProceed);
    juce::PropertiesFile& getAppProperties() { return *props; }
    void openFileFromCommandLine(const juce::File& f, bool analyse);

    // ApplicationCommandTarget
    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands(juce::Array<juce::CommandID>& commands) override;
    void getCommandInfo(juce::CommandID id, juce::ApplicationCommandInfo& info) override;
    bool perform(const juce::ApplicationCommandTarget::InvocationInfo& info) override;

    // ScrollBar::Listener
    void scrollBarMoved(juce::ScrollBar* bar, double newRangeStart) override;

    // NoteModel::Listener
    void notesChanged() override;
    void selectionChanged() override;
    void tempoChanged() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    enum CommandIDs
    {
        cmdOpen = 0x2001, cmdSave, cmdSaveAs, cmdAnalyse, cmdPlay, cmdStop, cmdGoToStart,
        cmdUndo, cmdRedo, cmdSelectAll, cmdDeselect, cmdDelete, cmdCopy, cmdCut, cmdPaste,
        cmdMerge, cmdTransposeUp, cmdTransposeDown, cmdOctaveUp, cmdOctaveDown,
        cmdNudgeLeft, cmdNudgeRight, cmdZoomIn, cmdZoomOut, cmdZoomFit,
        cmdToolSelect, cmdToolDraw, cmdToolSplit, cmdTogglePianoRoll,
        cmdExportMidi, cmdExportMusicXml, cmdSettings, cmdToggleLoop, cmdLoopSelection
    };

private:
    // A horizontal drag handle between waveform and spectrogram
    class Divider : public juce::Component
    {
    public:
        std::function<void(int)> onDrag;
        Divider() { setMouseCursor(juce::MouseCursor::UpDownResizeCursor); }
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent&) override { lastY = 0; }
        void mouseDrag(const juce::MouseEvent& e) override
        {
            const int dy = e.getDistanceFromDragStartY();
            if (onDrag) onDrag(dy - lastY);
            lastY = dy;
        }
    private:
        int lastY = 0;
    };

    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override { updateToolbarState(); }
    AnalysisParams effectiveAnalysisParams() const;
    void clearAudioState(const juce::String& hint);

    // ---- Workflow ----
    void openWithChooser();
    void openFile(const juce::File& file);
    bool loadAudio(const juce::File& file, bool startNewProject);
    void loadProject(const juce::File& file);
    void startSpectrogram(bool thenDetect);
    void onSpectrogramReady(SpectrogramPtr data);
    void runAnalysis();
    void startDetection(bool chained);
    void onDetectionComplete(std::vector<Note> notes);
    void cancelAnalysis();
    void showProgress(const juce::String& message);
    void hideProgress();
    void estimateTempo(bool quiet);

    void save(std::function<void(bool)> onDone = {});
    void saveAs(std::function<void(bool)> onDone = {});
    void confirmUnsaved(std::function<void()> proceed);
    void exportMidi();
    void exportMusicXml();
    void showSettings();
    void alert(bool warning, const juce::String& title, const juce::String& message);

    // ---- View ----
    void updateViewState();
    void handleWheel(const juce::MouseEvent& e, const juce::MouseWheelDetails& w);
    void onPositionChanged(double pos);
    void autoRangeMidi();
    void zoomToFit();
    void setTool(EditTool tool);
    void setPianoRollVisible(bool visible);
    void setGrid(int id);
    double gridSeconds() const;
    double snapTime(double t) const;
    void applyGrid();
    void syncPlaybackNotes();
    void updateTitle();
    void updateToolbarState();
    void seek(double t);

    // ---- Loop ----
    void setLoop(double startSec, double endSec, bool enabled);
    void toggleLoop();
    void loopSelection();
    double snapLoopEdge(double t) const;

    juce::ValueTree makeViewSettings() const;
    void applyViewSettings(const juce::ValueTree& vt);
    void loadAppState();
    void saveAppState();

    // ---- Members ----
    SoundstaveLookAndFeel laf;
    std::unique_ptr<juce::TooltipWindow> tooltips;
    std::unique_ptr<juce::PropertiesFile> props;

    NoteModel          noteModel;
    AudioEngine        audioEngine;
    SpectralAnalyzer   spectralAnalyzer;
    PitchDetector      pitchDetector;
    ProjectManager     projectManager { noteModel };

    Toolbar                 toolbar;
    TransportBar            transportBar;
    TimelineRuler           ruler;
    WaveformDisplay         waveform;
    Divider                 divider;
    PianoKeyboard           keyboard;
    SpectrogramDisplay      spectrogram;
    PianoRoll               pianoRoll;
    NoteOverlay             noteOverlay { noteModel };
    juce::ScrollBar         hScrollBar { false };
    juce::ScrollBar         vScrollBar { true };
    AnalysisProgressOverlay progressOverlay;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::File lastDirectory;

    SpectrogramPtr  spectrogramData;
    AnalysisParams  spectrogramParams;      // params the current spectrogram was computed with
    bool            pendingDetection = false;
    float           detectionBase = 0.0f;   // progress offset when detection follows the STFT
    juce::String    progressMessage;

    AppSettings     settings;
    ViewState       viewState;
    double          totalDuration = 0.0;
    int             gridId = GridSixteenth;
    bool            pianoRollVisible = true;
    int             waveformHeight = 90;
    bool            tempoUserSet = false;
    QuantizeResolution lastXmlResolution = QuantizeResolution::Sixteenth;
    float           audioVol = 0.8f, midiVol = 0.6f;
    double          loopStart = -1.0, loopEnd = -1.0;
    bool            loopOn = false;

    juce::ApplicationCommandManager commandManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace Soundstave
