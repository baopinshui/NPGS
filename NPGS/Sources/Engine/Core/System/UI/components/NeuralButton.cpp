#include "NeuralButton.h"
#include "../TechUtils.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN


NeuralButton::NeuralButton(const std::string& t)
{
    m_rect.h = 20.0f;
    m_block_input = true;

    // 创建并配置 TechText 子组件
    m_label = std::make_shared<TechText>(t);
    m_label->m_align_h = Alignment::Center; // 水平居中
    m_label->m_align_v = Alignment::Center; // 垂直居中
    // 注意：m_fill_h/v 属性在这里实际上不起作用，因为父级不是布局容器，
    // 但保留它们有助于理解意图。真正的布局在下面的 Update 中完成。
    m_label->m_fill_h = true;
    m_label->m_fill_v = true;
    m_label->m_block_input = false;
    AddChild(m_label);
}

void NeuralButton::Update(float dt, const ImVec2& parent_abs_pos)
{
    // 悬停动画逻辑
    float speed = 8.0f;
    if (m_hovered)
    {
        m_hover_progress += dt * speed;
    }
    else
    {
        m_hover_progress -= dt * speed;
    }
    m_hover_progress = std::clamp(m_hover_progress, 0.0f, 1.0f);
    // 动态更新子组件的颜色
    const auto& theme = UIContext::Get().m_theme;
    ImVec4 txt_normal = theme.color_accent;
    ImVec4 txt_hover = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    ImVec4 current_txt_vec4 = TechUtils::LerpColor(txt_normal, txt_hover, m_hover_progress);
    m_label->SetColor(current_txt_vec4);

    // --- [核心修复] ---
    // 在调用基类Update之前，手动处理子元素的布局。
    // 这确保了 TechText 组件的矩形区域与按钮本身的大小完全一致。
    if (m_label)
    {
        m_label->m_rect.x = 0;
        m_label->m_rect.y = 0;
        m_label->m_rect.w = this->m_rect.w;
        m_label->m_rect.h = this->m_rect.h;
    }
    // --- [修复结束] ---

    // 现在调用基类 Update。它会使用我们刚刚为子元素设置好的正确 m_rect
    // 来计算子节点的 absolute_pos，并继续递归更新。
    UIElement::Update(dt, parent_abs_pos);
}

bool NeuralButton::HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release)
{
    bool ret = UIElement::HandleMouseEvent(p, down, click, release);
    if (m_clicked && click && on_click_callback) on_click_callback();
    return ret;
}

void NeuralButton::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    // Draw 方法现在只负责绘制背景和边框
    const auto& theme = UIContext::Get().m_theme;

    // 计算背景颜色
    ImVec4 bg_normal = theme.color_accent;
    bg_normal.w = 0.15f;
    ImVec4 bg_hover = theme.color_accent;
    ImVec4 current_bg_vec4 = TechUtils::LerpColor(bg_normal, bg_hover, m_hover_progress);
    ImU32 bg_col = GetColorWithAlpha(current_bg_vec4, 1.0f);
    ImU32 border_col = GetColorWithAlpha(theme.color_accent, 1.0f);

    ImVec2 p_min = m_absolute_pos;
    ImVec2 p_max = ImVec2(p_min.x + m_rect.w, p_min.y + m_rect.h);

    // 绘制背景和边框
    dl->AddRectFilled(p_min, p_max, bg_col);
    TechUtils::DrawBracketedBox(dl, p_min, p_max, border_col, 2.0f, 8.0f);

    // 绘制子元素 (即 TechText)
    UIElement::Draw(dl);
}

void NeuralButton::ResetInteraction()
{
    UIElement::ResetInteraction();
    m_hover_progress = 0.0f;
}

_UI_END
_SYSTEM_END
_NPGS_END