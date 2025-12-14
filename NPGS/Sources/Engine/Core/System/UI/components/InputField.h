#pragma once
#include "../ui_framework.h"
#include <string>
#include <variant>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

enum class UnderlineDisplayMode { Never, OnHoverOrFocus, Always };

class InputField : public UIElement
{
public:
    using OnChangeCallback = std::function<void(const std::string&)>;

    std::string* m_target_string = nullptr;
    OnChangeCallback on_change;
    OnChangeCallback on_commit;

    StyleColor m_text_color;
    StyleColor m_border_color;

    int m_cursor_pos = 0;
    int m_selection_anchor = 0;
    float m_cursor_blink_timer = 0.0f;
    bool m_show_cursor = true;

    UnderlineDisplayMode m_underline_mode = UnderlineDisplayMode::Always;

    // [新增] 下划线与文本的间距
    float m_underline_spacing = 2.0f;

    // 下划线动画状态 (保持不变)
    float m_anim_head = 0.0f;
    float m_anim_tail = 0.0f;
    int m_activation_count = 0;
    bool m_last_show_state = false;
    bool m_last_frame_focused = false;

    InputField(std::string* target);

    // --- [核心修改] 重写新的生命周期函数 ---
    void Update(float dt) override;
    ImVec2 Measure(ImVec2 available_size) override;
    // Arrange 使用基类默认实现
    void Draw(ImDrawList* draw_list) override;

    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled) override;
    bool HandleKeyboardEvent() override;
    ImFont* GetFont() const override;

    // 辅助函数 (保持不变)
    bool HasSelection() const;
    void GetSelectionBounds(int& start, int& end) const;
    void DeleteSelection();
    void SelectAll();
    void ResetSelection();
    int GetCharIndexAt(float local_x);
};

_UI_END
_SYSTEM_END
_NPGS_END