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
    // Draw is handled by base class calling children

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
    void CalculateTargetLayout(const std::string& key);

    State m_state = State::Hidden;
    std::string m_current_key;

    // 目标布局（用于检测是否需要重定位）
    ImVec2 m_target_pos = { 0, 0 };
    ImVec2 m_target_size = { 0, 0 };

    // 内部组件
    std::shared_ptr<TechBorderPanel> m_panel;
    std::shared_ptr<TechText> m_text_label;
};

_UI_END
_SYSTEM_END
_NPGS_END