#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace Soundstave {

// -------------------------------------------------------------------------
// Soundstave farvepalette – lyst, farverigt, professionelt
// -------------------------------------------------------------------------
namespace Colours {
    // Baggrunde
    constexpr uint32_t Background       = 0xFFF0F3F8;  // kølig hvid
    constexpr uint32_t Surface          = 0xFFFFFFFF;  // ren hvid
    constexpr uint32_t SurfaceRaised    = 0xFFE8EDF5;  // lys blågrå toolbar
    constexpr uint32_t SurfaceCard      = 0xFFF5F7FB;  // lys kortbaggrund
    constexpr uint32_t Border           = 0xFFBDC5D8;  // blågrå kant
    constexpr uint32_t BorderLight      = 0xFFDAE0EB;  // endnu lysere kant

    // Accenter
    constexpr uint32_t Accent           = 0xFF2563EB;  // dyb blå
    constexpr uint32_t AccentDim        = 0xFFBFD4FB;  // lys blå tint
    constexpr uint32_t AccentHover      = 0xFF1D4ED8;  // mørkere blå

    // Knap-farver (farverige, men ikke mørke)
    constexpr uint32_t BtnFile          = 0xFF2563EB;  // blå
    constexpr uint32_t BtnFileHover     = 0xFF3B78F7;
    constexpr uint32_t BtnAnalyse       = 0xFF7C3AED;  // lilla
    constexpr uint32_t BtnAnalyseHover  = 0xFF8B5CF6;
    constexpr uint32_t BtnTool          = 0xFF64748B;  // blågrå
    constexpr uint32_t BtnToolActive    = 0xFF059669;  // grøn
    constexpr uint32_t BtnToolHover     = 0xFF475569;
    constexpr uint32_t BtnDelete        = 0xFFDC2626;  // rød
    constexpr uint32_t BtnDeleteHover   = 0xFFE53E3E;
    constexpr uint32_t BtnExport        = 0xFFD97706;  // ravgul
    constexpr uint32_t BtnExportHover   = 0xFFF59E0B;
    constexpr uint32_t BtnView          = 0xFF0891B2;  // cyan

    // Tekst – mørk på lys baggrund
    constexpr uint32_t TextPrimary      = 0xFF1E293B;
    constexpr uint32_t TextDark         = 0xFF1E293B;
    constexpr uint32_t TextSecondary    = 0xFF64748B;
    constexpr uint32_t TextDisabled     = 0xFFB0BAC8;

    // Note-farver
    constexpr uint32_t NoteColour       = 0xFFFFCC00;
    constexpr uint32_t NoteSelected     = 0xFFFF6600;
    constexpr uint32_t NoteBorder       = 0xFFCC9900;

    // Visning
    constexpr uint32_t Waveform         = 0xFF2563EB;
    constexpr uint32_t Playhead         = 0xFFDC2626;
    constexpr uint32_t GridLine         = 0xFFE2E8F0;
    constexpr uint32_t KeyWhite         = 0xFFFFFFFF;
    constexpr uint32_t KeyBlack         = 0xFF334155;
    constexpr uint32_t KeyPressed       = 0xFF2563EB;

    constexpr uint32_t Danger           = 0xFFDC2626;
    constexpr uint32_t Warning          = 0xFFD97706;
    constexpr uint32_t Success          = 0xFF059669;
}

// UTF-8 string literal helper. juce::String(const char*) assumes ASCII, so all
// Danish text (æ, ø, å, é) must go through this.
inline juce::String U(const char* utf8) { return juce::String::fromUTF8(utf8); }

// -------------------------------------------------------------------------
// Knap-kategorier – bruges af Toolbar til at farvelægge hvert knap
// -------------------------------------------------------------------------
enum class ButtonRole { Default, File, Analyse, Tool, Delete, Export, View };

class SoundstaveLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SoundstaveLookAndFeel();

    // Scrollbar
    int getDefaultScrollbarWidth() override { return 12; }
    void drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar,
                       int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition,
                       int thumbSize, bool isMouseOver, bool isMouseDown) override;

    // Knapper
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    int getTextButtonWidthToFitText(juce::TextButton& btn, int btnH) override;

    // ComboBox
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;

    // Slider
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override;

    // Label
    juce::Font getLabelFont(juce::Label& label) override;

    // Popup menu
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColour) override;

    // Fonts
    static juce::Font getDefaultFont(float height = 14.0f);
    static juce::Font getBoldFont(float height = 14.0f);
    static juce::Font getMonoFont(float height = 12.0f);

    // Hjælper: sæt rolle-farve på en knap via component property
    static void setButtonRole(juce::TextButton& btn, ButtonRole role);
    static ButtonRole getButtonRole(const juce::Button& btn);

private:
    // Returnerer base/hover/text farver for en given rolle og tilstand
    static void roleColours(ButtonRole role, bool toggled, bool hover, bool down,
                             juce::Colour& outBg, juce::Colour& outText);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundstaveLookAndFeel)
};

} // namespace Soundstave
