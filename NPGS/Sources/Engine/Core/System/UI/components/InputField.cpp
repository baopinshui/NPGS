#include "InputField.h"
#include "../TechUtils.h"
#include <cmath>
#include <algorithm> // for min/max

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

ImFont * InputField::GetFont() const
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

    
    m_cursor_pos = 0;
    m_selection_anchor = 0;
    

    m_anim_head = 0.0f;
    m_anim_tail = 0.0f;
    m_activation_count = 0;
    m_last_show_state = false;
}

// [新增] 辅助：是否有选区
bool InputField::HasSelection() const
{
    return m_cursor_pos != m_selection_anchor;
}

// [新增] 辅助：获取排序后的选区范围
void InputField::GetSelectionBounds(int& start, int& end) const
{
    if (m_cursor_pos < m_selection_anchor)
    {
        start = m_cursor_pos;
        end = m_selection_anchor;
    }
    else
    {
        start = m_selection_anchor;
        end = m_cursor_pos;
    }
}

// [新增] 辅助：删除选区内容
void InputField::DeleteSelection()
{
    if (!HasSelection() || !m_target_string) return;
    int s, e;
    GetSelectionBounds(s, e);

    m_target_string->erase(s, e - s);
    m_cursor_pos = s;
    m_selection_anchor = s;

    if (on_change) on_change(*m_target_string);
}

// [新增] 辅助：全选
void InputField::SelectAll()
{
    if (!m_target_string) return;
    m_selection_anchor = 0;
    m_cursor_pos = (int)m_target_string->length();
}

// [新增] 辅助：重置选区（取消选择）
void InputField::ResetSelection()
{
    m_selection_anchor = m_cursor_pos;
}

// [新增] 辅助：计算鼠标位置对应的字符索引
int InputField::GetCharIndexAt(float local_x)
{
    if (!m_target_string || m_target_string->empty()) return 0;

    if (local_x <= 0) return 0;

    ImFont* font = GetFont();
    // 必须临时 Push 字体以确保 CalcTextSize 准确
    bool pop_font = false;
    if (ImGui::GetFont() != font)
    {
        ImGui::PushFont(font);
        pop_font = true;
    }

    int best_index = 0;
    float best_diff = 99999.0f;

    // 简单的线性扫描，对于短文本足够快
    for (int i = 0; i <= m_target_string->length(); ++i)
    {
        std::string sub = m_target_string->substr(0, i);
        float width = ImGui::CalcTextSize(sub.c_str()).x;
        float diff = std::abs(local_x - width);
        if (diff < best_diff)
        {
            best_diff = diff;
            best_index = i;
        }
    }

    if (pop_font) ImGui::PopFont();

    return best_index;
}

void InputField::Update(float dt, const ImVec2& parent_abs_pos)
{
    bool current_focused = IsFocused();
    if (m_last_frame_focused && !current_focused)
    {
        // 失去焦点 -> 提交并取消选择
        ResetSelection();
        if (on_commit && m_target_string)
        {
            on_commit(*m_target_string);
        }
    }
    m_last_frame_focused = current_focused;
    UIElement::Update(dt, parent_abs_pos);

    if (IsFocused())
    {
        // [修改] 如果有选区，不闪烁；否则正常闪烁
        if (HasSelection())
        {
            m_show_cursor = false;
        }
        else
        {
            m_cursor_blink_timer += dt;
            if (m_cursor_blink_timer > 1.0f) m_cursor_blink_timer = 0.0f;
            m_show_cursor = m_cursor_blink_timer < 0.5f;
        }
    }

    // [新增] 处理鼠标拖拽选择
    // 如果已经被点击(m_clicked)且鼠标仍然按下，则视为拖拽
    if (m_clicked && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        float mouse_x_local = ImGui::GetMousePos().x - m_absolute_pos.x;
        int new_cursor = GetCharIndexAt(mouse_x_local);

        // 只有位置变化时才更新，避免无意义重绘
        if (new_cursor != m_cursor_pos)
        {
            m_cursor_pos = new_cursor;
            // 拖拽时 m_selection_anchor 保持在 HandleMouseEvent 中按下的那个位置不变
            m_show_cursor = true;
            m_cursor_blink_timer = 0.0f; // 拖拽时让光标/选区保持常亮
        }
    }

    // 下划线动画逻辑 (保持不变)
    bool should_show = false;
    switch (m_underline_mode)
    {
    case UnderlineDisplayMode::Always: should_show = true; break;
    case UnderlineDisplayMode::OnHoverOrFocus: should_show = m_hovered || IsFocused(); break;
    case UnderlineDisplayMode::Never: should_show = false; break;
    }

    if (should_show && !m_last_show_state) m_activation_count++;
    m_last_show_state = should_show;

    float target_head = static_cast<float>(m_activation_count);
    float target_tail = should_show ? static_cast<float>(m_activation_count - 1) : static_cast<float>(m_activation_count);
    float speed = 8.0f;

    auto UpdateAnim = [&](float& current, float target, float dt_speed)
    {
        if (current < target)
        {
            current += (target - current + 0.1f) * dt_speed * dt;
            if (std::abs(target - current) < 0.001f) current = target;
        }
    };

    UpdateAnim(m_anim_head, target_head, speed);
    UpdateAnim(m_anim_tail, target_tail, speed);

    if (m_anim_tail > m_anim_head) m_anim_tail = m_anim_head;

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

    const auto& theme = UIContext::Get().m_theme;
    ImVec4 final_border_color = m_border_color.value_or(theme.color_accent); // [新增]
    ImVec4 final_text_color = m_text_color.value_or(theme.color_accent); // [新增]

    ImU32 border_col = GetColorWithAlpha(final_border_color, 1.0f);
    ImU32 text_col = GetColorWithAlpha(final_text_color, 1.0f);

    // 选区颜色：背景为 m_border_color，文字为黑色
    ImU32 select_bg_col = border_col;
    ImU32 select_text_col = IM_COL32(0, 0, 0, 255);

    ImVec2 text_pos = ImVec2(start_x, draw_y);

    if (m_target_string && !m_target_string->empty())
    {
        // 1. 无论是否选中，先绘制底层完整的原色文本
        // 这保证了文字位置绝对不动
        draw_list->AddText(text_pos, text_col, m_target_string->c_str());

        // 2. 如果有选区，使用“裁剪绘制”覆盖中间部分
        if (HasSelection())
        {
            int s, e;
            GetSelectionBounds(s, e);
            int len = (int)m_target_string->length();
            if (s > len) s = len;
            if (e > len) e = len;

            // 计算裁剪区域的 X 坐标
            // 为了消除分段误差，我们根据从头开始的子串长度来计算位置
            float w_start = 0.0f;
            if (s > 0)
            {
                std::string sub_start = m_target_string->substr(0, s);
                w_start = ImGui::CalcTextSize(sub_start.c_str()).x;
            }

            float w_end = 0.0f;
            if (e > 0)
            {
                std::string sub_end = m_target_string->substr(0, e);
                w_end = ImGui::CalcTextSize(sub_end.c_str()).x;
            }

            ImVec2 clip_min(start_x + w_start, draw_y);
            ImVec2 clip_max(start_x + w_end, draw_y + font_size + 2.0f); //稍微加高一点覆盖

            // [修复抖动核心] 
            // 压入一个裁剪矩形，只限制在这个矩形内绘制。
            // 这样我们在完全相同的位置再画一遍文字，只有选区内的部分会被看见。
            draw_list->PushClipRect(clip_min, clip_max, true);

            // A. 绘制选区背景
            draw_list->AddRectFilled(clip_min, clip_max, select_bg_col);

            // B. 绘制选区文字（覆盖在原文字之上）
            // 注意：这里绘制的是*完整*的字符串，位置也和底层完全一样
            draw_list->AddText(text_pos, select_text_col, m_target_string->c_str());

            draw_list->PopClipRect();
        }
    }

    // 绘制光标 (仅当 focused 且不在选区状态且 timer 允许时)
    if (IsFocused() && m_show_cursor && !HasSelection())
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

    // 下划线绘制 (保持不变)
    float line_y = draw_y + font_size + 2.0f;
    int cycle_head = static_cast<int>(std::floor(m_anim_head));
    int cycle_tail = static_cast<int>(std::floor(m_anim_tail));
    float ratio_head = m_anim_head - cycle_head;
    float ratio_tail = m_anim_tail - cycle_tail;

    auto DrawSegment = [&](float r_start, float r_end)
    {
        const float clip_min = 0.01f;
        const float clip_max = 0.99f;
        const float clip_range = clip_max - clip_min;
        auto Remap = [&](float val) -> float
        {
            if (val <= clip_min) return 0.0f;
            if (val >= clip_max) return 1.0f;
            return (val - clip_min) / clip_range;
        };
        float mapped_start = Remap(r_start);
        float mapped_end = Remap(r_end);
        if (mapped_end <= mapped_start + 0.001f) return;
        TechUtils::DrawHardLine(draw_list,
            ImVec2(start_x + width * mapped_start, line_y),
            ImVec2(start_x + width * mapped_end, line_y),
            border_col, 1.0f);
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
    // 调用基类处理通用交互 (hovered, clicked, focus request)
    UIElement::HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);

    // 如果在该控件上发生点击
    if (m_clicked && mouse_clicked)
    {
        RequestFocus();
        CaptureMouse();

        float click_local_x = mouse_pos.x - m_absolute_pos.x;
        int click_idx = GetCharIndexAt(click_local_x);

        ImGuiIO& io = ImGui::GetIO();

        // [新增] 处理 Shift 点击
        if (io.KeyShift)
        {
            // 按住 Shift：更新光标位置，但保留之前的锚点
            // 如果之前没有选择(anchor == cursor)，则锚点自然就是之前的 cursor
            m_cursor_pos = click_idx;
        }
        else
        {
            // 普通点击：重置锚点到当前位置
            m_cursor_pos = click_idx;
            m_selection_anchor = click_idx;
        }

        m_cursor_blink_timer = 0.0f;
        m_show_cursor = true;
    }
    if (mouse_released)
    {
        // 只有当自己拥有捕获权时才释放，避免干扰其他逻辑（虽然通常 ReleaseCapture 会处理）
        if (UIContext::Get().m_captured_element == this)
        {
            ReleaseMouse();
        }
    }
}

bool InputField::HandleKeyboardEvent()
{
    if (!IsFocused() || !m_target_string) return false;

    ImGuiIO& io = ImGui::GetIO();
    bool handled = false;

    bool is_ctrl_a = (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A));

    if (is_ctrl_a)
    {
        SelectAll();
        handled = true;
    }

    // 字符输入
    if (!io.InputQueueCharacters.empty())
    {
        for (int i = 0; i < io.InputQueueCharacters.Size; i++)
        {
            unsigned int c = (unsigned int)io.InputQueueCharacters[i];
            if (c >= 32)
            {
                // [新增] 输入字符时，如果有选区，先删除选区
                if (HasSelection()) DeleteSelection();

                m_target_string->insert(m_cursor_pos, 1, (char)c);
                m_cursor_pos++;
                // 此时无选区，同步锚点
                ResetSelection();
                handled = true;
            }
        }
        io.InputQueueCharacters.clear();
    }

    // Backspace
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace))
    {
        if (HasSelection())
        {
            // 有选区：删除选区
            DeleteSelection();
            handled = true;
        }
        else if (m_cursor_pos > 0)
        {
            // 无选区：删除前一个字符
            m_target_string->erase(m_cursor_pos - 1, 1);
            m_cursor_pos--;
            ResetSelection(); // 确保同步
            handled = true;
        }
    }

    // Delete
    if (ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        if (HasSelection())
        {
            DeleteSelection();
            handled = true;
        }
        else if (m_cursor_pos < m_target_string->length())
        {
            m_target_string->erase(m_cursor_pos, 1);
            handled = true;
        }
    }

    // [修改] 方向键处理，支持 Shift 选择
    auto MoveCursor = [&](int delta)
    {
        int old_pos = m_cursor_pos;
        m_cursor_pos = std::clamp(m_cursor_pos + delta, 0, (int)m_target_string->length());

        if (io.KeyShift)
        {
            // 按住 Shift：锚点不动，只动光标
        }
        else
        {
            // 没按 Shift：如果是取消选区，通常行为是：
            // 左移：光标跳到选区左端；右移：光标跳到选区右端 (标准编辑器行为)
            // 这里简化为：直接移动光标并重置锚点
            if (HasSelection())
            {
                // 如果之前有选区，按左右键通常是取消选区并跳到对应一端
                // 简单起见，我们直接让光标动，并把锚点拉过来
                m_selection_anchor = m_cursor_pos;
            }
            else
            {
                m_selection_anchor = m_cursor_pos;
            }
        }
        return old_pos != m_cursor_pos;
    };

    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
    {
        if (HasSelection() && !io.KeyShift)
        {
            // 有选区且未按Shift：向左取消选区，光标停在选区最左侧
            int s, e; GetSelectionBounds(s, e);
            m_cursor_pos = s;
            m_selection_anchor = s;
            handled = true;
        }
        else
        {
            if (MoveCursor(-1)) handled = true;
        }
        m_cursor_blink_timer = 0.0f; m_show_cursor = true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
    {
        if (HasSelection() && !io.KeyShift)
        {
            // 有选区且未按Shift：向右取消选区，光标停在选区最右侧
            int s, e; GetSelectionBounds(s, e);
            m_cursor_pos = e;
            m_selection_anchor = e;
            handled = true;
        }
        else
        {
            if (MoveCursor(1)) handled = true;
        }
        m_cursor_blink_timer = 0.0f; m_show_cursor = true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Home))
    {
        m_cursor_pos = 0;
        if (!io.KeyShift) ResetSelection();
        handled = true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_End))
    {
        m_cursor_pos = m_target_string->length();
        if (!io.KeyShift) ResetSelection();
        handled = true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        UIContext::Get().ClearFocus();
        ResetSelection(); // 失去焦点清理选区
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