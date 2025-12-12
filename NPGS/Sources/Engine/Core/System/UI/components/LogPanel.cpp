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
    // [REFACTOR] 使用新的API声明固定尺寸
    SetFixedSize(CARD_WIDTH, CARD_HEIGHT);

    const auto& theme = UIContext::Get().m_theme;
    ImVec4 title_col = (type == LogType::Alert) ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) : ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    ImVec4 msg_col = (type == LogType::Alert) ? ImVec4(1.0f, 0.8f, 0.8f, 1.0f) : theme.color_text_disabled;

    m_title_text = std::make_shared<TechText>(title, title_col);
    m_title_text->SetAnimMode(TechTextAnimMode::Hacker);
    m_title_text->SetHeight(Length::Fix(16.0f)); // 固定文本高度
    m_title_text->m_font = UIContext::Get().m_font_bold;
    AddChild(m_title_text);

    m_msg_text = std::make_shared<TechText>(message, msg_col);
    m_msg_text->SetHeight(Length::Fix(14.0f)); // 固定文本高度
    m_msg_text->m_font = UIContext::Get().m_font_regular;
    AddChild(m_msg_text);
}

void LogCard::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (m_type == LogType::Alert) m_pulse_timer += dt * 5.0f;

    // 简单的子元素布局
    if (m_title_text && m_msg_text)
    {
        float content_h = 32.0f;
        float start_y = (m_rect.h - content_h) * 0.5f-2.0f;
        m_title_text->m_rect.x = 12.0f;
        m_title_text->m_rect.y = start_y;
        m_msg_text->m_rect.x = 12.0f;
        m_msg_text->m_rect.y = start_y + 18.0f;
    }
    UIElement::Update(dt, parent_abs_pos);
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

ImVec2 LogPanel::Measure(const ImVec2& available_size)
{
    // 1. 基类计算 (处理 Fixed/Fill)
    ImVec2 size = UIElement::Measure(available_size);

    // 2. Content 策略委托给 m_root_vbox
    if (m_root_vbox)
    {
        // 注意：LogPanel 可能有 padding，这里简单起见假设 padding 为 0
        // 如果 LogPanel 自身有 padding，需要在这里加上
        ImVec2 content_size = m_root_vbox->Measure(available_size);

        if (m_width_policy.type == LengthType::Content) size.x = content_size.x;
        if (m_height_policy.type == LengthType::Content) size.y = content_size.y;
    }

    return size;
}

LogPanel::LogPanel(const std::string& syskey, const std::string& savekey)
{
    m_block_input = false;
    // [REFACTOR] LogPanel 自身尺寸由内部 VBox 决定
    SetWidth(Length::Fix(LogCard::CARD_WIDTH));
    SetHeight(Length::Content());

    auto& ctx = UIContext::Get();

    // 1. 创建根 VBox，它将管理所有内部元素的垂直布局
    m_root_vbox = std::make_shared<VBox>();
    m_root_vbox->SetWidth(Length::Fill());    // 宽度撑满 LogPanel
    m_root_vbox->SetHeight(Length::Content()); // 高度自适应
    m_root_vbox->m_padding = 0.0f;
    AddChild(m_root_vbox);

    // 2. 创建列表裁剪区
    m_clipping_area = std::make_shared<Panel>();
    m_clipping_area->SetWidth(Length::Fill());
    m_clipping_area->SetHeight(Length::Fix(LIST_AREA_HEIGHT));
    m_clipping_area->m_bg_color = ThemeColorID::None; // 透明背景
    m_clipping_area->m_block_input = false;
    m_root_vbox->AddChild(m_clipping_area);

    // 3. 在裁剪区内创建日志容器
    m_log_container = std::make_shared<VBox>();
    m_log_container->SetWidth(Length::Fill());
    m_log_container->SetHeight(Length::Content());
    m_log_container->m_padding = 0.0f;
    m_clipping_area->AddChild(m_log_container);

    // 4. 创建分割线
    m_divider = std::make_shared<TechDivider>(StyleColor(ThemeColorID::TextDisabled).WithAlpha(0.5f));
    m_divider->SetHeight(Length::Fix(10.0f)); // 给分割线上下的空间
    m_divider->m_visual_height = 2.0f;
    m_divider->m_align_v = Alignment::Center; // 在10px的高度里居中
    m_root_vbox->AddChild(m_divider);

    // 5. 创建页脚文本
    m_system_text = std::make_shared<TechText>(syskey, ThemeColorID::TextDisabled);
    m_system_text->SetAnimMode(TechTextAnimMode::Hacker);
    m_system_text->SetHeight(Length::Fix(14.0f));
    m_system_text->m_font = ctx.m_font_regular;
    m_root_vbox->AddChild(m_system_text);

    m_autosave_text = std::make_shared<TechText>(savekey, ThemeColorID::TextDisabled);
    m_autosave_text->SetAnimMode(TechTextAnimMode::Scroll);
    m_autosave_text->SetHeight(Length::Fix(14.0f));
    m_autosave_text->m_font = ctx.m_font_regular;
    m_root_vbox->AddChild(m_autosave_text);
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
    if (!m_log_container) return;

    auto card = std::make_shared<LogCard>(type, title, message);
    m_log_container->AddChild(card);

    // 动画逻辑保持不变，它驱动的是一个偏移量
    m_slide_offset += LogCard::CARD_HEIGHT;
}

void LogPanel::Update(float dt, const ImVec2& parent_abs_pos)
{
    auto& ctx = UIContext::Get();

    // 1. 滑动动画 (阻尼衰减)
    float speed = 10.0f;
    if (std::abs(m_slide_offset) > 0.5f)
    {
        m_slide_offset += (0.0f - m_slide_offset) * speed * dt;
    }
    else
    {
        m_slide_offset = 0.0f;
    }

    // 2. 清理逻辑
    if (m_log_container)
    {
        while (m_log_container->m_children.size() > MAX_LOG_COUNT)
        {
            // [REFACTOR] 逻辑简化
            // 只要动画偏移量足够小，并且条目超出，就可以安全移除
            // 这里的判断条件可以根据视觉效果微调
            if (m_slide_offset < 1.0f)
            {
                auto oldest = m_log_container->m_children.begin();
                m_log_container->RemoveChild(*oldest);
            }
            else
            {
                break; // 动画仍在进行，等待下一帧
            }
        }
    }

    // 3. 绝对布局计算 (保持不变，用于整个面板的定位)
    float margin_left = 20.0f;
    float margin_bottom = 20.0f;
    // [REFACTOR] 总高度现在由 m_root_vbox 自动计算
    m_rect.h = m_root_vbox->Measure({ m_rect.w, 0 }).y;
    m_rect.x = margin_left;
    m_rect.y = ctx.m_display_size.y - margin_bottom - m_rect.h;

    // 4. 更新自身和子元素
    // 首先更新自身位置
    UpdateSelf(dt, parent_abs_pos);

    // [REFACTOR] m_root_vbox 会处理所有内部布局
    // 我们只需要手动应用滑动动画
    if (m_log_container)
    {
        // 测量 log_container 的实际高度
        float real_list_h = m_log_container->Measure({ m_clipping_area->m_rect.w, 0 }).y;
        // 底部对齐 + 动画偏移
        m_log_container->m_rect.y = (LIST_AREA_HEIGHT - real_list_h) + m_slide_offset;
    }

    // [REFACTOR] 调用根 VBox 的 Update，它会自动排列所有子元素
    if (m_root_vbox)
    {
        m_root_vbox->Update(dt, m_absolute_pos);
    }
}
void LogPanel::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;


    if (m_divider) m_divider->Draw(dl);
    if (m_system_text) m_system_text->Draw(dl);
    if (m_autosave_text) m_autosave_text->Draw(dl);

    // 2. 手动设置裁剪区并绘制列表内容
    if (m_clipping_area && m_log_container)
    {
        ImVec2 clip_min = m_clipping_area->m_absolute_pos;
        ImVec2 clip_max = ImVec2(clip_min.x + m_clipping_area->m_rect.w, clip_min.y + m_clipping_area->m_rect.h);

        dl->PushClipRect(clip_min, clip_max, true);
        m_log_container->Draw(dl);
        dl->PopClipRect();
    }
}
_UI_END
_SYSTEM_END
_NPGS_END