#include "InputField.h"
#include "../TechUtils.h"
#include <cmath>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

ImFont* InputField::GetFont() const
{
    if (m_font) return m_font;
    ImFont* mono = UIContext::Get().m_font_bold;
    if (mono) return mono;
    return UIElement::GetFont();
}

InputField::InputField(std::string* target) : m_target_string(target)
{
    m_block_input = true;
    m_focusable = true;
    m_rect.h = 20.0f;
    if (m_target_string) m_cursor_pos = m_target_string->length();

    const auto& theme = UIContext::Get().m_theme;
    m_text_color = theme.color_text;
    m_border_color = theme.color_border;

    m_anim_head = 0.0f;
    m_anim_tail = 0.0f;
    m_activation_count = 0;
    m_last_show_state = false;
}

void InputField::Update(float dt, const ImVec2& parent_abs_pos)
{
    bool current_focused = IsFocused();
    if (m_last_frame_focused && !current_focused)
    {
        // 刚才有焦点，现在没了 -> 触发提交
        if (on_commit && m_target_string)
        {
            on_commit(*m_target_string);
        }
    }
    m_last_frame_focused = current_focused;
    UIElement::Update(dt, parent_abs_pos);
    if (IsFocused())
    {
        m_cursor_blink_timer += dt;
        if (m_cursor_blink_timer > 1.0f) m_cursor_blink_timer = 0.0f;
        m_show_cursor = m_cursor_blink_timer < 0.5f;
    }

    bool should_show = false;
    switch (m_underline_mode)
    {
    case UnderlineDisplayMode::Always: should_show = true; break;
    case UnderlineDisplayMode::OnHoverOrFocus: should_show = m_hovered || IsFocused(); break;
    case UnderlineDisplayMode::Never: should_show = false; break;
    }

    if (should_show && !m_last_show_state)
    {
        m_activation_count++;
    }
    m_last_show_state = should_show;

    float target_head = static_cast<float>(m_activation_count);

    // Tail 的目标：如果是显示状态，保持在上一圈的末尾（保证线是满的）
    // 如果是隐藏状态，追赶到当前圈的末尾（让线消失）
    float target_tail = should_show ?
        static_cast<float>(m_activation_count - 1) :
        static_cast<float>(m_activation_count);

    float speed = 8.0f;

    auto UpdateValue = [&](float& current, float target, float dt_speed)
    {
        if (current < target)
        {
            current += (target - current+0.1) * dt_speed * dt;
            if (std::abs(target - current) < 0.001f) current = target;
        }
    };

    UpdateValue(m_anim_head, target_head, speed);

    // [修复 1] 将尾部速度改为与头部一致 (去掉 * 1.2f)
    // 这样在快速划过时，尾部会以相同的速度跟随头部，形成一个向右移动的“光标”，而不会中途吞噬头部
    UpdateValue(m_anim_tail, target_tail, speed);

    // [修复 2] 强制约束：尾部永远不能超过头部
    // 这解决了闪烁问题，确保任何时候线段长度 >= 0
    if (m_anim_tail > m_anim_head)
    {
        m_anim_tail = m_anim_head;
    }

    // 重置逻辑 (防止浮点数无限增大)
    if (!should_show && m_anim_head == m_anim_tail && m_anim_head > 1.0f)
    {
        m_anim_head = 0.0f;
        m_anim_tail = 0.0f;
        m_activation_count = 0;
    }
}

void InputField::Draw(ImDrawList* draw_list)
{
    if (!m_visible || m_alpha <= 0.01f) return;
    ImFont* font = GetFont();
    ImGui::PushFont(font);

    float font_size = ImGui::GetFontSize();
    float draw_y = m_absolute_pos.y;
    float width = m_rect.w;
    float start_x = m_absolute_pos.x;

    ImU32 border_col = GetColorWithAlpha(m_border_color, 1.0f);
    ImU32 text_col = GetColorWithAlpha(m_text_color, 1.0f);

    ImVec2 text_pos = ImVec2(start_x, draw_y);
    if (m_target_string && !m_target_string->empty())
    {
        draw_list->AddText(text_pos, text_col, m_target_string->c_str());
    }

    if (IsFocused() && m_show_cursor)
    {
        if (m_cursor_pos > m_target_string->length()) m_cursor_pos = m_target_string->length();
        std::string sub = m_target_string->substr(0, m_cursor_pos);
        ImVec2 cursor_offset = ImGui::CalcTextSize(sub.c_str());
        float cursor_x = text_pos.x + cursor_offset.x;
        draw_list->AddLine(
            ImVec2(cursor_x, draw_y),
            ImVec2(cursor_x, draw_y + font_size),
            text_col,
            1.5f
        );
    }

    float line_y = draw_y + font_size + 2.0f;

    int cycle_head = static_cast<int>(std::floor(m_anim_head));
    int cycle_tail = static_cast<int>(std::floor(m_anim_tail));

    float ratio_head = m_anim_head - cycle_head;
    float ratio_tail = m_anim_tail - cycle_tail;

    // [MODIFICATION] The change is entirely within this lambda
    auto DrawSegment = [&](float r_start, float r_end)
    {
        // 1. 定义裁剪区间 (硬编码调节)
        // 意思：逻辑进度走过 0.1 才开始在屏幕上显示，走到 0.9 时就在屏幕上显示满了
        const float clip_min = 0.01f; // 裁剪掉头部的 1%
        const float clip_max = 0.99f; // 裁剪掉尾部的 1%
        const float clip_range = clip_max - clip_min;

        // 2. 辅助 Remap 函数：将逻辑值 [clip_min, clip_max] 映射到 视觉值 [0, 1]
        auto Remap = [&](float val) -> float
        {
            if (val <= clip_min) return 0.0f;
            if (val >= clip_max) return 1.0f;
            return (val - clip_min) / clip_range;
        };

        // 3. 计算映射后的坐标
        float mapped_start = Remap(r_start);
        float mapped_end = Remap(r_end);

        // 如果映射后长度太短（说明整段都在裁剪区间内），则不绘制
        if (mapped_end <= mapped_start + 0.001f) return;

        // 4. 使用映射后的全长 [0, 1] 进行绘制
        TechUtils::DrawHardLine(draw_list,
            ImVec2(start_x + width * mapped_start, line_y),
            ImVec2(start_x + width * mapped_end, line_y),
            border_col,
            1.0f
        );
    };

    if (cycle_head == cycle_tail)
    {
        DrawSegment(ratio_tail, ratio_head);
    }
    else
    {
        DrawSegment(ratio_tail, 1.0f);
        DrawSegment(0.0f, ratio_head);
    }

    ImGui::PopFont();
}


void InputField::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled)
{
    // ... (This function remains unchanged)
    UIElement::HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);
    if (m_clicked && mouse_clicked)
    {
        RequestFocus();
        if (m_target_string && !m_target_string->empty())
        {
            ImFont* font = GetFont();
            ImGui::PushFont(font);
            float click_local_x = mouse_pos.x - m_absolute_pos.x;
            if (click_local_x <= 0)
            {
                m_cursor_pos = 0;
            }
            else
            {
                float best_diff = 99999.0f;
                int best_index = 0;
                for (int i = 0; i <= m_target_string->length(); ++i)
                {
                    std::string sub = m_target_string->substr(0, i);
                    float width = ImGui::CalcTextSize(sub.c_str()).x;
                    float diff = std::abs(click_local_x - width);
                    if (diff < best_diff)
                    {
                        best_diff = diff;
                        best_index = i;
                    }
                }
                m_cursor_pos = best_index;
            }
            ImGui::PopFont();
        }
        else
        {
            m_cursor_pos = 0;
        }
        m_cursor_blink_timer = 0.0f;
        m_show_cursor = true;
    }
}

bool InputField::HandleKeyboardEvent()
{
    // ... (This function remains unchanged)
    if (!IsFocused() || !m_target_string) return false;
    ImGuiIO& io = ImGui::GetIO();
    bool handled = false;
    if (!io.InputQueueCharacters.empty())
    {
        for (int i = 0; i < io.InputQueueCharacters.Size; i++)
        {
            unsigned int c = (unsigned int)io.InputQueueCharacters[i];
            if (c >= 32)
            {
                m_target_string->insert(m_cursor_pos, 1, (char)c);
                m_cursor_pos++;
                handled = true;
            }
        }
        io.InputQueueCharacters.clear();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && m_cursor_pos > 0)
    {
        m_target_string->erase(m_cursor_pos - 1, 1);
        m_cursor_pos--;
        handled = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && m_cursor_pos < m_target_string->length())
    {
        m_target_string->erase(m_cursor_pos, 1);
        handled = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && m_cursor_pos > 0)
    {
        m_cursor_pos--;
        handled = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && m_cursor_pos < m_target_string->length())
    {
        m_cursor_pos++;
        handled = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home)) { m_cursor_pos = 0; handled = true; }
    if (ImGui::IsKeyPressed(ImGuiKey_End)) { m_cursor_pos = m_target_string->length(); handled = true; }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        UIContext::Get().ClearFocus();
        handled = true;
    }
    if (handled && on_change)
    {
        on_change(*m_target_string);
    }
    return handled;
}

_UI_END
_SYSTEM_END
_NPGS_END