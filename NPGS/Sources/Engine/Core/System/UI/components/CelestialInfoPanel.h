// --- START OF FILE CelestialInfoPanel.h ---

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

// --- 数据结构定义 ---
struct InfoItem
{
    std::string key;
    std::string value;
    bool highlight = false; // 是否高亮显示
};

struct InfoGroup
{
    std::vector<InfoItem> items;
};

struct InfoPage
{
    std::string name; // Tab 名字，如 "PHYSICS"
    std::vector<InfoGroup> groups;
};

// 完整的数据包
using CelestialData = std::vector<InfoPage>;

// 这是一个容器级组件，负责管理右侧栏的所有子元素和动画逻辑
class CelestialInfoPanel : public UIElement
{
public:
    // [修改] 构造函数增加 closetext 参数
    CelestialInfoPanel(
        const std::string& fold_key,
        const std::string& close_key,
        const std::string& progress_label_key,
        const std::string& coil_label_key
    );
    void SetData(const CelestialData& data);

    void SetObjectImage(ImTextureID texture_id, float img_w = 0.0f, float img_h = 0.0f, ImVec4 Col = { 1.0f,1.0f,1.0f,1.0f });

    void SetTitle(const std::string& title, const std::string& subtitle = "");

    // [修改] 更新为新的生命周期函数
    void Update(float dt) override;
    ImVec2 Measure(ImVec2 available_size) override;
    void Arrange(const Rect& final_rect) override;

    // 布局常量
    const float PANEL_WIDTH = 320.0f;
    const float PANEL_HEIGHT = 650.0f;
    // [修改] 边距常量由 Anchor 系统处理，可以移除
    // float TOP_MARGIN = 150.0f;
    // float RIGHT_MARGIN = 20.0f;

private:
    void ToggleCollapse();
    void SelectTab(int index);   // 切换 Tab
    void RefreshContent();       // 根据当前 Tab 刷新列表

    std::shared_ptr<TechText> m_title_text; // 保存标题组件指针
    std::shared_ptr<TechText> m_subtitle_text; // 保存副标题组件指针

    std::shared_ptr<Image> m_preview_image;

    // 子组件引用 (用于在Update中操作它们)
    std::shared_ptr<TechBorderPanel> m_main_panel;
    std::shared_ptr<TechButton> m_collapsed_btn;
    // 保存容器指针以便动态修改
    std::shared_ptr<HBox> m_tabs_container;    // 存放 Tab 按钮
    std::shared_ptr<ScrollView> m_scroll_view; // [新增] 保存 ScrollView 引用
    std::shared_ptr<VBox> m_content_vbox;      // 存放滚动内容

    // 数据状态
    CelestialData m_current_data;
    int m_current_tab_index = 0;

    // 状态
    bool m_is_collapsed = true;   // [修改] 默认为收起状态
    float m_anim_progress = 1.0f; // [修改] 1.0 表示完全收起 (初始状态)
};

_UI_END
_SYSTEM_END
_NPGS_END