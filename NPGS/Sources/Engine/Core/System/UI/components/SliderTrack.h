#pragma once
#include "../ui_framework.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// 这是一个纯粹的交互式轨道组件
class SliderTrack : public UIElement
{
public:
    using OnChangeCallback = std::function<void(float normalized_value)>;

    // --- Data ---
    float m_normalized_value = 0.0f; // 0.0 to 1.0
    OnChangeCallback on_value_changed;

    // --- Visuals ---
    StyleColor m_track_color;
    StyleColor m_grab_color;
    bool m_is_rgb_mode = false; // 特殊模式，影响颜色

private:
    bool m_is_dragging = false;

public:
    SliderTrack();

    // API
    void SetValue(float normalized_value) { m_normalized_value = std::clamp(normalized_value, 0.0f, 1.0f); }

    // Overrides
    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* dl) override;
    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled) override;
};

_UI_END
_SYSTEM_END
_NPGS_END