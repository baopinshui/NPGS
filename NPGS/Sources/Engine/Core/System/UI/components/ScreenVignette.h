#pragma once
#include "../ui_framework.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// A purely visual component that draws a dark, feathered border around the screen.
// This helps improve UI contrast against bright backgrounds.
class ScreenVignette : public UIElement
{
public:
    // The color of the vignette at the screen edges. Alpha is used for intensity.
    ImVec4 m_color = { 0.0f, 0.0f, 0.0f, 0.85f };

    // The size of the gradient falloff, from 0.0 (hard edge) to 1.0 (gradient starts from the center).
    float m_feather = 0.2f;

    ScreenVignette();

    // The vignette is drawn fullscreen, ignoring parent layout.
    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* dl) override;
};

_UI_END
_SYSTEM_END
_NPGS_END