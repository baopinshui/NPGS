#include "NeuralMenu.h" 
#include <cmath>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// --- NeuralMenu Implementation ---
NeuralMenu::NeuralMenu(const std::string& mainbuttonstr1, const std::string& mainbuttonstr2, const std::string& settingstr, const std::string& closestr )
{
    // [核心修改]
    // NeuralMenu 自身现在是 UI 树的一部分。它的 rect 代表了它在屏幕上的位置和大小。
    m_rect = { 20, 20, m_collapsed_size.x, m_collapsed_size.y };
    m_block_input = false; // 自身不阻挡，让子元素(root_panel)处理

    // 1. Root Panel 现在是 NeuralMenu 的子元素，填满父容器
    root_panel = std::make_shared<TechBorderPanel>();
    root_panel->m_rect = { 0, 0, m_collapsed_size.x, m_collapsed_size.y };
    root_panel->m_thickness = 2.0f;
    root_panel->m_use_glass_effect = true;
    AddChild(root_panel); // [核心修改] 将 panel 作为子元素添加

    // --- 内部组件的创建逻辑不变，只是它们的父级是 root_panel ---

    // 2. Background Particles
    bg_view = std::make_shared<NeuralParticleView>(80);
    bg_view->m_rect = { 0, 0, m_collapsed_size.x, m_collapsed_size.y };
    bg_view->SetSizes(m_collapsed_size, m_expanded_size);
    root_panel->AddChild(bg_view);

    // 3. Collapsed View
    collapsed_btn = std::make_shared<CollapsedMainButton>(mainbuttonstr1, mainbuttonstr2);
    collapsed_btn->m_font = UIContext::Get().m_font_small;
    collapsed_btn->m_rect = { 0, 0, m_collapsed_size.x, m_collapsed_size.y };
    collapsed_btn->on_click_callback = [this]() { this->ToggleExpand(); };
    collapsed_btn->m_alpha = 1.0f;
    root_panel->AddChild(collapsed_btn);

    // 4. Expanded View
    main_layout = std::make_shared<VBox>();
    main_layout->m_rect = { 15, 15, m_expanded_size.x - 30, m_expanded_size.y - 30 };
    main_layout->m_alpha = 0.0f;
    main_layout->m_visible = false;
    main_layout->m_block_input = false;
    main_layout->m_padding = 8.0f;
    root_panel->AddChild(main_layout);

    const auto& theme = UIContext::Get().m_theme;

    // A. Header Title
    header_title = std::make_shared<TechText>(settingstr, std::nullopt, true);
    header_title->SetAnimMode(TechTextAnimMode::Hacker);
    header_title->m_rect.h = 20.0f;
    header_title->m_align_h = Alignment::Start;
    header_title->m_font = UIContext::Get().m_font_bold;
    header_title->SetColor(theme.color_text_highlight);
    main_layout->AddChild(header_title);

    // B. Top Divider
    auto sep_top = std::make_shared<TechDivider>();
    sep_top->m_color = theme.color_accent;
    main_layout->AddChild(sep_top);

    // C. Scroll Area
    scroll_view = std::make_shared<ScrollView>();
    scroll_view->m_fill_v = true;
    main_layout->AddChild(scroll_view);

    content_vbox = std::make_shared<VBox>();
    content_vbox->m_padding = 10.0f;
    scroll_view->AddChild(content_vbox);

    // D. Bottom Divider
    auto sep_bot = std::make_shared<TechDivider>();
    sep_bot->m_color = theme.color_accent;
    main_layout->AddChild(sep_bot);

    // E. Footer Button
    auto close_btn = std::make_shared<TechButton>(closestr, TechButton::Style::Normal);
    close_btn->m_rect.h = 34.0f;
    close_btn->m_align_h = Alignment::Stretch;
    close_btn->on_click = [this]() { this->ToggleExpand(); };
    main_layout->AddChild(close_btn);
}


void NeuralMenu::Update(float dt, const ImVec2& parent_abs_pos)
{
    // 调用基类 Update 即可，它会自动处理动画和子节点的更新
    UIElement::Update(dt, parent_abs_pos);
}


void NeuralMenu::ToggleExpand()
{
    m_expanded = !m_expanded;
    float dur = 0.25f;

    if (m_expanded)
    {
        collapsed_btn->ResetInteraction();
        main_layout->ResetInteraction();
        main_layout->m_visible = true;
        main_layout->m_block_input = true;

        bg_view->SetState(m_expanded, true); 
        header_title->RestartEffect();

        // [核心修改]
        // 同时动画化 NeuralMenu 自身 和 它的子元素 root_panel
        // 这样可以确保组件的包围盒正确更新
        this->To(&this->m_rect.w, m_expanded_size.x, dur, EasingType::EaseInOutQuad);
        this->To(&this->m_rect.h, m_expanded_size.y, dur, EasingType::EaseInOutQuad);

        root_panel->To(&root_panel->m_rect.w, m_expanded_size.x, dur, EasingType::EaseInOutQuad);
        root_panel->To(&root_panel->m_rect.h, m_expanded_size.y, dur, EasingType::EaseInOutQuad);

        bg_view->To(&bg_view->m_rect.w, m_expanded_size.x, dur, EasingType::EaseInOutQuad,
            [this]() { this->bg_view->SetState(true, false); });
        bg_view->To(&bg_view->m_rect.h, m_expanded_size.y, dur, EasingType::EaseInOutQuad);

        collapsed_btn->To(&collapsed_btn->m_alpha, 0.0f, dur / 2);
        collapsed_btn->m_block_input = false;

        main_layout->To(&main_layout->m_alpha, 1.0f, dur);
    }
    else
    {
        // [核心修改] 同时动画化 NeuralMenu 自身
        this->To(&this->m_rect.w, m_collapsed_size.x, dur, EasingType::EaseInOutQuad);
        this->To(&this->m_rect.h, m_collapsed_size.y, dur, EasingType::EaseInOutQuad);

        root_panel->To(&root_panel->m_rect.w, m_collapsed_size.x, dur, EasingType::EaseInOutQuad);
        root_panel->To(&root_panel->m_rect.h, m_collapsed_size.y, dur, EasingType::EaseInOutQuad);

        bg_view->To(&bg_view->m_rect.w, m_collapsed_size.x, dur, EasingType::EaseInOutQuad,
            [this]() { this->bg_view->SetState(false, false); });
        bg_view->To(&bg_view->m_rect.h, m_collapsed_size.y, dur, EasingType::EaseInOutQuad);

        collapsed_btn->To(&collapsed_btn->m_alpha, 1.0f, dur);
        collapsed_btn->m_block_input = true;

        main_layout->m_block_input = false;
        main_layout->To(&main_layout->m_alpha, 0.0f, dur / 2, EasingType::EaseOutQuad,
            [this]() { this->main_layout->m_visible = false; });
    }
}

_UI_END
_SYSTEM_END
_NPGS_END