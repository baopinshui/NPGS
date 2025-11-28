#include "NeuralMenuController.h" 

#include <cmath>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// --- NeuralMenuController Implementation ---
NeuralMenuController::NeuralMenuController()
{
    // 1. Root Container (The Border Panel)
    // [视觉调整] 稍微加大初始尺寸，确保展开后看起来更宽敞
    m_expanded_size = { 340, 300 };

    root_panel = std::make_shared<TechBorderPanel>();
    root_panel->m_rect = { 20, 20, m_collapsed_size.x, m_collapsed_size.y };
    root_panel->m_thickness = 2.0f;
    root_panel->m_use_glass_effect = true;

    // 2. Background Particles
    bg_view = std::make_shared<NeuralParticleView>(80);
    bg_view->m_rect = { 0, 0, m_collapsed_size.x, m_collapsed_size.y };
    bg_view->SetSizes(m_collapsed_size, m_expanded_size);
    root_panel->AddChild(bg_view);

    // 3. Collapsed View (The @ Button)
    collapsed_btn = std::make_shared<CollapsedMainButton>();
    collapsed_btn->m_font = UIContext::Get().m_font_small;
    collapsed_btn->m_rect = { 0, 0, m_collapsed_size.x, m_collapsed_size.y };
    collapsed_btn->on_click_callback = [this]() { this->ToggleExpand(); };
    collapsed_btn->m_alpha = 1.0f;
    root_panel->AddChild(collapsed_btn);

    // 4. 主布局容器 (Expanded View) - 使用VBox进行全自动布局
    main_layout = std::make_shared<VBox>();
    main_layout->m_rect = { 15, 15, m_expanded_size.x - 30, m_expanded_size.y - 30 }; // VBox在父容器内留出边距
    main_layout->m_alpha = 0.0f;
    main_layout->m_visible = false;
    main_layout->m_block_input = false;
    main_layout->m_padding = 8.0f; // 设置主布局内部各组件的垂直间距

    const auto& theme = UIContext::Get().m_theme;

    // --- [布局重构 Start] ---

    // A. 顶部标题 (Header) - 替代原来的关闭按钮位置，平衡视觉
    auto header_title = std::make_shared<NeuralButton>(" > SETTINGS ");
    header_title->m_rect.h = 20.0f;           // 只需定义高度
    header_title->m_rect.w = 120.0f;          // 定义宽度，用于对齐
    header_title->m_align_h = Alignment::Start; // 水平左对齐
    header_title->m_block_input = false;      // 设为不可交互
    main_layout->AddChild(header_title);

    // 装饰性线条
    auto sep_top = std::make_shared<Panel>();
    sep_top->m_bg_color = theme.color_accent;
    sep_top->m_rect.h = 1.0f;                 // 只需定义高度，宽度会因VBox的默认拉伸而自动填满
    main_layout->AddChild(sep_top);


    // B. 中间滚动区域
    scroll_view = std::make_shared<ScrollView>();
    scroll_view->m_fill_v = true;             // [核心] 告诉VBox，这个控件要填满所有剩余的垂直空间
    main_layout->AddChild(scroll_view);

    // 用于滚动内容的VBox (这个逻辑不变)
    content_vbox = std::make_shared<VBox>();
    content_vbox->m_padding = 10.0f;
    scroll_view->AddChild(content_vbox);

    //layout_container = std::make_shared<ScrollView>();
    //// [位置调整] 顶部留出 45px (标题)，底部留出 40px (关闭按钮)
    //// 宽度两边各留 15px，显得不那么拥挤
    //layout_container->m_rect = { 20, 48, m_expanded_size.x - 40, m_expanded_size.y - 48 - 70 };
    //layout_container->m_padding = 10.0f; // 增加控件间距
    //content_container->AddChild(layout_container);
    //
    // 装饰性线条 - 底部按钮上方
    // 装饰性线条
    auto sep_bot = std::make_shared<Panel>();
    sep_bot->m_bg_color = theme.color_accent;
    sep_bot->m_rect.h = 1.0f;                 // 只需定义高度
    main_layout->AddChild(sep_bot);


    // C. 底部关闭按钮 (Footer)
    auto close_btn = std::make_shared<NeuralButton>(" CLOSE TERMINAL ");
    close_btn->m_rect.h = 34.0f;              // 只需定义高度
    close_btn->m_rect.w = 300.0f;             // 定义宽度，用于对齐
    close_btn->m_align_h = Alignment::Center; // 水平居中对齐
    close_btn->on_click_callback = [this]() { this->ToggleExpand(); };
    main_layout->AddChild(close_btn);

    // --- [布局重构 End] ---

    root_panel->AddChild(main_layout);
}
void NeuralMenuController::ToggleExpand()
{
    m_expanded = !m_expanded;
    float dur = 0.25f; // 稍微放慢一点动画，更有质感

    if (m_expanded)
    {
        // -- EXPANDING --
        collapsed_btn->ResetInteraction();
        main_layout->ResetInteraction();
        main_layout->m_visible = true;
        main_layout->m_block_input = true;

        bg_view->SetState(m_expanded, true);

        // 动画
        root_panel->To(&root_panel->m_rect.w, m_expanded_size.x, dur, EasingType::EaseInOutQuad);
        root_panel->To(&root_panel->m_rect.h, m_expanded_size.y, dur, EasingType::EaseInOutQuad);

        bg_view->To(&bg_view->m_rect.w, m_expanded_size.x, dur, EasingType::EaseInOutQuad,
            [this]() { this->bg_view->SetState(true, false); });
        bg_view->To(&bg_view->m_rect.h, m_expanded_size.y, dur, EasingType::EaseInOutQuad);

        collapsed_btn->To(&collapsed_btn->m_alpha, 0.0f, dur / 2);
        collapsed_btn->m_block_input = false;

        main_layout->To(&main_layout->m_alpha, 1.0f, dur);

        // [核心修改] 动态更新 Layout 尺寸以匹配新的展开尺寸
        // 保持 header(48) 和 footer(40) 的预留空间
        scroll_view->m_rect.w = m_expanded_size.x - 40;
        scroll_view->m_rect.h = m_expanded_size.y - 48 - 70;
    }
    else
    {
        // -- COLLAPSING --
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
// 在 neural_ui.cpp 中


_UI_END
_SYSTEM_END
_NPGS_END