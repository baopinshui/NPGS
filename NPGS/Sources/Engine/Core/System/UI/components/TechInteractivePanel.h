#pragma once
#include "../ui_framework.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

/**
 * @class TechInteractivePanel
 * @brief 一个具备交互功能的垂直布局容器 (VBox)。
 *
 * 这个组件继承自 VBox，拥有其所有的自动垂直布局能力。
 * 同时，它增加了背景色悬停高亮和点击事件回调功能，
 * 非常适合用作列表中的可点击项目。
 */
    class TechInteractivePanel : public VBox
{
public:
    // --- 样式属性 ---
    StyleColor m_color_bg;         // 静止时的背景色
    StyleColor m_color_bg_hover;   // 鼠标悬停时的背景色
    StyleColor m_color_bg_pressed; // 鼠标按下时的背景色
    float m_anim_speed = 5.0f;     // 颜色过渡动画的速度

public:
    TechInteractivePanel();

    // --- 核心生命周期函数重写 ---
    void Update(float dt) override;
    void Draw(ImDrawList* dl) override;
    // Measure 和 Arrange 函数直接继承自 VBox，无需重写

private:
    float m_hover_progress = 0.0f; // 用于动画插值 (0.0: 静止, 1.0: 悬停)
};

_UI_END
_SYSTEM_END
_NPGS_END