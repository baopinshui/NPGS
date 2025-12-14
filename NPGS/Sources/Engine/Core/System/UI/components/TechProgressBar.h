// --- START OF FILE TechProgressBar.h --- (完整实现)

#pragma once
#include "../ui_framework.h"
#include <cmath> // for sinf
#include <algorithm> // for std::clamp

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class TechProgressBar : public UIElement
{
public:
    float m_progress = 0.0f; // 0.0 - 1.0
    std::string m_label;

    StyleColor m_color_bg;
    StyleColor m_color_fill;
    StyleColor m_color_text;

    float m_bar_height = 8.0f;
    float m_label_spacing = 4.0f;

    TechProgressBar(const std::string& label = "")
        : m_label(label),
        m_color_bg(ImVec4(0.2f, 0.2f, 0.2f, 0.5f)),
        m_color_fill(ThemeColorID::Accent),
        m_color_text(ThemeColorID::Text)
    {
        m_width = Length::Stretch();
        m_height = Length::Content();
    }

    // [新增] 实现 Measure
    ImVec2 Measure(ImVec2 available_size) override
    {
        if (!m_visible)
        {
            m_desired_size = { 0, 0 };
            return m_desired_size;
        }

        // 如果高度是固定的，则使用固定值
        if (m_height.IsFixed())
        {
            m_desired_size.y = m_height.value;
        }
        else // 否则计算内容高度
        {
            float total_h = m_bar_height;
            if (!m_label.empty())
            {
                ImFont* font = UIContext::Get().m_font_small ? UIContext::Get().m_font_small : ImGui::GetFont();
                total_h += font->FontSize + m_label_spacing;
            }
            m_desired_size.y = total_h;
        }

        // 宽度通常是 Stretch，在 Measure 阶段返回 0，除非是 Fixed 或 Content
        if (m_width.IsFixed())
        {
            m_desired_size.x = m_width.value;
        }
        else if (m_width.IsContent() && !m_label.empty())
        {
            ImFont* font = UIContext::Get().m_font_small ? UIContext::Get().m_font_small : ImGui::GetFont();
            m_desired_size.x = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.f, m_label.c_str()).x;
        }
        else
        {
            m_desired_size.x = 0; // Stretch
        }

        return m_desired_size;
    }

    // [修改] Draw 函数不再计算尺寸，只负责渲染
    void Draw(ImDrawList* dl) override
    {
        if (!m_visible || m_alpha <= 0.01f) return;

        ImU32 col_bg = GetColorWithAlpha(m_color_bg.Resolve(), 1.0f);
        ImU32 col_fill = GetColorWithAlpha(m_color_fill.Resolve(), 1.0f);
        ImU32 col_text = GetColorWithAlpha(m_color_text.Resolve(), 1.0f);

        float text_h_with_spacing = 0.0f;
        if (!m_label.empty())
        {
            ImFont* font = UIContext::Get().m_font_small;
            if (!font) font = ImGui::GetFont();

            if (font) ImGui::PushFont(font);
            dl->AddText(m_absolute_pos, col_text, m_label.c_str());
            text_h_with_spacing = font->FontSize + m_label_spacing;
            if (font) ImGui::PopFont();
        }

        // 使用 Arrange 阶段计算好的 m_rect 和 m_absolute_pos
        ImVec2 bar_min = { m_absolute_pos.x, m_absolute_pos.y + text_h_with_spacing };
        // 进度条的高度是总高度减去文本占用的高度
        ImVec2 bar_max = { m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h };

        // 确保进度条有最小高度
        if (bar_max.y - bar_min.y < 2.0f) bar_max.y = bar_min.y + 2.0f;

        dl->AddRectFilled(bar_min, bar_max, col_bg);

        float fill_w = (bar_max.x - bar_min.x) * std::clamp(m_progress, 0.0f, 1.0f);
        float pulse = 0.8f + 0.2f * sinf((float)ImGui::GetTime() * 5.0f);

        ImVec4 fill_base_color = m_color_fill.Resolve();
        ImU32 col_fill_pulse = GetColorWithAlpha(fill_base_color, pulse * fill_base_color.w); // 修正：脉冲应与基础alpha相乘

        dl->AddRectFilled(bar_min, ImVec2(bar_min.x + fill_w, bar_max.y), col_fill_pulse);
    }
};

_UI_END
_SYSTEM_END
_NPGS_END