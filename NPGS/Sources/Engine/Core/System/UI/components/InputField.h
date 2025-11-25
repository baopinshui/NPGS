#pragma once
#include "../ui_framework.h"
#include <string>
#include <variant>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// InputField 类的声明
// (从旧的 neural_ui.h 中剪切并粘贴到这里)
class InputField : public UIElement
{
public:
    // 使用 variant 来支持不同类型的回调
    using OnChangeCallback = std::function<void(const std::string&)>;

    std::string* m_target_string = nullptr;
    OnChangeCallback on_change;

    // 视觉
    std::optional<ImVec4> m_bg_color;
    ImVec4 m_text_color  ;
    ImVec4 m_border_color;

    // 状态
    int m_cursor_pos = 0;
    float m_cursor_blink_timer = 0.0f;
    bool m_show_cursor = true;

    InputField(std::string* target);
    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* draw_list) override;
    bool HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released) override;
    bool HandleKeyboardEvent() override; // 核心：处理键盘输入
    ImFont* GetFont() const override;
};

_UI_END
_SYSTEM_END
_NPGS_END