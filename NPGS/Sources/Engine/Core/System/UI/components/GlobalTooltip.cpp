// --- START OF FILE GlobalTooltip.cpp ---

#include "GlobalTooltip.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

GlobalTooltip::GlobalTooltip()
{
    m_block_input = false;
    m_visible = false;

    // [MODIFIED] 1. 设置自身的尺寸策略为自适应内容
    SetWidth(Length::Content());
    SetHeight(Length::Content());

    // 2. 创建背景面板并设置为 Fill
    m_panel = std::make_shared<TechBorderPanel>();
    m_panel->SetWidth(Length::Fill())->SetHeight(Length::Fill());
    m_panel->m_use_glass_effect = true;
    m_panel->m_glass_col = StyleColor(ImVec4(0.0f, 0.0f, 0.0f, 0.5f));
    m_panel->m_thickness = 2.0f;
    m_panel->m_block_input = false;
    AddChild(m_panel);

    // 3. 创建内部文本并设置为 Fill/Content
    m_text_label = std::make_shared<TechText>("");
    m_text_label->SetWidth(Length::Fill())      // 宽度填充父容器（的内边距区域）
        ->SetHeight(Length::Content()); // 高度根据换行自适应
    m_text_label->SetAnimMode(TechTextAnimMode::Hacker);
    m_text_label->m_block_input = false;
    m_panel->AddChild(m_text_label);
}

void GlobalTooltip::Show(const std::string& key)
{
    m_state = State::Expanding;
    m_visible = true;
    m_current_key = key;

    m_text_label->SetSourceText(key);
    m_text_label->RestartEffect();

    // 强制下一帧重新测量和动画
    m_target_size = { -1.0f, -1.0f };

    // 立即计算初始 Pivot，避免从旧位置飞入
    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
    ImVec2 display_sz = UIContext::Get().m_display_size;
    float initial_check_w = 200.0f; // 预估宽度
    m_target_pivot = { 0.0f, 0.0f };
    if (mouse_pos.x + 15.0f + initial_check_w > display_sz.x) m_target_pivot.x = 1.0f;
    if (mouse_pos.y + 15.0f + 50.0f > display_sz.y) m_target_pivot.y = 1.0f; // 预估高度
    m_current_pivot_lerp = m_target_pivot; // 直接设置，不做动画

    m_tweens.clear(); // 清除所有旧动画
}

void GlobalTooltip::Hide()
{
    if (m_state == State::Collapsing || m_state == State::Hidden) return;
    m_state = State::Collapsing;
    To(&m_rect.w, 0.0f, 0.2f, EasingType::EaseInQuad);
    To(&m_rect.h, 0.0f, 0.2f, EasingType::EaseInQuad, [this]()
    {
        this->m_state = State::Hidden;
        this->m_visible = false;
        this->m_current_key.clear();
        this->m_rect = { 0, 0, 0, 0 };
    });
}

// [NEW] 核心尺寸计算逻辑
ImVec2 GlobalTooltip::Measure(const ImVec2& available_size)
{
    // 如果没有文本，尺寸为0
    if (m_current_key.empty() || !m_text_label)
    {
        return { 0.0f, 0.0f };
    }

    // --- 1. 初步测量 ---
    // 假设文本不换行，测量其自然宽度
    m_text_label->SetWidth(Length::Content());
    float natural_text_width = m_text_label->Measure({}).x;
    float tooltip_w = natural_text_width + m_padding * 2.0f;

    // --- 2. 处理最大宽度约束 ---
    float final_text_w = natural_text_width;
    if (tooltip_w > m_max_width)
    {
        tooltip_w = m_max_width;
        final_text_w = m_max_width - m_padding * 2.0f;
    }

    // --- 3. 最终测量 ---
    // 使用确定的文本宽度来测量可能换行后的文本高度
    m_text_label->SetWidth(Length::Fix(final_text_w));
    float final_text_h = m_text_label->Measure({ final_text_w, 0 }).y;
    float tooltip_h = final_text_h + m_padding * 2.0f;

    return { std::ceil(tooltip_w), std::ceil(tooltip_h) };
}

// [MODIFIED] Update 流程简化
void GlobalTooltip::Update(float dt, const ImVec2& parent_abs_pos)
{
    auto& ctx = UIContext::Get();
    const std::string& active_key = ctx.m_tooltip_active_key;

    // --- 1. 状态机逻辑 (不变) ---
    if (!active_key.empty())
    {
        if (m_state == State::Hidden || m_state == State::Collapsing || m_current_key != active_key)
        {
            Show(active_key);
        }
    }
    else
    {
        if (m_state == State::Visible || m_state == State::Expanding)
        {
            Hide();
        }
    }

    if (m_visible)
    {
        // --- 2. [NEW] 获取目标尺寸并触发动画 ---
        ImVec2 ideal_size = Measure(ctx.m_display_size);
        if (std::abs(m_target_size.x - ideal_size.x) > 1.0f || std::abs(m_target_size.y - ideal_size.y) > 1.0f)
        {
            m_target_size = ideal_size;
            if (m_state == State::Expanding || m_state == State::Visible)
            {
                To(&m_rect.w, m_target_size.x, 0.2f, EasingType::EaseOutQuad);
                To(&m_rect.h, m_target_size.y, 0.2f, EasingType::EaseOutQuad, [this]()
                {
                    if (m_state == State::Expanding) m_state = State::Visible;
                });
            }
        }

        // --- 3. 更新自身 (应用动画 & 位置计算) ---
        UpdateSelf(dt, { 0,0 });

        // --- 4. 鼠标跟随与翻转逻辑 (不变) ---
        ImVec2 mouse_pos = ImGui::GetIO().MousePos;
        ImVec2 display_sz = ctx.m_display_size;
        float offset = 15.0f;

        ImVec2 next_target_pivot = { 0.0f, 0.0f };
        if (mouse_pos.x + offset + m_rect.w > display_sz.x) next_target_pivot.x = 1.0f;
        if (mouse_pos.y + offset + m_rect.h > display_sz.y) next_target_pivot.y = 1.0f;

        if (m_target_pivot.x != next_target_pivot.x || m_target_pivot.y != next_target_pivot.y)
        {
            m_target_pivot = next_target_pivot;
            To(&m_current_pivot_lerp.x, m_target_pivot.x, 0.2f, EasingType::EaseOutQuad);
            To(&m_current_pivot_lerp.y, m_target_pivot.y, 0.2f, EasingType::EaseOutQuad);
        }

        m_rect.x = (mouse_pos.x + offset) + ((-offset * 2) - m_rect.w) * m_current_pivot_lerp.x;
        m_rect.y = (mouse_pos.y + offset) + ((-offset * 2) - m_rect.h) * m_current_pivot_lerp.y;

        // --- 5. [NEW] 内部布局 (Arrange) ---
        // Panel 填满 GlobalTooltip
        m_panel->m_rect = { 0, 0, m_rect.w, m_rect.h };

        // Text 在 Panel 内部，有 padding
        m_text_label->m_rect.x = m_padding;
        m_text_label->m_rect.y = m_padding;
        m_text_label->m_rect.w = m_rect.w - m_padding * 2.0f;
        m_text_label->m_rect.h = m_rect.h - m_padding * 2.0f; // 高度给足，让 TechText 内部自己对齐

        // --- 6. 递归更新子元素 ---
        for (auto& child : m_children)
        {
            child->Update(dt, m_absolute_pos);
        }
    }
    else
    {
        // 即使不可见，也要处理收起动画
        UpdateSelf(dt, { 0,0 });
    }
}

_UI_END
_SYSTEM_END
_NPGS_END