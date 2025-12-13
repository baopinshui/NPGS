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


float CalculateRenderedHeight(ImFont* font, const std::string& text_to_measure, float wrap_w)
{
    if (!font || text_to_measure.empty()) return 0.0f;

    float font_size = font->FontSize;
    float current_h = 0.0f;

    const char* text_begin = text_to_measure.c_str();
    const char* text_end = text_begin + text_to_measure.size();
    const char* s = text_begin;

    while (s < text_end)
    {
        const char* paragraph_end = strchr(s, '\n');
        if (!paragraph_end) paragraph_end = text_end;

        if (s == paragraph_end)
        {
            // Empty paragraph (blank line)
            current_h += font_size;
        }
        else
        {
            const char* line_start = s;
            while (line_start < paragraph_end)
            {
                const char* line_end;
                if (wrap_w > 0.0f)
                {
                    const char* content_start = line_start;
                    while (content_start < paragraph_end && (*content_start == ' ' || *content_start == '\t'))
                        content_start++;

                    float indent_width = 0.0f;
                    if (content_start > line_start)
                    {
                        indent_width = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, line_start, content_start).x;
                    }

                    float remaining_width = wrap_w - indent_width;
                    if (remaining_width <= 0) remaining_width = 1.0f;

                    line_end = font->CalcWordWrapPositionA(1.0f, content_start, paragraph_end, remaining_width);
                }
                else
                {
                    line_end = paragraph_end;
                }

                current_h += font_size;
                line_start = line_end;
                while (line_start < paragraph_end && (*line_start == ' ' || *line_start == '\t'))
                    line_start++;
            }
        }

        s = paragraph_end;
        if (s < text_end)
        {
            s++;
        }
    }

    return current_h;
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
        m_rect.h = CalculateRenderedHeight(font, text_to_measure, wrap_w);
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

    float hacker_progress = 1.0f;
    bool is_hacker_mode = (m_anim_mode == TechTextAnimMode::Hacker && m_hacker_effect.m_active);
    if (is_hacker_mode)
    {
        hacker_progress = m_hacker_effect.GetProgress();
    }

    float wrap_width = -1.0f;
    if (m_sizing_mode == TechTextSizingMode::AutoHeight)
    {
        wrap_width = m_rect.w;
        if (wrap_width > 0.0f && wrap_width < font_size) wrap_width = font_size;
    }

    float total_content_height = 0.0f;
    if (m_sizing_mode == TechTextSizingMode::Fixed && m_align_v != Alignment::Start)
    {
        ImVec2 total_sz = font->CalcTextSizeA(font_size, FLT_MAX, wrap_width > 0.0f ? wrap_width : -1.0f, text_to_draw.c_str());
        total_content_height = total_sz.y;
    }

    float current_y = m_absolute_pos.y + offset_y;
    if (m_sizing_mode == TechTextSizingMode::Fixed)
    {
        if (m_align_v == Alignment::Center) current_y += (m_rect.h - total_content_height) * 0.5f;
        else if (m_align_v == Alignment::End) current_y += (m_rect.h - total_content_height);
    }
    
    // --- 5. [BUG修复 2.0] 再次重构绘制循环 ---
    const char* text_begin = text_to_draw.c_str();
    const char* text_end = text_begin + text_to_draw.size();
    const char* s = text_begin;

    while (s < text_end)
    {
        // A. 查找段落结束点 (下一个换行符或字符串末尾)
        const char* paragraph_end = strchr(s, '\n');
        if (!paragraph_end) paragraph_end = text_end;

        // B. 判断当前段落是否为空
        if (s == paragraph_end)
        {
            // [正确逻辑] 这是一个空行，仅增加行高
            current_y += font_size;
        }
        else
        {
            // [正确逻辑] 这是一个带内容的段落，对其进行自动换行
            const char* line_start = s;
            while (line_start < paragraph_end)
            {
                const char* line_end;
                if (wrap_width > 0.0f)
                {
                    const char* content_start = line_start;
                    while (content_start < paragraph_end && (*content_start == ' ' || *content_start == '\t'))
                        content_start++;
                    
                    float indent_width = 0.0f;
                    if (content_start > line_start)
                    {
                        indent_width = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, line_start, content_start).x;
                    }
                    
                    float remaining_width = wrap_width - indent_width;
                    if (remaining_width <= 0) remaining_width = 1.0f; 
                    
                    line_end = font->CalcWordWrapPositionA(1.0f, content_start, paragraph_end, remaining_width);
                }
                else
                {
                    line_end = paragraph_end;
                }

                // 计算当前行的尺寸与位置
                std::string line_text_str;
                const char* draw_start = line_start;
                const char* draw_end = line_end;

                if (is_hacker_mode)
                {
                    // 计算当前行在原始字符串中的字节偏移量
                    size_t global_offset = line_start - text_begin;

                    // 调用 Helper 获取这一行的混合结果
                    // 这里传入 hacker_progress，意味着所有行共享同一个进度 0% -> 100%
                    line_text_str = m_hacker_effect.GetMixedSubString(line_start, line_end, global_offset, hacker_progress);

                    // 更新绘制指针指向新的临时字符串
                    draw_start = line_text_str.c_str();
                    draw_end = draw_start + line_text_str.size();
                }

                // 2. 计算尺寸 (注意使用 draw_start, draw_end)
                ImVec2 line_sz = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, draw_start, draw_end);

                // ... (计算 draw_pos x坐标代码不变) ...
                float line_x = m_absolute_pos.x;
                if (m_align_h == Alignment::Center) line_x += (m_rect.w - line_sz.x) * 0.5f;
                else if (m_align_h == Alignment::End) line_x += (m_rect.w - line_sz.x);
                ImVec2 draw_pos(line_x, current_y);

                // 3. 绘制辉光
                if (use_glow)
                {
                    for (const auto& tap : kernel)
                    {
                        float step_alpha = glow_alpha_factor * tap.w * alpha_mult;
                        if (step_alpha <= 0.005f) continue;
                        ImU32 glow_col = GetColorWithAlpha(glow_base_vec, std::min(1.0f, step_alpha));
                        // 注意这里传入的是 draw_start, draw_end
                        dl->AddText(font, font_size, ImVec2(draw_pos.x + tap.x * m_glow_spread, draw_pos.y + tap.y * m_glow_spread), glow_col, draw_start, draw_end);
                    }
                }

                // 4. 绘制本体
                dl->AddText(font, font_size, draw_pos, text_col, draw_start, draw_end);

                // ================== [核心修改区域 END] ==================

                current_y += font_size;
                line_start = line_end;
                while (line_start < paragraph_end && (*line_start == ' ' || *line_start == '\t'))
                    line_start++;
            }
        }

        s = paragraph_end;
        if (s < text_end) s++;
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
        DrawTextContent(dl, m_current_display_text, 0.0f, 1.0f);
    }

    dl->PopClipRect();

    if (font) ImGui::PopFont();
}

_UI_END
_SYSTEM_END
_NPGS_END