// --- START OF FILE TechText.cpp ---

#include "TechText.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// [MODIFIED] 构造函数实现
TechText::TechText(const std::string& key_or_text, const StyleColor& color)
    : m_source_key_or_text(key_or_text), m_color(color)
{
    m_block_input = false;
    // 默认行为：文本的尺寸由其内容决定
    m_width_policy = Length::Content();
    m_height_policy = Length::Content();

    // 初始文本获取
    m_current_display_text = TR(m_source_key_or_text);
}

// [MODIFIED] SetAnimMode (不变，但语义更清晰)
TechText* TechText::SetAnimMode(TechTextAnimMode mode)
{
    m_anim_mode = mode;
    return this;
}

// [NEW] SetGlow (新的链式API)
TechText* TechText::SetGlow(bool enable, const StyleColor& color, float intensity,float spread)
{
    m_use_glow = enable;
    if (color.HasValue())
    {
        m_glow_color = color;
    }
    m_glow_spread = spread;
    m_glow_intensity = intensity;
    return this;
}

// [MODIFIED] SetSourceText (不变，但与新的I18n更新流程配合)
void TechText::SetSourceText(const std::string& new_key_or_text)
{
    if (m_source_key_or_text == new_key_or_text) return;
    m_source_key_or_text = new_key_or_text;
    // 强制下一帧进行更新检查
    m_local_i18n_version = 0;
}

// [MODIFIED] RestartEffect (不变)
void TechText::RestartEffect()
{
    if (m_anim_mode == TechTextAnimMode::Hacker)
    {
        m_hacker_effect.Start(m_current_display_text, 0.0f);
    }
    else if (m_anim_mode == TechTextAnimMode::Scroll)
    {
        m_old_text = "";
        m_scroll_progress = 0.0f;
    }
}

// [NEW] 核心布局方法：Measure
ImVec2 TechText::Measure(const ImVec2& available_size)
{
    ImFont* font = GetFont();
    if (!font) return { 0, 0 }; // 无法测量

    // 为了尺寸稳定，我们总是基于最终的目标文本来测量
    const std::string& text_to_measure = m_current_display_text;
    if (text_to_measure.empty()) return { 0, 0 };

    float final_w = 0.0f;
    float final_h = 0.0f;

    // --- 步骤 1: 确定宽度 ---
    float wrap_width = 0.0f; // 0.0f 表示不换行

    if (m_width_policy.type == LengthType::Fixed)
    {
        final_w = m_width_policy.value;
        wrap_width = final_w;
    }
    else if (m_width_policy.type == LengthType::Fill)
    {
        final_w = available_size.x;
        wrap_width = final_w;
    }
    else // LengthType::Content
    {
        // 测量单行文本的自然宽度
        ImVec2 size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text_to_measure.c_str());
        final_w = size.x;
        // Content 宽度意味着不主动换行
    }

    // --- 步骤 2: 确定高度 ---
    if (m_height_policy.type == LengthType::Fixed)
    {
        final_h = m_height_policy.value;
    }
    else if (m_height_policy.type == LengthType::Fill)
    {
        final_h = available_size.y;
    }
    else // LengthType::Content
    {
        // 高度自适应内容，需要考虑由宽度决定的换行
        ImVec2 size = font->CalcTextSizeA(font->FontSize, FLT_MAX, wrap_width > 0.0f ? wrap_width : -1.0f, text_to_measure.c_str());
        final_h = size.y;
    }

    return ImVec2(final_w, final_h);
}


// [MODIFIED] Update: 移除所有尺寸计算
void TechText::Update(float dt, const ImVec2& parent_abs_pos)
{
    // --- 1. I18n 文本更新检查 ---
    auto& i18n = System::I18nManager::Get();
    if (m_local_i18n_version != i18n.GetVersion())
    {
        std::string new_display_text = TR(m_source_key_or_text);
        if (m_current_display_text != new_display_text)
        {
            // 文本变化，触发动画
            if (m_anim_mode == TechTextAnimMode::Scroll)
            {
                m_old_text = m_current_display_text;
                m_current_display_text = new_display_text;
                m_scroll_progress = 0.0f;
            }
            else if (m_anim_mode == TechTextAnimMode::Hacker)
            {
                m_current_display_text = new_display_text;
                m_hacker_effect.Start(m_current_display_text, 0.0f);
            }
            else
            {
                m_current_display_text = new_display_text;
            }
        }
        m_local_i18n_version = i18n.GetVersion();
    }

    // --- 2. 基类更新 (处理动画和 m_absolute_pos) ---
    // 尺寸 m_rect 已经被父容器在它的 Update 中设置好了
    UpdateSelf(dt, parent_abs_pos);

    // --- 3. 更新内部动画状态 ---
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

// [MODIFIED] DrawTextContent: 逻辑不变，但现在更可靠
void TechText::DrawTextContent(ImDrawList* dl, const std::string& text_to_draw, float offset_y, float alpha_mult)
{
    if (text_to_draw.empty() || alpha_mult <= 0.01f) return;

    ImFont* font = GetFont();
    if (!font) return;
    float font_size = font->FontSize;

    // --- 颜色计算 ---
    ImU32 text_col = GetColorWithAlpha(m_color.Resolve(), 1.0f * alpha_mult);

    // --- 辉光准备 ---
    // (Glow rendering logic is unchanged, it works fine with the new system)
    bool use_glow = m_use_glow;
    ImVec4 glow_base_vec = m_glow_color.Resolve();
    // ... same glow kernel setup ...
    struct GlowTap { float x, y, w; };
    static const GlowTap kernel[12] = {
        { 0.5f,  0.0f, 1.05f}, { -0.5f, 0.0f, 1.05f}, { 0.0f,  0.5f, 1.05f}, { 0.0f, -0.5f, 1.05f},
        { 0.8f,  0.8f, 0.65f}, { 0.8f, -0.8f, 0.65f}, {-0.8f,  0.8f, 0.65f}, {-0.8f, -0.8f, 0.65f},
        { 1.5f,  0.0f, 0.30f}, {-1.5f, 0.0f, 0.30f}, { 0.0f,  1.5f, 0.30f}, { 0.0f, -1.5f, 0.30f}
    };


    // --- 布局计算 ---
    // 关键：现在 m_rect.w 就是父容器分配给我们的最终宽度，直接用作换行宽度
    float wrap_width = m_rect.w > 0.0f ? m_rect.w : -1.0f;
    ImVec2 content_size = font->CalcTextSizeA(font_size, FLT_MAX, wrap_width, text_to_draw.c_str());

    ImVec2 draw_pos = m_absolute_pos;
    draw_pos.y += offset_y;

    // 应用对齐
    if (m_align_h == Alignment::Center) draw_pos.x += (m_rect.w - content_size.x) * 0.5f;
    else if (m_align_h == Alignment::End) draw_pos.x += (m_rect.w - content_size.x);

    if (m_align_v == Alignment::Center) draw_pos.y += (m_rect.h - content_size.y) * 0.5f;
    else if (m_align_v == Alignment::End) draw_pos.y += (m_rect.h - content_size.y);

    std::string final_text_to_draw = text_to_draw;
    if (m_anim_mode == TechTextAnimMode::Hacker && m_hacker_effect.m_active)
    {
        // Hacker 模式需要逐行处理，这里简化为对整个文本块应用效果
        // （注意：这可能导致换行位置与目标文本不一致，但对于单行或固定行数的文本效果很好）
        final_text_to_draw = m_hacker_effect.GetMixedSubString(
            m_current_display_text.c_str(),
            m_current_display_text.c_str() + m_current_display_text.size(),
            0,
            m_hacker_effect.GetProgress()
        );
    }

    // --- 绘制 ---
    if (use_glow)
    {
        float glow_alpha_factor = 0.4f * m_glow_intensity;
        for (const auto& tap : kernel)
        {
            float step_alpha = glow_alpha_factor * tap.w * alpha_mult;
            if (step_alpha <= 0.005f) continue;
            ImU32 glow_col = GetColorWithAlpha(glow_base_vec, std::min(1.0f, step_alpha));
            dl->AddText(font, font_size, ImVec2(draw_pos.x + tap.x * m_glow_spread, draw_pos.y + tap.y * m_glow_spread), glow_col, final_text_to_draw.c_str(), NULL, wrap_width);
        }
    }

    dl->AddText(font, font_size, draw_pos, text_col, final_text_to_draw.c_str(), NULL, wrap_width);
}

// [MODIFIED] Draw: 现在只负责调用 DrawTextContent
void TechText::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    ImFont* font = GetFont();
    if (font) ImGui::PushFont(font);

    // 裁剪区，为辉光留出空间
    ImVec2 clip_min(m_absolute_pos.x - 50.0f, m_absolute_pos.y - 50.0f);
    ImVec2 clip_max(m_absolute_pos.x + m_rect.w + 50.0f, m_absolute_pos.y + m_rect.h + 50.0f);
    dl->PushClipRect(clip_min, clip_max, true);

    if (m_anim_mode == TechTextAnimMode::Scroll && m_scroll_progress < 1.0f)
    {
        float t = AnimationUtils::Ease(m_scroll_progress, EasingType::EaseOutQuad);
        float font_h = font ? font->FontSize : 13.0f;
        float move_dist = font_h * 1.2f;

        DrawTextContent(dl, m_old_text, -move_dist * t, 1.0f - t);
        DrawTextContent(dl, m_current_display_text, move_dist * (1.0f - t), t);
    }
    else
    {
        DrawTextContent(dl, m_current_display_text, 0.0f, 1.0f);
    }

    dl->PopClipRect();

    if (font) ImGui::PopFont();
}

_UI_END
_SYSTEM_END
_NPGS_END