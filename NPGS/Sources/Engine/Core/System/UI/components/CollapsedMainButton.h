// --- START OF FILE CollapsedMainButton.h ---

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
    CollapsedMainButton(const std::string& key1, const std::string& key2);

    // [MODIFIED] 重写 Update 和 Measure
    void Update(float dt, const ImVec2& parent_abs_pos) override;
    ImVec2 Measure(const ImVec2& available_size) override;

    // Draw 和 HandleMouseEvent 保持不变，因为它们不直接参与布局计算
    void HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release, bool& handled) override;
    void Draw(ImDrawList* dl) override;

private:
    std::shared_ptr<TechText> m_symbol;
    std::shared_ptr<VBox> m_layout_vbox; // 类型改为 VBox 以便访问特定属性
};


_UI_END
_SYSTEM_END
_NPGS_END