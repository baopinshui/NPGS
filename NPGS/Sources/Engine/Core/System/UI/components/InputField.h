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
    OnChangeCallback on_commit;
    // 视觉
    StyleColor m_text_color;
    StyleColor m_border_color;

    // 状态
    int m_cursor_pos = 0;
    int m_selection_anchor = 0;

    float m_cursor_blink_timer = 0.0f;
    bool m_show_cursor = true;

    UnderlineDisplayMode m_underline_mode = UnderlineDisplayMode::Always;

    // 下划线动画相关的状态变量
    float m_anim_head = 0.0f;
    float m_anim_tail = 0.0f;
    int m_activation_count = 0;
    bool m_last_show_state = false;
    bool m_last_frame_focused = false;

    InputField(std::string* target);
    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* draw_list) override;
    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled) override;
    bool HandleKeyboardEvent() override;
    ImFont* GetFont() const override;

    // [新增] 辅助函数
    bool HasSelection() const;
    void GetSelectionBounds(int& start, int& end) const;
    void DeleteSelection();
    void SelectAll();
    void ResetSelection(); // 清除选择，将anchor设为cursor
    int GetCharIndexAt(float local_x); // 计算鼠标位置对应的字符索引
};

_UI_END
_SYSTEM_END
_NPGS_END