#pragma once
#include "../ui_framework.h"
#include <string>
#include <variant>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class TechBorderPanel : public Panel
{
public:
    float m_thickness = 2.0f;
    TechBorderPanel();

    // [新增] 重写事件处理，确保视觉上的 Hover 状态正确
    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled) override;

    void Draw(ImDrawList* draw_list) override;
};

_UI_END
_SYSTEM_END
_NPGS_END