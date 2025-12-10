#include "GlobalTooltip.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

GlobalTooltip::GlobalTooltip()
{
    m_block_input = false; // Tooltip 自身不交互
    m_visible = false;
    m_rect = { 0, 0, 0, 0 };

    // 1. 创建背景面板
    m_panel = std::make_shared<TechBorderPanel>();
    m_panel->m_use_glass_effect = false; // 不使用场景模糊
    m_panel->m_bg_color = ImVec4(0.05f, 0.08f, 0.1f, 0.9f); // 暗色半透明背景
    m_panel->m_thickness = 1.0f;
    m_panel->m_show_flow_border = true; // 开启流光边框
    m_panel->m_flow_length_ratio = 0.15f;
    m_panel->m_flow_period = 4.0f;
    m_panel->m_block_input = false;
    AddChild(m_panel);

    // 2. 创建内部文本
    m_text_label = std::make_shared<TechText>("");
    m_text_label->SetAnimMode(TechTextAnimMode::Hacker);
    m_text_label->m_rect = { 10, 8, 0, 0 }; // 内部边距
    m_text_label->m_block_input = false;
    m_panel->AddChild(m_text_label);
}

// 核心：计算 Tooltip 应该在的位置和大小
void GlobalTooltip::CalculateTargetLayout(const std::string& key)
{
    auto& ctx = UIContext::Get();
    std::string text = TR(key);
    if (text.empty() || text == key)
    {
        m_target_size = { 0, 0 };
        return;
    }

    ImFont* font = m_text_label->GetFont();
    float max_w = 300.0f; // 最大宽度
    ImVec2 padding = { 20.0f, 16.0f }; // (10+10, 8+8)
    ImVec2 text_size = font->CalcTextSizeA(font->FontSize, max_w, 0.0f, text.c_str());

    m_target_size = { text_size.x + padding.x, text_size.y + padding.y };

    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
    m_target_pos = { mouse_pos.x + 15.0f, mouse_pos.y + 15.0f };

    // 边缘检测与重定位
    if (m_target_pos.x + m_target_size.x > ctx.m_display_size.x)
    {
        m_target_pos.x = mouse_pos.x - m_target_size.x - 5.0f;
    }
    if (m_target_pos.y + m_target_size.y > ctx.m_display_size.y)
    {
        m_target_pos.y = mouse_pos.y - m_target_size.y - 5.0f;
    }
}

// 触发显示
void GlobalTooltip::Show(const std::string& key)
{
    m_state = State::Expanding;
    m_visible = true;
    m_current_key = key;

    CalculateTargetLayout(key);

    m_rect.x = m_target_pos.x + m_target_size.x * 0.5f;
    m_rect.y = m_target_pos.y + m_target_size.y * 0.5f;
    m_rect.w = 0.0f;
    m_rect.h = 0.0f;

    m_text_label->SetSourceText(key);
    m_text_label->RestartEffect();

    // 动画：展开
    To(&m_rect.w, m_target_size.x, 0.25f, EasingType::EaseOutQuad);
    To(&m_rect.h, m_target_size.y, 0.25f, EasingType::EaseOutQuad, [this]()
    {
        this->m_state = State::Visible;
    });
}

// 触发隐藏
void GlobalTooltip::Hide()
{
    m_state = State::Collapsing;

    // 动画：收起
    To(&m_rect.w, 0.0f, 0.2f, EasingType::EaseInQuad);
    To(&m_rect.h, 0.0f, 0.2f, EasingType::EaseInQuad, [this]()
    {
        this->m_state = State::Hidden;
        this->m_visible = false;
        this->m_current_key.clear();
    });
}

void GlobalTooltip::Update(float dt, const ImVec2& parent_abs_pos)
{
    auto& ctx = UIContext::Get();
    const std::string& active_key = ctx.m_tooltip_active_key;

    // --- 状态机驱动 ---
    if (!active_key.empty())
    {
        if (m_state == State::Hidden)
        {
            // 从无到有 -> 显示
            Show(active_key);
        }
        else if (m_state == State::Visible)
        {
            // 持续显示，但需要检查是否需要重定位
            ImVec2 old_target_pos = m_target_pos;
            CalculateTargetLayout(active_key);

            if (std::abs(m_target_pos.x - old_target_pos.x) > 1.0f ||
                std::abs(m_target_pos.y - old_target_pos.y) > 1.0f)
            {
                // 位置发生变化 -> 先收起，在收起动画的回调里再展开
                m_state = State::Collapsing;
                To(&m_rect.w, 0.0f, 0.2f, EasingType::EaseInQuad);
                To(&m_rect.h, 0.0f, 0.2f, EasingType::EaseInQuad, [this]()
                {
                    // 在这里，我们使用最新的 active_key 重新触发 Show
                    Show(UIContext::Get().m_tooltip_active_key);
                });
            }
            else
            {
                // 如果只是鼠标轻微移动，平滑地更新位置
                m_rect.x = m_target_pos.x;
                m_rect.y = m_target_pos.y;
            }
        }
    }
    else // active_key 为空
    {
        if (m_state == State::Visible || m_state == State::Expanding)
        {
            // 从有到无 -> 隐藏
            Hide();
        }
    }

    // --- 动画更新与布局 ---
    if (m_visible)
    {
        // 中心点对齐动画
        m_rect.x = m_target_pos.x + (m_target_size.x - m_rect.w) * 0.5f;
        m_rect.y = m_target_pos.y + (m_target_size.y - m_rect.h) * 0.5f;

        // 更新子元素尺寸
        m_panel->m_rect.w = m_rect.w;
        m_panel->m_rect.h = m_rect.h;
        m_text_label->m_rect.w = m_rect.w - 20.0f;
        m_text_label->m_rect.h = m_rect.h - 16.0f;
        m_text_label->m_alpha = std::clamp((m_rect.w / m_target_size.x) * 2.0f - 1.0f, 0.0f, 1.0f); // 展开过半才显示文字
    }

    // 调用基类来处理动画插值和子节点更新
    UIElement::Update(dt, parent_abs_pos);
}

_UI_END
_SYSTEM_END
_NPGS_END