#include "NeuralButton.h"
#include "../TechUtils.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

static ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}

NeuralButton::NeuralButton(const std::string& t) : text(t)
{
    m_rect.h = 20.0f; // 稍微调高一点，容纳角框
    m_block_input = true;
}

// [新增] Update 实现动画逻辑
void NeuralButton::Update(float dt, const ImVec2& parent_abs_pos)
{
    UIElement::Update(dt, parent_abs_pos);

    // 悬停动画：悬停时趋向 1.0，离开时趋向 0.0
    float speed = 8.0f; // 动画速度
    if (m_hovered)
    {
        m_hover_progress += dt * speed;
    }
    else
    {
        m_hover_progress -= dt * speed;
    }
    m_hover_progress = std::clamp(m_hover_progress, 0.0f, 1.0f);
}
bool NeuralButton::HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release)
{
    bool ret = UIElement::HandleMouseEvent(p, down, click, release);
    if (m_clicked && click && on_click_callback) on_click_callback();
    return ret;
}
// [重写] Draw 实现新的视觉风格
// ... 前面的 include 和 LerpColor ...

void NeuralButton::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;
    ImFont* font = GetFont();
    if (font) ImGui::PushFont(font);

    const auto& theme = UIContext::Get().m_theme;

    // 背景动画逻辑保持不变

    ImVec4 bg_normal = theme.color_accent;       // 正常：暗主题色
    bg_normal.w = 0.15f;;
    ImVec4 bg_hover = theme.color_accent;           // 悬停：主题色

    // --- 文字颜色 ---
    ImVec4 txt_normal = theme.color_accent;         // 正常：主题色
    ImVec4 txt_hover = ImVec4(0.0f, 0.0f, 0.0f, 1.0f); // 悬停：不透明的黑色
    // 如果悬停高亮，文字变亮白，否则是主题色
   
    
    ImVec4 current_bg_vec4 = LerpColor(bg_normal, bg_hover, m_hover_progress);
    ImVec4 current_txt_vec4 = LerpColor(txt_normal, txt_hover, m_hover_progress);    
    ImU32 bg_col = GetColorWithAlpha(current_bg_vec4, 1.0f);
    ImU32 txt_col = GetColorWithAlpha(current_txt_vec4, 1.0f);
    ImU32 border_col = GetColorWithAlpha(theme.color_accent, 1.0f);

    // 2. 绘制背景
    dl->AddRectFilled(
        m_absolute_pos,
        ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h),
        bg_col
    );

    // 3. 绘制 L 型边框 (内描边)
    float corner_len = 8.0f; // 稍微小一点，更精致
    float thick = 2.0f;      // 细线更有科技感

    ImVec2 p_min = m_absolute_pos;
    ImVec2 p_max = ImVec2(p_min.x + m_rect.w, p_min.y + m_rect.h);

    // 这里的偏移量 (offset) 决定是内描边还是外描边。
    // 为了不画出边界，我们向内缩半个像素或一个像素
    float off = 0.5* thick;

    TechUtils::DrawCorner(dl, ImVec2(p_min.x + off, p_min.y + off), corner_len, corner_len, thick, border_col);
    TechUtils::DrawCorner(dl, ImVec2(p_max.x - off, p_min.y + off), -corner_len, corner_len, thick, border_col);
    TechUtils::DrawCorner(dl, ImVec2(p_min.x + off, p_max.y - off), corner_len, -corner_len, thick, border_col);
    TechUtils::DrawCorner(dl, ImVec2(p_max.x - off, p_max.y - off), -corner_len, -corner_len, thick, border_col);

    // 4. 绘制文字 (居中)
    // [视觉优化] 确保字体对齐到像素，防止模糊
    ImVec2 txt_sz = ImGui::CalcTextSize(text.c_str());
    ImVec2 txt_pos = ImVec2(
        std::floor(m_absolute_pos.x + (m_rect.w - txt_sz.x) * 0.5f),
        std::floor(m_absolute_pos.y + (m_rect.h - txt_sz.y) * 0.5f)
    );

    dl->AddText(txt_pos, txt_col, text.c_str());

    if (font) ImGui::PopFont();
}
void NeuralButton::ResetInteraction()
{
    // 1. 先调用基类方法，重置 m_hovered, m_clicked 以及递归重置子节点
    UIElement::ResetInteraction();

    // 2. [关键修复] 强制将动画进度归零
    m_hover_progress = 0.0f;
}

_UI_END
_SYSTEM_END
_NPGS_END