#pragma once
#include "../ui_framework.h"
#include "TechText.h"
#include <string>
#include <variant>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class CollapsedMainButton : public UIElement
{
public:
    std::function<void()> on_click_callback;
    CollapsedMainButton();

    // [修正] 确保重写了 Update，用于处理动画和 Alpha 传递
    void Update(float dt, const ImVec2& parent_abs_pos) override;

    bool HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release) override;

    // [修正] Draw 方法现在是空的，所有绘制由子元素完成
    void Draw(ImDrawList* dl) override;

private:
    std::shared_ptr<TechText> m_symbol;
    std::shared_ptr<UIElement> m_layout_vbox; 
};


_UI_END
_SYSTEM_END
_NPGS_END