#pragma once
#include "../ui_framework.h"
#include <string>
#include <variant>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

enum class UnderlineDisplayMode
{
    Never,
    OnHoverOrFocus,
    Always
};

class InputField : public UIElement
{
public:
    using OnChangeCallback = std::function<void(const std::string&)>;

    std::string* m_target_string = nullptr;
    OnChangeCallback on_change;

    // 视觉
    std::optional<ImVec4> m_bg_color;
    ImVec4 m_text_color;
    ImVec4 m_border_color;

    // 状态
    int m_cursor_pos = 0;
    float m_cursor_blink_timer = 0.0f;
    bool m_show_cursor = true;

    UnderlineDisplayMode m_underline_mode = UnderlineDisplayMode::Always;

    // [新增] 下划线动画相关的状态变量
    // 想象圆上的两个点，它们只能逆时针（数值增加）运动
    float m_anim_head = 0.0f; // 点 A (前端，负责延伸)
    float m_anim_tail = 0.0f; // 点 B (后端，负责擦除)
    int m_activation_count = 0; // 记录触发了多少次“显示”，用于计算目标相位
    bool m_last_show_state = false; // 上一帧是否应该显示

    InputField(std::string* target);
    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* draw_list) override;
    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled) override;
    bool HandleKeyboardEvent() override;
    ImFont* GetFont() const override;
};

_UI_END
_SYSTEM_END
_NPGS_END