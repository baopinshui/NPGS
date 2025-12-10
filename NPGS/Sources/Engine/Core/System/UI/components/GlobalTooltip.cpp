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
    m_panel->m_use_glass_effect = false;
    m_panel->m_bg_color = StyleColor(ImVec4(0.0f, 0.0f, 0.0f, 0.9f));
    m_panel->m_thickness = 1.0f;
    m_panel->m_block_input = false;
    AddChild(m_panel);

    // 2. 创建内部文本
    m_text_label = std::make_shared<TechText>("");
    m_text_label->SetAnimMode(TechTextAnimMode::Hacker);
    m_text_label->SetSizing(TechTextSizingMode::AutoWidthHeight);
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
        // --- 2. 计算位置 (跟随鼠标) ---
        // 这一步必须在 UpdateSelf 之前，确保 m_rect.x/y 被设置到正确的位置
        ImVec2 mouse_pos = ImGui::GetIO().MousePos;
        ImVec2 display_sz = ctx.m_display_size;
        float offset = 15.0f;

        // 使用 m_target_size (最终尺寸) 来预判翻转，防止动画过程中来回闪烁
        // 如果正在收缩中，target_size 可能是 0，此时不翻转也没关系
        float check_w = (m_target_size.x > 1.0f) ? m_target_size.x : m_rect.w;
        float check_h = (m_target_size.y > 1.0f) ? m_target_size.y : m_rect.h;

        PivotState next_pivot;
        if (mouse_pos.x + offset + check_w > display_sz.x) next_pivot.flip_x = true;
        if (mouse_pos.y + offset + check_h > display_sz.y) next_pivot.flip_y = true;
        m_current_pivot = next_pivot;

        // 设置 Tooltip 相对于屏幕左上角(0,0) 的位置
        // 注意：GlobalTooltip 是根元素，所以 m_rect.x/y 就是绝对位置
        if (!m_current_pivot.flip_x) m_rect.x = mouse_pos.x + offset;
        else m_rect.x = mouse_pos.x - offset - m_rect.w; // 注意这里用 m_rect.w 保证当前帧渲染正确

        if (!m_current_pivot.flip_y) m_rect.y = mouse_pos.y + offset;
        else m_rect.y = mouse_pos.y - offset - m_rect.h;

        // --- 3. 更新自身 (应用动画与位置) ---
        // 传入 {0,0} 因为我们已经在上面计算了绝对坐标并填入了 m_rect.x/y
        UpdateSelf(dt, { 0,0 });

        // --- 4. 更新子元素 (传递正确的绝对坐标) ---
        // 这一步会让 TechText 根据内容计算出自己的 m_rect.w/h
        for (auto& child : m_children)
        {
            child->Update(dt, m_absolute_pos);
        }

        // --- 5. 尺寸检测与动画触发 ---
        // 现在子元素已经更新完毕，我们可以读取 TechText 的大小了
        if (m_state == State::Expanding || m_state == State::Visible)
        {
            ImVec2 text_size = { m_text_label->m_rect.w, m_text_label->m_rect.h };
            ImVec2 ideal_size = { text_size.x + m_padding * 2.0f, text_size.y + m_padding * 2.0f };
            ideal_size.x += 1.0f; // 容错

            // 检查尺寸是否变化
            // 由于 Show() 中重置了 m_target_size 为 0，第一次这里一定会触发
            if (std::abs(m_target_size.x - ideal_size.x) > 1.0f || std::abs(m_target_size.y - ideal_size.y) > 1.0f)
            {
                m_target_size = ideal_size;

                // 启动动画
                To(&m_rect.w, m_target_size.x, 0.2f, EasingType::EaseOutQuad);
                To(&m_rect.h, m_target_size.y, 0.2f, EasingType::EaseOutQuad, [this]()
                {
                    if (m_state == State::Expanding) m_state = State::Visible;
                });
            }
        }

        // --- 6. 同步背景面板尺寸 ---
        // 确保背景板跟上当前的 m_rect 动画进度
        m_panel->m_rect.w = m_rect.w;
        m_panel->m_rect.h = m_rect.h;
    }
    else
    {
        // 不可见时也需要 update tween 防止回调不执行
        UpdateSelf(dt, { 0,0 });
    }
}

_UI_END
_SYSTEM_END
_NPGS_END