#include "NeuralMenu.h" 
#include <cmath>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// --- NeuralMenu Implementation ---
NeuralMenu::NeuralMenu(const std::string& main_button_key1, const std::string& main_button_key2, const std::string& settings_key, const std::string& close_key)
{
    // --- 1. 设置 NeuralMenu 自身 ---
    // [核心修改]
    // NeuralMenu 自身的尺寸由动画驱动，因此我们使用 Fixed 类型。
    // 布局系统将根据 m_width.value 和 m_height.value 进行布局。
    SetAbsolutePos(20.0f, 20.0f);
    m_width = Length::Fixed(m_collapsed_size.x);
    m_height = Length::Fixed(m_collapsed_size.y);
    m_block_input = false; // 自身不阻挡，让子元素(root_panel)处理

    // --- 2. 创建并配置子组件 (声明式布局) ---

    // Root Panel: 作为所有内容的容器，应始终填满 NeuralMenu
    root_panel = std::make_shared<TechBorderPanel>();
    root_panel->m_width = Length::Stretch();
    root_panel->m_height = Length::Stretch();
    root_panel->m_thickness = 2.0f;
    root_panel->m_use_glass_effect = true;
    AddChild(root_panel); // 将 panel 作为子元素添加

    // Background Particles: 填满 root_panel
    bg_view = std::make_shared<NeuralParticleView>(80);
    bg_view->m_width = Length::Stretch();
    bg_view->m_height = Length::Stretch();
    bg_view->SetSizes(m_collapsed_size, m_expanded_size);
    root_panel->AddChild(bg_view);

    // Collapsed View Button: 填满 root_panel
    collapsed_btn = std::make_shared<CollapsedMainButton>(main_button_key1, main_button_key2);
    collapsed_btn->m_width = Length::Stretch();
    collapsed_btn->m_height = Length::Stretch();
    collapsed_btn->on_click_callback = [this]() { this->ToggleExpand(); };
    collapsed_btn->m_alpha = 1.0f;
    root_panel->AddChild(collapsed_btn);

    // Expanded View (VBox): 填满 root_panel，但带有 15px 的内边距
    // [修改] 我们让 VBox 具有固定大小并居中，以模拟内边距效果
    base = std::make_shared <UIElement>();
    base->m_width = Length::Fixed(m_expanded_size.x);
    base->m_height = Length::Fixed(m_expanded_size.y);
    base->m_visible = false;
    base->m_block_input = false;
    root_panel->AddChild(base);

    main_layout = std::make_shared<VBox>();
    main_layout->m_width = Length::Fixed(m_expanded_size.x - 30.0f);////////////////////////////////////
    main_layout->m_height = Length::Fixed(m_expanded_size.y - 30.0f);
    main_layout->m_align_h = Alignment::Center;
    main_layout->m_align_v = Alignment::Center;
    main_layout->m_alpha = 0.0f;
    main_layout->m_visible = false;
    main_layout->m_block_input = false;
    main_layout->m_padding = 8.0f;
    base->AddChild(main_layout);
    // --- 3. 构建 Expanded View 的内部结构 ---
    // 这些子组件的布局由它们的父容器 main_layout (VBox) 管理

    // A. Header Title: 宽度撑满，高度由内容决定
    header_title = std::make_shared<TechText>(settings_key, ThemeColorID::TextHighlight);
    header_title->SetAnimMode(TechTextAnimMode::Hacker);
    header_title->m_width = Length::Stretch();
    header_title->m_height = Length::Content(); // 高度自适应
    header_title->m_align_h = Alignment::Start;
    header_title->m_font = UIContext::Get().m_font_bold;
    main_layout->AddChild(header_title);

    // B. Top Divider: 宽度撑满，高度固定
    auto sep_top = std::make_shared<TechDivider>();
    main_layout->AddChild(sep_top);

    // C. Scroll Area: 占据所有剩余的垂直空间
    scroll_view = std::make_shared<ScrollView>();
    scroll_view->m_width = Length::Stretch();
    scroll_view->m_height = Length::Stretch(1.0f); // Stretch 权重为1，表示占据可用空间
    main_layout->AddChild(scroll_view);

    // C.1. ScrollView 的内容
    content_vbox = std::make_shared<VBox>();
    content_vbox->m_width = Length::Stretch();
    content_vbox->m_height = Length::Content(); // 高度由所有滑块决定
    content_vbox->m_padding = 10.0f;
    scroll_view->AddChild(content_vbox);

    // D. Bottom Divider
    auto sep_bot = std::make_shared<TechDivider>();
    main_layout->AddChild(sep_bot);

    // E. Footer Button: 宽度撑满，高度由内容决定
    auto close_btn = std::make_shared<TechButton>(close_key, TechButton::Style::Normal);
    close_btn->m_width = Length::Stretch();
    close_btn->m_height = Length::Fixed(34.0f); // 高度自适应
    close_btn->on_click = [this]() { this->ToggleExpand(); };
    main_layout->AddChild(close_btn);
}



void NeuralMenu::Update(float dt)
{
    UIElement::Update(dt);
}



void NeuralMenu::ToggleExpand()
{
    m_expanded = !m_expanded;
    float dur = 0.25f;

    if (m_expanded)
    {
        // --- 准备展开 ---
        main_layout->ResetInteraction();
        main_layout->m_visible = true;
        main_layout->m_block_input = true;
        base->m_visible = true;
        collapsed_btn->m_block_input = false;

        bg_view->SetState(m_expanded, true);
        header_title->RestartEffect();

        // [核心修改] 动画逻辑简化
        // 只需动画化 NeuralMenu 自身的尺寸。
        // 子元素 root_panel 和 bg_view 因为是 Stretch，会自动跟随变化。
        To(&m_width.value, m_expanded_size.x, dur, EasingType::EaseInOutQuad);
        To(&m_height.value, m_expanded_size.y, dur, EasingType::EaseInOutQuad,
            // 在动画完成时，通知粒子效果停止动画状态
            [this]() { this->bg_view->SetState(true, false); });

        // Alpha 动画保持不变
        collapsed_btn->To(&collapsed_btn->m_alpha, 0.0f, dur / 2);
        main_layout->To(&main_layout->m_alpha, 1.0f, dur);
    }
    else
    {
        // --- 准备收起 ---
        main_layout->m_block_input = false;
        collapsed_btn->m_block_input = true;

        // [核心修改] 动画逻辑简化
        To(&m_width.value, m_collapsed_size.x, dur, EasingType::EaseInOutQuad);
        To(&m_height.value, m_collapsed_size.y, dur, EasingType::EaseInOutQuad,
            [this]() { this->bg_view->SetState(false, false); });

        // Alpha 动画保持不变
        collapsed_btn->To(&collapsed_btn->m_alpha, 1.0f, dur);
        main_layout->To(&main_layout->m_alpha, 0.0f, dur / 2, EasingType::EaseOutQuad,
            [this]() { this->main_layout->m_visible = false;  this->base->m_visible = false;});
    }
}
_UI_END
_SYSTEM_END
_NPGS_END