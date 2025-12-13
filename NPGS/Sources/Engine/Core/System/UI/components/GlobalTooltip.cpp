// --- START OF FILE GlobalTooltip.cpp ---

#include "GlobalTooltip.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

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
    AddChild(m_panel);

    // 2. 创建内部文本
    m_text_label = std::make_shared<TechText>("");
    m_text_label->SetAnimMode(TechTextAnimMode::Hacker);
    m_text_label->SetSizing(TechTextSizingMode::AutoHeight);
    m_text_label->m_rect.x = m_padding;
    m_text_label->m_rect.y = m_padding;
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

    // [核心] 强制重置目标尺寸为0
    // 这样 update 循环会检测到尺寸不匹配，从而触发从 0 开始的展开动画
    m_target_size = { 0.0f, 0.0f };

    // 如果是从隐藏状态开始，重置当前物理尺寸
    if (m_rect.w < 1.0f && m_rect.h < 1.0f)
    {
        m_rect.w = 0.0f;
        m_rect.h = 0.0f;
    }

    // [新增] 立即计算并设置初始 Pivot，避免从旧位置飞入
    // 在第一帧 Update 之前，手动计算一次初始位置
    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
    ImVec2 display_sz = UIContext::Get().m_display_size;
    float offset = 15.0f;

    // 预估一个初始宽度用于判断
    float initial_check_w = 200.0f;

    ImVec2 initial_pivot = { 0.0f, 0.0f };
    if (mouse_pos.x + offset + initial_check_w > display_sz.x) initial_pivot.x = 1.0f;
    if (mouse_pos.y + offset + m_rect.h > display_sz.y) initial_pivot.y = 1.0f;

    m_target_pivot = initial_pivot;
    m_current_pivot_lerp = initial_pivot; // 直接设置为目标值，不做动画

    // 清除可能存在的旧动画
    m_tweens.erase(std::remove_if(m_tweens.begin(), m_tweens.end(),
        [this](const Tween& t)
    {
        return t.target == &m_current_pivot_lerp.x || t.target == &m_current_pivot_lerp.y;
    }), m_tweens.end());
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
        this->m_rect = { 0,0,0,0 };
    });
}


void GlobalTooltip::Update(float dt, const ImVec2& parent_abs_pos)
{
    auto& ctx = UIContext::Get();
    const std::string& active_key = ctx.m_tooltip_active_key;

    // --- 1. 状态逻辑 ---
    if (!active_key.empty())
    {
        if (m_state == State::Hidden || m_state == State::Collapsing)
        {
            Show(active_key);
        }
        else if (m_current_key != active_key)
        {
            m_current_key = active_key;
            m_text_label->SetSourceText(active_key);
            m_text_label->RestartEffect();
            m_target_size = { 0,0 }; // 强制重新计算尺寸
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
        // --- A. 测量文本自然宽度 ---
        float natural_text_width = 0.0f;
        ImFont* font = m_text_label->GetFont();
        if (font && !m_text_label->m_current_display_text.empty())
        {
            ImVec2 sz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, m_text_label->m_current_display_text.c_str());
            natural_text_width = sz.x;
        }

        // --- B. 决定布局模式 ---
        float max_text_width = m_max_width - m_padding * 2.0f;
        float final_tooltip_width = 0.0f;
        float final_text_width = 0.0f;

        if (natural_text_width > max_text_width)
        {
            final_tooltip_width = m_max_width;
            final_text_width = max_text_width;
        }
        else
        {
            final_tooltip_width = natural_text_width + m_padding * 2.0f;
            final_text_width = natural_text_width;
        }

        // --- 2. [核心修改] 计算位置 (跟随鼠标并平滑翻转) ---
        ImVec2 mouse_pos = ImGui::GetIO().MousePos;
        ImVec2 display_sz = ctx.m_display_size;
        float offset = 15.0f;

        // --- 2a. 计算目标 Pivot ---
        float check_w = (m_target_size.x > 1.0f) ? m_target_size.x : final_tooltip_width;
        float check_h = (m_target_size.y > 1.0f) ? m_target_size.y : m_rect.h;
        if (check_h < 20.0f) check_h = 20.0f; // 防止高度为0时判断错误

        ImVec2 next_target_pivot = { 0.0f, 0.0f };
        if (mouse_pos.x + offset + check_w > display_sz.x) next_target_pivot.x = 1.0f; // 目标: 翻转X
        if (mouse_pos.y + offset + check_h > display_sz.y) next_target_pivot.y = 1.0f; // 目标: 翻转Y

        // --- 2b. 检测变化并启动动画 ---
        if (m_target_pivot.x != next_target_pivot.x || m_target_pivot.y != next_target_pivot.y)
        {
            m_target_pivot = next_target_pivot;
            To(&m_current_pivot_lerp.x, m_target_pivot.x, 0.2f, EasingType::EaseOutQuad);
            To(&m_current_pivot_lerp.y, m_target_pivot.y, 0.2f, EasingType::EaseOutQuad);
        }

        // --- 2c. 使用插值计算当前位置 ---
        float pos_x_normal = mouse_pos.x + offset;
        float pos_x_flipped = mouse_pos.x - offset - m_rect.w;
        m_rect.x = pos_x_normal + (pos_x_flipped - pos_x_normal) * m_current_pivot_lerp.x;

        float pos_y_normal = mouse_pos.y + offset;
        float pos_y_flipped = mouse_pos.y - offset - m_rect.h;
        m_rect.y = pos_y_normal + (pos_y_flipped - pos_y_normal) * m_current_pivot_lerp.y;

        // --- 3. 更新自身 (应用动画与位置) ---
        UpdateSelf(dt, { 0,0 });

        // --- C. 应用宽度并更新子元素 ---
        m_text_label->m_rect.w = final_text_width;

        for (auto& child : m_children)
        {
            child->Update(dt, m_absolute_pos);
        }

        // --- 5. 尺寸检测与动画触发 ---
        if (m_state == State::Expanding || m_state == State::Visible)
        {
            ImVec2 text_size = { m_text_label->m_rect.w, m_text_label->m_rect.h };
            ImVec2 ideal_size = { final_tooltip_width, text_size.y + m_padding * 2.0f };
            ideal_size.x = std::ceil(ideal_size.x); // 向上取整避免像素抖动

            if (std::abs(m_target_size.x - ideal_size.x) > 1.0f || std::abs(m_target_size.y - ideal_size.y) > 1.0f)
            {
                m_target_size = ideal_size;
                To(&m_rect.w, m_target_size.x, 0.2f, EasingType::EaseOutQuad);
                To(&m_rect.h, m_target_size.y, 0.2f, EasingType::EaseOutQuad, [this]()
                {
                    if (m_state == State::Expanding) m_state = State::Visible;
                });
            }
        }

        // --- 6. 同步背景面板尺寸 ---
        m_panel->m_rect.w = m_rect.w;
        m_panel->m_rect.h = m_rect.h;
    }
    else
    {
        UpdateSelf(dt, { 0,0 });
    }
}

_UI_END
_SYSTEM_END
_NPGS_END