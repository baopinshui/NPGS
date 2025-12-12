#include "NeuralMenu.h" 
#include <cmath>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// --- NeuralMenu Implementation ---
NeuralMenu::NeuralMenu(const std::string& main_button_key1, const std::string& main_button_key2, const std::string& settings_key, const std::string& close_key )
{
    // [核心修改]
    // NeuralMenu 自身现在是 UI 树的一部分。它的 rect 代表了它在屏幕上的位置和大小。
    SetFixedSize(m_collapsed_size.x, m_collapsed_size.y);
    m_block_input = false; // 自身不阻挡，让子元素(root_panel)处理

    // 1. Root Panel 现在是 NeuralMenu 的子元素，填满父容器
    root_panel = std::make_shared<TechBorderPanel>();
    root_panel->m_thickness = 2.0f;
    root_panel->m_use_glass_effect = true;
    // [KEY CHANGE] 声明 root_panel 总是填满 NeuralMenu
    root_panel->SetWidth(Length::Fill())->SetHeight(Length::Fill());
    AddChild(root_panel);
    // --- 内部组件的创建逻辑不变，只是它们的父级是 root_panel ---

    // 2. Background Particles
    bg_view = std::make_shared<NeuralParticleView>(80);
    bg_view->SetSizes(m_collapsed_size, m_expanded_size);
    bg_view->SetWidth(Length::Fill())->SetHeight(Length::Fill());
    root_panel->AddChild(bg_view);

    // 4. Collapsed View Button
    collapsed_btn = std::make_shared<CollapsedMainButton>(main_button_key1, main_button_key2);
    collapsed_btn->on_click_callback = [this]() { this->ToggleExpand(); };
    collapsed_btn->m_alpha = 1.0f;
    collapsed_btn->SetWidth(Length::Fill())->SetHeight(Length::Fill());
    root_panel->AddChild(collapsed_btn);

    // 5. Expanded View (VBox)
    main_layout = std::make_shared<VBox>();
    main_layout->m_alpha = 0.0f;
    main_layout->m_visible = false;
    main_layout->m_block_input = false;
    main_layout->m_padding = 8.0f;
    main_layout->SetWidth(Length::Fill())->SetHeight(Length::Fill());
    root_panel->AddChild(main_layout);

    // --- 展开视图的内部内容，它们的布局由 VBox (main_layout) 管理 ---

    header_title = std::make_shared<TechText>(settings_key, ThemeColorID::TextHighlight);
    header_title->SetAnimMode(TechTextAnimMode::Hacker);
    header_title->SetHeight(Length::Fix(20.0f));
    header_title->SetWidth(Length::Fill()); // 标题宽度填满 VBox
    header_title->SetAlignment(Alignment::Start, Alignment::Center);
    header_title->m_font = UIContext::Get().m_font_bold;
    main_layout->AddChild(header_title);

    auto sep_top = std::make_shared<TechDivider>();
    sep_top->SetWidth(Length::Fill()); // 分割线宽度填满
    main_layout->AddChild(sep_top);

    scroll_view = std::make_shared<ScrollView>();
    scroll_view->SetWidth(Length::Fill());
    scroll_view->SetHeight(Length::Fill(1.0f)); // 滚动区填充所有剩余垂直空间
    main_layout->AddChild(scroll_view);

    content_vbox = std::make_shared<VBox>();
    content_vbox->m_padding = 10.0f;
    content_vbox->SetWidth(Length::Fill()); // 内容区宽度填满 ScrollView
    content_vbox->SetHeight(Length::Content()); // 高度由内容决定
    scroll_view->AddChild(content_vbox);

    auto sep_bot = std::make_shared<TechDivider>();
    sep_bot->SetWidth(Length::Fill());
    main_layout->AddChild(sep_bot);

    auto close_btn = std::make_shared<TechButton>(close_key, TechButton::Style::Normal);
    close_btn->SetHeight(Length::Fix(34.0f));
    close_btn->SetWidth(Length::Fill()); // 关闭按钮宽度填满 VBox
    close_btn->on_click = [this]() { this->ToggleExpand(); };
    main_layout->AddChild(close_btn);
}


void NeuralMenu::Update(float dt, const ImVec2& parent_abs_pos)
{
    // 1. 更新自身动画和绝对位置
    UpdateSelf(dt, parent_abs_pos);
    if (!m_visible) return;

    // 2. [容器职责] 安排直接子元素 (root_panel) 的布局
    if (root_panel)
    {
        // 因为 root_panel 是 Fill，所以它的 rect 和 NeuralMenu 完全一样
        root_panel->m_rect.x = 0;
        root_panel->m_rect.y = 0;
        root_panel->m_rect.w = m_rect.w;
        root_panel->m_rect.h = m_rect.h;

        // 递归更新，这将触发 Panel 的 Update，
        // Panel 会根据 Fill 策略继续安排 bg_view, collapsed_btn, main_layout 的布局
        root_panel->Update(dt, m_absolute_pos);

        // 3. [特例] 为 main_layout 应用 padding
        // 一个更完善的系统会把 padding 作为容器属性，但这里手动调整是可行的
        if (main_layout)
        {
            float padding = 15.0f;
            main_layout->m_rect.x += padding;
            main_layout->m_rect.y += padding;
            main_layout->m_rect.w -= padding * 2.0f;
            main_layout->m_rect.h -= padding * 2.0f;
            // 重新计算一下绝对位置
            main_layout->m_absolute_pos.x = root_panel->m_absolute_pos.x + main_layout->m_rect.x;
            main_layout->m_absolute_pos.y = root_panel->m_absolute_pos.y + main_layout->m_rect.y;
        }
    }
}

void NeuralMenu::ToggleExpand()
{
    m_expanded = !m_expanded;
    float dur = 0.25f;

    // 确定目标尺寸
    ImVec2 target_size = m_expanded ? m_expanded_size : m_collapsed_size;

    // [KEY CHANGE] 只需要对 NeuralMenu 自身应用尺寸动画
    To(&m_rect.w, target_size.x, dur, EasingType::EaseInOutQuad);
    To(&m_rect.h, target_size.y, dur, EasingType::EaseInOutQuad);

    // 更新粒子视图的目标状态
    bg_view->SetState(m_expanded, true);
    // 动画结束后通知粒子视图
    To(&m_alpha, m_alpha, dur, EasingType::Linear, [this]()
    {
        this->bg_view->SetState(this->m_expanded, false);
    });

    if (m_expanded)
    {
        // --- 处理视图切换和交互 ---
        collapsed_btn->ResetInteraction();
        main_layout->ResetInteraction();
        main_layout->m_visible = true;
        main_layout->m_block_input = true;
        collapsed_btn->m_block_input = false;

        // 触发标题动画
        header_title->RestartEffect();

        // --- Alpha 动画 ---
        collapsed_btn->To(&collapsed_btn->m_alpha, 0.0f, dur / 2);
        main_layout->To(&main_layout->m_alpha, 1.0f, dur);
    }
    else
    {
        // --- 处理视图切换和交互 ---
        main_layout->m_block_input = false;
        collapsed_btn->m_block_input = true;

        // --- Alpha 动画 ---
        collapsed_btn->To(&collapsed_btn->m_alpha, 1.0f, dur);
        main_layout->To(&main_layout->m_alpha, 0.0f, dur / 2, EasingType::EaseOutQuad,
            [this]() { this->main_layout->m_visible = false; });
    }
}

_UI_END
_SYSTEM_END
_NPGS_END