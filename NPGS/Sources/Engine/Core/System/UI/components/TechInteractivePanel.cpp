#include "TechInteractivePanel.h"
#include "../TechUtils.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

TechInteractivePanel::TechInteractivePanel()
{
    // 关键：将 m_block_input 设为 true，这样此面板才能捕获鼠标事件
    m_block_input = true;

    // 设置默认样式：
    // - 静止时完全透明
    // - 悬停时显示带有主题色的半透明高亮
    m_color_bg = StyleColor(ImVec4(0, 0, 0, 0));
    m_color_bg_hover = StyleColor(ThemeColorID::Accent).WithAlpha(0.15f);
    m_color_bg_pressed = StyleColor(ThemeColorID::Accent).WithAlpha(0.25f);
}

void TechInteractivePanel::Update(float dt)
{
    // 根据 m_hovered 和 m_clicked 状态更新动画进度条
    if (m_hovered || m_clicked)
    {
        m_hover_progress += dt * m_anim_speed;
    }
    else
    {
        m_hover_progress -= dt * m_anim_speed;
    }
    m_hover_progress = std::clamp(m_hover_progress, 0.0f, 1.0f);

    // 调用基类 VBox::Update，它会负责更新所有子元素的逻辑（如文本动画）
    VBox::Update(dt);
}

void TechInteractivePanel::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    // 1. 首先绘制背景
    // 解析颜色
    ImVec4 bg_idle = m_color_bg.Resolve();
    // 如果鼠标正处于按下状态，使用 pressed 颜色，否则使用 hover 颜色
    ImVec4 bg_target = m_clicked ? m_color_bg_pressed.Resolve() : m_color_bg_hover.Resolve();

    // 使用动画进度条进行颜色插值，实现平滑过渡
    ImU32 final_bg_col = GetColorWithAlpha(TechUtils::LerpColor(bg_idle, bg_target, m_hover_progress), 1.0f);

    // 仅在背景不是完全透明时才进行绘制，以优化性能
    if ((final_bg_col & IM_COL32_A_MASK) > 0)
    {
        dl->AddRectFilled(
            m_absolute_pos,
            ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h),
            final_bg_col
        );
    }

    // 2. 调用基类 VBox::Draw，在其背景之上绘制所有子元素
    //   （VBox::Draw 内部会调用 UIElement::Draw，完成子元素的递归绘制）
    VBox::Draw(dl);
}


_UI_END
_SYSTEM_END
_NPGS_END