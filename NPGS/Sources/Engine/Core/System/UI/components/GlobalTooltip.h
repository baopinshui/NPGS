#pragma once

#include "../ui_framework.h"
#include "TechBorderPanel.h"
#include "TechText.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class GlobalTooltip : public UIElement
{
public:
    GlobalTooltip();

    void Update(float dt, const ImVec2& parent_abs_pos) override;
    // Draw 使用基类逻辑

private:
    enum class State
    {
        Hidden,
        Expanding,
        Visible,
        Collapsing
    };

    // 用于记录当前的对齐策略（象限）
    struct PivotState
    {
        bool flip_x = false; // false=右侧(正常), true=左侧
        bool flip_y = false; // false=下侧(正常), true=上侧

        bool operator!=(const PivotState& other) const
        {
            return flip_x != other.flip_x || flip_y != other.flip_y;
        }
    };

    void Show(const std::string& key);
    void Hide();

    State m_state = State::Hidden;
    std::string m_current_key;      // 当前逻辑上的Key

    // 目标状态
    ImVec2 m_target_size = { 0, 0 };
    PivotState m_current_pivot;

    // 内部组件
    std::shared_ptr<TechBorderPanel> m_panel;
    std::shared_ptr<TechText> m_text_label;

    const float m_padding = 12.0f;
};

_UI_END
_SYSTEM_END
_NPGS_END