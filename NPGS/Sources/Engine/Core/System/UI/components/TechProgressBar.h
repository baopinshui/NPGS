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

    TechProgressBar(const std::string& label = "") : m_label(label) {}

    void Draw(ImDrawList* dl) override
    {
        if (!m_visible || m_alpha <= 0.01f) return;

        const auto& theme = UIContext::Get().m_theme;
        ImU32 col_bg = GetColorWithAlpha(ImVec4(0.2f, 0.2f, 0.2f, 0.5f), 1.0f);
        ImU32 col_fill = GetColorWithAlpha(theme.color_accent, 1.0f);
        ImU32 col_text = GetColorWithAlpha(theme.color_text, 1.0f);

        // 1. 绘制标签
        float text_h = 0.0f;
        if (!m_label.empty())
        {
            ImFont* font = UIContext::Get().m_font_small; // 假设有小字体，或者用默认
            if (font) ImGui::PushFont(font);
            dl->AddText(m_absolute_pos, col_text, m_label.c_str());
            text_h = ImGui::GetFontSize() + 4.0f;
            if (font) ImGui::PopFont();
        }

        // 2. 绘制进度条背景槽
        ImVec2 bar_min = { m_absolute_pos.x, m_absolute_pos.y + text_h };
        ImVec2 bar_max = { m_absolute_pos.x + m_rect.w, m_absolute_pos.y + text_h + m_rect.h - text_h }; // 减去文字高度

        // 如果高度不够，强制给一点
        if (bar_max.y - bar_min.y < 4.0f) bar_max.y = bar_min.y + 4.0f;

        dl->AddRectFilled(bar_min, bar_max, col_bg);

        // 3. 绘制填充 (带简单的脉冲动画效果模拟)
        float fill_w = (bar_max.x - bar_min.x) * std::clamp(m_progress, 0.0f, 1.0f);

        // 模拟 HTML 中的 animate-pulse，通过 alpha 变化
        float pulse = 0.8f + 0.2f * sinf(ImGui::GetTime() * 5.0f);
        ImU32 col_fill_pulse = GetColorWithAlpha(theme.color_accent, pulse);

        dl->AddRectFilled(bar_min, ImVec2(bar_min.x + fill_w, bar_max.y), col_fill_pulse);
    }
};

_UI_END
_SYSTEM_END
_NPGS_END