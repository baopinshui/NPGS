// --- START OF FILE GlobalTooltip.cpp ---

#include "GlobalTooltip.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// --- START OF FILE GlobalTooltip.cpp --- (部分修改)

GlobalTooltip::GlobalTooltip()
{
    m_block_input = false;
    m_visible = false;
    m_rect = { 0, 0, 0, 0 };

    // 1. 创建背景面板
    m_panel = std::make_shared<TechBorderPanel>();
    m_panel->m_use_glass_effect = true;
    m_panel->m_glass_col = StyleColor(ImVec4(0.0f, 0.0f, 0.0f, 0.5f));
    m_panel->m_thickness = 2.0f;
    m_panel->m_block_input = false;

    // [修复 1] 面板应该由内容撑开，而不是拉伸
    m_panel->m_width = Length::Content();
    m_panel->m_height = Length::Content();

    AddChild(m_panel);

    // 2. 创建内部文本
    m_text_label = std::make_shared<TechText>("");
    m_text_label->SetAnimMode(TechTextAnimMode::Hacker);

    // [修复 2] 设置为 AutoWidthHeight，让文本先尝试自然展开
    // 如果设置为 AutoHeight，它会强行占满传入的 max_width (682px)，导致短文本也显示巨大的框
    m_text_label->SetSizing(TechTextSizingMode::AutoWidthHeight);

    m_text_label->m_rect.x = m_padding;
    m_text_label->m_rect.y = m_padding;
    m_text_label->m_block_input = false;

    // [修复 3] 文本也应该由内容决定大小
    m_text_label->m_width = Length::Content();
    m_text_label->m_height = Length::Content();

    m_text_label->m_align_h = Alignment::Center;
    m_text_label->m_align_v= Alignment::Center;
    m_panel->AddChild(m_text_label);
}

void GlobalTooltip::Show(const std::string& key)
{
    // 如果是从完全隐藏状态开始，重置所有动画状态
    if (m_state == State::Hidden)
    {
        m_rect = { 0, 0, 0, 0 };
        m_target_size = { 0, 0 };
        // 立即计算初始 Pivot，避免从 (0,0) 飞来
        ImVec2 mouse_pos = ImGui::GetIO().MousePos;
        ImVec2 display_sz = UIContext::Get().m_display_size;
        m_target_pivot = { (mouse_pos.x + 200.0f > display_sz.x) ? 1.0f : 0.0f, (mouse_pos.y + 50.0f > display_sz.y) ? 1.0f : 0.0f };
        m_current_pivot_lerp = m_target_pivot; // 直接设置为目标值，不做动画
    }

    m_state = State::Expanding;
    m_visible = true;
    m_current_key = key;

    m_text_label->SetSourceText(key);
    m_text_label->RestartEffect();
}

void GlobalTooltip::Hide()
{
    if (m_state == State::Collapsing || m_state == State::Hidden) return;

    m_state = State::Collapsing;

    // 启动收缩动画
    To(&m_rect.w, 0.0f, 0.2f, EasingType::EaseInQuad);
    To(&m_rect.h, 0.0f, 0.2f, EasingType::EaseInQuad, [this]()
    {
        this->m_state = State::Hidden;
        this->m_visible = false;
        this->m_current_key.clear();
    });
}

// [MODIFIED] Update 现在是总调度器
void GlobalTooltip::Update(float dt)
{
    auto& ctx = UIContext::Get();
    const std::string& active_key = ctx.m_tooltip_active_key;

    // --- 1. 状态机逻辑 ---
    if (!active_key.empty() && (m_state == State::Hidden || m_current_key != active_key))
    {
        Show(active_key);
    }
    else if (active_key.empty() && (m_state == State::Visible || m_state == State::Expanding))
    {
        Hide();
    }
    UIElement::Update(dt);

    // 只有在可见或正在动画时才执行布局和渲染
    if (!m_visible)
    {
        // 即使不可见，也要调用 UpdateSelf 来处理可能正在进行的收缩动画
        UpdateSelf(dt);
        return;
    }

    // --- 2. 布局与定位 ---

    // [PASS 1] 测量 (Measure): 计算理想尺寸
    ImVec2 ideal_size = Measure({ m_max_width, FLT_MAX });

    // [PASS 1.5] 触发尺寸动画
    if (std::abs(m_target_size.x - ideal_size.x) > 1.0f || std::abs(m_target_size.y - ideal_size.y) > 1.0f)
    {
        m_target_size = ideal_size;
        To(&m_rect.w, m_target_size.x, 0.2f, EasingType::EaseOutQuad);
        To(&m_rect.h, m_target_size.y, 0.2f, EasingType::EaseOutQuad, [this]()
        {
            if (m_state == State::Expanding) m_state = State::Visible;
        });
    }

    // [PASS 2] 排列 (Arrange): 计算最终位置
    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
    ImVec2 display_sz = ctx.m_display_size;
    float offset = 15.0f;

    // a. 计算目标 Pivot 并触发翻转动画
    ImVec2 next_target_pivot = { 0.0f, 0.0f };
    if (mouse_pos.x + offset + m_rect.w > display_sz.x) next_target_pivot.x = 1.0f;
    if (mouse_pos.y + offset + m_rect.h > display_sz.y) next_target_pivot.y = 1.0f;

    if (m_target_pivot.x != next_target_pivot.x)
    {
        m_target_pivot.x = next_target_pivot.x;
        To(&m_current_pivot_lerp.x, m_target_pivot.x, 0.2f, EasingType::EaseOutQuad);
    }
    if (m_target_pivot.y != next_target_pivot.y)
    {
        m_target_pivot.y = next_target_pivot.y;
        To(&m_current_pivot_lerp.y, m_target_pivot.y, 0.2f, EasingType::EaseOutQuad);
    }

    // b. 使用插值计算当前帧的位置
    float pos_x = m_current_pivot_lerp.x == 0.0f ? (mouse_pos.x + offset) : (mouse_pos.x - offset - m_rect.w);
    float pos_y = m_current_pivot_lerp.y == 0.0f ? (mouse_pos.y + offset) : (mouse_pos.y - offset - m_rect.h);
    pos_x = std::lerp(mouse_pos.x + offset, mouse_pos.x - offset - m_rect.w, m_current_pivot_lerp.x);
    pos_y = std::lerp(mouse_pos.y + offset, mouse_pos.y - offset - m_rect.h, m_current_pivot_lerp.y);

    // c. 执行排列
    Arrange({ pos_x, pos_y, m_rect.w, m_rect.h });


}

// [NEW] Measure 实现
ImVec2 GlobalTooltip::Measure(ImVec2 available_size)
{
    if (!m_visible) return { 0,0 };

    // 1. 计算子元素(面板)的可用空间，即总可用空间减去我们的内边距
    ImVec2 panel_available_size = {
        std::max(0.0f, available_size.x - m_padding * 2.0f),
        FLT_MAX // 高度不限
    };

    // 2. 测量子元素(面板)。这会递归地测量到最深处的文本。
    ImVec2 panel_desired_size = m_panel->Measure(panel_available_size);

    // 3. 我们的期望尺寸 = 面板的期望尺寸 + 内边距
    m_desired_size = {
        panel_desired_size.x + m_padding * 2.0f,
        panel_desired_size.y + m_padding * 2.0f
    };

    // 对齐到像素以避免抖动
    m_desired_size.x = std::ceil(m_desired_size.x);
    m_desired_size.y = std::ceil(m_desired_size.y);

    return m_desired_size;
}

// [NEW] Arrange 实现
void GlobalTooltip::Arrange(const Rect& final_rect)
{
    // 1. 设置我们自己的最终位置和尺寸
    m_rect = final_rect;
    // 由于我们是顶级浮动元素，相对坐标就是绝对坐标
    m_absolute_pos = { final_rect.x, final_rect.y };

    if (!m_visible) return;

    // 2. 为子元素(面板)计算其在我们的坐标系中的位置和尺寸
    // 由于面板要撑满我们，所以它的 rect 就是我们的整个区域
    // 注意：这里的 Arrange 是递归的，面板接收到这个 rect 后，会再去排列它自己的子项(文本)
    Rect panel_rect = { 0, 0, m_rect.w, m_rect.h };
    m_panel->Arrange(panel_rect);
}

_UI_END
_SYSTEM_END
_NPGS_END