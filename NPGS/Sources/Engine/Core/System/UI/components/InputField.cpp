#include "InputField.h"
#include "../TechUtils.h"
_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN
// [修改] 重写 GetFont，强制使用 Monospace
ImFont* InputField::GetFont() const
{
    // 1. 如果用户手动给这个 InputField 设置了特定字体 (m_font)，优先级最高
    if (m_font) return m_font;

    // 2. 默认情况：InputField 必须用等宽字体
    // 注意：你之前的代码里写的是 m_font_bold，这会导致错位，必须用 m_font_mono
    // 请确保 UIContext 也就是 ui_framework.h/cpp 里加载了这个字体
    ImFont* mono = UIContext::Get().m_font_bold;

    // 3. 兜底
    if (mono) return mono;
    return UIElement::GetFont(); // 回退到基类逻辑
}
// ... in InputField.cpp
InputField::InputField(std::string* target) : m_target_string(target)
{
    m_block_input = true;
    m_focusable = true;
    m_rect.h = 20.0f;
    if (m_target_string) m_cursor_pos = m_target_string->length();

    // 从UITheme初始化颜色
    const auto& theme = UIContext::Get().m_theme;
    m_text_color = theme.color_text;
    m_border_color = theme.color_border;
}
void InputField::Update(float dt, const ImVec2& parent_abs_pos)
{
    UIElement::Update(dt, parent_abs_pos);
    if (IsFocused())
    {
        m_cursor_blink_timer += dt;
        if (m_cursor_blink_timer > 1.0f) m_cursor_blink_timer = 0.0f;
        m_show_cursor = m_cursor_blink_timer < 0.5f;
    }
}

void InputField::Draw(ImDrawList* draw_list)
{
    if (!m_visible || m_alpha <= 0.01f) return;
    ImFont* font = GetFont();
    ImGui::PushFont(font);
    // --- 1. 获取关键数据 ---
    float font_size = ImGui::GetFontSize(); // 自适应字体高度

    // [修改] 顶对齐：直接使用组件的绝对 Y 坐标，不计算居中偏移
    float draw_y = m_absolute_pos.y;

    // --- 2. 准备颜色 ---
    ImU32 border_col = GetColorWithAlpha(m_border_color, 1.0f);
    ImU32 text_col = GetColorWithAlpha(m_text_color, 1.0f);

    // --- 3. 绘制文本 (顶对齐) ---
    ImVec2 text_pos = ImVec2(m_absolute_pos.x, draw_y);
    if (m_target_string && !m_target_string->empty())
    {
        draw_list->AddText(text_pos, text_col, m_target_string->c_str());
    }

    // --- 4. 绘制光标 (自适应) ---
    if (IsFocused() && m_show_cursor)
    {
        // 确保光标位置不越界
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

    // --- 5. 绘制下划线 (自适应) ---
    // 下划线位置 = 顶部 + 字体高度 + 2像素间距
    // 这样无论字体多大，下划线永远紧跟在文字下方
    float line_y = draw_y + font_size + 2.0f;

    // 使用 AddLine 绘制底线
    TechUtils::DrawHardLine(draw_list,
        ImVec2(m_absolute_pos.x, line_y),
        ImVec2(m_absolute_pos.x + m_rect.w, line_y),
        border_col,
        1.0f
    );
    ImGui::PopFont(); // 必须成对出现
}

void InputField::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled)
{
    // 调用基类处理通用逻辑（如 hover 状态）
     UIElement::HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released,handled);

    if (m_clicked && mouse_clicked)
    {
        RequestFocus();

        // [核心功能]：鼠标点击定位光标
        if (m_target_string && !m_target_string->empty())
        {
            ImFont* font = GetFont();
            ImGui::PushFont(font);

            // 2. 计算鼠标相对于文本起点的偏移
            float click_local_x = mouse_pos.x - m_absolute_pos.x;

            // 如果点在最左边，光标归零
            if (click_local_x <= 0)
            {
                m_cursor_pos = 0;
            }
            else
            {
                // 3. 遍历寻找最近的字符间隙
                // 我们通过比较 "鼠标位置" 和 "字符中心点" 来决定光标在字符前还是字符后
                float best_diff = 99999.0f;
                int best_index = 0;

                // 遍历 0 到 length (包括末尾位置)
                for (int i = 0; i <= m_target_string->length(); ++i)
                {
                    // 获取从开头到当前索引 i 的子字符串宽度
                    std::string sub = m_target_string->substr(0, i);
                    float width = ImGui::CalcTextSize(sub.c_str()).x;

                    float diff = std::abs(click_local_x - width);

                    // 如果当前的距离比之前的更近，更新最佳位置
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
            // 字符串为空，光标只能在 0
            m_cursor_pos = 0;
        }

        // 重置光标闪烁，让用户点击后立刻看到光标
        m_cursor_blink_timer = 0.0f;
        m_show_cursor = true;
    }
}
bool InputField::HandleKeyboardEvent()
{
    if (!IsFocused() || !m_target_string) return false;

    ImGuiIO& io = ImGui::GetIO();
    bool handled = false;

    // 字符输入
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
    // 功能键
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