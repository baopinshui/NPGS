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
    {
        SetWidth(Length::Fill());       // 我想填满水平空间
        SetHeight(Length::Content());   // 我的高度由我的内容（字体）决定
    }
    ImVec2 Measure(const ImVec2& available_size) override
    {
        float final_w = 0.0f;
        if (m_width_policy.type == LengthType::Fill)
        {
            final_w = available_size.x;
        }
        else if (m_width_policy.type == LengthType::Fixed)
        {
            final_w = m_width_policy.value;
        }
        // Content 宽度对于进度条意义不大，可以视为0

        float final_h = 0.0f;
        if (m_height_policy.type == LengthType::Content)
        {
            // 高度由字体大小和 Bar 的最小高度决定
            float bar_h = 6.0f; // 进度条本身的最小高度
            float text_h = 0.0f;
            if (!m_label.empty())
            {
                ImFont* font = UIContext::Get().m_font_small;
                if (!font) font = GetFont();
                if (font) text_h = font->FontSize + 4.0f;
            }
            final_h = text_h + bar_h;
        }
        else // Fixed or Fill
        {
            final_h = UIElement::Measure(available_size).y;
        }

        return { final_w, final_h };
    }
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