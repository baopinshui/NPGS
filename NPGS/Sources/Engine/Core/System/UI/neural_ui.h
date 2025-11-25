#pragma once

// 包含基础框架
#include "ui_framework.h"

// 包含所有拆分出去的组件
#include "components/NeuralParticleView.h"
#include "components/TechBorderPanel.h"
#include "components/CollapsedMainButton.h"
#include "components/NeuralButton.h"
#include "components/TechSliders.h"
#include "components/HackerTextHelper.h"
#include "components/InputField.h"
#include "components/PulsarButton.h"
_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// 只保留 NeuralMenuController 的声明
class NeuralMenuController
{
public:
    // 构造函数保持不变
    NeuralMenuController();

    // AddLinear 和 AddThrottle 保持不变
    template<typename T>
    void AddLinear(const std::string& name, T* ptr, T min, T max)
    {
        auto slider = std::make_shared<LinearTechSlider<T>>(name, ptr, min, max);
        layout_container->AddChild(slider);
    }

    template<typename T>
    void AddThrottle(const std::string& name, T* ptr, T feature_a = 0)
    {
        auto slider = std::make_shared<ThrottleTechSlider<T>>(name, ptr, feature_a);
        layout_container->AddChild(slider);
    }

    // [修改] 提供一个获取根面板的接口
    std::shared_ptr<TechBorderPanel> GetRootPanel() const { return root_panel; }

private:
    // [修改] 移除 UpdateAndDraw 方法，它的职责已移交 UIRoot
    void ToggleExpand();

    // 成员变量保持不变
    std::shared_ptr<TechBorderPanel> root_panel;
    std::shared_ptr<NeuralParticleView> bg_view;
    std::shared_ptr<UIElement> content_container;
    std::shared_ptr<CollapsedMainButton> collapsed_btn;
    std::shared_ptr<ScrollView> layout_container;

    bool m_expanded = false;
    ImVec2 m_collapsed_size = { 80, 80 };
    ImVec2 m_expanded_size = { 320, 256 };
};

_UI_END
_SYSTEM_END
_NPGS_END