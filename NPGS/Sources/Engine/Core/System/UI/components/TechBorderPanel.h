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
    void Draw(ImDrawList* draw_list) override;
};


_UI_END
_SYSTEM_END
_NPGS_END