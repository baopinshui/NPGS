#include "CelestialInfoPanel.h"
#include "../TechUtils.h"
#include "TechText.h"
#include "TechDivider.h"
#include "TechButton.h"      
#include "TechProgressBar.h" 
#include "TechInteractivePanel.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

CelestialInfoPanel::CelestialInfoPanel(const std::string& fold_key, const std::string& close_key, const std::string& progress_label_key, const std::string& coil_label_key)
{
    m_block_input = false; // 自身作为透明容器不拦截输入

    // =========================================================
    // 1. 创建侧边把手 (Collapsed Tab)
    // =========================================================
    m_collapsed_btn = std::make_shared<TechButton>(fold_key, TechButton::Style::Vertical);
    m_collapsed_btn->SetName("foldButton"); // [命名] 关键交互按钮
    m_collapsed_btn->m_width = Length::Fixed(24.0f);
    m_collapsed_btn->m_height = Length::Fixed(100.0f);
    m_collapsed_btn->SetUseGlass(true);
    m_collapsed_btn->on_click = [this]() { this->ToggleCollapse(); };
    AddChild(m_collapsed_btn);

    // =========================================================
    // 2. 创建主面板背景
    // =========================================================
    m_main_panel = std::make_shared<TechBorderPanel>();
    m_main_panel->SetName("mainPanel"); // [命名] 复合组件的主体，方便整体控制
    m_main_panel->m_width = Length::Fixed(PANEL_WIDTH);
    m_main_panel->m_height = Length::Fixed(PANEL_HEIGHT);
    m_main_panel->m_use_glass_effect = true;
    m_main_panel->m_thickness = 2.0f;
    AddChild(m_main_panel);

    // --- 主面板内部布局容器 (撑满整个面板) ---
    auto root_vbox = std::make_shared<VBox>(); // 纯布局容器，无需命名
    root_vbox->m_width = Length::Stretch();
    root_vbox->m_height = Length::Stretch();
    root_vbox->m_padding = 0.0f;
    m_main_panel->AddChild(root_vbox);

    // =========================================================
    // A. Header 区域 (使用带内边距的容器)
    // =========================================================
    auto header_wrapper = std::make_shared<Panel>(); // 纯布局容器 (用于居中)，无需命名
    header_wrapper->m_width = Length::Fixed(PANEL_WIDTH - 30.0f);
    header_wrapper->m_height = Length::Content();
    header_wrapper->m_align_h = Alignment::Center;
    root_vbox->AddChild(header_wrapper);

    auto header_container = std::make_shared<VBox>(); // 纯布局容器，无需命名
    header_container->m_width = Length::Stretch();
    header_container->m_height = Length::Content();
    header_container->m_padding = 4.0f;
    header_wrapper->AddChild(header_container);
    {
        // 顶部留白
        auto top_spacer = std::make_shared<UIElement>(); // 纯布局元素，无需命名
        top_spacer->m_height = Length::Fixed(15.0f);
        header_container->AddChild(top_spacer);

        m_title_text = std::make_shared<TechText>(TR("i18ntext.ui.celestial.no_target"));
        m_title_text->SetName("title"); // [命名] 显示动态数据 (天体名称)
        m_title_text->SetSizing(TechTextSizingMode::AutoHeight);
        m_title_text->m_width = Length::Content();
        m_title_text->m_height = Length::Content();
        m_title_text->SetColor(ThemeColorID::Accent);
        m_title_text->m_font = UIContext::Get().m_font_bold;
        m_title_text->m_width = Length::Stretch();
        m_title_text->m_height = Length::Content();
        header_container->AddChild(m_title_text);

        m_subtitle_text = std::make_shared<TechText>("");
        m_subtitle_text->SetName("subtitle"); // [命名] 显示动态数据 (天体类型)
        m_subtitle_text->SetSizing(TechTextSizingMode::AutoWidthHeight);
        m_subtitle_text->m_width = Length::Content();
        m_subtitle_text->m_height = Length::Content();
        m_subtitle_text->SetColor(ThemeColorID::TextDisabled);
        m_subtitle_text->m_font = UIContext::Get().m_font_regular;
        m_subtitle_text->m_width = Length::Content();
        m_subtitle_text->m_height = Length::Content();
        m_subtitle_text->m_align_h = Alignment::End;
        header_container->AddChild(m_subtitle_text);

        auto div = std::make_shared<TechDivider>(); // 装饰性元素，无需命名
        div->m_height = Length::Fixed(8.0f);
        header_container->AddChild(div);
    }

    // =========================================================
    // B. Image 预览区域
    // =========================================================
    m_preview_image = std::make_shared<Image>(0);
    m_preview_image->SetName("previewImage"); // [命名] 显示动态图片，且需要控制可见性
    m_preview_image->m_width = Length::Fixed(PANEL_WIDTH);
    m_preview_image->m_height = Length::Content();
    m_preview_image->m_aspect_ratio = 16.0f / 9.0f;
    m_preview_image->m_visible = false;
    root_vbox->AddChild(m_preview_image);

    // =========================================================
    // C. 主内容区域 (Tabs, ScrollView, Footer), 填充剩余空间
    // =========================================================
    auto main_content_box = std::make_shared<VBox>(); // 纯布局容器，无需命名
    main_content_box->m_width = Length::Fixed(PANEL_WIDTH - 30.0f);
    main_content_box->m_height = Length::Stretch(1.0f);
    main_content_box->m_align_h = Alignment::Center;
    main_content_box->m_padding = 4.0f;
    root_vbox->AddChild(main_content_box);
    {
        // Tabs 区域
        auto tabs_scroll_view = std::make_shared<HorizontalScrollView>(); // 纯布局容器，无需命名
        tabs_scroll_view->m_height = Length::Fixed(20.0f);
        tabs_scroll_view->m_width = Length::Stretch();
        main_content_box->AddChild(tabs_scroll_view);

        m_tabs_container = std::make_shared<HBox>();
        m_tabs_container->SetName("tabs"); // [命名] 用于动态添加/删除 Tab 按钮
        m_tabs_container->m_height = Length::Stretch();
        m_tabs_container->m_width = Length::Content();
        m_tabs_container->m_padding = 4.0f;
        tabs_scroll_view->AddChild(m_tabs_container);

        auto pad = std::make_shared<UIElement>(); // 纯布局元素，无需命名
        pad->m_height = Length::Fixed(4.0f);
        main_content_box->AddChild(pad);

        // 数据滚动区
        m_scroll_view = std::make_shared<ScrollView>();
        m_scroll_view->SetName("dataScrollView"); // [命名] 主要功能区域，可能需要外部控制
        m_scroll_view->m_height = Length::Stretch(1.0f);
        main_content_box->AddChild(m_scroll_view);

        m_content_vbox = std::make_shared<VBox>();
        m_content_vbox->SetName("content"); // [命名] 关键容器，用于动态添加/删除信息行
        m_content_vbox->m_width = Length::Stretch();
        m_content_vbox->m_height = Length::Content();
        m_content_vbox->m_padding = 8.0f;
        m_scroll_view->AddChild(m_content_vbox);

        // Footer 区域
        auto footer_box = std::make_shared<VBox>(); // 纯布局容器，无需命名
        footer_box->m_width = Length::Stretch();
        footer_box->m_height = Length::Content();
        footer_box->m_padding = 4.0f;
        main_content_box->AddChild(footer_box);
        {
            auto sep = std::make_shared<TechDivider>(); // 装饰性元素，无需命名
            sep->m_height = Length::Fixed(8.0f);
            footer_box->AddChild(sep);

            auto prog_label = std::make_shared<TechText>(progress_label_key);
            prog_label->SetName("progressLabel"); // [命名] 进度条的标签，可能与进度条一起显示/隐藏
            prog_label->m_font = UIContext::Get().m_font_small;
            prog_label->m_height = Length::Content();
            footer_box->AddChild(prog_label);

            auto progress = std::make_shared<TechProgressBar>("");
            progress->SetName("progressBar"); // [命名] 核心动态数据显示组件
            progress->m_height = Length::Fixed(6.0f);
            progress->m_progress = 0.45f;
            footer_box->AddChild(progress);

            auto prog_info = std::make_shared<HBox>(); // 纯布局容器，无需命名
            prog_info->m_height = Length::Fixed(14.0f);
            auto t1 = std::make_shared<TechText>(coil_label_key); // 静态标签，可以不命名
            t1->SetColor(ThemeColorID::TextDisabled);
            t1->m_font = UIContext::Get().m_font_small;
            t1->m_width = Length::Fixed(100.0f);
            auto t2 = std::make_shared<TechText>("45%");
            t2->SetName("progressPercentage"); // [命名] 显示动态的百分比文本
            t2->SetColor(ThemeColorID::Text);
            t2->m_font = UIContext::Get().m_font_small;
            t2->m_width = Length::Stretch();
            t2->m_align_h = Alignment::End;
            prog_info->AddChild(t1);
            prog_info->AddChild(t2);
            footer_box->AddChild(prog_info);

            auto sep_btn = std::make_shared<TechDivider>(); // 装饰性元素，无需命名
            sep_btn->m_height = Length::Fixed(14.0f);
            sep_btn->m_visual_height = 1.0f;
            footer_box->AddChild(sep_btn);

            auto close_btn = std::make_shared<TechButton>(close_key, TechButton::Style::Normal);
            close_btn->SetName("closeButton"); // [命名] 关键交互按钮
            close_btn->m_height = Length::Fixed(32.0f);
            close_btn->m_width = Length::Stretch();
            close_btn->on_click = [this]() { this->ToggleCollapse(); };
            footer_box->AddChild(close_btn);

            auto final_pad = std::make_shared<UIElement>(); // 纯布局元素，无需命名
            final_pad->m_height = Length::Fixed(8.0f);
            footer_box->AddChild(final_pad);
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

    // [修改] 移除所有位置计算代码，只保留动画状态更新
    float speed = 5.0f * dt;
    if (m_is_collapsed) m_anim_progress = std::min(1.0f, m_anim_progress + speed);
    else m_anim_progress = std::max(0.0f, m_anim_progress - speed);

    m_collapsed_btn->m_alpha = AnimationUtils::Ease(m_anim_progress, EasingType::Linear);
    m_collapsed_btn->m_visible = m_collapsed_btn->m_alpha > 0.1f;

    // 调用基类 Update，它会递归更新所有子元素的动画等状态
    UIElement::Update(dt);
}

ImVec2 CelestialInfoPanel::Measure(ImVec2 available_size)
{
    if (!m_visible) return { 0, 0 };

    // 此面板具有固定的逻辑尺寸
    m_desired_size = { PANEL_WIDTH, PANEL_HEIGHT };

    // 尽管我们有固定尺寸，但仍需测量子元素以确保它们的 desired_size 是最新的，
    // 以便在 Arrange 阶段正确布局。
    for (auto& child : m_children)
    {
        child->Measure({ PANEL_WIDTH, PANEL_HEIGHT });
    }

    return m_desired_size;
}

void CelestialInfoPanel::Arrange(const Rect& final_rect)
{
    // 1. 基类方法会根据 final_rect 设置好我们自己的 m_rect 和 m_absolute_pos
    UIElement::Arrange(final_rect);

    if (!m_visible) return;

    // 2. 在我们自己的坐标系内，根据动画状态排列子元素
    float t_panel = AnimationUtils::Ease(m_anim_progress, EasingType::EaseInOutQuad);
    // 计算滑出偏移量：0 表示完全展开，PANEL_WIDTH 表示完全收起
    float slide_offset = PANEL_WIDTH * t_panel;

    // a. 排列主面板，应用偏移
    // 注意：这里的坐标是相对于 CelestialInfoPanel 自身的
    m_main_panel->Arrange({ slide_offset, 0, PANEL_WIDTH, PANEL_HEIGHT });

    // b. 排列折叠按钮，使其位置相对于滑动的主面板
    float btn_w = m_collapsed_btn->m_width.IsFixed() ? m_collapsed_btn->m_width.value : 24.0f;
    float btn_h = m_collapsed_btn->m_height.IsFixed() ? m_collapsed_btn->m_height.value : 100.0f;
    m_collapsed_btn->Arrange({ slide_offset - btn_w - 10.0f, 40.0f, btn_w, btn_h });
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

    if (m_current_tab_index < 0 || m_current_tab_index >= m_current_data.size()) return;

    const auto& current_page = m_current_data[m_current_tab_index];

    ImFont* target_font = UIContext::Get().m_font_regular ? UIContext::Get().m_font_regular : ImGui::GetFont();
    if (!target_font) return;

    const float available_content_width = PANEL_WIDTH - 30.0f - 2.0f - 8.0f;
    const float hbox_padding = 8.0f;

    // [核心修正] 增加一个安全边距，防止因浮点精度等问题导致的临界布局失败
    const float SAFETY_MARGIN = 5.0f; // 5个像素的缓冲

    for (const auto& group : current_page.groups)
    {
        auto group_hbox = std::make_shared<HBox>();
        group_hbox->m_padding = 8.0f;
        group_hbox->m_height = Length::Content();
        group_hbox->m_width = Length::Stretch();

        auto line = std::make_shared<Panel>();
        line->m_bg_color = StyleColor(ThemeColorID::Accent).WithAlpha(0.5f);
        line->m_width = Length::Fixed(2.0f);
        line->m_height = Length::Stretch();
        group_hbox->AddChild(line);

        auto content_col = std::make_shared<TechInteractivePanel>();
        content_col->m_padding = 8.0f;
        content_col->m_width = Length::Stretch();
        content_col->m_height = Length::Content();

        for (const auto& item : group.items)
        {
            std::string key_text = item.key + ":";
            ImVec2 key_size = target_font->CalcTextSizeA(target_font->FontSize, FLT_MAX, 0.0f, key_text.c_str());
            ImVec2 value_size = target_font->CalcTextSizeA(target_font->FontSize, FLT_MAX, 0.0f, item.value.c_str());

            float required_width = key_size.x + value_size.x + hbox_padding;

            auto k = std::make_shared<TechText>(key_text, ThemeColorID::TextDisabled);
            k->m_font = target_font;

            auto v = std::make_shared<TechText>(item.value, ThemeColorID::Text, true);
            v->m_font = target_font;
            if (item.highlight) v->SetColor(ThemeColorID::Accent);

            // [核心修正] 在判断时使用安全边距
            if (required_width < available_content_width - SAFETY_MARGIN)
            {
                // [模式A: 单行 HBox] - 空间足够
                auto row_hbox = std::make_shared<HBox>();
                row_hbox->m_padding = hbox_padding;
                row_hbox->m_height = Length::Content();
                row_hbox->m_align_v = Alignment::Center;
                row_hbox->m_block_input = false;

                k->SetSizing(TechTextSizingMode::ForceAutoWidthHeight);
                v->SetSizing(TechTextSizingMode::ForceAutoWidthHeight);
                v->m_width = Length::Content();
                v->m_height = Length::Content();
                k->m_width = Length::Content();
                k->m_height = Length::Content();

                k->m_width = Length::Content();
                v->m_width = Length::Stretch();
                v->m_align_h = Alignment::End;
                v->SetTextAlign(Alignment::End);

                row_hbox->AddChild(k);
                row_hbox->AddChild(v);
                content_col->AddChild(row_hbox);
            }
            else
            {
                // [模式B: 多行 VBox] - 空间不足或处于临界状态
                auto row_vbox = std::make_shared<VBox>();
                row_vbox->m_padding = 0.0f;
                row_vbox->m_width = Length::Stretch();
                row_vbox->m_height = Length::Content();
                row_vbox->m_block_input = false;

                k->SetSizing(TechTextSizingMode::AutoHeight);
                v->SetSizing(TechTextSizingMode::AutoHeight);
                v->m_width = Length::Content();
                v->m_height = Length::Content();
                k->m_width = Length::Content();
                k->m_height = Length::Content();

                k->m_width = Length::Stretch();
                k->m_align_h = Alignment::Start;

                v->m_width = Length::Stretch();
                v->m_align_h = Alignment::End;
                v->SetTextAlign(Alignment::End);

                row_vbox->AddChild(k);
                row_vbox->AddChild(v);
                content_col->AddChild(row_vbox);
            }
        }

        group_hbox->AddChild(content_col);
        m_content_vbox->AddChild(group_hbox);

        auto pad = std::make_shared<UIElement>();
        pad->m_height = Length::Fixed(12.0f);
        m_content_vbox->AddChild(pad);
    }
}

_UI_END
_SYSTEM_END
_NPGS_END