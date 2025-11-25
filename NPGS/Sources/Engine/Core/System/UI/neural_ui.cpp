#include "neural_ui.h"
#include <cmath>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN



// --- NeuralMenuController Implementation ---
NeuralMenuController::NeuralMenuController()
{
    // 1. Root Container (The Border Panel)
    root_panel = std::make_shared<TechBorderPanel>();
    root_panel->m_rect = { 20, 20, m_collapsed_size.x, m_collapsed_size.y };

    // 2. Background Particles
    bg_view = std::make_shared<NeuralParticleView>(80);
    bg_view->m_rect = { 0, 0, m_collapsed_size.x, m_collapsed_size.y }; // Will animate   
    bg_view->SetSizes(m_collapsed_size, m_expanded_size);
    root_panel->AddChild(bg_view);

    // 3. Collapsed View (The @ Button)
    collapsed_btn = std::make_shared<CollapsedMainButton>();
    collapsed_btn->m_font= UIContext::Get().m_font_small; // 获取上下文以访问字体

    collapsed_btn->m_rect = { 0, 0, m_collapsed_size.x, m_collapsed_size.y };
    collapsed_btn->on_click_callback = [this]() { this->ToggleExpand(); };
    collapsed_btn->m_alpha = 1.0f;
    root_panel->AddChild(collapsed_btn);

    // 4. Content Container (Expanded View)
    content_container = std::make_shared<UIElement>();
    content_container->m_rect = { 0, 0, m_expanded_size.x, m_expanded_size.y };
    content_container->m_alpha = 0.0f;
    content_container->m_visible = false;
    content_container->m_block_input = false;

    // Close Button
    auto close_btn = std::make_shared<NeuralButton>(" CLOSE TERMINAL ");
    close_btn->m_rect = { 15, 15, 140, 26 };
    close_btn->on_click_callback = [this]() { this->ToggleExpand(); };
    content_container->AddChild(close_btn);

    auto sep = std::make_shared<Panel>();
    const auto& theme = UIContext::Get().m_theme;
    sep->m_bg_color = theme.color_accent;
    sep->m_rect = { 15, 45, m_expanded_size.x - 30, 1 };
    content_container->AddChild(sep);

    // [修改] Layout container initialization
    layout_container = std::make_shared<ScrollView>();
    // 设置位置和尺寸：注意高度需要固定，这样才能产生滚动
    // 这里我们让它占据剩余空间：Total Height - Header Height (60) - Padding
    layout_container->m_rect = { 15, 60, m_expanded_size.x - 30, m_expanded_size.y - 70 };
    layout_container->m_padding = 8.0f;

    content_container->AddChild(layout_container);

    root_panel->AddChild(content_container);
}

void NeuralMenuController::ToggleExpand()
{
    m_expanded = !m_expanded;
    float dur = 0.2f;

    if (m_expanded)
    {
        // -- EXPANDING --
        collapsed_btn->ResetInteraction();



        content_container->ResetInteraction();
        // Enable content
        content_container->m_visible = true;
        content_container->m_block_input = true;


        bg_view->SetState(m_expanded, true);
        // Animate Dimensions
        root_panel->To(&root_panel->m_rect.w, m_expanded_size.x, dur, EasingType::EaseInOutQuad);
        root_panel->To(&root_panel->m_rect.h, m_expanded_size.y, dur, EasingType::EaseInOutQuad);


        bg_view->To(&bg_view->m_rect.w, m_expanded_size.x, dur, EasingType::EaseInOutQuad,
            [this]()
        {
            // 动画结束回调：设置动画标志为 false
            this->bg_view->SetState(true, false);
        });
        bg_view->To(&bg_view->m_rect.h, m_expanded_size.y, dur, EasingType::EaseInOutQuad);

        // Fade Out Collapsed / Fade In Content
        collapsed_btn->To(&collapsed_btn->m_alpha, 0.0f, dur / 2);
        collapsed_btn->m_block_input = false;

        content_container->To(&content_container->m_alpha, 1.0f, dur);

        // Update Layout container width to match new size
        layout_container->m_rect.w = m_expanded_size.x - 30;
        layout_container->m_rect.h = m_expanded_size.y - 70;
    }
    else
    {
        // -- COLLAPSING --
        // Animate Dimensions
        root_panel->To(&root_panel->m_rect.w, m_collapsed_size.x, dur, EasingType::EaseInOutQuad);
        root_panel->To(&root_panel->m_rect.h, m_collapsed_size.y, dur, EasingType::EaseInOutQuad);

        bg_view->To(&bg_view->m_rect.w, m_collapsed_size.x, dur, EasingType::EaseInOutQuad,
            [this]()
        {
            // 动画结束回调：设置动画标志为 false
            this->bg_view->SetState(false, false);
        });
        bg_view->To(&bg_view->m_rect.h, m_collapsed_size.y, dur, EasingType::EaseInOutQuad);
        // Fade In Collapsed
        collapsed_btn->To(&collapsed_btn->m_alpha, 1.0f, dur);
        collapsed_btn->m_block_input = true;

        // Fade Out Content
        content_container->m_block_input = false;
        content_container->To(&content_container->m_alpha, 0.0f, dur / 2, EasingType::EaseOutQuad,
            [this]() { this->content_container->m_visible = false; });
    }
}

// 在 neural_ui.cpp 中


_UI_END
_SYSTEM_END
_NPGS_END