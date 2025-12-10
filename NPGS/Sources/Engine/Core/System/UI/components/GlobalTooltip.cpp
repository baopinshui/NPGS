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
    m_panel->m_bg_color = StyleColor(ImVec4(0.0f, 0.0f, 0.0f, 0.5f));
    m_panel->m_thickness = 1.0f;
    m_panel->m_block_input = false;
    AddChild(m_panel);

    // 2. 创建内部文本
    m_text_label = std::make_shared<TechText>("");
    m_text_label->SetAnimMode(TechTextAnimMode::Hacker);
    // [修改] 初始位置设为 padding，确保左上角留白
    m_text_label->m_rect = { m_padding, m_padding, 0, 0 };
    m_text_label->m_block_input = false;
    m_panel->AddChild(m_text_label);
}

// [修改] 精确计算尺寸，确保留白均匀
void GlobalTooltip::CalculateLayoutInfo(const std::string& key, ImVec2& out_size, PivotState& out_pivot)
{
    auto& ctx = UIContext::Get();
    std::string text = TR(key);

    if (text.empty())
    {
        out_size = { 0,0 };
        return;
    }

    ImFont* font = m_text_label->GetFont();
    // 稍微放宽一点最大宽度
    float max_w = 400.0f;

    // [关键] 计算纯文本尺寸
    ImVec2 text_size = font->CalcTextSizeA(font->FontSize, max_w, 0.0f, text.c_str());

    // [关键] 总尺寸 = 文本尺寸 + 2倍边距 (左右各一，上下各一)
    out_size = { text_size.x + m_padding * 2.0f, text_size.y + m_padding * 2.0f };

    // 加上微量容错，防止浮点误差导致换行
    out_size.x += 1.0f;

    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
    ImVec2 display_sz = ctx.m_display_size;
    float offset = 15.0f;

    // 判定象限逻辑
    out_pivot.flip_x = false;
    out_pivot.flip_y = false;

    if (mouse_pos.x + offset + out_size.x > display_sz.x)
        out_pivot.flip_x = true;

    if (mouse_pos.y + offset + out_size.y > display_sz.y)
        out_pivot.flip_y = true;
}

// [新增] 提交新Key到显示组件
void GlobalTooltip::CommitKey(const std::string& key, bool force_restart)
{
    // 逻辑：如果强制重启，或者 内容发生了变化，则更新并播放特效
    if (force_restart || m_text_label->m_source_key_or_text != key)
    {
        m_text_label->SetSourceText(key);
        m_text_label->RestartEffect();
    }
}

void GlobalTooltip::Show(const std::string& key)
{
    // 初始化计算
    PivotState new_pivot;
    CalculateLayoutInfo(key, m_target_size, new_pivot);

    m_state = State::Expanding;
    m_visible = true;
    m_current_key = key;
    m_current_pivot = new_pivot;

    // 如果是从隐藏状态开始，重置尺寸
    if (m_rect.w < 1.0f)
    {
        m_rect.w = 0.0f;
        m_rect.h = 0.0f;
    }

    // 立即显示内容
    CommitKey(key);

    // 动画：展开到目标尺寸
    To(&m_rect.w, m_target_size.x, 0.25f, EasingType::EaseOutQuad);
    To(&m_rect.h, m_target_size.y, 0.25f, EasingType::EaseOutQuad, [this]()
    {
        this->m_state = State::Visible;
    });
}

void GlobalTooltip::Hide()
{
    m_state = State::Collapsing;

    To(&m_rect.w, 0.0f, 0.2f, EasingType::EaseInQuad);
    To(&m_rect.h, 0.0f, 0.2f, EasingType::EaseInQuad, [this]()
    {
        this->m_state = State::Hidden;
        this->m_visible = false;
        this->m_current_key.clear();
        this->m_has_pending_key = false;
    });
}

void GlobalTooltip::Update(float dt, const ImVec2& parent_abs_pos)
{
    auto& ctx = UIContext::Get();
    const std::string& active_key = ctx.m_tooltip_active_key;

    // --- 1. 状态流转逻辑 ---
    if (!active_key.empty())
    {
        if (m_state == State::Hidden)
        {
            Show(active_key);
        }
        else if (m_state == State::Visible || m_state == State::Expanding)
        {
            // [修复] 每一帧都根据 active_key 重新计算理想尺寸
            // 这样即使 active_key 字符串没变（例如只是ID），但只要翻译后的内容变长/变短了，我们也能检测到。
            ImVec2 next_size;
            PivotState next_pivot;
            CalculateLayoutInfo(active_key, next_size, next_pivot);

            // 检测变化：Key 变了，或者 目标尺寸 变了，或者 对齐方向 变了
            bool size_changed = (std::abs(next_size.x - m_target_size.x) > 1.0f) || (std::abs(next_size.y - m_target_size.y) > 1.0f);
            bool key_changed = (active_key != m_current_key);
            bool pivot_changed = (next_pivot != m_current_pivot);

            if (pivot_changed)
            {
                // 如果方向变了（比如碰到了屏幕边缘），必须 Hide 再 Show 避免跳变
                m_current_key = active_key; // 更新以防止下一帧重复触发
                Hide();
            }
            else if (size_changed || key_changed)
            {
                m_current_key = active_key;

                // 检查是否需要变大
                bool need_expand_w = next_size.x > m_rect.w;
                bool need_expand_h = next_size.y > m_rect.h;
                bool is_expanding = need_expand_w || need_expand_h;

                // 更新目标尺寸
                m_target_size = next_size;

                // 无论变大变小，都启动 Tween 动画
                To(&m_rect.w, m_target_size.x, 0.20f, EasingType::EaseOutQuad);
                To(&m_rect.h, m_target_size.y, 0.20f, EasingType::EaseOutQuad);

                if (is_expanding)
                {
                    // 逻辑 A: 需要变大 -> 先存 Pending，暂不更新文字
                    m_pending_key = active_key;
                    m_has_pending_key = true;
                }
                else
                {
                    // 逻辑 B: 变小或一样 -> 直接更新文字，边框跟随缩小
                    CommitKey(active_key);
                    m_has_pending_key = false;
                }
            }
            // [Pending Check] 检查是否还在等待边框变大
            else if (m_has_pending_key)
            {
                // 检查当前尺寸是否已经足够接近目标尺寸
                // 只有当宽度和高度都撑开到足够容纳新内容时，才显示新内容
                bool width_ready = m_rect.w >= m_target_size.x - 5.0f; // 留一点余量让文字早点出来
                bool height_ready = m_rect.h >= m_target_size.y - 5.0f;

                if (width_ready && height_ready)
                {
                    CommitKey(m_pending_key);
                    m_has_pending_key = false;
                }
            }
            else
            {
                // 既没有变化，也不是在等待 Pending
                // 如果没有动画在运行，强制对齐目标尺寸防止浮点漂移
                if (m_tweens.empty())
                {
                    m_rect.w = m_target_size.x;
                    m_rect.h = m_target_size.y;
                }

                // 确保文字内容与 Key 同步 (防止 size 没变但 key 变了的情况)
                if (m_text_label->m_source_key_or_text != active_key)
                {
                    CommitKey(active_key);
                }
            }
        }
    }
    else
    {
        if (m_state == State::Visible || m_state == State::Expanding)
        {
            Hide();
        }
    }

    // --- 2. 位置跟随与布局逻辑 ---
    if (m_visible)
    {
        ImVec2 mouse_pos = ImGui::GetIO().MousePos;
        float offset = 15.0f;

        // X轴逻辑：翻转时从右向左长
        if (!m_current_pivot.flip_x) m_rect.x = mouse_pos.x + offset;
        else m_rect.x = mouse_pos.x - offset - m_rect.w;

        // Y轴逻辑：翻转时从下向上长
        if (!m_current_pivot.flip_y) m_rect.y = mouse_pos.y + offset;
        else m_rect.y = mouse_pos.y - offset - m_rect.h;

        // 同步子组件尺寸
        m_panel->m_rect.w = m_rect.w;
        m_panel->m_rect.h = m_rect.h;

        // [关键] 内部文字区域定位
        // 始终固定在左上角 (padding, padding)
        m_text_label->m_rect.x = m_padding;
        m_text_label->m_rect.y = m_padding;
        // 宽高根据当前容器尺寸减去边距，确保不溢出
        m_text_label->m_rect.w = std::max(0.0f, m_rect.w - m_padding * 2.0f);
        m_text_label->m_rect.h = std::max(0.0f, m_rect.h - m_padding * 2.0f);

        // 文字透明度控制
        if (m_target_size.x > 0.1f)
        {
            float ratio = m_rect.w / m_target_size.x;
            // 当尺寸达到 70% 时开始淡入文字
            float target_alpha = std::clamp((ratio - 0.7f) / 0.3f, 0.0f, 1.0f);

            // 如果处于 Pending 状态（内容还没切），我们可以选择保持旧文字 (alpha=1) 或者淡出旧文字
            // 这里的实现是：如果有 Pending，说明我们在撑大框，此时旧文字显得太小，
            // 为了视觉整洁，我们可以让它稍微变淡，或者保持原样。
            // 这里选择保持逻辑简单，只由尺寸比例控制。
            m_text_label->m_alpha = target_alpha;
        }
    }

    // 调用基类处理动画插值
    UIElement::Update(dt, parent_abs_pos);
}

_UI_END
_SYSTEM_END
_NPGS_END