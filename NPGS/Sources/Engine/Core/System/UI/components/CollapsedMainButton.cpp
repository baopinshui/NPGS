#include "CollapsedMainButton.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

CollapsedMainButton::CollapsedMainButton()
{
    m_block_input = true;
}

bool CollapsedMainButton::HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release)
{
    bool ret = UIElement::HandleMouseEvent(p, down, click, release);
    if (m_clicked && click && on_click_callback) on_click_callback();
    return ret;
}

void CollapsedMainButton::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    // Center calculation

    auto& ctx = UIContext::Get();
    ImFont* font = GetFont();
    if (font) ImGui::PushFont(font);

    const auto& theme = UIContext::Get().m_theme;
    ImVec2 center = ImVec2(m_absolute_pos.x + m_rect.w * 0.5f, m_absolute_pos.y + m_rect.h * 0.5f);
    ImU32 col = GetColorWithAlpha(theme.color_accent, 1.0f);

    // 1. Draw "@" symbol
    // Need a larger font ideally, but we simulate placement
    // [视觉对齐] 1. 模拟 animate-pulse
    float pulse_alpha = 0.7f + 0.3f * sinf(ImGui::GetTime() * 4.0f);
    ImU32 symbol_col = GetColorWithAlpha(theme.color_accent, pulse_alpha);
    ImU32 text_col = GetColorWithAlpha(theme.color_accent, 1.0f);

    // [视觉对齐] 2. 绘制 "◈" 符号 (使用大字体)
    const char* symbol = "◈";
    if (ctx.m_font_large) ImGui::PushFont(ctx.m_font_large);
    ImVec2 sym_sz = ImGui::CalcTextSize(symbol);
    // 向上移动一点以匹配布局
    dl->AddText(ImVec2(center.x - sym_sz.x * 0.5f, center.y - sym_sz.y * 0.5f - 10.0f), symbol_col, symbol);
    if (ctx.m_font_large) ImGui::PopFont();
    // 2. Draw "MANAGE NETWORK"
    const char* text = "MANAGE";
    ImVec2 txt_sz = ImGui::CalcTextSize(text);
    dl->AddText(ImVec2(center.x - txt_sz.x * 0.5f, center.y + 5), col, text);
    const char* text2 = "NETWORK";
    ImVec2 txt_sz2 = ImGui::CalcTextSize(text2);
    dl->AddText(ImVec2(center.x - txt_sz2.x * 0.5f, center.y + 18), col, text2);
    if (font) ImGui::PopFont();
}

_UI_END
_SYSTEM_END
_NPGS_END