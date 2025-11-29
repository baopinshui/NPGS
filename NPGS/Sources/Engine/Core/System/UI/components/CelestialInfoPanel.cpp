#include "CelestialInfoPanel.h"
#include "../TechUtils.h"
#include "TechText.h"
#include "TechDivider.h"
#include "TechButton.h"      // 引入统一的 TechButton
#include "TechProgressBar.h" // 引入进度条组件

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

CelestialInfoPanel::CelestialInfoPanel()
{
    // 自身作为容器，不阻挡输入，全透明
    m_rect = { 0, 0, 0, 0 };
    m_visible = true;
    m_block_input = false;

    auto& theme = UIContext::Get().m_theme;

    // =========================================================
    // 1. 创建侧边把手 (Collapsed Tab) - 重构为 TechButton
    // =========================================================
    // 不再需要外层 Panel 套娃，直接使用支持 Vertical 样式和 Glass 效果的按钮
    m_collapsed_btn = std::make_shared<TechButton>("INFO", TechButton::Style::Vertical);
    m_collapsed_btn->m_rect = { 0, 0, 24, 100 }; // 窄条设计
    m_collapsed_btn->SetUseGlass(true);          // 开启毛玻璃背景
    m_collapsed_btn->on_click = [this]() { this->ToggleCollapse(); };

    AddChild(m_collapsed_btn);

    // =========================================================
    // 2. 创建主面板背景
    // =========================================================
    m_main_panel = std::make_shared<TechBorderPanel>();
    m_main_panel->m_rect = { 0, 0, PANEL_WIDTH, PANEL_HEIGHT };
    m_main_panel->m_use_glass_effect = true;
    m_main_panel->m_thickness = 2.0f;
    AddChild(m_main_panel);

    // --- 主面板内部布局容器 ---
    auto main_vbox = std::make_shared<VBox>();
    main_vbox->m_rect = { 15, 15, PANEL_WIDTH - 30, PANEL_HEIGHT - 30 };
    main_vbox->m_padding = 4.0f; // 整体布局紧凑
    m_main_panel->AddChild(main_vbox);

    // =========================================================
    // A. Header 区域 (标题 + 分割线)
    // =========================================================
    {
        auto header_row = std::make_shared<HBox>();
        header_row->m_rect.h = 28.0f; // 固定高度

        // 1. 主标题 (左对齐，大号，高亮)
        m_title_text = std::make_shared<TechText>("No Target");
        m_title_text->m_font = UIContext::Get().m_font_bold;
        m_title_text->SetColor(theme.color_accent);
        m_title_text->m_align_v = Alignment::End; // 底部对齐
        m_title_text->m_rect.w = 120.0f;          // 预留宽度

        // 2. 副标题 (右对齐，小号，灰色)

        m_subtitle_text = std::make_shared<TechText>("");
        m_subtitle_text->SetColor(theme.color_text_disabled);
        m_subtitle_text->m_font = UIContext::Get().m_font_regular;
        m_subtitle_text->m_align_v = Alignment::End; // 底部对齐
        m_subtitle_text->m_align_h = Alignment::End; // 靠右
        m_subtitle_text->m_fill_h = true;            // 填满剩余空间

        header_row->AddChild(m_title_text);
        header_row->AddChild(m_subtitle_text);

        // [修复] 必须添加到主布局中
        main_vbox->AddChild(header_row);

        // 3. Header 下方的分割线
        auto div = std::make_shared<TechDivider>();
        div->m_color = theme.color_accent;
        div->m_rect.h = 8.0f; // 分割线占位高度
        main_vbox->AddChild(div);
    }

    // =========================================================
    // B. Tabs 区域 (实心/透明切换风格)
    // =========================================================
    {
        // 1. 创建横向滚动容器
        auto tabs_scroll_view = std::make_shared<HorizontalScrollView>();
        tabs_scroll_view->m_rect.h = 20.0f; // 滚动区域高度
        tabs_scroll_view->m_block_input = true;

        // 2. 创建 HBox 并保存指针
        m_tabs_container = std::make_shared<HBox>();
        m_tabs_container->m_padding = 4.0f;

        // [关键] 
        // HBox 不再 m_fill_h。它会根据内容自适应宽度。
        m_tabs_container->m_fill_h = false;
        // 高度设置为父容器高度
        m_tabs_container->m_rect.h = 20.0f;

        // 3. 将 HBox 作为子元素添加到滚动视图中
        tabs_scroll_view->AddChild(m_tabs_container);

        // 4. 将滚动视图添加到主布局
        main_vbox->AddChild(tabs_scroll_view);
    }

    // 增加一点垂直间距
    auto spacer = std::make_shared<UIElement>();
    spacer->m_rect.h = 4.0f;
    main_vbox->AddChild(spacer);

    // =========================================================
    // C. 数据滚动区
    // =========================================================
    auto scroll_view = std::make_shared<ScrollView>();
    scroll_view->m_fill_v = true;

    // 创建内容容器并保存指针
    m_content_vbox = std::make_shared<VBox>();
    m_content_vbox->m_padding = 8.0f;

    scroll_view->AddChild(m_content_vbox);
    main_vbox->AddChild(scroll_view);

    // =========================================================
    // D. Footer 区域 (进度条)
    // =========================================================
    {
        auto footer_box = std::make_shared<VBox>();
        footer_box->m_rect.h = 50.0f; // 固定 footer 高度
        footer_box->m_padding = 2.0f;

        // 1. 顶部分割线 (灰色，细一点)
        auto sep = std::make_shared<TechDivider>();
        sep->m_color = theme.color_border;
        sep->m_rect.h = 8.0f;
        footer_box->AddChild(sep);

        // 2. 标题
        auto prog_label = std::make_shared<TechText>(">>> Star Lifter Progress");
        prog_label->SetColor(theme.color_accent);
        prog_label->m_rect.h = 16.0f;
        prog_label->m_font = UIContext::Get().m_font_small;

        // 3. 进度条 (带背景槽的样式在 TechProgressBar 内部实现)
        auto progress = std::make_shared<TechProgressBar>("");
        progress->m_rect.h = 6.0f;
        progress->m_progress = 0.45f;

        // 4. 底部详情 (Main Coil ... 45%)
        auto prog_info = std::make_shared<HBox>();
        prog_info->m_rect.h = 14.0f;

        auto t1 = std::make_shared<TechText>("Main Coil");
        t1->SetColor(theme.color_text_disabled);
        t1->m_font = UIContext::Get().m_font_small;
        t1->m_rect.w = 100.0f;

        auto t2 = std::make_shared<TechText>("45%");
        t2->SetColor(theme.color_text);
        t2->m_font = UIContext::Get().m_font_small; // 建议用等宽字体
        t2->m_fill_h = true;
        t2->m_align_h = Alignment::End;

        prog_info->AddChild(t1);
        prog_info->AddChild(t2);

        footer_box->AddChild(prog_label);
        footer_box->AddChild(progress);
        footer_box->AddChild(prog_info);

        main_vbox->AddChild(footer_box);
    }
}
void CelestialInfoPanel::SetData(const CelestialData& data)
{
    // 1. 更新数据模型
    m_current_data = data;

    // 重置 Tab 索引 (防止越界，如果新数据为空则设为 -1 或 0)
    m_current_tab_index = 0;
    if (m_current_data.empty()) m_current_tab_index = -1;

    // 2. 重建 Tab 按钮
    if (m_tabs_container)
    {
        m_tabs_container->m_children.clear();

        for (int i = 0; i < m_current_data.size(); ++i)
        {
            const auto& page = m_current_data[i];

            auto btn = std::make_shared<TechButton>(page.name, TechButton::Style::Tab);

            // [关键] 按钮不再 fill，而是使用固定宽度
            // 这样 HBox 的总宽度才能正确计算
            btn->m_fill_h = false;
            btn->m_rect.w = 90.0f; // 给每个 Tab 一个固定宽度

            btn->on_click = [this, i]()
            {
                this->SelectTab(i);
            };
            m_tabs_container->AddChild(btn);
        }
    }

    // 3. 刷新选中状态和内容
    SelectTab(m_current_tab_index);
}
void CelestialInfoPanel::SetTitle(const std::string& title, const std::string& subtitle)
{
    if (m_title_text) m_title_text->SetText(title);
    if (m_subtitle_text) m_subtitle_text->SetText(subtitle);
}
void CelestialInfoPanel::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (!m_visible) return;

    ImVec2 display_sz = UIContext::Get().m_display_size;

    // 1. 动画状态机
    float speed = 5.0f * dt;
    if (m_is_collapsed)
        m_anim_progress += speed;
    else
        m_anim_progress -= speed;
    m_anim_progress = std::clamp(m_anim_progress, 0.0f, 1.0f);

    // 2. 计算位置 (EaseInOutQuad 插值)

    // [修改] 展开时：面板右边缘紧贴屏幕右侧 (X = 屏幕宽 - 面板宽)
    // 去掉了之前的 RIGHT_MARGIN
    float expanded_x = display_sz.x - PANEL_WIDTH;

    // [修改] 收起时：面板左边缘正好在屏幕右侧外 (X = 屏幕宽)
    float collapsed_x = display_sz.x;

    // 插值计算当前 X
    float current_x = expanded_x + (collapsed_x - expanded_x) * AnimationUtils::Ease(m_anim_progress, EasingType::EaseInOutQuad);

    // 设置主面板位置
    m_main_panel->m_rect.x = current_x;
    m_main_panel->m_rect.y = TOP_MARGIN;

    // 3. 设置把手位置 (始终吸附在面板左侧)
    m_collapsed_btn->m_visible = true;

    m_collapsed_btn->m_rect.x = current_x - m_collapsed_btn->m_rect.w - 10.0f;

    m_collapsed_btn->m_rect.y = TOP_MARGIN + 40.0f; // 把手垂直偏移

    // 4. 调用基类 Update
    UIElement::Update(dt, parent_abs_pos);
}

void CelestialInfoPanel::ToggleCollapse()
{
    m_is_collapsed = !m_is_collapsed;
    m_collapsed_btn->ResetInteraction();
}
// 切换 Tab 的逻辑
void CelestialInfoPanel::SelectTab(int index)
{
    if (index < 0 || index >= m_current_data.size()) return;

    m_current_tab_index = index;

    // 1. 更新 Tab 按钮的视觉选中状态
    if (m_tabs_container)
    {
        for (int i = 0; i < m_tabs_container->m_children.size(); ++i)
        {
            auto btn = std::dynamic_pointer_cast<TechButton>(m_tabs_container->m_children[i]);
            if (btn)
            {
                btn->SetSelected(i == m_current_tab_index);
            }
        }
    }

    // 2. 刷新下方内容区
    RefreshContent();
}

// 根据当前选中的 Page 生成列表
void CelestialInfoPanel::RefreshContent()
{
    if (!m_content_vbox) return;
    m_content_vbox->m_children.clear(); // 清空旧内容

    if (m_current_tab_index < 0 || m_current_tab_index >= m_current_data.size()) return;

    // 获取当前页的数据
    const auto& current_page = m_current_data[m_current_tab_index];
    const auto& theme = UIContext::Get().m_theme;

    // 样式准备
    ImFont* target_font = UIContext::Get().m_font_regular;
    if (!target_font) target_font = ImGui::GetFont();
    float font_size = target_font->FontSize;
    float row_height = font_size + 4.0f;
    float key_width = font_size * 6.5f;

    ImVec4 col_line = theme.color_accent; col_line.w = 0.5f;
    ImVec4 col_key = theme.color_text_disabled;
    ImVec4 col_val = theme.color_text;
    ImVec4 col_highlight = theme.color_accent;

    // 遍历该页的所有组
    for (const auto& group : current_page.groups)
    {
        float total_group_height = group.items.size() * row_height;

        // 1. 组容器
        auto group_hbox = std::make_shared<HBox>();
        group_hbox->m_padding = 8.0f;
        group_hbox->m_rect.h = total_group_height;

        // 2. 左侧竖线
        auto line = std::make_shared<Panel>();
        line->m_rect.w = 2.0f;
        line->m_bg_color = col_line;
        line->m_align_v = Alignment::Start;
        line->m_rect.h = total_group_height;
        group_hbox->AddChild(line);

        // 3. 内容列
        auto content_col = std::make_shared<VBox>();
        content_col->m_padding = 0.0f;
        content_col->m_rect.h = total_group_height;
        content_col->m_fill_h = true;

        for (const auto& item : group.items)
        {
            auto row = std::make_shared<HBox>();
            row->m_rect.h = row_height;

            // Key
            auto k = std::make_shared<TechText>(item.key + ":", col_key);
            k->m_align_h = Alignment::Start;
            k->m_align_v = Alignment::Center;
            k->m_font = target_font;
            k->m_rect.w = key_width;

            // Value
            auto v = std::make_shared<TechText>(item.value, col_val);
            v->m_align_h = Alignment::End;
            v->m_align_v = Alignment::Center;
            v->m_fill_h = true;
            v->m_font = target_font;

            // 处理高亮
            if (item.highlight) v->SetColor(col_highlight);

            row->AddChild(k);
            row->AddChild(v);
            content_col->AddChild(row);
        }

        group_hbox->AddChild(content_col);
        m_content_vbox->AddChild(group_hbox);

        // 组间距
        auto spacer = std::make_shared<UIElement>();
        spacer->m_rect.h = row_height * 0.5f;
        m_content_vbox->AddChild(spacer);
    }
}


_UI_END
_SYSTEM_END
_NPGS_END