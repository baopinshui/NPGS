#pragma once
#include "../ui_framework.h"
#include <string>
#include <variant>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class NeuralButton : public UIElement
{
public:
    std::string text;
    std::function<void()> on_click_callback;

    // [新增] 动画进度 0.0(常态) -> 1.0(悬停)
    float m_hover_progress = 0.0f;

    NeuralButton(const std::string& t);

    // [新增] 覆盖 Update 以处理渐变动画
    void Update(float dt, const ImVec2& parent_abs_pos) override;

    bool HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release) override;
    void Draw(ImDrawList* dl) override;

    void ResetInteraction() override;
};

_UI_END
_SYSTEM_END
_NPGS_END