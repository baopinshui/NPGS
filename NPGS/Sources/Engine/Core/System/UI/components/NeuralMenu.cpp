// --- START OF FILE NeuralMenu.cpp ---

#include "NeuralMenu.h" 
#include <cmath>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

NeuralMenu::NeuralMenu(const std::string& main_button_key1, const std::string& main_button_key2, const std::string& settings_key, const std::string& close_key)
{
    // --- 1. 设置 NeuralMenu 自身 ---
    m_width = Length::Fixed(m_collapsed_size.x);
    m_height = Length::Fixed(m_collapsed_size.y);
    m_block_input = false;

    // --- 2. 根容器与背景 ---
    root_panel = std::make_shared<TechBorderPanel>();
    root_panel->m_width = Length::Stretch();
    root_panel->m_height = Length::Stretch();
    root_panel->m_thickness = 2.0f;
    root_panel->m_use_glass_effect = true;
    AddChild(root_panel);

    bg_view = std::make_shared<NeuralParticleView>(80);
    bg_view->SetName("particleView");
    bg_view->m_width = Length::Stretch();
    bg_view->m_height = Length::Stretch();
    bg_view->SetSizes(m_collapsed_size, m_expanded_size);
    root_panel->AddChild(bg_view);

    // --- 3. 收起状态按钮 ---
    collapsed_btn = std::make_shared<CollapsedMainButton>(main_button_key1, main_button_key2);
    collapsed_btn->SetName("collapsedButton");
    collapsed_btn->m_width = Length::Stretch();
    collapsed_btn->m_height = Length::Stretch();
    collapsed_btn->on_click_callback = [this]() { this->ToggleExpand(); };
    collapsed_btn->m_alpha = 1.0f;
    root_panel->AddChild(collapsed_btn);

    // --- 4. 展开状态容器 ---
    base = std::make_shared<UIElement>();
    base->m_width = Length::Fixed(m_expanded_size.x);
    base->m_height = Length::Fixed(m_expanded_size.y);
    base->m_visible = false;
    base->m_block_input = false;
    root_panel->AddChild(base);

    main_layout = std::make_shared<VBox>();
    main_layout->m_width = Length::Fixed(m_expanded_size.x - 30.0f);
    main_layout->m_height = Length::Fixed(m_expanded_size.y - 30.0f);
    main_layout->m_align_h = Alignment::Center;
    main_layout->m_align_v = Alignment::Center;
    main_layout->m_alpha = 0.0f;
    main_layout->m_visible = false;
    main_layout->m_block_input = false;
    main_layout->m_padding = 8.0f;
    base->AddChild(main_layout);

    // === A. 头部区域 (标题 + 页面切换按钮) ===
    header_box = std::make_shared<HBox>();
    header_box->m_width = Length::Stretch();
    header_box->m_height = Length::Fixed(24.0f);
    header_box->m_padding = 0.0f; // 紧凑布局
    main_layout->AddChild(header_box);

    // 标题
    header_title = std::make_shared<TechText>(settings_key, ThemeColorID::TextHighlight);
    header_title->SetName("title");
    header_title->SetAnimMode(TechTextAnimMode::Hacker);
    header_title->m_width = Length::Stretch(1.0f); // 占据左侧空间
    header_title->m_align_v = Alignment::Center;
    header_title->m_font = UIContext::Get().m_font_bold;
    header_box->AddChild(header_title);

    // 切换按钮 (INFO / CUSTOM)
    page_toggle_btn = std::make_shared<TechButton>(" INFO ", TechButton::Style::Tab);
    page_toggle_btn->SetName("pageToggleButton");
    page_toggle_btn->m_width = Length::Fixed(60.0f);
    page_toggle_btn->m_height = Length::Stretch();
    page_toggle_btn->SetFont(UIContext::Get().m_font_small); // 用小字体
    page_toggle_btn->on_click = [this]() { this->SwitchPage(); };
    header_box->AddChild(page_toggle_btn);

    // 分割线
    auto sep_top = std::make_shared<TechDivider>();
    main_layout->AddChild(sep_top);

    // === B. 页面内容区域 (包含两个重叠/切换的容器) ===

    // B1. 信息页面容器 (Info Page)
    m_page_info_container = std::make_shared<VBox>();
    m_page_info_container->SetName("infoPage");
    m_page_info_container->m_width = Length::Stretch();
    m_page_info_container->m_height = Length::Stretch(1.0f);
    m_page_info_container->m_padding = 4.0f;
    main_layout->AddChild(m_page_info_container);

    // 仿照 HTML 添加一些“假”数据展示
    CreateInfoRow(m_page_info_container, "NODE_COUNT", "1,094,201");
    CreateInfoRow(m_page_info_container, "CPU_LOAD", "89.4%");
    CreateInfoRow(m_page_info_container, "LATENCY", "0.002ms");
    CreateInfoRow(m_page_info_container, "PROTOCOL", "V.99.1", ImVec4(0.3f, 1.0f, 0.3f, 1.0f));

    // B2. 设置页面容器 (Settings Page)
    m_page_settings_container = std::make_shared<VBox>();
    m_page_settings_container->SetName("settingsPage");
    m_page_settings_container->m_width = Length::Stretch();
    m_page_settings_container->m_height = Length::Stretch(1.0f);
    m_page_settings_container->m_visible = false; // 默认隐藏
    main_layout->AddChild(m_page_settings_container);

    // 滚动视图用于放置大量滑块
    auto scroll_view = std::make_shared<ScrollView>();
    scroll_view->SetName("settingsScroll");
    scroll_view->m_width = Length::Stretch();
    scroll_view->m_height = Length::Stretch(1.0f);
    m_page_settings_container->AddChild(scroll_view);

    // 滑块的实际父容器
    m_settings_content_box = std::make_shared<VBox>();
    m_settings_content_box->SetName("settingsContent");
    m_settings_content_box->m_width = Length::Stretch();
    m_settings_content_box->m_height = Length::Content();
    m_settings_content_box->m_padding = 10.0f;
    scroll_view->AddChild(m_settings_content_box);

    // [新增] 在设置页面底部添加退出按钮
    // 为了美观，可以在滑块和退出按钮间加个小分割或间距
    auto spacer = std::make_shared<UIElement>();
    spacer->m_height = Length::Fixed(10.0f);
    m_settings_content_box->AddChild(spacer);

    m_exit_btn = std::make_shared<TechButton>("ui.menu.quit_game", TechButton::Style::Normal);
    m_exit_btn->SetName("exitButton");
    m_exit_btn->m_width = Length::Stretch();
    m_exit_btn->m_height = Length::Fixed(30.0f);
    m_exit_btn->m_color_bg = ImVec4(0.3f, 0.0f, 0.0f, 0.5f); // 红色背景警示
    m_exit_btn->m_color_border = ImVec4(1.0f, 0.2f, 0.2f, 0.8f);
    m_settings_content_box->AddChild(m_exit_btn);


    // === C. 底部区域 ===
    auto sep_bot = std::make_shared<TechDivider>();
    main_layout->AddChild(sep_bot);

    m_close_menu_btn = std::make_shared<TechButton>(close_key, TechButton::Style::Normal);
    m_close_menu_btn->SetName("closeButton");
    m_close_menu_btn->m_width = Length::Stretch();
    m_close_menu_btn->m_height = Length::Fixed(34.0f);
    m_close_menu_btn->on_click = [this]() { this->ToggleExpand(); };
    main_layout->AddChild(m_close_menu_btn);
    ToggleExpand();
}

// 辅助函数：创建像 HTML 那样的 Key-Value 行
void NeuralMenu::CreateInfoRow(std::shared_ptr<VBox> parent, const std::string& label, const std::string& value, const StyleColor& val_color)
{
    auto row = std::make_shared<HBox>();
    row->m_width = Length::Stretch();
    row->m_height = Length::Fixed(20.0f);
    row->m_padding = 0.0f;
    // 添加一点悬停效果背景 (可选)
    // row->m_bg_color = ...

    auto key_txt = std::make_shared<TechText>(label, ThemeColorID::TextDisabled);
    key_txt->m_font = UIContext::Get().m_font_small;
    key_txt->m_width = Length::Stretch(1.0f);
    row->AddChild(key_txt);

    auto val_txt = std::make_shared<TechText>(value, val_color);
    val_txt->m_font = UIContext::Get().m_font_small; // 这里可以用 monospace 字体如果上下文有的话
    val_txt->m_width = Length::Content();
    row->AddChild(val_txt);

    parent->AddChild(row);
}

void NeuralMenu::SwitchPage()
{
    // 切换状态
    if (m_current_page == Page::Info)
    {
        m_current_page = Page::Settings;
        page_toggle_btn->SetSourceText(" CUSTOM "); // 按钮显示当前模式或目标模式？通常显示当前状态
    }
    else
    {
        m_current_page = Page::Info;
        page_toggle_btn->SetSourceText(" INFO ");
    }

    // 更新可见性
    // 注意：这里利用 VBox 布局特性，隐藏的元素不占空间（如果 Length::Stretch 处理得当）
    // 或者我们在此处手动移除/添加子节点，但修改 Visible 更简单

    // 在布局系统中，我们必须确保 VBox 会忽略不可见的子元素进行布局计算。
    // 假设布局系统支持 m_visible = false 时不占位。

    // 为了更稳妥，我们在这里手动控制 AddChild / RemoveChild 或者 
    // 让两个容器都在 main_layout 中，但互斥显示。

    m_page_info_container->m_visible = (m_current_page == Page::Info);
    m_page_settings_container->m_visible = (m_current_page == Page::Settings);
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
        // 展开
        main_layout->ResetInteraction();
        main_layout->m_visible = true;
        main_layout->m_block_input = true;
        base->m_visible = true;
        collapsed_btn->m_block_input = false;

        // 默认重置回 Info 页
        m_current_page = Page::Info;
        page_toggle_btn->SetSourceText(" INFO ");
        m_page_info_container->m_visible = true;
        m_page_settings_container->m_visible = false;

        bg_view->SetState(m_expanded, true);
        header_title->RestartEffect();

        To(&m_width.value, m_expanded_size.x, dur, EasingType::EaseInOutQuad);
        To(&m_height.value, m_expanded_size.y, dur, EasingType::EaseInOutQuad,
            [this]() { this->bg_view->SetState(true, false); });

        collapsed_btn->To(&collapsed_btn->m_alpha, 0.0f, dur / 2);
        main_layout->To(&main_layout->m_alpha, 1.0f, dur);
    }
    else
    {
        // 收起
        main_layout->m_block_input = false;
        collapsed_btn->m_block_input = true;

        To(&m_width.value, m_collapsed_size.x, dur, EasingType::EaseInOutQuad);
        To(&m_height.value, m_collapsed_size.y, dur, EasingType::EaseInOutQuad,
            [this]() { this->bg_view->SetState(false, false); });

        collapsed_btn->To(&collapsed_btn->m_alpha, 1.0f, dur);
        main_layout->To(&main_layout->m_alpha, 0.0f, dur / 2, EasingType::EaseOutQuad,
            [this]() { this->main_layout->m_visible = false;  this->base->m_visible = false; });
    }
}

_UI_END
_SYSTEM_END
_NPGS_END