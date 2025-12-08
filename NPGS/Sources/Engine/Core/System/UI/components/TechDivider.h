#pragma once
#include "../ui_framework.h"
#include "../TechUtils.h"
_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class TechDivider : public UIElement
{
public:
    std::optional<ImVec4> m_color_override;
    float m_visual_height=1.0f;
    bool m_use_gradient = false; // [新增] 是否启用向两端透明的渐变
    TechDivider()
    {
        m_block_input = false;
        m_rect.h = 1.0f; // 默认高度
        m_rect.w = 10.0f;
    }

    void Draw(ImDrawList* dl) override
    {
        if (!m_visible || m_alpha <= 0.01f) return;
        float h = std::max(m_visual_height, 1.0f);
        const auto& theme = UIContext::Get().m_theme; // [新增] 实时获取主题
        ImVec4 final_color = m_color_override.value_or(theme.color_border); // [新增] 决定最终颜色
        if (m_use_gradient)
        {
            // --- 渐变模式 (中心实色，两端透明) ---
            ImU32 col_center = GetColorWithAlpha(final_color, 1.0);
            ImU32 col_edge = GetColorWithAlpha(final_color, 0.0f);

            float center_x = m_absolute_pos.x + m_rect.w * 0.5f;
            float y = m_absolute_pos.y;
            // 确保高度至少有一点，否则画不出矩形
          

            // 左半段 (左透明 -> 右实色)
            dl->AddRectFilledMultiColor(
                TechUtils::Snap(ImVec2(m_absolute_pos.x, y)),
                TechUtils::Snap(ImVec2(center_x, y + h)),
                col_edge, col_center, col_center, col_edge
            );

            // 右半段 (左实色 -> 右透明)
            dl->AddRectFilledMultiColor(
                TechUtils::Snap(ImVec2(center_x, y)),
                TechUtils::Snap(ImVec2(m_absolute_pos.x + m_rect.w, y + h)),
                col_center, col_edge, col_edge, col_center
            );
        }
        else
        {
            // --- 普通线条模式 ---
            ImU32 col = GetColorWithAlpha(final_color, 1.0);
            TechUtils::DrawHardLine(dl,
                ImVec2(m_absolute_pos.x, m_absolute_pos.y + m_rect.h * 0.5f),
                ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h * 0.5f),
                col, h);

        }
    }
};

_UI_END
_SYSTEM_END
_NPGS_END