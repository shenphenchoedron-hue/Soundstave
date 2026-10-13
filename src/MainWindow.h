// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/MainComponent.h"

namespace Soundstave {

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow(const juce::String& name);
    ~MainWindow() override;

    void closeButtonPressed() override;
    MainComponent* getMainComponent() const;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

} // namespace Soundstave
