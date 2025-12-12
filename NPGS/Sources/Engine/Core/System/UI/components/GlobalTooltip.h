// --- START OF FILE GlobalTooltip.h ---

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
    ImVec2 Measure(const ImVec2& available_size) override;
    // Draw 使用基类逻辑

private:
    enum class State
    {
        Hidden,
        Expanding,
        Visible,
        Collapsing
    };

    void Show(const std::string& key);
    void Hide();

    State m_state = State::Hidden;
    std::string m_current_key;      // 当前逻辑上的Key

    // 目标状态
    ImVec2 m_target_size = { 0, 0 };

    // [核心修改] 使用可动画的浮点值替代布尔状态
    ImVec2 m_target_pivot = { 0, 0 };         // 目标翻转状态 (0=正常, 1=翻转)
    ImVec2 m_current_pivot_lerp = { 0, 0 };   // 当前用于平滑过渡的插值 (0.0 -> 1.0)


    // 内部组件
    std::shared_ptr<TechBorderPanel> m_panel;
    std::shared_ptr<TechText> m_text_label;

    float m_max_width = 682.0f;

    const float m_padding = 12.0f;
};

_UI_END
_SYSTEM_END
_NPGS_END