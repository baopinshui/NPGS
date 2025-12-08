#pragma once
#include "../ui_framework.h" 
#include "NeuralParticleView.h"
#include "TechBorderPanel.h"
#include "CollapsedMainButton.h"
#include "TechButton.h"
#include "TechSliders.h"
#include "TechText.h"
#include "TechDivider.h"
#include <memory>
#include <string>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// 从"控制器"重构为"复合组件"
class NeuralMenu : public UIElement
{
public:
    NeuralMenu(
        const std::string& main_button_key1 = "ui.manage",
        const std::string& main_button_key2 = "ui.network",
        const std::string& settings_key = "ui.settings",
        const std::string& close_key = "ui.close_terminal"
    );

    // API 保持不变，用于动态添加滑块
    template<typename T>
    void AddLinear(const std::string& name, T* ptr, T min, T max)
    {
        auto slider = std::make_shared<LinearTechSlider<T>>(name, ptr, min, max);
        content_vbox->AddChild(slider);
    }

    template<typename T>
    void AddThrottle(const std::string& name, T* ptr, T feature_a = 0)
    {
        auto slider = std::make_shared<ThrottleTechSlider<T>>(name, ptr, feature_a);
        content_vbox->AddChild(slider);
    }

    // [新增] 重写 Update，确保子元素的动画被处理
    void Update(float dt, const ImVec2& parent_abs_pos) override;

private:
    void ToggleExpand();

    // 内部子组件引用
    std::shared_ptr<TechBorderPanel> root_panel;
    std::shared_ptr<NeuralParticleView> bg_view;
    std::shared_ptr<VBox> main_layout;
    std::shared_ptr<ScrollView> scroll_view;
    std::shared_ptr<VBox> content_vbox;
    std::shared_ptr<CollapsedMainButton> collapsed_btn;
    // [新增] 标题的引用，用于在展开时触发动画
    std::shared_ptr<TechText> header_title;


    // 状态
    bool m_expanded = false;
    ImVec2 m_collapsed_size = { 80, 80 };
    ImVec2 m_expanded_size = { 340, 300 };
};

_UI_END
_SYSTEM_END
_NPGS_END
