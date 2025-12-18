#pragma once
#include "ui_framework.h"
#include "UIFactory.h"
#include "nlohmann/json.hpp" // Or your preferred JSON library
#include <string>
#include <unordered_map>
#include <functional>
#include "neural_ui.h" 
_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN
using json = nlohmann::json;

class UILoader
{
public:
    using ActionCallback = std::function<void()>;

    // 加载UI树
    std::shared_ptr<UIRoot> LoadFromFile(const std::string& filepath);

    // 注册事件处理逻辑
    void RegisterAction(const std::string& action_id, ActionCallback callback);

private:
    std::shared_ptr<UIElement> ParseElement(const json& node);

    // 属性解析辅助函数
    void ApplyProperties(UIElement* element, const json& props_node);

    // 复杂类型解析
    Length ParseLength(const json& node);
    StyleColor ParseColor(const json& node);
    Alignment ParseAlignment(const json& node);
    ImFont* ParseFont(const std::string& font_name);
    ImTextureID ParseTexture(const std::string& texture_name); // Placeholder for texture loading
    AnchorPoint ParseAnchorPoint(const json& node);
    TechButton::Style ParseTechButtonStyle(const json& node);
    TechTextAnimMode ParseTechTextAnimMode(const json& node);
    TechTextSizingMode ParseTechTextSizingMode(const json& node);

    std::unordered_map<std::string, ActionCallback> m_actions;
};

_UI_END
_SYSTEM_END
_NPGS_END