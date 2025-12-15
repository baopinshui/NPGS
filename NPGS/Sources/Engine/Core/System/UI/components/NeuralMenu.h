// --- START OF FILE NeuralMenu.h ---

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
#include <vector>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class NeuralMenu : public UIElement
{
public:
    enum class Page { Info, Settings };

    NeuralMenu(
        const std::string& main_button_key1,
        const std::string& main_button_key2,
        const std::string& settings_key,
        const std::string& close_key
    );

    // API 保持不变，用于动态添加滑块 (现在会自动添加到设置页面的容器中)
    template<typename T>
    void AddLinear(const std::string& name, T* ptr, T min, T max)
    {
        // 确保添加到设置页面的滚动视图容器中
        auto slider = std::make_shared<LinearTechSlider<T>>(name, ptr, min, max);
        m_settings_content_box->AddChild(slider);
    }

    template<typename T>
    void AddThrottle(const std::string& name, T* ptr, T feature_a = 0)
    {
        auto slider = std::make_shared<ThrottleTechSlider<T>>(name, ptr, feature_a);
        m_settings_content_box->AddChild(slider);
    }

    void Update(float dt) override;

    // [新增] 获取退出按钮以绑定回调
    std::shared_ptr<TechButton> GetExitButton() { return m_exit_btn; }

private:
    void ToggleExpand();
    void SwitchPage(); // 切换 Info/Settings
    void CreateInfoRow(std::shared_ptr<VBox> parent, const std::string& label, const std::string& value, const StyleColor& val_color = ThemeColorID::Text);

    // 内部子组件引用
    std::shared_ptr<TechBorderPanel> root_panel;
    std::shared_ptr<NeuralParticleView> bg_view;
    std::shared_ptr<UIElement> base; // 展开后的基础容器
    std::shared_ptr<VBox> main_layout;

    // 头部组件
    std::shared_ptr<HBox> header_box;
    std::shared_ptr<TechText> header_title;
    std::shared_ptr<TechButton> page_toggle_btn; // INFO / CUSTOM 切换按钮

    // 页面容器
    std::shared_ptr<VBox> m_page_info_container;
    std::shared_ptr<VBox> m_page_settings_container;
    std::shared_ptr<VBox> m_settings_content_box; // 设置页内的滚动内容容器

    // 底部组件
    std::shared_ptr<TechButton> m_close_menu_btn; // 关闭菜单按钮
    std::shared_ptr<TechButton> m_exit_btn;       // [新增] 退出游戏按钮

    std::shared_ptr<CollapsedMainButton> collapsed_btn;

    // 状态
    bool m_expanded = false;
    Page m_current_page = Page::Info;
    ImVec2 m_collapsed_size = { 80, 80 };
    ImVec2 m_expanded_size = { 340, 320 }; //稍微加高一点以容纳更多内容
};

_UI_END
_SYSTEM_END
_NPGS_END