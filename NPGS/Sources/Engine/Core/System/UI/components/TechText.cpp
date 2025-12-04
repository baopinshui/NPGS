#include "TechText.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

TechText::TechText(const std::string& text,
    const std::optional<ImVec4>& color,
    bool use_hacker_effect,
    bool use_glow,
    const std::optional<ImVec4>& glow_color)
    : m_text(text)
    , m_color_override(color)
    , m_use_effect(use_hacker_effect)
    , m_use_glow(use_glow)
    , m_glow_color(glow_color)
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
    const char* text_to_draw = m_use_effect ? m_hacker_effect.m_display_text.c_str() : m_text.c_str();

    // 2. 确定主颜色
    const auto& theme = UIContext::Get().m_theme;
    ImVec4 final_col_vec = m_color_override.value_or(theme.color_text);

    // 3. 布局计算
    ImVec2 text_size = ImGui::CalcTextSize(text_to_draw);
    float draw_x = m_absolute_pos.x;
    float draw_y = m_absolute_pos.y;

    if (m_align_h == Alignment::Center) draw_x += (m_rect.w - text_size.x) * 0.5f;
    else if (m_align_h == Alignment::End) draw_x += (m_rect.w - text_size.x);

    if (m_align_v == Alignment::Center) draw_y += (m_rect.h - text_size.y) * 0.5f;
    else if (m_align_v == Alignment::End) draw_y += (m_rect.h - text_size.y);

    ImVec2 draw_pos(draw_x, draw_y);

    // --- [新增] 荧光绘制逻辑 ---
    if (m_use_glow)
    {
        // A. 确定荧光颜色：如果有指定用指定的，否则用文字颜色
        ImVec4 glow_base = m_glow_color.value_or(final_col_vec);

        // B. 计算荧光 Alpha：通常比主文字淡，且受全局 Alpha 影响
        // 我们将其设为较低的透明度，比如 0.3 ~ 0.5，因为我们会叠加绘制多次
        float glow_alpha_factor = 0.4f * m_glow_intensity;
        ImU32 glow_col = GetColorWithAlpha(glow_base, glow_alpha_factor);

        // C. 多重偏移绘制 (模拟高斯模糊)
        // 在 8 个方向偏移绘制，使得光晕看起来比较圆润
        float s = m_glow_spread;
        const float offsets[8][2] = {
            {-s, -s}, {0, -s}, {s, -s},
            {-s,  0},          {s,  0},
            {-s,  s}, {0,  s}, {s,  s}
        };

        // 也可以只画4次（上下左右）以节省性能，但8次效果更好
        for (int i = 0; i < 8; ++i)
        {
            dl->AddText(ImVec2(draw_pos.x + offsets[i][0], draw_pos.y + offsets[i][1]), glow_col, text_to_draw);
        }

        // 可选：再画一圈更远的更淡的，增加层次感 (此处为了性能暂省略)
    }

    // 4. 绘制主文字 (画在荧光上面)
    ImU32 col = GetColorWithAlpha(final_col_vec, 1.0f);
    dl->AddText(draw_pos, col, text_to_draw);

    if (font) ImGui::PopFont();
}
_UI_END
_SYSTEM_END
_NPGS_END