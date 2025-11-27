#include "neural_ui.h"
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

    // 4. Content Container (Expanded View)
    content_container = std::make_shared<UIElement>();
    content_container->m_rect = { 0, 0, m_expanded_size.x, m_expanded_size.y };
    content_container->m_alpha = 0.0f;
    content_container->m_visible = false;
    content_container->m_block_input = false;

    // --- [布局重构 Start] ---

    // A. 顶部标题 (Header) - 替代原来的关闭按钮位置，平衡视觉
    auto header_title = std::make_shared<NeuralButton>(" > SETTINGS ");
    header_title->m_rect = { 20, 15, 120, 20 }; // 左上角
    header_title->m_block_input = false; // 纯展示，不可点
    // 稍微定制一下标题的样式（可选：可以在NeuralButton里加个flag，这里简单复用）
    content_container->AddChild(header_title);

    // 装饰性线条 - 标题下方
    auto sep_top = std::make_shared<Panel>();
    const auto& theme = UIContext::Get().m_theme;
    sep_top->m_bg_color = theme.color_accent;
    sep_top->m_rect = { 20, 40, m_expanded_size.x - 40, 1 };
    content_container->AddChild(sep_top);

    // B. 中间滚动区域 (ScrollView)
    //     scroll_view = std::make_shared<ScrollView>();
    // 确定 ScrollView 的绝对显示区域 (窗口大小)
    // 依然使用绝对布局定位 ScrollView 本身
    scroll_view = std::make_shared<ScrollView>();
    scroll_view->m_rect = { 20, 48, m_expanded_size.x - 40, m_expanded_size.y - 48 - 70 };
    scroll_view->m_show_scrollbar = true; // 可选
    content_container->AddChild(scroll_view);

    // [新增] 创建 VBox 负责堆叠
    content_vbox = std::make_shared<VBox>();
    content_vbox->m_padding = 10.0f; // 控件间距
    content_vbox->m_align_h = Alignment::Stretch; // 让 Slider 自动填满宽度
    scroll_view->AddChild(content_vbox);

    //layout_container = std::make_shared<ScrollView>();
    //// [位置调整] 顶部留出 45px (标题)，底部留出 40px (关闭按钮)
    //// 宽度两边各留 15px，显得不那么拥挤
    //layout_container->m_rect = { 20, 48, m_expanded_size.x - 40, m_expanded_size.y - 48 - 70 };
    //layout_container->m_padding = 10.0f; // 增加控件间距
    //content_container->AddChild(layout_container);
    //
    // 装饰性线条 - 底部按钮上方
    auto sep_bot = std::make_shared<Panel>();
    sep_bot->m_bg_color = theme.color_accent;
    sep_bot->m_rect = { 20, m_expanded_size.y - 65, m_expanded_size.x - 40, 1 };
    content_container->AddChild(sep_bot);
    
    // C. 底部关闭按钮 (Footer) - [核心修改：下移至底部居中]
    float close_btn_w = 300.0f;
    float close_btn_x = (m_expanded_size.x - close_btn_w) * 0.5f; // 居中计算
    auto close_btn = std::make_shared<NeuralButton>(" CLOSE TERMINAL ");
    close_btn->m_rect = { close_btn_x, m_expanded_size.y - 50, close_btn_w, 34 };
    close_btn->on_click_callback = [this]() { this->ToggleExpand(); };
    content_container->AddChild(close_btn);

    // --- [布局重构 End] ---

    root_panel->AddChild(content_container);
}
void NeuralMenuController::ToggleExpand()
{
    m_expanded = !m_expanded;
    float dur = 0.25f; // 稍微放慢一点动画，更有质感

    if (m_expanded)
    {
        // -- EXPANDING --
        collapsed_btn->ResetInteraction();
        content_container->ResetInteraction();
        content_container->m_visible = true;
        content_container->m_block_input = true;

        bg_view->SetState(m_expanded, true);

        // 动画
        root_panel->To(&root_panel->m_rect.w, m_expanded_size.x, dur, EasingType::EaseInOutQuad);
        root_panel->To(&root_panel->m_rect.h, m_expanded_size.y, dur, EasingType::EaseInOutQuad);

        bg_view->To(&bg_view->m_rect.w, m_expanded_size.x, dur, EasingType::EaseInOutQuad,
            [this]() { this->bg_view->SetState(true, false); });
        bg_view->To(&bg_view->m_rect.h, m_expanded_size.y, dur, EasingType::EaseInOutQuad);

        collapsed_btn->To(&collapsed_btn->m_alpha, 0.0f, dur / 2);
        collapsed_btn->m_block_input = false;

        content_container->To(&content_container->m_alpha, 1.0f, dur);

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

        content_container->m_block_input = false;
        content_container->To(&content_container->m_alpha, 0.0f, dur / 2, EasingType::EaseOutQuad,
            [this]() { this->content_container->m_visible = false; });
    }
}
// 在 neural_ui.cpp 中


_UI_END
_SYSTEM_END
_NPGS_END