// SPDX-License-Identifier: GPL-3.0-or-later
#include <juce_gui_basics/juce_gui_basics.h>
#include "MainWindow.h"

namespace Soundstave {

class SoundstaveApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName()    override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed()          override { return true; }

    void initialise(const juce::String& commandLine) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
        openFromCommandLine(commandLine);
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr) mainWindow->closeButtonPressed();
        else quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        openFromCommandLine(commandLine);
    }

private:
    void openFromCommandLine(const juce::String& commandLine)
    {
        if (mainWindow == nullptr) return;
        // Usage: Soundstave [--analyse] <audio file | project.soundstave>
        const auto args = juce::StringArray::fromTokens(commandLine, true);
        const bool analyse = args.contains("--analyse") || args.contains("--analyze");
        for (auto arg : args)
        {
            const juce::File f(arg.unquoted());
            if (f.existsAsFile())
            {
                if (auto* mc = mainWindow->getMainComponent()) mc->openFileFromCommandLine(f, analyse);
                break;
            }
        }
    }

    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace Soundstave

START_JUCE_APPLICATION(Soundstave::SoundstaveApplication)
