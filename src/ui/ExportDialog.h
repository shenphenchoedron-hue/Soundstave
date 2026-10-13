// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include "../export/QuantizationEngine.h"

namespace Soundstave {

// MusicXML export: choose quantization and preview the result before saving.
class MusicXmlExportPanel : public juce::Component
{
public:
    MusicXmlExportPanel(std::vector<Note> notes, TempoInfo tempo, QuantizeResolution initial);

    std::function<void(QuantizeResolution)> onExport;

    void resized() override;
    void paint(juce::Graphics& g) override;

    static void show(juce::Component* parent, std::vector<Note> notes, const TempoInfo& tempo,
                     QuantizeResolution initial, std::function<void(QuantizeResolution)> onExport);

private:
    class Preview : public juce::Component
    {
    public:
        void setData(const std::vector<Note>* notes, const TempoInfo* tempo, const QuantizedScore* score);
        void paint(juce::Graphics& g) override;
        static constexpr int kPxPerQuarter = 34;
    private:
        const std::vector<Note>* notes = nullptr;
        const TempoInfo*         tempo = nullptr;
        const QuantizedScore*    score = nullptr;
    };

    void updatePreview();
    QuantizeResolution selected() const;

    std::vector<Note> notes;
    TempoInfo         tempo;
    QuantizedScore    score;

    juce::Label       lblRes, lblInfo, lblLegend;
    juce::ComboBox    cmbRes;
    juce::Viewport    viewport;
    Preview           preview;
    juce::TextButton  btnExport, btnCancel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusicXmlExportPanel)
};

} // namespace Soundstave
