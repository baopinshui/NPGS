// --- START OF FILE TechText.cpp --- (修改部分)

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
    , m_use_glow(use_glow)
    , m_glow_color(glow_color)
{
    m_block_input = false;
    m_rect.h = 20.0f;

    // 初始化模式
    if (use_hacker_effect)
    {
        m_anim_mode = TechTextAnimMode::Hacker;
        m_hacker_effect.Start(m_text, 0.0f);
    }
    else
    {
        m_anim_mode = TechTextAnimMode::None;
    }
}

TechText* TechText::SetAnimMode(TechTextAnimMode mode)
{
    m_anim_mode = mode;
    return this;
}

void TechText::SetText(const std::string& new_text)
{
    if (m_text == new_text) return;

    if (m_anim_mode == TechTextAnimMode::Scroll)
    {
        // [滚动模式逻辑]
        // 1. 当前显示的文本变成旧文本
        m_old_text = m_text;
        // 2. 更新新文本
        m_text = new_text;
        // 3. 重置进度，开始动画
        m_scroll_progress = 0.0f;
    }
    else if (m_anim_mode == TechTextAnimMode::Hacker)
    {
        // [Hacker 模式逻辑]
        m_text = new_text;
        m_hacker_effect.Start(m_text, 0.0f);
    }
    else
    {
        // [普通模式] 直接赋值
        m_text = new_text;
    }
}

void TechText::RestartEffect()
{
    if (m_anim_mode == TechTextAnimMode::Hacker)
    {
        m_hacker_effect.Start(m_text, 0.0f);
    }
    else if (m_anim_mode == TechTextAnimMode::Scroll)
    {
        // Scroll 模式下 Restart 可以理解为重新播放一次进入动画
        // 让 old_text 为空，只播放 new_text 的淡入
        m_old_text = "";
        m_scroll_progress = 0.0f;
    }
}

void TechText::Update(float dt, const ImVec2& parent_abs_pos)
{
    UIElement::Update(dt, parent_abs_pos);

    if (m_anim_mode == TechTextAnimMode::Hacker)
    {
        m_hacker_effect.Update(dt);
    }
    else if (m_anim_mode == TechTextAnimMode::Scroll)
    {
        if (m_scroll_progress < 1.0f)
        {
            m_scroll_progress += dt / m_scroll_duration;
            if (m_scroll_progress > 1.0f) m_scroll_progress = 1.0f;
        }
    }
}

// [核心重构] 提取出的绘制单行文本的函数（包含对齐和荧光）
void TechText::DrawTextContent(ImDrawList* dl, const std::string& text_to_draw, float offset_y, float alpha_mult)
{
    if (text_to_draw.empty() || alpha_mult <= 0.01f) return;

    // 1. 颜色计算
    const auto& theme = UIContext::Get().m_theme;
    ImVec4 final_col_vec = m_color_override.value_or(theme.color_text);

    // 2. 布局计算
    ImVec2 text_size = ImGui::CalcTextSize(text_to_draw.c_str());
    float draw_x = m_absolute_pos.x;
    float draw_y = m_absolute_pos.y + offset_y; // 应用Y轴偏移

    if (m_align_h == Alignment::Center) draw_x += (m_rect.w - text_size.x) * 0.5f;
    else if (m_align_h == Alignment::End) draw_x += (m_rect.w - text_size.x);

    if (m_align_v == Alignment::Center) draw_y += (m_rect.h - text_size.y) * 0.5f;
    else if (m_align_v == Alignment::End) draw_y += (m_rect.h - text_size.y);

    ImVec2 draw_pos(draw_x, draw_y);

    bool is_highlight_white = (final_col_vec.x > 0.99f && final_col_vec.y > 0.99f && final_col_vec.z > 0.99f);
    if (is_highlight_white&&!m_use_glow)
    {
        ImVec4 glow_base = ImVec4{0.0f,0.0f,0.0f,1.0f};

        float base_alpha_val = 0.4f * 0.2f * alpha_mult;

        // 扩散半径
        float s = 0.5f;

        struct GlowTap { float x, y, w; };
        static const GlowTap kernel[12] = {
            { 0.5f,  0.0f, 1.05f}, { -0.5f, 0.0f, 1.05f}, { 0.0f,  0.5f, 1.05f}, { 0.0f, -0.5f, 1.05f},

            { 0.8f,  0.8f, 0.65f}, { 0.8f, -0.8f, 0.65f}, {-0.8f,  0.8f, 0.65f}, {-0.8f, -0.8f, 0.65f},

            { 1.5f,  0.0f, 0.30f}, {-1.5f, 0.0f, 0.30f}, { 0.0f,  1.5f, 0.30f}, { 0.0f, -1.5f, 0.30f}
        };

        for (const auto& tap : kernel)
        {
            float step_alpha = base_alpha_val * tap.w;

            if (step_alpha <= 0.005f) continue;

            if (step_alpha > 1.0f) step_alpha = 1.0f;

            ImU32 glow_col = GetColorWithAlpha(glow_base, step_alpha);

            dl->AddText(
                ImVec2(draw_pos.x + tap.x * s, draw_pos.y + tap.y * s),
                glow_col,
                text_to_draw.c_str()
            );
        }
    }
    if (m_use_glow)
    {
        ImVec4 glow_base = m_glow_color.value_or(final_col_vec);

        // 基础 Alpha：保持原来的 0.4 系数，作为基准强度
        float base_alpha_val = 0.4f * m_glow_intensity * alpha_mult;

        // 扩散半径
        float s = m_glow_spread;

        // 定义高斯采样核 (x_mult, y_mult, weight)
        // 这里的 weight 总和设计为 8.0，以匹配旧版 8 点均匀采样的总亮度
        struct GlowTap { float x, y, w; };
        static const GlowTap kernel[12] = {
            // 内圈 (紧实的核心，距离 ~0.5s) - 权重总和 4.2
            { 0.5f,  0.0f, 1.05f}, { -0.5f, 0.0f, 1.05f}, { 0.0f,  0.5f, 1.05f}, { 0.0f, -0.5f, 1.05f},

            // 中圈 (对角线填充，距离 ~1.1s) - 权重总和 2.6
            { 0.8f,  0.8f, 0.65f}, { 0.8f, -0.8f, 0.65f}, {-0.8f,  0.8f, 0.65f}, {-0.8f, -0.8f, 0.65f},

            // 外圈 (柔和的边缘，距离 ~1.5s) - 权重总和 1.2
            { 1.5f,  0.0f, 0.30f}, {-1.5f, 0.0f, 0.30f}, { 0.0f,  1.5f, 0.30f}, { 0.0f, -1.5f, 0.30f}
        };

        for (const auto& tap : kernel)
        {
            // 计算当前点的 alpha
            float step_alpha = base_alpha_val * tap.w;

            // 性能优化：极淡的像素直接跳过绘制
            if (step_alpha <= 0.005f) continue;

            // 安全钳制
            if (step_alpha > 1.0f) step_alpha = 1.0f;

            // 只有当 Alpha 变化时才重新计算颜色 (减少 GetColorWithAlpha 开销)
            ImU32 glow_col = GetColorWithAlpha(glow_base, step_alpha);

            dl->AddText(
                ImVec2(draw_pos.x + tap.x * s, draw_pos.y + tap.y * s),
                glow_col,
                text_to_draw.c_str()
            );
        }
    }
    // 4. 主体绘制
    ImU32 col = GetColorWithAlpha(final_col_vec, 1.0f * alpha_mult);
    dl->AddText(draw_pos, col, text_to_draw.c_str());
}

void TechText::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    ImFont* font = GetFont();
    if (font) ImGui::PushFont(font);

    // 1. 判断当前是否处于滚动动画状态
    bool is_scrolling = (m_anim_mode == TechTextAnimMode::Scroll && m_scroll_progress < 1.0f);

    ImVec2 clip_min, clip_max;

    if (is_scrolling)
    {
        // [滚动时]：启用严格的垂直裁剪，水平放宽
        // 这样文字才能在组件的上下边缘平滑地出现和消失
        clip_min = ImVec2(m_absolute_pos.x - 2000.0f, m_absolute_pos.y);
        clip_max = ImVec2(m_absolute_pos.x + 4000.0f, m_absolute_pos.y + m_rect.h);
    }
    else
    {
        // [静止时]：垂直和水平方向都放宽裁剪，以完整显示 Glow 效果
        // 上下各多留 50px 的空间，足以容纳任何辉光
        clip_min = ImVec2(m_absolute_pos.x - 2000.0f, m_absolute_pos.y - 50.0f);
        clip_max = ImVec2(m_absolute_pos.x + 4000.0f, m_absolute_pos.y + m_rect.h + 50.0f);
    }

    dl->PushClipRect(clip_min, clip_max, true);

    if (m_anim_mode == TechTextAnimMode::Scroll && m_scroll_progress < 1.0f)
    {
        // === 滚动动画绘制逻辑 ===

        // 使用 EaseOutQuad 让运动更自然
        float t = AnimationUtils::Ease(m_scroll_progress, EasingType::EaseOutQuad);
        float font_h = font ? font->FontSize : 13.0f;
        float move_dist = font_h * 1.2f; // 移动距离稍微超过一个字高

        // 1. 绘制旧文本 (向上移动，变透明)
        // Y: 0 -> -move_dist
        // Alpha: 1 -> 0
        float old_y_offset = -move_dist * t;
        float old_alpha = 1.0f - t;
        DrawTextContent(dl, m_old_text, old_y_offset, old_alpha);

        // 2. 绘制新文本 (从下方移入，变不透明)
        // Y: +move_dist -> 0
        // Alpha: 0 -> 1
        // 这里 t 稍微做个映射，让新文字稍微晚一点点出来会更有层次感，或者直接同步
        float new_y_offset = move_dist * (1.0f - t);
        float new_alpha = t;
        DrawTextContent(dl, m_text, new_y_offset, new_alpha);
    }
    else
    {
        // === 常规/Hacker 绘制逻辑 ===
        std::string display_str = (m_anim_mode == TechTextAnimMode::Hacker) ? m_hacker_effect.m_display_text : m_text;
        DrawTextContent(dl, display_str, 0.0f, 1.0f);
    }

    dl->PopClipRect();

    if (font) ImGui::PopFont();
}

_UI_END
_SYSTEM_END
_NPGS_END