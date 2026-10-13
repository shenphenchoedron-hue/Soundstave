#include "LookAndFeel.h"

namespace Soundstave {

// -------------------------------------------------------------------------
// Hjælpere
// -------------------------------------------------------------------------
static const juce::Identifier kRoleKey("SS_ButtonRole");

void SoundstaveLookAndFeel::setButtonRole(juce::TextButton& btn, ButtonRole role)
{
    btn.getProperties().set(kRoleKey, (int)role);
}

ButtonRole SoundstaveLookAndFeel::getButtonRole(const juce::Button& btn)
{
    return (ButtonRole)(int)btn.getProperties().getWithDefault(kRoleKey, (int)ButtonRole::Default);
}

void SoundstaveLookAndFeel::roleColours(ButtonRole role, bool toggled, bool hover, bool down,
                                         juce::Colour& outBg, juce::Colour& outText)
{
    // Hvid tekst på mættede knapper, mørk tekst på lyse
    outText = juce::Colours::white;

    switch (role)
    {
        case ButtonRole::File:
            outBg = down ? juce::Colour(Colours::BtnFile).darker(0.2f)
                         : hover ? juce::Colour(Colours::BtnFileHover) : juce::Colour(Colours::BtnFile);
            break;
        case ButtonRole::Analyse:
            outBg = down ? juce::Colour(Colours::BtnAnalyse).darker(0.2f)
                         : hover ? juce::Colour(Colours::BtnAnalyseHover) : juce::Colour(Colours::BtnAnalyse);
            break;
        case ButtonRole::Tool:
            if (toggled)
            {
                outBg   = juce::Colour(Colours::BtnToolActive);
                outText = juce::Colours::white;
            }
            else
            {
                outBg   = down  ? juce::Colour(0xFFBDC5D8)
                        : hover ? juce::Colour(0xFFCDD5E4) : juce::Colour(0xFFDAE0EB);
                outText = juce::Colour(Colours::TextPrimary);
            }
            break;
        case ButtonRole::Delete:
            outBg = down ? juce::Colour(Colours::BtnDelete).darker(0.2f)
                         : hover ? juce::Colour(Colours::BtnDeleteHover) : juce::Colour(Colours::BtnDelete);
            break;
        case ButtonRole::Export:
            outBg = down ? juce::Colour(Colours::BtnExport).darker(0.2f)
                         : hover ? juce::Colour(Colours::BtnExportHover) : juce::Colour(Colours::BtnExport);
            break;
        case ButtonRole::View:
            outBg = toggled ? juce::Colour(Colours::Accent)
                            : down ? juce::Colour(Colours::BtnView).darker(0.2f)
                            : hover ? juce::Colour(Colours::BtnView).brighter(0.1f) : juce::Colour(Colours::BtnView);
            break;
        case ButtonRole::Default:
        default:
            outBg   = down ? juce::Colour(Colours::Border)
                           : hover ? juce::Colour(0xFFD0D8E8) : juce::Colour(0xFFDDE4EF);
            outText = juce::Colour(Colours::TextPrimary);  // mørk tekst på lys grå
            break;
    }
}

// -------------------------------------------------------------------------
// Konstruktør
// -------------------------------------------------------------------------
SoundstaveLookAndFeel::SoundstaveLookAndFeel()
{
    // Window
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(Colours::Background));
    setColour(juce::DocumentWindow::backgroundColourId,  juce::Colour(Colours::SurfaceRaised));

    // Knapper – defaults (overrides i drawButton*)
    setColour(juce::TextButton::buttonColourId,   juce::Colour(Colours::SurfaceCard));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(Colours::BtnToolActive));
    setColour(juce::TextButton::textColourOffId,  juce::Colour(Colours::TextPrimary));
    setColour(juce::TextButton::textColourOnId,   juce::Colour(Colours::TextPrimary));

    // Labels
    setColour(juce::Label::textColourId,       juce::Colour(Colours::TextPrimary));
    setColour(juce::Label::backgroundColourId, juce::Colour(0x00000000));

    // ComboBox
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(Colours::SurfaceCard));
    setColour(juce::ComboBox::outlineColourId,     juce::Colour(Colours::Border));
    setColour(juce::ComboBox::textColourId,        juce::Colour(Colours::TextPrimary));
    setColour(juce::ComboBox::arrowColourId,       juce::Colour(Colours::TextSecondary));

    // Popup menu
    setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(Colours::SurfaceRaised));
    setColour(juce::PopupMenu::textColourId,                  juce::Colour(Colours::TextPrimary));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(Colours::AccentDim));
    setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(Colours::TextPrimary));

    // Scrollbar
    setColour(juce::ScrollBar::backgroundColourId, juce::Colour(Colours::SurfaceRaised));
    setColour(juce::ScrollBar::thumbColourId,      juce::Colour(Colours::Border));

    // Slider
    setColour(juce::Slider::backgroundColourId,        juce::Colour(Colours::SurfaceCard));
    setColour(juce::Slider::trackColourId,             juce::Colour(Colours::AccentDim));
    setColour(juce::Slider::thumbColourId,             juce::Colour(Colours::Accent));
    setColour(juce::Slider::textBoxTextColourId,       juce::Colour(Colours::TextPrimary));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(Colours::SurfaceCard));
    setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(Colours::Border));

    // AlertWindow
    setColour(juce::AlertWindow::backgroundColourId, juce::Colour(Colours::SurfaceRaised));
    setColour(juce::AlertWindow::textColourId,       juce::Colour(Colours::TextPrimary));
    setColour(juce::AlertWindow::outlineColourId,    juce::Colour(Colours::Border));

    // TextEditor
    setColour(juce::TextEditor::backgroundColourId,      juce::Colour(Colours::SurfaceCard));
    setColour(juce::TextEditor::textColourId,            juce::Colour(Colours::TextPrimary));
    setColour(juce::TextEditor::outlineColourId,         juce::Colour(Colours::Border));
    setColour(juce::TextEditor::focusedOutlineColourId,  juce::Colour(Colours::Accent));
    setColour(juce::TextEditor::highlightColourId,       juce::Colour(Colours::AccentDim));
}

// -------------------------------------------------------------------------
// Fonts
// -------------------------------------------------------------------------
juce::Font SoundstaveLookAndFeel::getDefaultFont(float height)
{
    return juce::Font(juce::FontOptions(height));
}

juce::Font SoundstaveLookAndFeel::getBoldFont(float height)
{
    return juce::Font(juce::FontOptions().withHeight(height).withStyle("Bold"));
}

juce::Font SoundstaveLookAndFeel::getMonoFont(float height)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
}

// -------------------------------------------------------------------------
// Scrollbar
// -------------------------------------------------------------------------
void SoundstaveLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar& /*scrollbar*/,
                                           int x, int y, int width, int height,
                                           bool isScrollbarVertical, int thumbStartPosition,
                                           int thumbSize, bool isMouseOver, bool /*isMouseDown*/)
{
    // Track
    g.setColour(juce::Colour(Colours::SurfaceRaised));
    g.fillRect(x, y, width, height);

    // Thumb
    juce::Rectangle<float> thumb;
    if (isScrollbarVertical)
        thumb = { (float)x + 2.0f, (float)(y + thumbStartPosition + 1), (float)(width - 4), (float)(thumbSize - 2) };
    else
        thumb = { (float)(x + thumbStartPosition + 1), (float)y + 2.0f, (float)(thumbSize - 2), (float)(height - 4) };

    auto colour = isMouseOver ? juce::Colour(Colours::Accent) : juce::Colour(Colours::Border);
    g.setColour(colour.withAlpha(0.8f));
    g.fillRoundedRectangle(thumb, 4.0f);
}

// -------------------------------------------------------------------------
// Knapper
// -------------------------------------------------------------------------
void SoundstaveLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                  const juce::Colour& /*bgColour*/,
                                                  bool highlighted, bool down)
{
    auto role    = getButtonRole(button);
    bool toggled = button.getToggleState();

    juce::Colour bg, txt;
    roleColours(role, toggled, highlighted, down, bg, txt);

    if (!button.isEnabled())
        bg = bg.withMultipliedSaturation(0.3f).withAlpha(0.45f);

    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const float radius = 6.0f;

    // Soft drop shadow
    g.setColour(juce::Colours::black.withAlpha(down ? 0.0f : 0.08f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 1.0f), radius);

    // Gradient: slightly lighter at the top
    juce::ColourGradient grad(bg.brighter(0.08f), 0.0f, bounds.getY(),
                              bg.darker(0.06f),   0.0f, bounds.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, radius);

    g.setColour(bg.darker(0.3f).withAlpha(0.45f));
    g.drawRoundedRectangle(bounds, radius, 1.0f);

    // Focus ring for active toggles
    if (toggled && button.isEnabled())
    {
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.drawRoundedRectangle(bounds.reduced(1.5f), radius - 1.5f, 1.0f);
    }
}

void SoundstaveLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                            bool /*highlighted*/, bool isDown)
{
    auto role    = getButtonRole(button);
    bool toggled = button.getToggleState();
    juce::Colour bg, txt;
    roleColours(role, toggled, false, false, bg, txt);

    if (!button.isEnabled())
        txt = juce::Colour(Colours::TextDisabled);
    else if (isDown)
        txt = txt.withAlpha(0.75f);

    g.setFont(getBoldFont(13.5f));
    g.setColour(txt);
    g.drawText(button.getButtonText(), button.getLocalBounds().reduced(4, 0),
               juce::Justification::centred, true);
}

int SoundstaveLookAndFeel::getTextButtonWidthToFitText(juce::TextButton& btn, int /*btnH*/)
{
    return juce::GlyphArrangement::getStringWidthInt(getBoldFont(14.0f), btn.getButtonText()) + 22;
}

// -------------------------------------------------------------------------
// ComboBox
// -------------------------------------------------------------------------
void SoundstaveLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                          bool /*isButtonDown*/,
                                          int /*buttonX*/, int /*buttonY*/,
                                          int /*buttonW*/, int /*buttonH*/,
                                          juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height);
    g.setColour(juce::Colour(Colours::SurfaceCard));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(box.hasKeyboardFocus(false) ? juce::Colour(Colours::Accent) : juce::Colour(Colours::Border));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    auto arrowZone = juce::Rectangle<float>((float)(width - 22), 0, 22, (float)height);
    juce::Path path;
    path.startNewSubPath(arrowZone.getX() + 4.0f, arrowZone.getCentreY() - 2.5f);
    path.lineTo(arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);
    path.lineTo(arrowZone.getRight() - 4.0f, arrowZone.getCentreY() - 2.5f);
    g.setColour(juce::Colour(Colours::TextSecondary));
    g.strokePath(path, juce::PathStrokeType(1.8f));
}

juce::Font SoundstaveLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return getDefaultFont(14.0f);
}

// -------------------------------------------------------------------------
// Slider
// -------------------------------------------------------------------------
void SoundstaveLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                              juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar)
    {
        float midY = (float)(y + height / 2);
        float trackH = 5.0f;

        // Track baggrund
        g.setColour(juce::Colour(Colours::SurfaceCard).brighter(0.1f));
        g.fillRoundedRectangle((float)x, midY - trackH * 0.5f, (float)width, trackH, 2.5f);

        // Fyldt del
        juce::ColourGradient grad(juce::Colour(Colours::Accent), (float)x, 0,
                                   juce::Colour(Colours::AccentHover), sliderPos, 0, false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle((float)x, midY - trackH * 0.5f, sliderPos - (float)x, trackH, 2.5f);

        // Thumb
        g.setColour(juce::Colours::white);
        g.fillEllipse(sliderPos - 8.0f, midY - 8.0f, 16.0f, 16.0f);
        g.setColour(juce::Colour(Colours::Accent));
        g.drawEllipse(sliderPos - 8.0f, midY - 8.0f, 16.0f, 16.0f, 2.0f);
    }
    else
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0, 0, style, slider);
    }
}

// -------------------------------------------------------------------------
// Label
// -------------------------------------------------------------------------
juce::Font SoundstaveLookAndFeel::getLabelFont(juce::Label&)
{
    return getDefaultFont(14.0f);
}

// -------------------------------------------------------------------------
// Popup menu
// -------------------------------------------------------------------------
void SoundstaveLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.setColour(juce::Colour(Colours::SurfaceRaised));
    g.fillRoundedRectangle(0, 0, (float)width, (float)height, 8.0f);
    g.setColour(juce::Colour(Colours::Border));
    g.drawRoundedRectangle(0.5f, 0.5f, (float)width - 1.0f, (float)height - 1.0f, 8.0f, 1.0f);
}

void SoundstaveLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                               bool isSeparator, bool /*isActive*/, bool isHighlighted,
                                               bool isTicked, bool /*hasSubMenu*/,
                                               const juce::String& text, const juce::String& shortcutKeyText,
                                               const juce::Drawable* /*icon*/,
                                               const juce::Colour* /*textColour*/)
{
    if (isSeparator)
    {
        g.setColour(juce::Colour(Colours::Border));
        g.fillRect(area.getX() + 10, area.getCentreY(), area.getWidth() - 20, 1);
        return;
    }

    if (isHighlighted)
    {
        g.setColour(juce::Colour(Colours::Accent).withAlpha(0.25f));
        g.fillRoundedRectangle(area.reduced(4, 2).toFloat(), 5.0f);
    }

    g.setColour(juce::Colour(isTicked ? Colours::Accent : Colours::TextPrimary));
    g.setFont(getDefaultFont(14.0f));
    g.drawText(text, area.getX() + 26, area.getY(), area.getWidth() - 40,
               area.getHeight(), juce::Justification::centredLeft);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour(juce::Colour(Colours::TextSecondary));
        g.setFont(getDefaultFont(12.0f));
        g.drawText(shortcutKeyText, area.getX(), area.getY(), area.getWidth() - 10,
                   area.getHeight(), juce::Justification::centredRight);
    }
}

} // namespace Soundstave
