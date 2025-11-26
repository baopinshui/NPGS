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
    ImVec2 ap_min = m_absolute_pos;
    ImVec2 ap_max = ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h);

    if (m_use_glass_effect)
    {
        DrawGlassBackground(dl, ap_min, ap_max); // 调用基类方法
    }

    // [视觉优化] 背景增加一点深度，稍微暗一点
    ImVec4 deep_bg = { 0.0f,0.0f,0.0f,0.6f };

    // 1. 绘制背景
    dl->AddRectFilled(
        m_absolute_pos,
        ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h),
        GetColorWithAlpha(deep_bg, 1.0f)
    );





    // 3. 绘制子元素 (Clip Rect)
    ImVec2 clip_min = m_absolute_pos;
    ImVec2 clip_max = ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h);
    dl->PushClipRect(clip_min, clip_max, true);
    UIElement::Draw(dl);
    dl->PopClipRect();

    // 4. 绘制科技边框角
    bool visual_hover = ImGui::IsMouseHoveringRect(
        m_absolute_pos,
        ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h), 0
    );

    float corner_sz = 10.0f;
    float thick = m_thickness; // 建议设为 1.0f 或 2.0f，不要有小数如 1.5
    ImVec2 pos = m_absolute_pos;
    ImVec2 sz = { m_rect.w, m_rect.h };

    ImVec2 p_min = pos;
    ImVec2 p_max = ImVec2(pos.x + sz.x, pos.y + sz.y);


    float corner_len = 10.0f;
    float thickness = m_thickness; // 建议设为 1.0f 或 2.0f，不要有小数如 1.5
    float half_t = thickness * 0.5f; // 计算半宽，用于内缩偏移
    ImU32 col;
    if (!visual_hover && m_rect.w < 100.0f)
        col = GetColorWithAlpha(theme.color_border, 1.0f);
    else
        col = GetColorWithAlpha(theme.color_accent, 1.0f);

    // 左上 (Top Left) -> 向右(+), 向下(+)
    TechUtils::DrawCorner(dl,
        ImVec2(p_min.x + half_t, p_min.y + half_t),
        corner_len, corner_len, thickness, col);

    // 右上 (Top Right) -> 向左(-), 向下(+)
    TechUtils::DrawCorner(dl,
        ImVec2(p_max.x - half_t, p_min.y + half_t),
        -corner_len, corner_len, thickness, col);

    // 左下 (Bottom Left) -> 向右(+), 向上(-)
    TechUtils::DrawCorner(dl,
        ImVec2(p_min.x + half_t, p_max.y - half_t),
        corner_len, -corner_len, thickness, col);

    // 右下 (Bottom Right) -> 向左(-), 向上(-)
    TechUtils::DrawCorner(dl,
        ImVec2(p_max.x - half_t, p_max.y - half_t),
        -corner_len, -corner_len, thickness, col);
}

_UI_END
_SYSTEM_END
_NPGS_END