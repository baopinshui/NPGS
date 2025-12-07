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

    const auto& theme = UIContext::Get().m_theme;
    ImVec4 title_col = (type == LogType::Alert) ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) : ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    ImVec4 msg_col = (type == LogType::Alert) ? ImVec4(1.0f, 0.8f, 0.8f, 1.0f) : theme.color_text_disabled;

    m_title_text = std::make_shared<TechText>(title, title_col, true);
    m_title_text->m_font = UIContext::Get().m_font_bold;
    m_title_text->m_rect.h = 16.0f;
    AddChild(m_title_text);

    m_msg_text = std::make_shared<TechText>(message, msg_col, false);
    m_msg_text->m_font = UIContext::Get().m_font_regular;
    m_msg_text->m_rect.h = 14.0f;
    AddChild(m_msg_text);

    m_rect.h = CARD_HEIGHT;
    m_rect.w = 260.0f;
    m_alpha = 1.0f;
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

LogPanel::LogPanel(const std::string& sysstr, const std::string& savestr )
{
    m_block_input = false;
    m_rect.w = 260.0f;

    const auto& theme = UIContext::Get().m_theme;
    auto& ctx = UIContext::Get();

    m_list_box = std::make_shared<VBox>();
    m_list_box->m_padding = 0.0f;
    m_list_box->m_fill_h = true;
    AddChild(m_list_box);

    m_divider = std::make_shared<TechDivider>();
    m_divider->m_color = theme.color_text_disabled;
    m_divider->m_color.w = 0.5f;
    m_divider->m_rect.h = 2.0f;
    AddChild(m_divider);

    m_footer_box = std::make_shared<VBox>();
    m_footer_box->m_padding = 2.0f;

    // 3. 底部静态信息
    m_footer_box = std::make_shared<VBox>();
    m_footer_box->m_padding = 2.0f;

    // [修改] 使用成员变量，并开启第3个参数 use_hacker_effect = true
    m_system_text = std::make_shared<TechText>(sysstr, theme.color_text_disabled, true);
    m_system_text->m_font = ctx.m_font_regular;
    m_system_text->m_rect.h = 14.0f;

    m_autosave_text = std::make_shared<TechText>(savestr, theme.color_text_disabled, true);
    m_autosave_text->SetAnimMode(TechTextAnimMode::Scroll);
    m_autosave_text->m_font = ctx.m_font_regular;
    m_autosave_text->m_rect.h = 14.0f;

    m_footer_box->AddChild(m_system_text);
    m_footer_box->AddChild(m_autosave_text);
    AddChild(m_footer_box);

}

void LogPanel::SetSystemStatus(const std::string& text)
{
    if (m_system_text) m_system_text->SetText(text);
}

void LogPanel::SetAutoSaveTime(const std::string& text)
{
    if (m_autosave_text) m_autosave_text->SetText(text);
}

void LogPanel::AddLog(LogType type, const std::string& title, const std::string& message)
{
    if (!m_list_box) return;

    auto card = std::make_shared<LogCard>(type, title, message);
    m_list_box->AddChild(card);

    // [修正] 使用 += 累加偏移。
    // 如果动画正在进行中（比如 offset=20），此时又加一条，offset 变为 20+48=68。
    // 这保证了现有的列表位置在这一帧不会发生任何跳变，继续平滑向上滑动。
    m_slide_offset += LogCard::CARD_HEIGHT;
    m_is_sliding = true;
}

void LogPanel::Update(float dt, const ImVec2& parent_abs_pos)
{
    auto& ctx = UIContext::Get();

    // 1. 滑动动画 (阻尼衰减)
    float speed = 10.0f;
    m_slide_offset += (0.0f - m_slide_offset) * speed * dt;

    // 2. [修正后的清理逻辑]：实时检查溢出
    // 只要有条目超出了 MAX 且已经滑出可视区，就立即清理，无需等待动画完全静止
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

    // 3. 绝对布局计算 (保持不变)
    float margin_left = 20.0f;
    float margin_bottom = 20.0f;
    float footer_h = 36.0f;
    float divider_area_h = 10.0f;

    float total_h = LIST_AREA_HEIGHT + divider_area_h + footer_h;

    m_rect.x = margin_left;
    m_rect.y = ctx.m_display_size.y - margin_bottom - total_h;

    m_absolute_pos.x = parent_abs_pos.x + m_rect.x;
    m_absolute_pos.y = parent_abs_pos.y + m_rect.y;

    float current_local_y = 0.0f;

    // [A] 列表区
    if (m_list_box)
    {
        // 先 Update 获取尺寸信息
        m_list_box->m_rect.w = m_rect.w;
        m_list_box->Update(dt, m_absolute_pos);
        float real_list_h = m_list_box->m_rect.h;

        // 底部对齐 + 偏移量
        m_list_box->m_rect.x = 0.0f;
        m_list_box->m_rect.y = (LIST_AREA_HEIGHT - real_list_h) + m_slide_offset;

        current_local_y += LIST_AREA_HEIGHT;
    }

    // [B] 分割线
    if (m_divider)
    {
        m_divider->m_rect.x = 0.0f;
        m_divider->m_rect.y = current_local_y + (divider_area_h - m_divider->m_rect.h) * 0.5f;
        m_divider->m_rect.w = m_rect.w;
        current_local_y += divider_area_h;
    }

    // [C] 页脚
    if (m_footer_box)
    {
        m_footer_box->m_rect.x = 0.0f;
        m_footer_box->m_rect.y = current_local_y;
        m_footer_box->m_rect.w = m_rect.w;
        m_footer_box->m_rect.h = footer_h;
    }

    UIElement::Update(dt, parent_abs_pos);
}

void LogPanel::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    if (m_list_box)
    {
        // 裁剪区域固定为列表显示区，超出部分(顶出的旧日志)会被切掉
        ImVec2 clip_min = m_absolute_pos;
        ImVec2 clip_max = ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + LIST_AREA_HEIGHT);

        dl->PushClipRect(clip_min, clip_max, true);
        m_list_box->Draw(dl);
        dl->PopClipRect();
    }

    if (m_divider) m_divider->Draw(dl);
    if (m_footer_box) m_footer_box->Draw(dl);
}

_UI_END
_SYSTEM_END
_NPGS_END