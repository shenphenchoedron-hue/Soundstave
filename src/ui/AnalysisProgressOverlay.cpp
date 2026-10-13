// SPDX-License-Identifier: GPL-3.0-or-later
#include "AnalysisProgressOverlay.h"

namespace Soundstave {

AnalysisProgressOverlay::AnalysisProgressOverlay()
{
    btnCancel.setButtonText(U("Annullér"));
    btnCancel.setWantsKeyboardFocus(false);
    btnCancel.onClick = [this]{ if (onCancel) onCancel(); };
    addAndMakeVisible(btnCancel);
}

void AnalysisProgressOverlay::setProgress(float p, const juce::String& msg)
{
    progress = juce::jlimit(0.0f, 1.0f, p);
    if (msg.isNotEmpty()) message = msg;
    repaint();
}

juce::Rectangle<int> AnalysisProgressOverlay::cardBounds() const
{
    return getLocalBounds().withSizeKeepingCentre(360, 112);
}

void AnalysisProgressOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0x880A0C20));

    const auto card = cardBounds().toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.fillRoundedRectangle(card.translated(0.0f, 3.0f), 12.0f);
    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(card, 12.0f);

    g.setColour(juce::Colour(Colours::TextPrimary));
    g.setFont(SoundstaveLookAndFeel::getBoldFont(15.0f));
    g.drawText(message, card.reduced(18.0f, 0.0f).withHeight(44.0f).toNearestInt(), juce::Justification::centredLeft);

    auto bar = juce::Rectangle<float>(card.getX() + 18.0f, card.getY() + 46.0f, card.getWidth() - 84.0f, 10.0f);
    g.setColour(juce::Colour(Colours::GridLine));
    g.fillRoundedRectangle(bar, 5.0f);
    g.setColour(juce::Colour(Colours::Accent));
    g.fillRoundedRectangle(bar.withWidth(std::max(10.0f, bar.getWidth() * progress)), 5.0f);

    g.setColour(juce::Colour(Colours::TextSecondary));
    g.setFont(SoundstaveLookAndFeel::getDefaultFont(12.0f));
    g.drawText(juce::String(juce::roundToInt(progress * 100.0f)) + " %",
               (int)bar.getRight() + 8, (int)bar.getY() - 4, 50, 18, juce::Justification::centredLeft);
}

void AnalysisProgressOverlay::resized()
{
    const auto card = cardBounds();
    btnCancel.setBounds(card.getX() + 18, card.getBottom() - 42, 100, 30);
}

} // namespace Soundstave
