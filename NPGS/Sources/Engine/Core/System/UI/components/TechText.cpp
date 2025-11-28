#include "TechText.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

TechText::TechText(const std::string& text, const std::optional<ImVec4>& color, bool use_hacker_effect)
    : m_text(text), m_color_override(color), m_use_effect(use_hacker_effect)
{
    m_block_input = false;
    m_rect.h = 20.0f; // 基础高度

    if (m_use_effect)
    {
        // 如果启用特效，初始化时就开始播放
        m_hacker_effect.Start(m_text, 0.0f);
    }
}

void TechText::SetText(const std::string& new_text)
{
    if (m_text == new_text) return;
    m_text = new_text;
    if (m_use_effect)
    {
        m_hacker_effect.Start(m_text, 0.0f);
    }
}
void TechText::RestartEffect()
{
    // 只有在启用了特效的情况下才执行
    if (m_use_effect)
    {
        // 调用 HackerTextHelper 的 Start 方法来重置并开始动画
        m_hacker_effect.Start(m_text, 0.0f);
    }
}
void TechText::Update(float dt, const ImVec2& parent_abs_pos)
{
    UIElement::Update(dt, parent_abs_pos);

    // 如果启用了特效，就要更新特效逻辑
    if (m_use_effect)
    {
        m_hacker_effect.Update(dt);
    }
}

void TechText::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    ImFont* font = GetFont();
    if (font) ImGui::PushFont(font);

    // 1. 决定要画什么字
    // 如果开启了特效，问 Helper 要当前显示什么；否则直接显示目标文本
    const char* text_to_draw = m_use_effect ? m_hacker_effect.m_display_text.c_str() : m_text.c_str();

    // 2. 颜色与透明度
    const auto& theme = UIContext::Get().m_theme;
    ImVec4 final_col_vec = m_color_override.value_or(theme.color_text);
    ImU32 col = GetColorWithAlpha(final_col_vec, 1.0f);

    // 3. 布局计算 (同之前)
    ImVec2 text_size = ImGui::CalcTextSize(text_to_draw);
    float draw_x = m_absolute_pos.x;
    float draw_y = m_absolute_pos.y;

    if (m_align_h == Alignment::Center) draw_x += (m_rect.w - text_size.x) * 0.5f;
    else if (m_align_h == Alignment::End) draw_x += (m_rect.w - text_size.x);

    if (m_align_v == Alignment::Center) draw_y += (m_rect.h - text_size.y) * 0.5f;
    else if (m_align_v == Alignment::End) draw_y += (m_rect.h - text_size.y); // 支持垂直底部对齐

    // 4. 绘制
    dl->AddText(ImVec2(draw_x, draw_y), col, text_to_draw);

    if (font) ImGui::PopFont();
}

_UI_END
_SYSTEM_END
_NPGS_END