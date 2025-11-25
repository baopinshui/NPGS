#pragma once
#include "../ui_framework.h"
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
    bool HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release) override;
    void Draw(ImDrawList* dl) override;
};


_UI_END
_SYSTEM_END
_NPGS_END