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

    // 计算目标尺寸和当前的对齐策略
    void CalculateLayoutInfo(const std::string& key, ImVec2& out_size, PivotState& out_pivot);

    // [新增] 提交新Key到显示组件（重置特效等）
    void CommitKey(const std::string& key, bool force_restart = true);
    State m_state = State::Hidden;
    std::string m_current_key;      // 当前逻辑上的Key

    // [新增] 类似 PulsarButton 的 Pending 机制
    std::string m_pending_key;
    bool m_has_pending_key = false;

    // 目标状态
    ImVec2 m_target_size = { 0, 0 };
    PivotState m_current_pivot; // 当前正在使用的对齐方式

    // 内部组件
    std::shared_ptr<TechBorderPanel> m_panel;
    std::shared_ptr<TechText> m_text_label;

    // [关键] 统一边距配置：改为 12.0f 或你喜欢的数值
    const float m_padding = 12.0f;
};

_UI_END
_SYSTEM_END
_NPGS_END