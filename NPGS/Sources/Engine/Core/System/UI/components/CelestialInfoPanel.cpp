#include "CelestialInfoPanel.h"
#include "../TechUtils.h"
#include "TechText.h"
#include "TechDivider.h"
#include "TechButton.h"      
#include "TechProgressBar.h" 

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// [修改] 构造函数实现，接收 closetext
CelestialInfoPanel::CelestialInfoPanel(const std::string& foldtext, const std::string& closetext)
{
    // 自身作为容器，不阻挡输入，全透明
    m_rect = { 0, 0, 0, 0 };
    m_visible = true;
    m_block_input = false;

    // [初始化状态] 默认为收起，只能看到把手
    m_is_collapsed = true;
    m_anim_progress = 1.0f;

    auto& theme = UIContext::Get().m_theme;

    // =========================================================
    // 1. 创建侧边把手 (Collapsed Tab)
    // =========================================================
    m_collapsed_btn = std::make_shared<TechButton>(foldtext, TechButton::Style::Vertical);
    m_collapsed_btn->m_rect = { 0, 0, 24, 100 }; // 窄条设计
    m_collapsed_btn->SetUseGlass(true);
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

    // --- 主面板内部布局容器 (无边距，撑满整个面板) ---
    auto root_vbox = std::make_shared<VBox>();
    root_vbox->m_rect = { 0, 0, PANEL_WIDTH, PANEL_HEIGHT };
    root_vbox->m_padding = 0.0f;
    root_vbox->m_fill_h = true;
    root_vbox->m_fill_v = true;
    m_main_panel->AddChild(root_vbox);

    // =========================================================
    // A. Header 区域 (放入一个带边距的容器)
    // =========================================================
    {
        auto header_content_box = std::make_shared<VBox>();
        // 设置水平边距，使其内容居中
        header_content_box->m_rect = { 15, 15, PANEL_WIDTH - 30, 0 };
        header_content_box->m_fill_h = false;
        header_content_box->m_align_h = Alignment::Center;
        header_content_box->m_padding = 4.0f;

        auto header_row = std::make_shared<HBox>();
        header_row->m_rect.h = 28.0f;

        m_title_text = std::make_shared<TechText>("No Target");
        m_title_text->m_font = UIContext::Get().m_font_bold;
        m_title_text->SetColor(theme.color_accent);
        m_title_text->m_align_v = Alignment::End;
        m_title_text->m_rect.w = 120.0f;

        m_subtitle_text = std::make_shared<TechText>("");
        m_subtitle_text->SetColor(theme.color_text_disabled);
        m_subtitle_text->m_font = UIContext::Get().m_font_regular;
        m_subtitle_text->m_align_v = Alignment::End;
        m_subtitle_text->m_align_h = Alignment::End;
        m_subtitle_text->m_fill_h = true;

        header_row->AddChild(m_title_text);
        header_row->AddChild(m_subtitle_text);

        header_content_box->AddChild(header_row);

        auto div = std::make_shared<TechDivider>();
        div->m_color = theme.color_accent;
        div->m_rect.h = 8.0f;
        header_content_box->AddChild(div);

        // 将带边距的 Header 容器加入到根容器中
        root_vbox->AddChild(header_content_box);
    }

    // =========================================================
    // [新位置] Image 预览区域 (直接加入 root_vbox，实现全宽)
    // =========================================================
    {
        m_preview_image = std::make_shared<Image>(0);
        m_preview_image->m_fill_h = true;       // 撑满父容器宽度
        m_preview_image->m_auto_height = true;
        m_preview_image->m_aspect_ratio = 16.0f / 9.0f;
        m_preview_image->m_visible = false;
        // 直接添加到 root_vbox，它将自动获得全宽
        root_vbox->AddChild(m_preview_image);
    }

    // =========================================================
    // B, C, D. 主内容区域 (Tabs, ScrollView, Footer)
    // (放入另一个带边距的容器)
    // =========================================================
    auto main_content_box = std::make_shared<VBox>();
    // 设置水平边距，顶部不再需要15px的边距
    main_content_box->m_rect = { 15, 0, PANEL_WIDTH - 30, 0 };
    main_content_box->m_fill_h = false;
    main_content_box->m_fill_v = true; // 此容器需要填充剩余的垂直空间
    main_content_box->m_align_h = Alignment::Center;
    main_content_box->m_padding = 4.0f;
    root_vbox->AddChild(main_content_box);

    // B. Tabs 区域
    {
        auto tabs_scroll_view = std::make_shared<HorizontalScrollView>();
        tabs_scroll_view->m_rect.h = 20.0f;
        tabs_scroll_view->m_block_input = true;

        m_tabs_container = std::make_shared<HBox>();
        m_tabs_container->m_padding = 4.0f;
        m_tabs_container->m_fill_h = false;
        m_tabs_container->m_rect.h = 20.0f;

        tabs_scroll_view->AddChild(m_tabs_container);
        main_content_box->AddChild(tabs_scroll_view);
    }

    auto spacer = std::make_shared<UIElement>();
    spacer->m_rect.h = 4.0f;
    main_content_box->AddChild(spacer);

    // C. 数据滚动区
    auto scroll_view = std::make_shared<ScrollView>();
    scroll_view->m_fill_v = true;

    m_content_vbox = std::make_shared<VBox>();
    m_content_vbox->m_padding = 8.0f;

    scroll_view->AddChild(m_content_vbox);
    main_content_box->AddChild(scroll_view);

    // D. Footer 区域
    {
        auto footer_box = std::make_shared<VBox>();
        footer_box->m_padding = 4.0f;

        // ... [Footer 内部代码与之前完全相同，此处省略] ...
        // --- 1. 顶部分割线 ---
        auto sep = std::make_shared<TechDivider>();
        sep->m_color = theme.color_border;
        sep->m_rect.h = 8.0f;
        footer_box->AddChild(sep);
        // --- 2. 进度条相关 ---
        auto prog_label = std::make_shared<TechText>(">>> Star Lifter Progress");
        prog_label->SetColor(theme.color_accent);
        prog_label->m_rect.h = 16.0f;
        prog_label->m_font = UIContext::Get().m_font_small;
        auto progress = std::make_shared<TechProgressBar>("");
        progress->m_rect.h = 6.0f;
        progress->m_progress = 0.45f;
        auto prog_info = std::make_shared<HBox>();
        prog_info->m_rect.h = 14.0f;
        auto t1 = std::make_shared<TechText>("Main Coil");
        t1->SetColor(theme.color_text_disabled);
        t1->m_font = UIContext::Get().m_font_small;
        t1->m_rect.w = 100.0f;
        auto t2 = std::make_shared<TechText>("45%");
        t2->SetColor(theme.color_text);
        t2->m_font = UIContext::Get().m_font_small;
        t2->m_fill_h = true;
        t2->m_align_h = Alignment::End;
        prog_info->AddChild(t1);
        prog_info->AddChild(t2);
        footer_box->AddChild(prog_label);
        footer_box->AddChild(progress);
        footer_box->AddChild(prog_info);
        // --- 3. 底部关闭按钮区域 ---
        auto sep_btn = std::make_shared<TechDivider>();
        sep_btn->m_color = theme.color_accent;
        sep_btn->m_color.w = 0.7;
        sep_btn->m_rect.h = 14.0f;
        sep_btn->m_visual_height = 1.0f;
        footer_box->AddChild(sep_btn);
        auto close_btn = std::make_shared<TechButton>(closetext, TechButton::Style::Normal);
        close_btn->m_rect.h = 32.0f;
        close_btn->m_align_h = Alignment::Stretch;
        close_btn->on_click = [this]() { this->ToggleCollapse(); };
        footer_box->AddChild(close_btn);
        auto spacer_btn1 = std::make_shared<UIElement>();
        spacer_btn1->m_rect.h = 8.0f;
        footer_box->AddChild(spacer_btn1);

        main_content_box->AddChild(footer_box);
    }
}

void CelestialInfoPanel::SetData(const CelestialData& data)
{
    // 1. 更新数据模型
    m_current_data = data;

    // 重置 Tab 索引 
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

            btn->m_fill_h = false;
            btn->m_rect.w = 90.0f;

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

void CelestialInfoPanel::SetObjectImage(ImTextureID texture_id, float img_w, float img_h, ImVec4 Col)
{
    if (m_preview_image)
    {
        m_preview_image->m_texture_id = texture_id;
        m_preview_image->m_tint_col = Col;
        if (img_w > 0.0f && img_h > 0.0f)
        {
            m_preview_image->SetOriginalSize(img_w, img_h);
        }
        m_preview_image->m_visible = (texture_id != 0);
    }
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

    // 1. 动画状态机 (m_is_collapsed 控制方向)
    // 收起时 m_anim_progress -> 1.0
    // 展开时 m_anim_progress -> 0.0
    float speed = 5.0f * dt;
    if (m_is_collapsed)
        m_anim_progress += speed;
    else
        m_anim_progress -= speed;
    m_anim_progress = std::clamp(m_anim_progress, 0.0f, 1.0f);

    // 2. 计算面板位置 (EaseInOutQuad 插值)
    float expanded_x = display_sz.x - PANEL_WIDTH; // 展开位置
    float collapsed_x = display_sz.x;              // 收起位置 (屏幕外)

    // progress 0.0 -> expanded_x, 1.0 -> collapsed_x
    float t_panel = AnimationUtils::Ease(m_anim_progress, EasingType::EaseInOutQuad);
    float current_x = expanded_x + (collapsed_x - expanded_x) * t_panel;

    // 设置主面板位置
    m_main_panel->m_rect.x = current_x;
    m_main_panel->m_rect.y = TOP_MARGIN;

    // 3. [修改] 竖向把手控制
    // 需求：收起时显示，点击后消失；展开后消失，面板底部有关闭按钮。

    // (A) 位置跟随
    m_collapsed_btn->m_rect.x = current_x - m_collapsed_btn->m_rect.w - 10.0f;
    m_collapsed_btn->m_rect.y = TOP_MARGIN + 40.0f;

    // (B) 透明度控制
    // progress: 1.0 (Collapsed) -> Alpha 1.0
    // progress: 0.0 (Expanded)  -> Alpha 0.0
    // 可以直接使用 m_anim_progress，或者加个 Ease 曲线让它消失得更快
    m_collapsed_btn->m_alpha = AnimationUtils::Ease(m_anim_progress, EasingType::Linear);

    // (C) 交互控制
    // 如果几乎看不见了，就禁用交互，防止展开后误触悬浮在半空的透明按钮
    if (m_collapsed_btn->m_alpha < 0.1f)
    {
        m_collapsed_btn->m_visible = false; // 直接不可见，或
        m_collapsed_btn->m_block_input = false;
    }
    else
    {
        m_collapsed_btn->m_visible = true;
        m_collapsed_btn->m_block_input = true;
    }

    // 4. 调用基类 Update
    UIElement::Update(dt, parent_abs_pos);
}

void CelestialInfoPanel::ToggleCollapse()
{
    m_is_collapsed = !m_is_collapsed;
    m_collapsed_btn->ResetInteraction();
}

void CelestialInfoPanel::SelectTab(int index)
{
    if (index < 0 || index >= m_current_data.size()) return;

    m_current_tab_index = index;

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
    RefreshContent();
}

void CelestialInfoPanel::RefreshContent()
{
    if (!m_content_vbox) return;
    m_content_vbox->m_children.clear();

    if (m_current_tab_index < 0 || m_current_tab_index >= m_current_data.size()) return;

    const auto& current_page = m_current_data[m_current_tab_index];
    const auto& theme = UIContext::Get().m_theme;

    ImFont* target_font = UIContext::Get().m_font_regular;
    if (!target_font) target_font = ImGui::GetFont();
    float font_size = target_font->FontSize;
    float row_height = font_size + 4.0f;
    float key_width = font_size * 6.5f;

    ImVec4 col_line = theme.color_accent; col_line.w = 0.5f;
    ImVec4 col_key = theme.color_text_disabled;
    ImVec4 col_val = theme.color_text;
    ImVec4 col_highlight = theme.color_accent;

    for (const auto& group : current_page.groups)
    {
        float total_group_height = group.items.size() * row_height;

        auto group_hbox = std::make_shared<HBox>();
        group_hbox->m_padding = 8.0f;
        group_hbox->m_rect.h = total_group_height;

        auto line = std::make_shared<Panel>();
        line->m_rect.w = 2.0f;
        line->m_bg_color = col_line;
        line->m_align_v = Alignment::Start;
        line->m_rect.h = total_group_height;
        group_hbox->AddChild(line);

        auto content_col = std::make_shared<VBox>();
        content_col->m_padding = 0.0f;
        content_col->m_rect.h = total_group_height;
        content_col->m_fill_h = true;

        for (const auto& item : group.items)
        {
            auto row = std::make_shared<HBox>();
            row->m_rect.h = row_height;

            auto k = std::make_shared<TechText>(item.key + ":", col_key);
            k->m_align_h = Alignment::Start;
            k->m_align_v = Alignment::Center;
            k->m_font = target_font;
            k->m_rect.w = key_width;

            auto v = std::make_shared<TechText>(item.value, col_val, true);
            v->m_align_h = Alignment::End;
            v->m_align_v = Alignment::Center;
            v->m_fill_h = true;
            v->m_font = target_font;

            if (item.highlight) v->SetColor(col_highlight);

            row->AddChild(k);
            row->AddChild(v);
            content_col->AddChild(row);
        }

        group_hbox->AddChild(content_col);
        m_content_vbox->AddChild(group_hbox);

        auto spacer = std::make_shared<UIElement>();
        spacer->m_rect.h = row_height * 0.5f;
        m_content_vbox->AddChild(spacer);
    }
}

_UI_END
_SYSTEM_END
_NPGS_END