#pragma once
#include "../ui_framework.h"
#include "../TechUtils.h"
_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class TechDivider : public UIElement
{
public:
    StyleColor m_color;
    float m_visual_height=1.0f;
    bool m_use_gradient = false; // [新增] 是否启用向两端透明的渐变
    TechDivider(const StyleColor& color = ThemeColorID::Border) : m_color(color)
    {
        m_block_input = false;
        m_width = Length::Stretch();
        m_height = Length::Fixed(1.0f);
    }

    void Draw(ImDrawList* dl) override
    {
        if (!m_visible || m_alpha <= 0.01f) return;

        // [修改] 计算中心 Y 坐标，以适应父容器分配的任意高度
        float center_y = m_absolute_pos.y + m_rect.h * 0.5f;
        float half_visual_h = std::max(m_visual_height, 1.0f) * 0.5f;

        ImVec4 final_color = m_color.Resolve();

        if (m_use_gradient)
        {
            ImU32 col_center = GetColorWithAlpha(final_color, 1.0f);
            ImU32 col_edge = GetColorWithAlpha(final_color, 0.0f);
            float center_x = m_absolute_pos.x + m_rect.w * 0.5f;

            // 计算绘制的 Y 范围
            float y_top = center_y - half_visual_h;
            float y_bottom = center_y + half_visual_h;

            // 左半段
            dl->AddRectFilledMultiColor(
                TechUtils::Snap(ImVec2(m_absolute_pos.x, y_top)),
                TechUtils::Snap(ImVec2(center_x, y_bottom)),
                col_edge, col_center, col_center, col_edge
            );

            // 右半段
            dl->AddRectFilledMultiColor(
                TechUtils::Snap(ImVec2(center_x, y_top)),
                TechUtils::Snap(ImVec2(m_absolute_pos.x + m_rect.w, y_bottom)),
                col_center, col_edge, col_edge, col_center
            );
        }
        else
        {
            ImU32 col = GetColorWithAlpha(final_color, 1.0f);
            // [修改] DrawHardLine 的 Y 坐标使用计算出的中心点
            TechUtils::DrawHardLine(dl,
                ImVec2(m_absolute_pos.x, center_y),
                ImVec2(m_absolute_pos.x + m_rect.w, center_y),
                col, m_visual_height);
        }
    }
};

_UI_END
_SYSTEM_END
_NPGS_END