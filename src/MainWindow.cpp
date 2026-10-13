// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

namespace Soundstave {

MainWindow::MainWindow(const juce::String& name)
    : DocumentWindow(name, juce::Colour(Colours::Background), DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(true);
    auto* content = new MainComponent();
    setContentOwned(content, true);
    setResizable(true, true);
    setResizeLimits(900, 560, 10000, 10000);

    const auto saved = content->getAppProperties().getValue("windowState");
    if (saved.isEmpty() || !restoreWindowStateFromString(saved))
        centreWithSize(1280, 820);

    setVisible(true);
    content->grabKeyboardFocus();
}

MainWindow::~MainWindow() = default;

MainComponent* MainWindow::getMainComponent() const
{
    return dynamic_cast<MainComponent*>(getContentComponent());
}

void MainWindow::closeButtonPressed()
{
    if (auto* mc = getMainComponent())
    {
        juce::Component::SafePointer<MainWindow> safe(this);
        mc->requestClose([safe]
        {
            if (safe != nullptr)
                if (auto* m = safe->getMainComponent())
                {
                    m->getAppProperties().setValue("windowState", safe->getWindowStateAsString());
                    m->getAppProperties().saveIfNeeded();
                }
            juce::JUCEApplication::getInstance()->quit();
        });
        return;
    }
    juce::JUCEApplication::getInstance()->quit();
}

} // namespace Soundstave
