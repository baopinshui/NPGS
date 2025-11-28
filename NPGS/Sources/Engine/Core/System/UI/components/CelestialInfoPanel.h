#pragma once
#include "../ui_framework.h"
#include "TechBorderPanel.h"
#include "TechButton.h"
#include "TechProgressBar.h"
#include <vector>
#include <string>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// 这是一个容器级组件，负责管理右侧栏的所有子元素和动画逻辑
class CelestialInfoPanel : public UIElement
{
public:
    CelestialInfoPanel();

    // 重写 Update，在基类递归更新子节点之前，计算子节点的动态位置
    void Update(float dt, const ImVec2& parent_abs_pos) override;

private:
    void ToggleCollapse();
    void RebuildDataList(); // 构建内部数据列表

    // 子组件引用 (用于在Update中操作它们)
    std::shared_ptr<TechBorderPanel> m_main_panel;
    std::shared_ptr<TechButton> m_collapsed_btn;
    std::shared_ptr<ScrollView> m_scroll_view;
    std::shared_ptr<VBox> m_content_vbox;

    // 状态
    bool m_is_collapsed = false;
    float m_anim_progress = 0.0f; // 0.0 (展开) -> 1.0 (收起)

    // 布局常量
    const float PANEL_WIDTH = 320.0f;
    const float PANEL_HEIGHT = 450.0f;
    const float TOP_MARGIN = 150.0f;
    const float RIGHT_MARGIN = 20.0f;
};

_UI_END
_SYSTEM_END
_NPGS_END