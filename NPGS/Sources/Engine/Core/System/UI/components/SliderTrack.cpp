// --- START OF FILE SliderTrack.cpp ---
#include "SliderTrack.h"
#include "../TechUtils.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

SliderTrack::SliderTrack()
{
    // 默认行为：水平填满，高度固定
    SetHeight(Length::Fix(16.0f));
    SetWidth(Length::Fill());

    m_block_input = true;
    m_track_color = StyleColor(ThemeColorID::Accent).WithAlpha(0.5f);
    m_grab_color = ThemeColorID::Accent;
}

void SliderTrack::Update(float dt, const ImVec2& parent_abs_pos)
{
    // 如果鼠标释放，且不是被自己捕获，则停止拖拽
    if (m_is_dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (UIContext::Get().m_captured_element == this)
        {
            ReleaseMouse();
        }
        m_is_dragging = false;
    }

    UIElement::Update(dt, parent_abs_pos);
}

void SliderTrack::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    // --- Color Logic ---
    const auto& theme = UIContext::Get().m_theme;
    ImVec4 accent_vec4 = m_grab_color.Resolve();

    if (m_is_rgb_mode)
    {
        // This is a simplification. A real implementation might need the label text.
        // For now, let's assume it's just a generic accent tint.
    }

    ImU32 col_accent = GetColorWithAlpha(accent_vec4, 1.0f);

    // --- Track Drawing ---
    float y_center = m_absolute_pos.y + m_rect.h * 0.5f;
    float track_h = 1.0f;
    dl->AddRectFilled(
        ImVec2(m_absolute_pos.x, y_center - track_h * 0.5f),
        ImVec2(m_absolute_pos.x + m_rect.w, y_center + track_h * 0.5f),
        GetColorWithAlpha(m_track_color.Resolve(), 1.0f)
    );

    // --- Grabber Drawing ---
    float grab_x = m_absolute_pos.x + m_rect.w * m_normalized_value;
    float grab_w = 6.0f;
    float grab_h = 14.0f;

    if (m_is_dragging)
    {
        grab_w = 8.0f;
        grab_h = 16.0f;
        col_accent = GetColorWithAlpha(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f);
    }
    else if (m_hovered)
    {
        accent_vec4.w *= 0.9f;
        col_accent = GetColorWithAlpha(accent_vec4, 1.0f);
    }

    dl->AddRectFilled(
        ImVec2(grab_x - grab_w * 0.5f, y_center - grab_h * 0.5f),
        ImVec2(grab_x + grab_w * 0.5f, y_center + grab_h * 0.5f),
        col_accent
    );
}

void SliderTrack::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled)
{
    // 先调用基类处理 hover, focus 等基础状态
    UIElement::HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);

    if (handled && this != UIContext::Get().m_captured_element)
    {
        // 如果事件被其他组件消耗了 (比如输入框)，而且不是我们自己捕获的，那就什么都不做
        m_is_dragging = false;
        return;
    }

    if (m_is_dragging)
    {
        float rel_x = (mouse_pos.x - m_absolute_pos.x) / m_rect.w;
        float new_val = std::clamp(rel_x, 0.0f, 1.0f);
        if (std::abs(new_val - m_normalized_value) > 0.001f)
        {
            SetValue(new_val);
            if (on_value_changed) on_value_changed(m_normalized_value);
        }
        handled = true; // 拖拽时消耗事件
    }

    if (m_clicked && mouse_clicked)
    {
        m_is_dragging = true;
        CaptureMouse();
        // 立即响应点击位置
        float rel_x = (mouse_pos.x - m_absolute_pos.x) / m_rect.w;
        SetValue(std::clamp(rel_x, 0.0f, 1.0f));
        if (on_value_changed) on_value_changed(m_normalized_value);
        handled = true;
    }

    if (mouse_released)
    {
        if (m_is_dragging)
        {
            ReleaseMouse();
            m_is_dragging = false;
        }
    }
}

_UI_END
_SYSTEM_END
_NPGS_END