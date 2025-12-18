// --- START OF FILE LogPanel.cpp ---
#include "LogPanel.h"
#include "../TechUtils.h"
#include <cmath>
#include <algorithm>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// =================================================================================
// LogCard 实现
// =================================================================================

LogCard::LogCard(LogType type, const std::string& title, const std::string& message)
    : m_type(type)
{
    m_block_input = false;
    m_width = Length::Stretch();
    m_height = Length::Fixed(CARD_HEIGHT);
    m_alpha = 1.0f;

    const auto& theme = UIContext::Get().m_theme;
    ImVec4 title_col = (type == LogType::Alert) ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) : ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    ImVec4 msg_col = (type == LogType::Alert) ? ImVec4(1.0f, 0.8f, 0.8f, 1.0f) : theme.color_text_disabled;

    // [NEW] 使用VBox来自动布局内部文本，并设置对齐
    m_content_box = std::make_shared<VBox>();
    m_content_box->m_padding = 2.0f;
    m_content_box->m_width = Length::Stretch();
    m_content_box->m_height = Length::Stretch();
    m_content_box->m_align_v = Alignment::Center; // 垂直居中整个VBox
    m_content_box->m_align_h = Alignment::Center; // 水平居中整个VBox
    AddChild(m_content_box);

    m_title_text = std::make_shared<TechText>(title, title_col, true);
    m_title_text->SetName("title"); // << NAME ADDED
    m_title_text->m_font = UIContext::Get().m_font_bold;
    m_title_text->m_width = Length::Stretch(); // 文本宽度撑满
    m_title_text->m_height = Length::Content();
    m_content_box->AddChild(m_title_text);

    m_msg_text = std::make_shared<TechText>(message, msg_col, false);
    m_msg_text->SetName("message"); // << NAME ADDED
    m_msg_text->m_font = UIContext::Get().m_font_regular;
    m_msg_text->m_width = Length::Stretch();
    m_msg_text->m_height = Length::Content();
    m_content_box->AddChild(m_msg_text);
}

void LogCard::Update(float dt)
{
    if (m_type == LogType::Alert) m_pulse_timer += dt * 5.0f;
    // 调用基类Update，它会递归更新子元素（如TechText的动画）
    UIElement::Update(dt);
}

void LogCard::Arrange(const Rect& final_rect)
{
    // [NEW] 手动为内容区域添加内边距
    UIElement::Arrange(final_rect);
    Rect content_rect = { 12.0f, 11.0f, m_rect.w - 12.0f, m_rect.h-22.0f };
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
        float pulse = (std::sin(0.5*m_pulse_timer) + 1.0f) * 0.5f;
        base_col = red_accent;
        base_col.w = 0.1f + 0.2f * pulse;
        border_col = TechUtils::LerpColor(red_accent, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), pulse * 0.5f);
    }
    else
    {
        base_col = theme.color_accent;
        base_col.w = 0.1f;
        border_col = theme.color_accent;
    }

    ImVec2 p_min = m_absolute_pos;
    ImVec2 p_max = ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h);
    float margin_y = 4.0f;
    p_min.y += margin_y;
    p_max.y -= margin_y;

    DrawGlassBackground(dl, p_min, p_max, base_col);
    dl->AddRectFilled(p_min, ImVec2(p_min.x + 4.0f, p_max.y), GetColorWithAlpha(border_col, 1.0f));
    UIElement::Draw(dl);
}

// =================================================================================
// LogPanel 实现
// =================================================================================

LogPanel::LogPanel(const std::string& syskey, const std::string& savekey)
{
    m_block_input = false;
    m_width = Length::Fixed(260.0f);
    m_height = Length::Content(); // 高度由内容决定

    // 子组件全部直接添加到 LogPanel
    m_list_box = std::make_shared<VBox>();
    m_list_box->SetName("logList"); // << NAME ADDED
    m_list_box->m_padding = 0.0f;
    m_list_box->m_width = Length::Stretch();
    // 高度由子项（LogCard）决定，所以是Content
    m_list_box->m_height = Length::Content();
    AddChild(m_list_box);

    m_divider = std::make_shared<TechDivider>(StyleColor(ThemeColorID::TextDisabled).WithAlpha(0.5f));
    m_divider->m_height = Length::Fixed(DIVIDER_AREA_HEIGHT); // 分割线占据固定高度区域
    AddChild(m_divider);

    m_footer_box = std::make_shared<VBox>();
    m_footer_box->m_padding = 2.0f;
    m_footer_box->m_width = Length::Stretch();
    m_footer_box->m_height = Length::Content();

    m_system_text = std::make_shared<TechText>(syskey, ThemeColorID::TextDisabled, true);
    m_system_text->SetName("systemStatus"); // << NAME ADDED
    m_system_text->SetSizing(TechTextSizingMode::AutoHeight); // 自动换行
    m_system_text->m_width = Length::Stretch(); // 宽度撑满
    m_system_text->m_height = Length::Content();

    m_autosave_text = std::make_shared<TechText>(savekey, ThemeColorID::TextDisabled, true);
    m_autosave_text->SetName("autosaveStatus"); // << NAME ADDED
    m_autosave_text->SetSizing(TechTextSizingMode::AutoWidthHeight);
    m_autosave_text->SetAnimMode(TechTextAnimMode::Scroll);
    m_autosave_text->m_width = Length::Stretch();
    m_autosave_text->m_height = Length::Content();

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
    if (!m_list_box) return;

    auto card = std::make_shared<LogCard>(type, title, message);
    m_list_box->AddChild(card);

    // 累加向上滑动偏移量，保证新日志从底部平滑进入
    m_slide_offset += LogCard::CARD_HEIGHT;
}

void LogPanel::Update(float dt)
{
    // 1. 滑动动画 (阻尼衰减)
    if (std::abs(m_slide_offset) > 0.1f)
    {
        float speed = 10.0f;
        m_slide_offset += (0.0f - m_slide_offset) * std::min(1.0f, speed * dt);
    }
    else
    {
        m_slide_offset = 0.0f;
    }

    // 2. 清理超出上限且已移出视野的日志
    if (m_list_box)
    {
        float item_h = LogCard::CARD_HEIGHT;

        // 循环检查头部，直到数量合规或头部仍在可视区内
        while (m_list_box->m_children.size() > MAX_LOG_COUNT)
        {
            size_t count = m_list_box->m_children.size();

            // 计算当前列表顶部的相对 Y 坐标
            // 列表底部对齐 Y = LIST_AREA_HEIGHT
            // 列表实际高度 = count * item_h
            // 列表顶部 Y = (LIST_AREA_HEIGHT - 列表实际高度) + 当前滑动偏移
            float list_visual_top_y = (LIST_AREA_HEIGHT - (count * item_h)) + m_slide_offset;

            // 最旧条目(顶部)的底边 Y 坐标
            float oldest_item_bottom_y = list_visual_top_y + item_h;

            // 判定：如果最旧条目的底边已经跑到了 0 (可视区顶部) 以上，说明完全不可见
            // 容差值 0.5f 防止浮点抖动
            if (oldest_item_bottom_y < 0.5f)
            {
                // 1. 物理删除
                auto oldest = m_list_box->m_children.begin();
                (*oldest)->m_parent = nullptr;
                m_list_box->m_children.erase(oldest);

            }
            else
            {
                // 最旧的条目还在可视范围内（正在滑出），暂停删除，等待下一帧动画
                break;
            }
        }
    }

    // 3. 递归更新子元素
    UIElement::Update(dt);
}


ImVec2 LogPanel::Measure(ImVec2 available_size)
{
    if (!m_visible) return { 0, 0 };

    // 测量页脚和分割线以确定它们的固定高度贡献
    m_divider->Measure(available_size);
    m_footer_box->Measure(available_size);

    float total_h = LIST_AREA_HEIGHT + m_divider->m_desired_size.y + m_footer_box->m_desired_size.y;

    m_desired_size = { m_width.value, total_h };
    return m_desired_size;
}

void LogPanel::Arrange(const Rect& final_rect)
{
    // [修改] 移除自己计算屏幕左下角位置的逻辑
    // 1. 使用父级传入的、已经计算好的位置来设置自身
    UIElement::Arrange(final_rect);

    // 2. 手动排列子元素 (这部分逻辑保持不变，因为是内部布局)
    float current_y = 0.0f;

    // A. 排列日志列表 (应用滑动动画)
    // 列表的实际高度由其内容决定
    float real_list_h = m_list_box->Measure({ m_rect.w, FLT_MAX }).y;
    // 计算Y坐标，使其底部对齐到列表区域底部，并应用滑动偏移
    float list_y = (LIST_AREA_HEIGHT - real_list_h) + m_slide_offset;
    m_list_box->Arrange({ 0.0f, list_y, m_rect.w, real_list_h });
    current_y += LIST_AREA_HEIGHT;

    // B. 排列分割线
    m_divider->Arrange({ 0.0f, current_y, m_rect.w, m_divider->m_desired_size.y });
    current_y += m_divider->m_rect.h;

    // C. 排列页脚
    m_footer_box->Arrange({ 0.0f, current_y, m_rect.w, m_footer_box->m_desired_size.y });
}


void LogPanel::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    // [核心] 仅对日志列表区域进行裁剪
    // 使用 m_absolute_pos，这是在 Arrange 阶段计算好的
    ImVec2 clip_min = m_absolute_pos;
    ImVec2 clip_max = ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + LIST_AREA_HEIGHT);

    dl->PushClipRect(clip_min, clip_max, true);
    if (m_list_box) m_list_box->Draw(dl);
    dl->PopClipRect();

    // 绘制无需裁剪的页脚和分割线
    if (m_divider) m_divider->Draw(dl);
    if (m_footer_box) m_footer_box->Draw(dl);
}

_UI_END
_SYSTEM_END
_NPGS_END