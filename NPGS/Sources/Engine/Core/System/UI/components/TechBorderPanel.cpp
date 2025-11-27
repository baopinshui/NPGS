#include "TechBorderPanel.h"
#include "../TechUtils.h"
_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

TechBorderPanel::TechBorderPanel()
{}

void TechBorderPanel::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;
    const auto& theme = UIContext::Get().m_theme;

    // -----------------------------------------------------------
    // 1. 绘制背景 (集成毛玻璃逻辑)
    // -----------------------------------------------------------
    ImVec2 p_min = m_absolute_pos;
    ImVec2 p_max = ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h);

    if (m_use_glass_effect)
    {
        DrawGlassBackground(dl, p_min, p_max); // 调用基类方法
    }

    // [视觉优化] 背景增加一点深度，稍微暗一点
    ImVec4 deep_bg = { 0.0f,0.0f,0.0f,0.6f };

    // 绘制透明黑
    dl->AddRectFilled(
        m_absolute_pos,
        ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h),
        GetColorWithAlpha(deep_bg, 1.0f)
    );





    // 2.绘制子元素 (Clip Rect)
    ImVec2 clip_min = m_absolute_pos;
    ImVec2 clip_max = ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h);
    dl->PushClipRect(clip_min, clip_max, true);
    UIElement::Draw(dl);
    dl->PopClipRect();

    // 4. 绘制科技边框角
    // 3. [重构] 绘制科技边框角
    // 使用通用逻辑 DrawBracketedBox
    bool visual_hover = ImGui::IsMouseHoveringRect(p_min, p_max, 0);

    ImU32 border_col;
    if (!visual_hover && m_rect.w < 100.0f)
        border_col = GetColorWithAlpha(theme.color_border, 1.0f);
    else
        border_col = GetColorWithAlpha(theme.color_accent, 1.0f);

    // 直接调用工具函数
    TechUtils::DrawBracketedBox(dl, p_min, p_max, border_col, m_thickness, 10.0f);
}
_UI_END
_SYSTEM_END
_NPGS_END