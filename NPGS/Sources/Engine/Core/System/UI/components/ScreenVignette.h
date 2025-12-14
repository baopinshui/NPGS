#pragma once
#include "../ui_framework.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// 一个纯视觉组件，在屏幕周围绘制一个深色的羽化边框。
// 这有助于提高UI在明亮背景下的对比度。
class ScreenVignette : public UIElement
{
public:
    // Vignette 在屏幕边缘的颜色。Alpha 用于控制强度。
    ImVec4 m_color = { 0.0f, 0.0f, 0.0f, 0.85f };

    // 渐变衰减的大小，从 0.0 (硬边缘) 到 1.0 (渐变从中心开始)。
    float m_feather = 0.2f;

    ScreenVignette();

    // [MODIFIED] 与新的布局系统集成
    void Update(float dt) override;
    // Measure 和 Arrange 使用基类默认实现即可
    void Draw(ImDrawList* dl) override;
};

_UI_END
_SYSTEM_END
_NPGS_END