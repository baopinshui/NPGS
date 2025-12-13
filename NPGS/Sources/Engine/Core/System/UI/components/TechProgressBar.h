#pragma once
#include "../ui_framework.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class TechProgressBar : public UIElement
{
public:
    float m_progress = 0.0f; // 0.0 - 1.0
    std::string m_label;

    // [NEW] StyleColor properties for customization
    StyleColor m_color_bg;
    StyleColor m_color_fill;
    StyleColor m_color_text;

    // [MODIFIED] Constructor sets default theme references
    TechProgressBar(const std::string& label = "")
        : m_label(label),
        m_color_bg(ImVec4(0.2f, 0.2f, 0.2f, 0.5f)), // A fixed color can still be a default
        m_color_fill(ThemeColorID::Accent),
        m_color_text(ThemeColorID::Text)
    {}

    void Draw(ImDrawList* dl) override
    {
        if (!m_visible || m_alpha <= 0.01f) return;

        // [MODIFIED] Simplified color resolution
        ImU32 col_bg = GetColorWithAlpha(m_color_bg.Resolve(), 1.0f);
        ImU32 col_fill = GetColorWithAlpha(m_color_fill.Resolve(), 1.0f);
        ImU32 col_text = GetColorWithAlpha(m_color_text.Resolve(), 1.0f);

        // ... rest of Draw logic is unchanged ...
        float text_h = 0.0f;
        if (!m_label.empty())
        {
            ImFont* font = UIContext::Get().m_font_small;
            if (font) ImGui::PushFont(font);
            dl->AddText(m_absolute_pos, col_text, m_label.c_str());
            text_h = ImGui::GetFontSize() + 4.0f;
            if (font) ImGui::PopFont();
        }

        ImVec2 bar_min = { m_absolute_pos.x, m_absolute_pos.y + text_h };
        ImVec2 bar_max = { m_absolute_pos.x + m_rect.w, m_absolute_pos.y + text_h + m_rect.h - text_h };
        if (bar_max.y - bar_min.y < 4.0f) bar_max.y = bar_min.y + 4.0f;
        dl->AddRectFilled(bar_min, bar_max, col_bg);

        float fill_w = (bar_max.x - bar_min.x) * std::clamp(m_progress, 0.0f, 1.0f);
        float pulse = 0.8f + 0.2f * sinf(ImGui::GetTime() * 5.0f);

        // Pulse animation now uses the resolved fill color
        ImVec4 fill_base_color = m_color_fill.Resolve();
        ImU32 col_fill_pulse = GetColorWithAlpha(fill_base_color, pulse);

        dl->AddRectFilled(bar_min, ImVec2(bar_min.x + fill_w, bar_max.y), col_fill_pulse);
    }
};

_UI_END
_SYSTEM_END
_NPGS_END