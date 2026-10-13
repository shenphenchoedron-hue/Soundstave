// SPDX-License-Identifier: GPL-3.0-or-later
#include "TimelineRuler.h"
#include <cmath>

namespace Soundstave {

TimelineRuler::TimelineRuler()
{
    setOpaque(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTooltip(juce::String::fromUTF8("Klik for at flytte afspilningen. Træk for at vælge et stykke, der skal loopes."));
}

void TimelineRuler::setViewState(const ViewState& v, double total)
{
    vs = v; totalDuration = total;
    repaint();
}

static juce::String formatTime(double t, double interval)
{
    const int m = (int)(t / 60.0);
    const double s = t - m * 60.0;
    const int decimals = interval < 0.1 ? 2 : (interval < 1.0 ? 1 : 0);
    if (m > 0)
        return juce::String(m) + ":" + juce::String(s, decimals).paddedLeft('0', decimals > 0 ? 3 + decimals : 2);
    return juce::String(s, decimals) + "s";
}

void TimelineRuler::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Colours::SurfaceRaised));

    const int w = getWidth();
    const int h = getHeight();

    g.setColour(juce::Colour(Colours::Border));
    g.drawHorizontalLine(h - 1, 0.0f, (float)w);

    if (totalDuration <= 0.0 || w <= 0) return;

    if (loopStart >= 0.0 && loopEnd > loopStart)
    {
        const float x1 = (float)vs.timeToX(loopStart, w), x2 = (float)vs.timeToX(loopEnd, w);
        const auto c = juce::Colour(Colours::Accent);
        g.setColour(c.withAlpha(loopOn ? 0.30f : 0.12f));
        g.fillRect(x1, 0.0f, x2 - x1, (float)h - 1.0f);
        g.setColour(c.withAlpha(loopOn ? 1.0f : 0.45f));
        g.fillRect(x1, (float)h - 5.0f, x2 - x1, 4.0f);
        juce::Path handles;
        handles.addTriangle(x1, 0.0f, x1 + 7.0f, 0.0f, x1, 9.0f);
        handles.addTriangle(x2, 0.0f, x2 - 7.0f, 0.0f, x2, 9.0f);
        g.fillPath(handles);
    }

    const double dur = vs.visibleDuration();
    const double rawInterval = dur * 90.0 / (double)w;
    const double base = std::pow(10.0, std::floor(std::log10(rawInterval)));
    double interval = base * 10.0;
    for (double m : { 1.0, 2.0, 5.0, 10.0 })
        if (rawInterval <= base * m) { interval = base * m; break; }

    const double minor = interval / 5.0;

    g.setFont(SoundstaveLookAndFeel::getDefaultFont(11.0f));

    const long long first = (long long)std::floor(vs.startTimeSec / minor);
    const long long last  = (long long)std::ceil (vs.endTimeSec   / minor);
    for (long long i = first; i <= last; ++i)
    {
        const double t = (double)i * minor;
        if (t < 0.0) continue;
        const float x = (float)vs.timeToX(t, w);
        const bool major = (i % 5) == 0;
        if (major)
        {
            g.setColour(juce::Colour(Colours::TextSecondary).withAlpha(0.7f));
            g.drawVerticalLine((int)x, (float)h * 0.45f, (float)h);
            g.setColour(juce::Colour(Colours::TextPrimary));
            g.drawText(formatTime(t, interval), (int)x + 4, 1, 70, h - 4,
                       juce::Justification::centredLeft, false);
        }
        else
        {
            g.setColour(juce::Colour(Colours::Border));
            g.drawVerticalLine((int)x, (float)h * 0.72f, (float)h);
        }
    }
}

void TimelineRuler::setLoopRegion(double s, double e, bool enabled)
{
    loopStart = s; loopEnd = e; loopOn = enabled;
    repaint();
}

double TimelineRuler::timeAt(const juce::MouseEvent& e, bool snapIt) const
{
    double t = juce::jlimit(0.0, totalDuration, vs.xToTime(e.position.x, getWidth()));
    if (snapIt && snapFn && !e.mods.isAltDown()) t = juce::jlimit(0.0, totalDuration, snapFn(t));
    return t;
}

void TimelineRuler::mouseDown(const juce::MouseEvent& e)
{
    if (totalDuration <= 0.0) return;
    const bool has = loopEnd > loopStart && loopStart >= 0.0;
    dragger.mouseDown(timeAt(e, true), e.position.x,
                      (float)vs.timeToX(loopStart, getWidth()), (float)vs.timeToX(loopEnd, getWidth()),
                      has, loopStart, loopEnd);
}

void TimelineRuler::mouseDrag(const juce::MouseEvent& e)
{
    if (totalDuration <= 0.0) return;
    if (dragger.mouseDrag(timeAt(e, true), e.getDistanceFromDragStart()) && onLoopDragged)
        onLoopDragged(dragger.start, dragger.end, false);
}

void TimelineRuler::mouseUp(const juce::MouseEvent& e)
{
    if (totalDuration <= 0.0) return;
    const auto mode = dragger.mouseUp();
    if (mode == LoopDragger::Mode::Click)
    {
        if (onSeek) onSeek(timeAt(e, false));
    }
    else if (mode != LoopDragger::Mode::None && onLoopDragged)
        onLoopDragged(dragger.start, dragger.end, true);
}

void TimelineRuler::mouseMove(const juce::MouseEvent& e)
{
    const bool has = loopEnd > loopStart && loopStart >= 0.0;
    const bool edge = LoopDragger::isOverEdge(e.position.x, (float)vs.timeToX(loopStart, getWidth()),
                                              (float)vs.timeToX(loopEnd, getWidth()), has);
    setMouseCursor(edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::PointingHandCursor);
}

} // namespace Soundstave
