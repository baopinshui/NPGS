// --- START OF FILE TechText.cpp --- (修改部分)

#include "TechText.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

TechText::TechText(const std::string& key_or_text,
    const StyleColor& color,
    bool use_hacker_effect,
    bool use_glow,
    const StyleColor& glow_color)
    : m_source_key_or_text(key_or_text), // 直接保存原始输入
    m_color(color),
    m_use_glow(use_glow),
    m_glow_color(glow_color)
{
    m_block_input = false;
    m_rect.h = 20.0f;
    m_sizing_mode = TechTextSizingMode::Fixed;
    m_current_display_text = TR(m_source_key_or_text);


    if (use_hacker_effect)
    {
        m_anim_mode = TechTextAnimMode::Hacker;
        m_hacker_effect.Start(m_current_display_text, 0.0f);
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

void TechText::SetSourceText(const std::string& new_key_or_text)
{
    if (m_source_key_or_text == new_key_or_text) return;

    m_source_key_or_text = new_key_or_text;

    // 强制下一帧进行更新检查
    m_local_i18n_version = 0;
}


void TechText::RestartEffect()
{
    if (m_anim_mode == TechTextAnimMode::Hacker)
    {
        m_hacker_effect.Start(m_current_display_text, 0.0f);
    }
    else if (m_anim_mode == TechTextAnimMode::Scroll)
    {
        // Scroll 模式下 Restart 可以理解为重新播放一次进入动画
        // 让 old_text 为空，只播放 new_text 的淡入
        m_old_text = "";
        m_scroll_progress = 0.0f;
    }
}
void TechText::RecomputeSize()
{
    // 如果是固定模式，完全不触碰 m_rect，以保持 UI 稳定性
    if (m_sizing_mode == TechTextSizingMode::Fixed) return;

    ImFont* font = GetFont();
    // 必须有 font 才能计算，如果没有就稍后重试（Draw时通常会有）
    if (!font) font = UIContext::Get().m_font_regular;
    if (!font) font = ImGui::GetFont();

    float font_size = font->FontSize;

    // 获取当前要展示的最终文本（对于 Hacker 模式，为了防止布局抖动，
    // 建议使用目标文本计算大小，或者如果你希望它抖动，就用 hacker.m_display_text）
    // 这里为了 UI 稳定，建议使用 m_current_display_text (目标文本)
    const std::string& text_to_measure = m_current_display_text;
    if (text_to_measure.empty())
    {
        if (m_sizing_mode == TechTextSizingMode::AutoHeight) m_rect.h = 0.0f;
        else { m_rect.w = 0.0f; m_rect.h = 0.0f; }
        return;
    }

    if (m_sizing_mode == TechTextSizingMode::AutoWidthHeight)
    {
        ImVec2 sz = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text_to_measure.c_str());
        m_rect.w = sz.x;
        m_rect.h = sz.y;
    }
    else if (m_sizing_mode == TechTextSizingMode::AutoHeight)
    {
        // 宽度由外部（如 VBox 或手动设置）决定，我们只算高度
        // 如果宽度太小（比如刚初始化），可能还没被布局，暂时设个极大值或保持原样
        float wrap_w = (m_rect.w > 1.0f) ? m_rect.w : 10000.0f;

        ImVec2 sz = font->CalcTextSizeA(font_size, FLT_MAX, wrap_w, text_to_measure.c_str());
        m_rect.h = sz.y;
    }
}
void TechText::Update(float dt, const ImVec2& parent_abs_pos)
{
    auto& i18n = System::I18nManager::Get();
    if (m_local_i18n_version != i18n.GetVersion())
    {
        std::string new_display_text = TR(m_source_key_or_text);

        if (m_current_display_text != new_display_text)
        {
            // 文本发生了变化，触发动画
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

    RecomputeSize();

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

    // --- 1. 资源与字体准备 ---
    ImFont* font = GetFont();
    if (!font) font = ImGui::GetFont();
    if (!font) return;
    float font_size = font->FontSize;

    // --- 2. 颜色计算 ---
    ImVec4 final_col_vec = m_color.Resolve();
    ImU32 text_col = GetColorWithAlpha(final_col_vec, 1.0f * alpha_mult);

    // --- 3. 辉光参数准备 ---
    bool use_glow = m_use_glow;
    ImVec4 glow_base_vec = m_glow_color.Resolve();
    float glow_alpha_factor = 0.4f * m_glow_intensity;

    bool is_highlight_white = (final_col_vec.x > 0.99f && final_col_vec.y > 0.99f && final_col_vec.z > 0.99f);
    if (is_highlight_white && !use_glow)
    {
        use_glow = true;
        glow_base_vec = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        glow_alpha_factor = 0.4f * 0.2f;
    }

    struct GlowTap { float x, y, w; };
    static const GlowTap kernel[12] = {
        { 0.5f,  0.0f, 1.05f}, { -0.5f, 0.0f, 1.05f}, { 0.0f,  0.5f, 1.05f}, { 0.0f, -0.5f, 1.05f},
        { 0.8f,  0.8f, 0.65f}, { 0.8f, -0.8f, 0.65f}, {-0.8f,  0.8f, 0.65f}, {-0.8f, -0.8f, 0.65f},
        { 1.5f,  0.0f, 0.30f}, {-1.5f, 0.0f, 0.30f}, { 0.0f,  1.5f, 0.30f}, { 0.0f, -1.5f, 0.30f}
    };

    // --- 4. 布局预计算 ---

    // 【修改点】：只有 AutoHeight 模式才启用自动换行 (wrap_width > 0)。
    // Fixed 模式下 wrap_width 设为 -1.0f，这意味着“无限宽度”，不仅不会在空格处换行，
    // 就算超过 m_rect.w 也不会换行（会保持单行直至遇到 \n），这与旧版 AddText 行为一致。
    float wrap_width = -1.0f;
    if (m_sizing_mode == TechTextSizingMode::AutoHeight)
    {
        wrap_width = m_rect.w;
        // 保护性设置：防止宽度过小导致死循环
        if (wrap_width > 0.0f && wrap_width < font_size) wrap_width = font_size;
    }

    // 计算总高度 (用于垂直对齐)
    float total_content_height = 0.0f;
    // 仅在 Fixed 模式且需要垂直非顶部对齐时计算，优化性能
    if (m_sizing_mode == TechTextSizingMode::Fixed && m_align_v != Alignment::Start)
    {
        ImVec2 total_sz = font->CalcTextSizeA(font_size, FLT_MAX, wrap_width, text_to_draw.c_str());
        total_content_height = total_sz.y;
    }

    // 计算起始 Y 坐标
    float current_y = m_absolute_pos.y + offset_y;
    if (m_sizing_mode == TechTextSizingMode::Fixed)
    {
        if (m_align_v == Alignment::Center) current_y += (m_rect.h - total_content_height) * 0.5f;
        else if (m_align_v == Alignment::End) current_y += (m_rect.h - total_content_height);
    }

    // --- 5. 逐行绘制循环 ---
    const char* text_begin = text_to_draw.c_str();
    const char* text_end = text_begin + text_to_draw.size();
    const char* s = text_begin;

    while (s < text_end)
    {
        // A. 寻找断行点
        const char* line_end;
        if (wrap_width > 0.0f)
        {
            // AutoHeight: 自动换行模式
            line_end = font->CalcWordWrapPositionA(1.0f, s, text_end, wrap_width);
            const char* next_newline = strchr(s, '\n');
            if (next_newline && next_newline < line_end) line_end = next_newline;
            if (line_end == s && s < text_end) line_end = s + 1;
        }
        else
        {
            // Fixed / AutoWidthHeight: 不自动换行，只认 \n
            line_end = strchr(s, '\n');
            if (!line_end) line_end = text_end;
        }

        // B. 计算当前行的尺寸
        ImVec2 line_sz = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, s, line_end);

        // C. 计算当前行的水平偏移
        float line_x = m_absolute_pos.x;

        // 在 Fixed 模式下，m_rect.w 是容器宽度，line_sz.x 是文字宽度，可以正常对齐
        if (m_align_h == Alignment::Center) line_x += (m_rect.w - line_sz.x) * 0.5f;
        else if (m_align_h == Alignment::End) line_x += (m_rect.w - line_sz.x);

        ImVec2 draw_pos(line_x, current_y);

        // D. 绘制辉光
        if (use_glow)
        {
            float s_spread = m_glow_spread;
            for (const auto& tap : kernel)
            {
                float step_alpha = glow_alpha_factor * tap.w * alpha_mult;
                if (step_alpha <= 0.005f) continue;
                if (step_alpha > 1.0f) step_alpha = 1.0f;

                ImU32 glow_col = GetColorWithAlpha(glow_base_vec, step_alpha);
                dl->AddText(font, font_size,
                    ImVec2(draw_pos.x + tap.x * s_spread, draw_pos.y + tap.y * s_spread),
                    glow_col, s, line_end);
            }
        }

        // E. 绘制本体
        dl->AddText(font, font_size, draw_pos, text_col, s, line_end);

        // F. 移动游标
        current_y += font_size;
        s = line_end;
        if (s < text_end && *s == '\n') s++;
    }
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
        DrawTextContent(dl, m_current_display_text, new_y_offset, new_alpha);
    }
    else
    {
        // === 常规/Hacker 绘制逻辑 ===
        std::string display_str = (m_anim_mode == TechTextAnimMode::Hacker) ? m_hacker_effect.m_display_text : m_current_display_text;
        DrawTextContent(dl, display_str, 0.0f, 1.0f);
    }

    dl->PopClipRect();

    if (font) ImGui::PopFont();
}

_UI_END
_SYSTEM_END
_NPGS_END