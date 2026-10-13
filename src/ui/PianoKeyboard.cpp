// SPDX-License-Identifier: GPL-3.0-or-later
#include "PianoKeyboard.h"
#include "TimelineRuler.h"
#include "../model/NoteModel.h"

namespace Soundstave {

PianoKeyboard::PianoKeyboard()
{
    setOpaque(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void PianoKeyboard::setRange(int mn, int mx)
{
    if (mn == minMidi && mx == maxMidi) return;
    minMidi = mn; maxMidi = mx;
    repaint();
}

void PianoKeyboard::setActiveNotes(const std::bitset<128>& a)
{
    if (a == active) return;
    active = a;
    repaint();
}

int PianoKeyboard::midiAt(float y) const noexcept
{
    return juce::jlimit(minMidi, maxMidi, juce::roundToInt(yToPitch(y, minMidi, maxMidi, getHeight())));
}

void PianoKeyboard::paint(juce::Graphics& g)
{
    const int w = getWidth(), h = getHeight();
    const float lane = laneHeight(minMidi, maxMidi, h);
    const float blackW = (float)w * 0.58f;

    g.fillAll(juce::Colour(0xFFF7F8FB));

    auto keyColour = [&](int m, juce::Colour normal)
    {
        if (m == pressed)  return juce::Colour(Colours::Accent);
        if (active[(size_t)m]) return juce::Colour(Colours::NoteColour);
        if (m == hover)    return normal.interpolatedWith(juce::Colour(Colours::Accent), 0.25f);
        return normal;
    };

    // White keys: full width lanes
    for (int m = minMidi; m <= maxMidi; ++m)
    {
        if (isBlackKey(m)) continue;
        const float yTop = pitchToY((float)m + 0.5f, minMidi, maxMidi, h);
        g.setColour(keyColour(m, juce::Colours::white));
        g.fillRect(0.0f, yTop, (float)w, lane);
    }

    // Black keys: narrower, drawn on top
    for (int m = minMidi; m <= maxMidi; ++m)
    {
        if (!isBlackKey(m)) continue;
        const float yTop = pitchToY((float)m + 0.5f, minMidi, maxMidi, h);
        auto r = juce::Rectangle<float>(0.0f, yTop, blackW, lane);
        g.setColour(keyColour(m, juce::Colour(Colours::KeyBlack)));
        g.fillRoundedRectangle(r.reduced(0.0f, lane > 4.0f ? 0.5f : 0.0f), std::min(2.0f, lane * 0.3f));
    }

    // Separators between adjacent white keys (B|C and E|F) and octave labels
    g.setFont(SoundstaveLookAndFeel::getBoldFont(juce::jlimit(8.0f, 11.0f, lane * 1.6f)));
    for (int m = minMidi; m <= maxMidi; ++m)
    {
        const int pc = m % 12;
        const float yBottom = pitchToY((float)m - 0.5f, minMidi, maxMidi, h);
        if (pc == 0 || pc == 5)
        {
            g.setColour(juce::Colour(Colours::Border));
            g.drawHorizontalLine((int)yBottom, 0.0f, (float)w);
        }
        else if (!isBlackKey(m) && lane >= 6.0f)
        {
            g.setColour(juce::Colour(Colours::BorderLight));
            g.drawHorizontalLine((int)yBottom, blackW, (float)w);
        }
        if (pc == 0 && lane * 2.0f >= 9.0f)
        {
            const float yc = pitchToY((float)m, minMidi, maxMidi, h);
            g.setColour(juce::Colour(Colours::TextSecondary));
            g.drawText(Note::midiToName(m), 0, (int)(yc - 7.0f), w - 4, 14, juce::Justification::centredRight, false);
        }
    }

    g.setColour(juce::Colour(Colours::Border));
    g.drawVerticalLine(w - 1, 0.0f, (float)h);
}

void PianoKeyboard::mouseDown(const juce::MouseEvent& e)
{
    pressed = midiAt(e.position.y);
    if (onKeyPressed) onKeyPressed(pressed);
    repaint();
}

void PianoKeyboard::mouseDrag(const juce::MouseEvent& e)
{
    const int m = midiAt(e.position.y);
    if (m != pressed)
    {
        pressed = m;
        if (onKeyPressed) onKeyPressed(m);
        repaint();
    }
}

void PianoKeyboard::mouseUp(const juce::MouseEvent&)
{
    pressed = -1;
    repaint();
}

void PianoKeyboard::mouseMove(const juce::MouseEvent& e)
{
    const int m = midiAt(e.position.y);
    if (m != hover)
    {
        hover = m;
        setTooltip(Note::midiToName(m) + "  (" + juce::String(Note::midiToFreq(m), 1) + " Hz)");
        repaint();
    }
}

void PianoKeyboard::mouseExit(const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

void PianoKeyboard::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (onWheel) onWheel(e, w);
}

} // namespace Soundstave
