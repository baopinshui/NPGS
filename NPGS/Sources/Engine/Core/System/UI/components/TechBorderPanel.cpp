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

void TechBorderPanel::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (m_visible && m_show_flow_border)
    {
        // 1. 外层进度 (周期 = T)
        if (m_flow_period > 0.001f)
        {
            m_progress_outer += dt / m_flow_period;
            if (m_progress_outer > 1.0f) m_progress_outer -= 1.0f;

            // 2. 内层进度 (周期 = T / sqrt(2))
            // 速度 = 1 / 周期 = sqrt(2) / T
            float inner_speed_mult = 1.41421356f;
            m_progress_inner += (dt * inner_speed_mult) / m_flow_period;
            if (m_progress_inner > 1.0f) m_progress_inner -= 1.0f;
        }
    }

    Panel::Update(dt, parent_abs_pos);
}

void TechBorderPanel::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    // 1. 绘制背景
    ImVec2 p_min = TechUtils::Snap(m_absolute_pos);
    ImVec2 p_max = TechUtils::Snap(ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h));

    if (m_use_glass_effect)
    {
        DrawGlassBackground(dl, p_min, p_max, m_glass_col.Resolve());
	}
	else
    {

        dl->AddRectFilled(
            p_min,
            p_max,
            GetColorWithAlpha(m_bg_color.Resolve(), 1.0f)
        );
    }
    // 2. 绘制子元素
    ImVec2 clip_min = m_absolute_pos;
    ImVec2 clip_max = ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h);
    dl->PushClipRect(clip_min, clip_max, true);
    UIElement::Draw(dl);
    dl->PopClipRect();

    // 3. 颜色逻辑
    const auto& theme = UIContext::Get().m_theme; // [新增] 实时获取主题
    ImU32 border_col;
    if (m_force_accent_color)
    {
        border_col = GetColorWithAlpha(theme.color_accent, 1.0f); // [修改]
    }
    else
    {
        if (!m_hovered && m_rect.w < 100.0f)
            border_col = GetColorWithAlpha(theme.color_border, 1.0f); // [修改]
        else
            border_col = GetColorWithAlpha(theme.color_accent, 1.0f); // [修改]
    }

    // 4. 双层流光绘制
    if (m_show_flow_border)
    {
        // 确保分段数至少为1
        int seg_count = std::max(1, m_flow_segment_count);

        // 层1：外层
        TechUtils::DrawGradientFlow(dl,
            p_min, p_max,
            -2.0f,
            1.0f,
            border_col,
            m_progress_outer,
            m_flow_length_ratio,
            m_flow_use_gradient,
            false, // 顺时针
            seg_count,
            m_flow_randomness 
        );

        // 层2：内层
        TechUtils::DrawGradientFlow(dl,
            p_min, p_max,
            0.0f,
            1.0f,
            border_col,
            m_progress_inner,
            m_flow_length_ratio,
            m_flow_use_gradient,
            true, // 逆时针
            seg_count,
            m_flow_randomness 
        );
    }
    else
    {
        TechUtils::DrawBracketedBox(dl, p_min, p_max, border_col, m_thickness, 10.0f);
    }
}

_UI_END
_SYSTEM_END
_NPGS_END