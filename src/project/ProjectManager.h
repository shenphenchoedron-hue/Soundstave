// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include "../model/NoteModel.h"

namespace Soundstave {

// Serialises the project (.soundstave, XML):
//   - reference to the audio file (absolute + relative to the project)
//   - notes (analysis results and manual edits), tempo, meter, grid offset
//   - view settings supplied by the UI as an opaque ValueTree
class ProjectManager
{
public:
    explicit ProjectManager(NoteModel& model);

    void newProject();
    bool openProject(const juce::File& file, juce::String& errorOut);
    bool saveProject();
    bool saveProjectAs(const juce::File& file);

    bool       hasProjectFile() const noexcept { return projectFile != juce::File(); }
    juce::File getProjectFile() const noexcept { return projectFile; }
    juce::File getAudioFile()   const noexcept { return audioFile; }
    void       setAudioFile(const juce::File& f) { audioFile = f; }

    void setViewSettings(const juce::ValueTree& vt) { viewSettings = vt.createCopy(); }
    juce::ValueTree getViewSettings() const { return viewSettings; }

    bool hasUnsavedChanges() const noexcept { return model.hasUnsavedChanges(); }
    juce::String getWindowTitle() const;

    static constexpr const char* kExtension = ".soundstave";

private:
    NoteModel&      model;
    juce::File      projectFile;
    juce::File      audioFile;
    juce::ValueTree viewSettings { "View" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectManager)
};

} // namespace Soundstave
