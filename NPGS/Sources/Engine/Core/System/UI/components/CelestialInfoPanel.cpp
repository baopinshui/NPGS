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
CelestialInfoPanel::CelestialInfoPanel(const std::string& fold_key, const std::string& close_key, const std::string& progress_label_key, const std::string& coil_label_key)
{
    m_block_input = false; // 自身作为透明容器不拦截输入

    // =========================================================
    // 1. 创建侧边把手 (Collapsed Tab)
    // =========================================================
    m_collapsed_btn = std::make_shared<TechButton>(fold_key, TechButton::Style::Vertical);
    m_collapsed_btn->m_width = Length::Fixed(24.0f);
    m_collapsed_btn->m_height = Length::Fixed(100.0f);
    m_collapsed_btn->SetUseGlass(true);
    m_collapsed_btn->on_click = [this]() { this->ToggleCollapse(); };
    AddChild(m_collapsed_btn); // 位置在 Update 中用 SetAbsolutePos 设置

    // =========================================================
    // 2. 创建主面板背景
    // =========================================================
    m_main_panel = std::make_shared<TechBorderPanel>();
    m_main_panel->m_width = Length::Fixed(PANEL_WIDTH);
    m_main_panel->m_height = Length::Fixed(PANEL_HEIGHT);
    m_main_panel->m_use_glass_effect = true;
    m_main_panel->m_thickness = 2.0f;
    AddChild(m_main_panel); // 位置在 Update 中用 SetAbsolutePos 设置

    // --- 主面板内部布局容器 (撑满整个面板) ---
    auto root_vbox = std::make_shared<VBox>();
    root_vbox->m_width = Length::Stretch(); // 默认撑满父级
    root_vbox->m_height = Length::Stretch();
    root_vbox->m_padding = 0.0f;
    m_main_panel->AddChild(root_vbox);

    // =========================================================
    // A. Header 区域 (使用带内边距的容器)
    // =========================================================
    auto header_container = std::make_shared<VBox>();
    header_container->m_width = Length::Stretch();
    header_container->m_height = Length::Content(); // 高度由内容决定
    header_container->m_padding = 4.0f;
    // [新方法] 使用另一个Panel作为带边距的包装器
    auto header_wrapper = std::make_shared<Panel>();
    header_wrapper->m_width = Length::Fixed(PANEL_WIDTH - 30.0f);
    header_wrapper->m_height = Length::Content();
    header_wrapper->m_align_h = Alignment::Center; // 在父VBox中水平居中
    header_wrapper->AddChild(header_container);
    root_vbox->AddChild(header_wrapper);
    {
        // 顶部留白
        auto top_spacer = std::make_shared<UIElement>();
        top_spacer->m_height = Length::Fixed(15.0f);
        header_container->AddChild(top_spacer);

        m_title_text = std::make_shared<TechText>(TR("ui.celestial.no_target"));
        m_title_text->SetSizing(TechTextSizingMode::AutoHeight); // 自动换行
        m_title_text->SetColor(ThemeColorID::Accent);
        m_title_text->m_font = UIContext::Get().m_font_bold;
        m_title_text->m_width = Length::Stretch();
        m_title_text->m_height = Length::Content();
        header_container->AddChild(m_title_text);

        m_subtitle_text = std::make_shared<TechText>("");
        m_subtitle_text->SetSizing(TechTextSizingMode::AutoWidthHeight);
        m_subtitle_text->SetColor(ThemeColorID::TextDisabled);
        m_subtitle_text->m_font = UIContext::Get().m_font_regular;
        m_subtitle_text->m_width = Length::Content();
        m_subtitle_text->m_height = Length::Content();
        m_subtitle_text->m_align_h = Alignment::End; // 在父容器中右对齐
        header_container->AddChild(m_subtitle_text);

        auto div = std::make_shared<TechDivider>();
        div->m_height = Length::Fixed(8.0f);
        header_container->AddChild(div);
    }

    // =========================================================
    // B. Image 预览区域
    // =========================================================
    m_preview_image = std::make_shared<Image>(0);
    m_preview_image->m_width = Length::Fixed(PANEL_WIDTH);
    m_preview_image->m_height = Length::Content(); // 高度由 Measure 根据宽高比计算
    m_preview_image->m_aspect_ratio = 16.0f / 9.0f;
    m_preview_image->m_visible = false;
    root_vbox->AddChild(m_preview_image);

    // =========================================================
    // C. 主内容区域 (Tabs, ScrollView, Footer), 填充剩余空间
    // =========================================================
    auto main_content_box = std::make_shared<VBox>();
    main_content_box->m_width = Length::Fixed(PANEL_WIDTH - 30.0f);
    main_content_box->m_height = Length::Stretch(1.0f); // 关键：填充剩余垂直空间
    main_content_box->m_align_h = Alignment::Center;
    main_content_box->m_padding = 4.0f;
    root_vbox->AddChild(main_content_box);
    {
        // Tabs 区域
        auto tabs_scroll_view = std::make_shared<HorizontalScrollView>();
        tabs_scroll_view->m_height = Length::Fixed(20.0f);
        tabs_scroll_view->m_width = Length::Stretch();
        main_content_box->AddChild(tabs_scroll_view);

        m_tabs_container = std::make_shared<HBox>();
        m_tabs_container->m_height = Length::Stretch();
        m_tabs_container->m_width = Length::Content(); // 宽度由子按钮决定
        m_tabs_container->m_padding = 4.0f;
        tabs_scroll_view->AddChild(m_tabs_container);


        auto pad = std::make_shared<UIElement>();
        pad->m_height = Length::Fixed(4.0f);
        main_content_box->AddChild(pad);

        // 数据滚动区
        m_scroll_view = std::make_shared<ScrollView>();
        m_scroll_view->m_height = Length::Stretch(1.0f); // 关键：填充 VBox 中的剩余空间
        main_content_box->AddChild(m_scroll_view);

        m_content_vbox = std::make_shared<VBox>();
        m_content_vbox->m_width = Length::Stretch();
        m_content_vbox->m_height = Length::Content(); // 高度由内容决定
        m_content_vbox->m_padding = 8.0f;
        m_scroll_view->AddChild(m_content_vbox);

        // Footer 区域
        auto footer_box = std::make_shared<VBox>();
        footer_box->m_width = Length::Stretch();
        footer_box->m_height = Length::Content(); // 高度由内容决定
        footer_box->m_padding = 4.0f;
        main_content_box->AddChild(footer_box);
        {
            auto sep = std::make_shared<TechDivider>();
            sep->m_height = Length::Fixed(8.0f);
            footer_box->AddChild(sep);

            auto prog_label = std::make_shared<TechText>(progress_label_key);
            prog_label->m_font = UIContext::Get().m_font_small;
            prog_label->m_height = Length::Content();
            footer_box->AddChild(prog_label);

            auto progress = std::make_shared<TechProgressBar>("");
            progress->m_height = Length::Fixed(6.0f);
            progress->m_progress = 0.45f;
            footer_box->AddChild(progress);

            auto prog_info = std::make_shared<HBox>();
            prog_info->m_height = Length::Fixed(14.0f);
            auto t1 = std::make_shared<TechText>(coil_label_key);
            t1->SetColor(ThemeColorID::TextDisabled);
            t1->m_font = UIContext::Get().m_font_small;
            t1->m_width = Length::Fixed(100.0f);
            auto t2 = std::make_shared<TechText>("45%");
            t2->SetColor(ThemeColorID::Text);
            t2->m_font = UIContext::Get().m_font_small;
            t2->m_width = Length::Stretch(); // 撑满剩余宽度
            t2->m_align_h = Alignment::End;
            prog_info->AddChild(t1);
            prog_info->AddChild(t2);
            footer_box->AddChild(prog_info);

            auto sep_btn = std::make_shared<TechDivider>();
            sep_btn->m_height = Length::Fixed(14.0f);
            sep_btn->m_visual_height = 1.0f;
            footer_box->AddChild(sep_btn);

            auto close_btn = std::make_shared<TechButton>(close_key, TechButton::Style::Normal);
            close_btn->m_height = Length::Fixed(32.0f);
            close_btn->m_width = Length::Stretch(); // 撑满
            close_btn->on_click = [this]() { this->ToggleCollapse(); };
            footer_box->AddChild(close_btn);


            auto pad = std::make_shared<UIElement>();
            pad->m_height = Length::Fixed(8.0f);
            footer_box->AddChild(pad);

        }
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
            auto btn = std::make_shared<TechButton>(m_current_data[i].name, TechButton::Style::Tab);
            btn->m_width = Length::Fixed(90.0f);
            btn->on_click = [this, i]() { this->SelectTab(i); };
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
    if (m_title_text) m_title_text->SetSourceText(title);
    if (m_subtitle_text) m_subtitle_text->SetSourceText(subtitle);
}

void CelestialInfoPanel::Update(float dt)
{
    if (!m_visible) return;

    ImVec2 display_sz = UIContext::Get().m_display_size;

    float speed = 5.0f * dt;
    if (m_is_collapsed) m_anim_progress = std::min(1.0f, m_anim_progress + speed);
    else m_anim_progress = std::max(0.0f, m_anim_progress - speed);

    float t_panel = AnimationUtils::Ease(m_anim_progress, EasingType::EaseInOutQuad);
    float expanded_x = display_sz.x - PANEL_WIDTH;
    float collapsed_x = display_sz.x;
    float current_x = expanded_x + (collapsed_x - expanded_x) * t_panel;

    // [MODIFIED] 使用 SetAbsolutePos 控制位置
    m_main_panel->SetAbsolutePos(current_x, TOP_MARGIN);

    float btn_w = m_collapsed_btn->m_width.IsFixed() ? m_collapsed_btn->m_width.value : 24.0f;
    m_collapsed_btn->SetAbsolutePos(current_x - btn_w - 10.0f, TOP_MARGIN + 40.0f);

    m_collapsed_btn->m_alpha = AnimationUtils::Ease(m_anim_progress, EasingType::Linear);

    // [MODIFIED] 隐藏并禁用输入，而不是仅设置 block_input
    m_collapsed_btn->m_visible = m_collapsed_btn->m_alpha > 0.1f;

    // 调用基类 Update，它会递归更新所有子元素的动画等状态
    UIElement::Update(dt);
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
    // [新增] 切换 Tab 时重置滚动位置
    if (m_scroll_view)
    {
        m_scroll_view->m_scroll_y = 0;
        m_scroll_view->m_target_scroll_y = 0;
    }
}

void CelestialInfoPanel::RefreshContent()
{
    if (!m_content_vbox) return;
    m_content_vbox->m_children.clear();

    if (m_current_tab_index < 0) return;

    const auto& current_page = m_current_data[m_current_tab_index];

    ImFont* target_font = UIContext::Get().m_font_regular ? UIContext::Get().m_font_regular : ImGui::GetFont();
    float row_height = target_font->FontSize + 4.0f;
    float key_width = target_font->FontSize * 6.5f;

    for (const auto& group : current_page.groups)
    {
        auto group_hbox = std::make_shared<HBox>();
        group_hbox->m_padding = 8.0f;
        group_hbox->m_height = Length::Content();

        auto line = std::make_shared<Panel>();
        line->m_bg_color = StyleColor(ThemeColorID::Accent).WithAlpha(0.5f);
        line->m_width = Length::Fixed(2.0f);
        line->m_height = Length::Stretch(); // 撑满 HBox 高度
        group_hbox->AddChild(line);

        auto content_col = std::make_shared<VBox>();
        content_col->m_padding = 0.0f;
        content_col->m_width = Length::Stretch();
        content_col->m_height = Length::Content();

        for (const auto& item : group.items)
        {
            auto row = std::make_shared<HBox>();
            row->m_height = Length::Fixed(row_height);
            row->m_align_v = Alignment::Center; // 子元素垂直居中

            auto k = std::make_shared<TechText>(item.key + ":", ThemeColorID::TextDisabled);
            k->m_font = target_font;
            k->m_width = Length::Fixed(key_width);

            auto v = std::make_shared<TechText>(item.value, ThemeColorID::Text, true);
            v->m_font = target_font;
            v->m_width = Length::Stretch();
            v->m_align_h = Alignment::End;
            if (item.highlight) v->SetColor(ThemeColorID::Accent);

            row->AddChild(k);
            row->AddChild(v);
            content_col->AddChild(row);
        }

        group_hbox->AddChild(content_col);
        m_content_vbox->AddChild(group_hbox);



        auto pad = std::make_shared<UIElement>();
        pad->m_height = Length::Fixed(row_height * 0.5f);
        m_content_vbox->AddChild(pad);

    }
}

_UI_END
_SYSTEM_END
_NPGS_END