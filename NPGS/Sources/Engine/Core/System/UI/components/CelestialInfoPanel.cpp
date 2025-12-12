#include "CelestialInfoPanel.h"
#include "../TechUtils.h"
#include "TechText.h"
#include "TechDivider.h"
#include "TechButton.h"
#include "TechProgressBar.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

ImVec2 CelestialInfoPanel::Measure(const ImVec2& available_size)
{
    ImVec2 size = UIElement::Measure(available_size);

    // 委托给 m_main_panel，它包含了所有可见内容
    if (m_main_panel)
    {
        ImVec2 content_size = m_main_panel->Measure(available_size);

        if (m_width_policy.type == LengthType::Content) size.x = content_size.x;
        if (m_height_policy.type == LengthType::Content) size.y = content_size.y;
    }

    return size;
}

CelestialInfoPanel::CelestialInfoPanel(const std::string& fold_key, const std::string& close_key, const std::string& progress_label_key, const std::string& coil_label_key)
{
    // [MODIFIED] 自身作为逻辑容器，尺寸由动画控制，不设置策略
    m_visible = true;
    m_block_input = false;
    m_is_collapsed = true;
    m_anim_progress = 1.0f;

    // =========================================================
    // 1. 侧边把手 (程序化定位，不受布局系统影响)
    // =========================================================
    m_collapsed_btn = std::make_shared<TechButton>(fold_key, TechButton::Style::Vertical);
    m_collapsed_btn->SetFixedSize(24, 100); // 使用新的API
    m_collapsed_btn->SetUseGlass(true);
    m_collapsed_btn->on_click = [this]() { this->ToggleCollapse(); };
    AddChild(m_collapsed_btn);

    // =========================================================
    // 2. 主面板背景
    // =========================================================
    m_main_panel = std::make_shared<TechBorderPanel>();
    m_main_panel->SetFixedSize(PANEL_WIDTH, PANEL_HEIGHT); // 尺寸由动画控制
    m_main_panel->m_use_glass_effect = true;
    m_main_panel->m_thickness = 2.0f;
    AddChild(m_main_panel);

    // --- [NEW] 主面板内部根布局容器 ---
    auto root_vbox = std::make_shared<VBox>();
    root_vbox->SetWidth(Length::Fill());  // 填满主面板
    root_vbox->SetHeight(Length::Fill());
    root_vbox->m_padding = 0.0f;          // 内部元素默认无间距
    m_main_panel->AddChild(root_vbox);

    // =========================================================
    // A. Header 区域 (需要内边距)
    // =========================================================
    {
        // [MODIFIED] 使用一个带固定宽度的 VBox 容器来实现内边距效果
        auto header_content_box = std::make_shared<VBox>();
        header_content_box->SetWidth(Length::Fix(PANEL_WIDTH - 30.0f)) // 固定宽度
            ->SetHeight(Length::Content())               // 高度自适应
            ->SetAlignment(Alignment::Center, Alignment::Start); // 在 root_vbox 中水平居中
        header_content_box->m_padding = 4.0f;

        auto header_row = std::make_shared<HBox>();
        header_row->SetWidth(Length::Fill())      // 填满 header_content_box
            ->SetHeight(Length::Fix(28.0f));

        m_title_text = std::make_shared<TechText>(TR("ui.celestial.no_target"));
        m_title_text->SetColor(ThemeColorID::Accent)
            ->SetWidth(Length::Fix(120.0f)) // 标题固定宽度
            ->SetAlignment(Alignment::Start, Alignment::Start);
        m_title_text->m_font = UIContext::Get().m_font_bold;

        m_subtitle_text = std::make_shared<TechText>("");
        m_subtitle_text->SetColor(ThemeColorID::TextDisabled)
            ->SetWidth(Length::Fill()) // 副标题填充剩余空间
            ->SetAlignment(Alignment::Start, Alignment::End);
        m_subtitle_text->m_font = UIContext::Get().m_font_regular;

        header_row->AddChild(m_title_text);
        header_row->AddChild(m_subtitle_text);

        header_content_box->AddChild(header_row);

        auto div = std::make_shared<TechDivider>();
        div->SetHeight(Length::Fix(8.0f)); // 分割线高度固定
        header_content_box->AddChild(div);

        root_vbox->AddChild(header_content_box);
    }

    // =========================================================
    // B. Image 预览区域 (全宽)
    // =========================================================
    {
        m_preview_image = std::make_shared<Image>(0);
        m_preview_image->SetWidth(Length::Fill())      // 宽度填满 root_vbox
            ->SetHeight(Length::Content()); // 高度根据宽高比自动计算
        m_preview_image->m_aspect_ratio = 16.0f / 9.0f;
        m_preview_image->m_visible = false;
        root_vbox->AddChild(m_preview_image);
    }

    // =========================================================
    // C. 主内容区域 (Tabs, ScrollView, Footer - 需要内边距且填充剩余空间)
    // =========================================================
    {
        // [MODIFIED] 同样使用一个包装 VBox 来实现内边距和填充
        auto main_content_box = std::make_shared<VBox>();
        main_content_box->SetWidth(Length::Fix(PANEL_WIDTH - 30.0f))
            ->SetHeight(Length::Fill()) // 关键：填充 root_vbox 的所有剩余垂直空间
            ->SetAlignment(Alignment::Center, Alignment::Start);
        main_content_box->m_padding = 4.0f;

        // Tabs 区域
        auto tabs_scroll_view = std::make_shared<HorizontalScrollView>();
        tabs_scroll_view->SetHeight(Length::Fix(20.0f)); // 固定高度
        tabs_scroll_view->m_block_input = true;

        m_tabs_container = std::make_shared<HBox>();
        m_tabs_container->SetWidth(Length::Content()) // 宽度由所有 tab 按钮撑开
            ->SetHeight(Length::Fill());  // 高度填满滚动区
        m_tabs_container->m_padding = 4.0f;

        tabs_scroll_view->AddChild(m_tabs_container);
        main_content_box->AddChild(tabs_scroll_view);

        // Spacer
        auto spacer = std::make_shared<UIElement>();
        spacer->SetHeight(Length::Fix(4.0f));
        main_content_box->AddChild(spacer);

        // 数据滚动区
        auto scroll_view = std::make_shared<ScrollView>();
        scroll_view->SetHeight(Length::Fill()); // 关键：填充 main_content_box 的剩余空间

        m_content_vbox = std::make_shared<VBox>();
        m_content_vbox->m_padding = 8.0f;
        // m_content_vbox 默认 Width=Fill, Height=Content，完美适配 ScrollView

        scroll_view->AddChild(m_content_vbox);
        main_content_box->AddChild(scroll_view);

        // Footer 区域
        auto footer_box = std::make_shared<VBox>();
        footer_box->SetWidth(Length::Fill())
            ->SetHeight(Length::Content()); // 高度由内容决定
        footer_box->m_padding = 4.0f;

        // --- Footer 内部组件 ---
        auto sep = std::make_shared<TechDivider>();
        sep->SetHeight(Length::Fix(8.0f));
        footer_box->AddChild(sep);

        auto prog_label = std::make_shared<TechText>(progress_label_key);
        prog_label->SetHeight(Length::Fix(16.0f));
        prog_label->m_font = UIContext::Get().m_font_small;
        footer_box->AddChild(prog_label);

        auto progress = std::make_shared<TechProgressBar>("");
        progress->SetHeight(Length::Fix(6.0f));
        progress->m_progress = 0.45f;
        footer_box->AddChild(progress);

        auto prog_info = std::make_shared<HBox>();
        prog_info->SetHeight(Length::Fix(14.0f));
        auto t1 = std::make_shared<TechText>(coil_label_key);
        t1->SetColor(ThemeColorID::TextDisabled)->SetWidth(Length::Fix(100.0f));
        t1->m_font = UIContext::Get().m_font_small;
        auto t2 = std::make_shared<TechText>("45%");
        t2->SetColor(ThemeColorID::Text)->SetWidth(Length::Fill())->SetAlignment(Alignment::End, Alignment::Start);
        t2->m_font = UIContext::Get().m_font_small;
        prog_info->AddChild(t1);
        prog_info->AddChild(t2);
        footer_box->AddChild(prog_info);

        auto sep_btn = std::make_shared<TechDivider>();
        sep_btn->SetHeight(Length::Fix(14.0f));
        sep_btn->m_visual_height = 1.0f;
        footer_box->AddChild(sep_btn);

        auto close_btn = std::make_shared<TechButton>(close_key, TechButton::Style::Normal);
        close_btn->SetHeight(Length::Fix(32.0f))->SetWidth(Length::Fill()); // 宽度填满
        close_btn->on_click = [this]() { this->ToggleCollapse(); };
        footer_box->AddChild(close_btn);

        auto spacer_btn1 = std::make_shared<UIElement>();
        spacer_btn1->SetHeight(Length::Fix(8.0f));
        footer_box->AddChild(spacer_btn1);

        main_content_box->AddChild(footer_box);

        root_vbox->AddChild(main_content_box);
    }
}

void CelestialInfoPanel::SetData(const CelestialData& data)
{
    m_current_data = data;
    m_current_tab_index = m_current_data.empty() ? -1 : 0;

    if (m_tabs_container)
    {
        m_tabs_container->m_children.clear();
        for (int i = 0; i < m_current_data.size(); ++i)
        {
            const auto& page = m_current_data[i];
            auto btn = std::make_shared<TechButton>(page.name, TechButton::Style::Tab);
            // [MODIFIED] 按钮尺寸由内容决定，或给个固定大小
            btn->SetWidth(Length::Fix(90.0f))
                ->SetHeight(Length::Fill()); // 高度填满 Tab 栏

            btn->on_click = [this, i]() { this->SelectTab(i); };
            m_tabs_container->AddChild(btn);
        }
    }
    SelectTab(m_current_tab_index);
}


void CelestialInfoPanel::RefreshContent()
{
    if (!m_content_vbox) return;
    m_content_vbox->m_children.clear();

    if (m_current_tab_index < 0 || m_current_tab_index >= m_current_data.size()) return;
    const auto& current_page = m_current_data[m_current_tab_index];

    float row_height = UIContext::Get().m_font_regular->FontSize + 4.0f;
    float key_width = UIContext::Get().m_font_regular->FontSize * 6.5f;

    for (const auto& group : current_page.groups)
    {
        float total_group_height = group.items.size() * row_height;

        auto group_hbox = std::make_shared<HBox>();
        group_hbox->SetHeight(Length::Fix(total_group_height)); // 高度固定
        group_hbox->m_padding = 8.0f;

        auto line = std::make_shared<Panel>();
        line->m_bg_color = StyleColor(ThemeColorID::Accent).WithAlpha(0.5f);
        line->SetWidth(Length::Fix(2.0f))->SetHeight(Length::Fill()); // 竖线填满 HBox 高度
        group_hbox->AddChild(line);

        auto content_col = std::make_shared<VBox>();
        content_col->SetWidth(Length::Fill()); // 填满 HBox 剩余宽度
        content_col->m_padding = 0.0f;

        for (const auto& item : group.items)
        {
            auto row = std::make_shared<HBox>();
            row->SetHeight(Length::Fix(row_height)); // 行高固定

            auto k = std::make_shared<TechText>(item.key + ":", ThemeColorID::TextDisabled);
            k->SetWidth(Length::Fix(key_width)) // Key 固定宽度
                ->SetAlignment(Alignment::Start, Alignment::Center);
            k->m_font = UIContext::Get().m_font_regular;

            auto v = std::make_shared<TechText>(item.value, ThemeColorID::Text);
            v->SetWidth(Length::Fill()) // Value 填充剩余宽度
                ->SetAlignment(Alignment::End, Alignment::Center);
            v->m_font = UIContext::Get().m_font_regular;
            if (item.highlight) v->SetColor(ThemeColorID::Accent);

            row->AddChild(k);
            row->AddChild(v);
            content_col->AddChild(row);
        }

        group_hbox->AddChild(content_col);
        m_content_vbox->AddChild(group_hbox);

        auto spacer = std::make_shared<UIElement>();
        spacer->SetHeight(Length::Fix(row_height * 0.5f));
        m_content_vbox->AddChild(spacer);
    }
}


// [NO CHANGE] Update, SetObjectImage, SetTitle, ToggleCollapse, SelectTab are not affected by layout refactoring

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

_UI_END
_SYSTEM_END
_NPGS_END