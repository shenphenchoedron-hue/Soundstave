// SPDX-License-Identifier: GPL-3.0-or-later
#include "ProjectManager.h"

namespace Soundstave {

ProjectManager::ProjectManager(NoteModel& m) : model(m) {}

void ProjectManager::newProject()
{
    model.clearNotes();
    model.setTempo(TempoInfo{});
    model.markClean();
    projectFile  = juce::File();
    audioFile    = juce::File();
    viewSettings = juce::ValueTree("View");
}

bool ProjectManager::openProject(const juce::File& file, juce::String& errorOut)
{
    if (!file.existsAsFile()) { errorOut = juce::String::fromUTF8("Projektfilen findes ikke."); return false; }

    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr) { errorOut = juce::String::fromUTF8("Projektfilen kunne ikke læses."); return false; }

    auto vt = juce::ValueTree::fromXml(*xml);
    if (!vt.hasType("SoundstaveProject"))
    {
        errorOut = juce::String::fromUTF8("Filen er ikke et Soundstave-projekt.");
        return false;
    }

    // Prefer the absolute path; fall back to the path relative to the project
    juce::File audio(vt.getProperty("audioFile", "").toString());
    if (!audio.existsAsFile())
    {
        const auto rel = vt.getProperty("audioFileRelative", "").toString();
        if (rel.isNotEmpty())
            audio = file.getParentDirectory().getChildFile(rel);
    }
    audioFile = audio;

    auto noteData = vt.getChildWithName("NoteModel");
    if (noteData.isValid())
        model.fromValueTree(noteData);
    else
        model.clearNotes();

    auto view = vt.getChildWithName("View");
    viewSettings = view.isValid() ? view.createCopy() : juce::ValueTree("View");

    projectFile = file;
    model.markClean();
    return true;
}

bool ProjectManager::saveProject()
{
    return hasProjectFile() && saveProjectAs(projectFile);
}

bool ProjectManager::saveProjectAs(const juce::File& fileIn)
{
    const juce::File file = fileIn.hasFileExtension(kExtension) ? fileIn : fileIn.withFileExtension(kExtension);

    juce::ValueTree root("SoundstaveProject");
    root.setProperty("version", 2, nullptr);
    if (audioFile != juce::File())
    {
        root.setProperty("audioFile", audioFile.getFullPathName(), nullptr);
        root.setProperty("audioFileRelative", audioFile.getRelativePathFrom(file.getParentDirectory()), nullptr);
    }
    root.addChild(model.toValueTree(), -1, nullptr);
    root.addChild(viewSettings.createCopy(), -1, nullptr);

    auto xml = root.createXml();
    if (xml == nullptr || !xml->writeTo(file))
        return false;

    projectFile = file;
    model.markClean();
    return true;
}

juce::String ProjectManager::getWindowTitle() const
{
    juce::String title = "Soundstave";
    if (hasProjectFile())
        title << " - " << projectFile.getFileNameWithoutExtension();
    else if (audioFile != juce::File())
        title << " - " << audioFile.getFileName();
    if (hasUnsavedChanges())
        title << " *";
    return title;
}

} // namespace Soundstave
