#pragma once
#include "../ui_framework.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class TechDivider : public UIElement
{
public:
    ImVec4 m_color;
    TechDivider()
    {
        m_block_input = false;
        m_rect.h = 1.0f; // 默认高度
        m_rect.w = 10.0f;
        m_color = UIContext::Get().m_theme.color_border;
        m_color.w = 0.3f; // 默认半透明
    }

    void Draw(ImDrawList* dl) override
    {
        if (!m_visible || m_alpha <= 0.01f) return;

        ImU32 col = GetColorWithAlpha(m_color, 1.0f);

        // 画一条线
        dl->AddLine(
            ImVec2(m_absolute_pos.x, m_absolute_pos.y + m_rect.h * 0.5f),
            ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h * 0.5f),
            col,
            1.0f
        );
    }
};

_UI_END
_SYSTEM_END
_NPGS_END