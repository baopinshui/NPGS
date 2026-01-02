// --- START OF FILE LogPanel.cpp ---
#include "LogPanel.h"
#include "../TechUtils.h"
#include <cmath>
#include <algorithm>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// =================================================================================
// LogCard 实现 (保持不变，已是最佳状态)
// =================================================================================

LogCard::LogCard(LogType type, const std::string& title, const std::string& message)
    : m_type(type)
{
    m_block_input = true;
    m_width = Length::Stretch();
    m_height = Length::Fixed(BASE_HEIGHT);
    m_current_height = BASE_HEIGHT;
    m_alpha = 1.0f;

    const auto& theme = UIContext::Get().m_theme;
    ImVec4 title_col = (type == LogType::Alert) ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) : ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    ImVec4 msg_col = (type == LogType::Alert) ? ImVec4(1.0f, 0.8f, 0.8f, 1.0f) : theme.color_text_disabled;

    m_content_box = std::make_shared<VBox>();
    m_content_box->m_padding = 2.0f;
    m_content_box->m_width = Length::Stretch();
    m_content_box->m_height = Length::Stretch();
    m_content_box->m_align_v = Alignment::Center;
    m_content_box->m_block_input = false;
    AddChild(m_content_box);

    m_title_text = std::make_shared<TechText>(title, title_col, true);
    m_title_text->SetName("title");
    m_title_text->m_font = UIContext::Get().m_font_bold;
    m_title_text->m_width = Length::Stretch();
    m_title_text->m_height = Length::Content();
    m_title_text->m_block_input = false;
    m_content_box->AddChild(m_title_text);

    m_msg_text = std::make_shared<TechText>(message, msg_col, false);
    m_msg_text->SetName("message");
    m_msg_text->m_font = UIContext::Get().m_font_regular;
    m_msg_text->m_width = Length::Stretch();
    m_msg_text->SetSizing(TechTextSizingMode::AutoWidthHeight);
    m_msg_text->m_block_input = false;
    m_content_box->AddChild(m_msg_text);
}

void LogCard::Update(float dt)
{
    if (m_type == LogType::Alert) m_pulse_timer += dt * 5.0f;

    // 保护：确保宽度有效，防止初始化时的奇怪数值
    float safe_width = m_rect.w > 10.0f ? m_rect.w : 260.0f;
    float content_padding_x = 24.0f;
    float content_padding_y = 22.0f;

    if (m_hovered)
    {
        m_msg_text->SetSizing(TechTextSizingMode::AutoHeight);
        m_content_box->m_align_v = Alignment::Start;
        ImVec2 full_size = m_content_box->Measure({ safe_width - content_padding_x, FLT_MAX });
        m_target_height = std::max(BASE_HEIGHT, full_size.y + content_padding_y);
    }
    else
    {
        m_msg_text->SetSizing(TechTextSizingMode::AutoWidthHeight);
        m_content_box->m_align_v = Alignment::Center;
        m_target_height = BASE_HEIGHT;
    }

    float diff = m_target_height - m_current_height;
    if (std::abs(diff) > 0.1f)
        m_current_height += diff * std::min(1.0f, m_expand_speed * dt);
    else
        m_current_height = m_target_height;

    m_height = Length::Fixed(m_current_height);
    UIElement::Update(dt);
}

ImVec2 LogCard::Measure(ImVec2 available_size)
{
    UIElement::Measure(available_size);
    m_desired_size.y = m_current_height;
    m_desired_size.x = (m_width.IsFixed()) ? m_width.value : available_size.x;
    return m_desired_size;
}

void LogCard::Arrange(const Rect& final_rect)
{
    UIElement::Arrange(final_rect);
    Rect content_rect = { 12.0f, 11.0f, m_rect.w - 12.0f, m_rect.h - 11.0f };
    m_content_box->Arrange(content_rect);
}

void LogCard::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    const auto& theme = UIContext::Get().m_theme;
    ImVec4 base_col, border_col;

    if (m_type == LogType::Alert)
    {
        ImVec4 red_accent(1.0f, 0.2f, 0.2f, 1.0f);
        float pulse = (std::sin(0.5f * m_pulse_timer) + 1.0f) * 0.5f;
        base_col = red_accent;
        base_col.w = 0.1f + 0.2f * pulse;
        border_col = TechUtils::LerpColor(red_accent, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), pulse * 0.5f);
    }
    else
    {
        base_col = theme.color_accent;
        base_col.w = 0.1f;
        border_col = theme.color_accent;
        if (m_hovered)
        {
            base_col.w = 0.2f;
            border_col.w = 1.0f;
        }
        else
        {
            border_col.w = 0.5f;
        }
    }

    ImVec2 p_min = m_absolute_pos;
    ImVec2 p_max = ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h);
    float margin_y = 2.0f;
    p_min.y += margin_y;
    p_max.y -= margin_y;

    DrawGlassBackground(dl, p_min, p_max, base_col);
    dl->AddRectFilled(p_min, ImVec2(p_min.x + 4.0f, p_max.y), GetColorWithAlpha(border_col, 1.0f));

    dl->PushClipRect(p_min, p_max, true);
    UIElement::Draw(dl);
    dl->PopClipRect();
}

// =================================================================================
// LogPanel 实现
// =================================================================================

LogPanel::LogPanel(const std::string& syskey, const std::string& savekey)
{
    m_block_input = false;
    m_width = Length::Fixed(260.0f);
    m_height = Length::Content();

    m_scroll_view = std::make_shared<ScrollView>();
    m_scroll_view->SetName("logScroll");
    m_scroll_view->m_width = Length::Stretch();
    m_scroll_view->m_height = Length::Fixed(LIST_VISIBLE_HEIGHT);
    m_scroll_view->m_show_scrollbar = false;
    m_scroll_view->m_smoothing_speed = 15.0f; // 保持和 visual_offset 衰减一致
    AddChild(m_scroll_view);

    m_list_content = std::make_shared<VBox>();
    m_list_content->SetName("logList");
    m_list_content->m_padding = 0.0f;
    m_list_content->m_width = Length::Stretch();
    m_list_content->m_height = Length::Content();
    m_scroll_view->AddChild(m_list_content);

    m_top_spacer = std::make_shared<UIElement>();
    m_top_spacer->SetName("spacer");
    m_top_spacer->m_width = Length::Stretch();
    m_top_spacer->m_height = Length::Fixed(0.0f);
    m_list_content->AddChild(m_top_spacer);

    m_divider = std::make_shared<TechDivider>(StyleColor(ThemeColorID::TextDisabled).WithAlpha(0.5f));
    m_divider->m_height = Length::Fixed(DIVIDER_AREA_HEIGHT);
    AddChild(m_divider);

    m_footer_box = std::make_shared<VBox>();
    m_footer_box->m_padding = 2.0f;
    m_footer_box->m_width = Length::Stretch();
    m_footer_box->m_height = Length::Content();

    m_system_text = std::make_shared<TechText>(syskey, ThemeColorID::TextDisabled, true);
    m_system_text->SetSizing(TechTextSizingMode::AutoHeight);
    m_system_text->m_width = Length::Stretch();

    m_autosave_text = std::make_shared<TechText>(savekey, ThemeColorID::TextDisabled, true);
    m_autosave_text->SetSizing(TechTextSizingMode::ForceAutoWidthHeight);
    m_autosave_text->SetAnimMode(TechTextAnimMode::Scroll);
    m_autosave_text->m_width = Length::Stretch();

    m_footer_box->AddChild(m_system_text);
    m_footer_box->AddChild(m_autosave_text);
    AddChild(m_footer_box);
}

void LogPanel::SetSystemStatus(const std::string& text)
{
    if (m_system_text) m_system_text->SetSourceText(text);
}

void LogPanel::SetAutoSaveTime(const std::string& text)
{
    if (m_autosave_text) m_autosave_text->SetSourceText(text);
}

void LogPanel::AddLog(LogType type, const std::string& title, const std::string& message)
{
    if (!m_list_content) return;

    auto card = std::make_shared<LogCard>(type, title, message);
    card->m_alpha = 0.0f;
    float* alpha_ptr = &card->m_alpha;
    card->To(alpha_ptr, 1.0f, 0.3f, EasingType::EaseOutQuad);

    m_list_content->AddChild(card);
}

void LogPanel::Update(float dt)
{
    // --- 1. 视觉偏移衰减 ---
    // 这个逻辑必须在 Layout 之前完成，它决定了 Draw 时的最终位置
    if (std::abs(m_visual_offset_y) > 0.01f)
    {
        // 速度需匹配 ScrollView 的 Smoothing Speed (15.0f)
        float speed = 15.0f;
        m_visual_offset_y += (0.0f - m_visual_offset_y) * std::min(1.0f, speed * dt);
    }
    else
    {
        m_visual_offset_y = 0.0f;
    }

    // --- 2. 自动滚动检测 ---
    // 使用上一帧的测量数据判断意图
    float content_h = m_scroll_view->m_content_height;
    float view_h = LIST_VISIBLE_HEIGHT;
    float max_scroll = std::max(0.0f, content_h - view_h);

    if (m_scroll_view->m_hovered)
    {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel > 0.0f) m_auto_scroll = false;
        else if (wheel < 0.0f && m_scroll_view->m_target_scroll_y >= max_scroll - 10.0f) m_auto_scroll = true;
    }

    if (m_auto_scroll)
    {
        m_scroll_view->m_target_scroll_y = 1000000.0f;
    }

    // 调用基类更新
    UIElement::Update(dt);
}

ImVec2 LogPanel::Measure(ImVec2 available_size)
{
    if (!m_visible) return { 0, 0 };

    // --- 1. 预先测量卡片 & 计算 Spacer (解决一帧抖动的核心) ---
    // 我们必须确保 Spacer 的大小是基于【当前帧所有子元素】的最新尺寸计算的

    float total_cards_h = 0.0f;
    float safe_width = (m_width.IsFixed()) ? m_width.value : available_size.x;

    // 遍历 VBox 的子元素 (跳过第一个 Spacer)
    for (size_t i = 1; i < m_list_content->m_children.size(); ++i)
    {
        auto& child = m_list_content->m_children[i];
        // 关键：手动调用 Measure 以获取最新高度 (包括刚 AddLog 进来的元素)
        child->Measure(ImVec2(safe_width, FLT_MAX));
        total_cards_h += child->m_desired_size.y;
        if (i > 1) total_cards_h += m_list_content->m_padding;
    }

    float view_h = LIST_VISIBLE_HEIGHT;
    float required_spacer = std::max(0.0f, view_h - total_cards_h);
    float current_spacer = m_top_spacer->m_height.value;

    // --- 2. 注入瞬时补偿脉冲 ---
    // 如果计算出 Spacer 本帧需要变小 X，说明内容增加了 X。
    // 我们给 visual_offset 增加 X，使得内容在视觉上保持原位。
    float spacer_diff = current_spacer - required_spacer;
    if (spacer_diff > 0.01f)
    {
        m_visual_offset_y += spacer_diff;
    }

    // --- 3. 应用新的布局参数 ---
    m_top_spacer->m_height = Length::Fixed(required_spacer);

    // --- 4. 常规测量 ---
    // Spacer 已经更新，ScrollView::Measure 会使用正确的高度进行后续计算
    m_divider->Measure(available_size);
    m_footer_box->Measure(available_size);
    m_scroll_view->Measure(available_size);

    float total_h = m_scroll_view->m_desired_size.y + m_divider->m_desired_size.y + m_footer_box->m_desired_size.y;
    m_desired_size = { m_width.value, total_h };
    return m_desired_size;
}

void LogPanel::Arrange(const Rect& final_rect)
{
    // 1. 常规 Arrange
    // 此时 ScrollView 会根据 Measure 阶段定好的 Spacer 高度，将内容排列好
    UIElement::Arrange(final_rect);

    // 2. 应用视觉补偿
    // 我们不破坏 ScrollView 内部的逻辑，而是基于它Arrange后的结果，整体偏移 m_list_content
    // 这是一个局部的 "Post-Arrange" 操作
    if (m_list_content && std::abs(m_visual_offset_y) > 0.001f)
    {
        // 获取 ScrollView 刚刚计算出的 rect (包含了 -scroll_y)
        Rect current_rect = m_list_content->m_rect;

        // 叠加我们的视觉偏移
        Rect offset_rect = current_rect;
        offset_rect.y += m_visual_offset_y;

        // 再次 Arrange VBox，这会递归更新所有 LogCard 的绝对位置 (m_absolute_pos)
        // 这样 Draw 阶段就能画在正确的位置上
        m_list_content->Arrange(offset_rect);

        // 注意：m_rect 最好复原，以免影响后续逻辑（虽然在这里下一帧会重算），
        // 但为了 m_absolute_pos 正确，Arrange 必须用 offset_rect。
        // 为了保持数据纯洁性，我们可以把 m_rect 改回来，但通常 UIElement 
        // 并不依赖上一帧的 m_rect.y 做逻辑，所以这里是安全的。
        // 为了保险，我们将 VBox 的 m_rect.y 设回逻辑值，但保留 m_absolute_pos。
        m_list_content->m_rect = current_rect;
    }

    float current_y = m_scroll_view->m_desired_size.y;
    m_divider->Arrange({ 0.0f, current_y, m_rect.w, m_divider->m_desired_size.y });
    current_y += m_divider->m_desired_size.y;
    m_footer_box->Arrange({ 0.0f, current_y, m_rect.w, m_footer_box->m_desired_size.y });
}

void LogPanel::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;
    UIElement::Draw(dl);
}

_UI_END
_SYSTEM_END
_NPGS_END