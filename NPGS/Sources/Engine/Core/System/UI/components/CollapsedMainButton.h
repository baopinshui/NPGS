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

    // [修改] 重写新的生命周期函数
    void Update(float dt) override;
    ImVec2 Measure(ImVec2 available_size) override;
    // Arrange 使用基类默认实现即可，因为它会根据 alignment 正确居中子元素
    void Draw(ImDrawList* dl) override;
    void HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release, bool& handled) override;

private:
    std::shared_ptr<TechText> m_symbol;
    std::shared_ptr<UIElement> m_layout_vbox;
};

_UI_END
_SYSTEM_END
_NPGS_END