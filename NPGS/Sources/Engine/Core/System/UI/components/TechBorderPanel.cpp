#include "TechBorderPanel.h"
#include "../TechUtils.h"
_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

TechBorderPanel::TechBorderPanel()
{}

// [新增] 处理鼠标事件
// 目的：即使子元素消耗了事件（handled=true），作为背景容器的我们也应该保持 m_hovered = true，
// 这样视觉上的边框高亮才不会因为鼠标移到了内部按钮上而突然消失。
void TechBorderPanel::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled)
{
    // 1. 记录进入此节点前的 handled 状态。
    // 如果 handled 已经是 true，说明被兄弟节点（通常是更上层的窗口）遮挡了，此时我们需要遵循基类逻辑（不 hover）。
    bool was_handled_externally = handled;

    // 2. 调用基类处理（递归子节点，处理子节点的点击等）
    // 注意：UIElement::HandleMouseEvent 会在子节点消耗事件后将 m_hovered 设为 false。
    UIElement::HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);

    // 3. [关键修正] 视觉状态矫正
    // 如果没有被外部（更上层 UI）遮挡，且鼠标确实在我们的范围内，
    // 那么无论内部子节点是否消耗了事件，对于这个边框面板来说，它都处于“悬停”状态。
    if (m_visible && m_alpha > 0.01f && !was_handled_externally)
    {
        Rect abs_rect = { m_absolute_pos.x, m_absolute_pos.y, m_rect.w, m_rect.h };
        if (abs_rect.Contains(mouse_pos))
        {
            m_hovered = true;
        }
    }
}
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

    // 2.绘制子元素 (Clip Rect)
    // 这里的 Clip Rect 逻辑保持不变
    ImVec2 clip_min = m_absolute_pos;
    ImVec2 clip_max = ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h);
    dl->PushClipRect(clip_min, clip_max, true);
    UIElement::Draw(dl);
    dl->PopClipRect();

    // 3. [重构] 绘制科技边框角
    // 不再使用 ImGui::IsMouseHoveringRect，直接使用由 HandleMouseEvent 计算出的 m_hovered
    // 这样能正确响应 UI 遮挡关系
    ImU32 border_col;

    // 逻辑：如果没有悬停，且宽度很窄（<100），则显示暗淡的边框色；
    // 否则（悬停中 或 面板很宽），显示高亮的主题色。
    if (!m_hovered && m_rect.w < 100.0f)
        border_col = GetColorWithAlpha(theme.color_border, 1.0f);
    else
        border_col = GetColorWithAlpha(theme.color_accent, 1.0f);

    // 直接调用工具函数
    TechUtils::DrawBracketedBox(dl, p_min, p_max, border_col, m_thickness, 10.0f);
}
_UI_END
_SYSTEM_END
_NPGS_END