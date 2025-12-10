#include "ui_framework.h"
#include "TechUtils.h"
#include "components/GlobalTooltip.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// --- AnimationUtils ---
float AnimationUtils::Ease(float t, EasingType type)
{
    t = std::clamp(t, 0.0f, 1.0f);
    switch (type)
    {
    case EasingType::EaseInQuad: return t * t;
    case EasingType::EaseOutQuad: return t * (2 - t);
    case EasingType::EaseInOutQuad: return t < .5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
    case EasingType::EaseOutBack: { float c1 = 1.70158f; float c3 = c1 + 1; return 1 + c3 * std::pow(t - 1, 3) + c1 * std::pow(t - 1, 2); }
    default: return t;
    }
}

ImVec4 StyleColor::Resolve() const
{
    ImVec4 base_color;
    if (id == ThemeColorID::Custom)
    {
        base_color = custom_value;
    }
    else if (id != ThemeColorID::None)
    {
        // 在 .cpp 文件中，UIContext::Get() 是完全可见的，不会再报错
        base_color = UIContext::Get().GetThemeColor(id);
    }
    else
    {
        return ImVec4(0, 0, 0, 0); // None
    }

    // Apply modifiers if they exist
    if (alpha_override >= 0.0f)
    {
        base_color.w = alpha_override;
    }

    return base_color;
}

void UIContext::NewFrame()
{
    m_tooltip_candidate_key.clear();
}

// [不变] 组件调用此方法请求显示 Tooltip
void UIContext::RequestTooltip(const std::string& key)
{
    if (m_tooltip_candidate_key.empty() && !key.empty())
    {
        m_tooltip_candidate_key = key;
    }
}

// [核心修正] 在 UIRoot Update 最后调用，更新计时器
void UIContext::UpdateTooltipLogic(float dt)
{
    // 1. 检测悬停的组件是否发生了变化
    if (m_tooltip_candidate_key != m_tooltip_previous_candidate_key)
    {
        // 鼠标移动到了新的组件上，或者移出了所有组件
        m_tooltip_timer = 0.0f;        // 重置计时器
        m_tooltip_active_key.clear();  // 立刻隐藏旧的 Tooltip
    }

    // 2. 如果当前有悬停的组件，则累加计时器
    if (!m_tooltip_candidate_key.empty())
    {
        m_tooltip_timer += dt;
    }

    // 3. 检查计时器是否达到显示阈值
    if (m_tooltip_timer > 0.5f)
    {
        // 将候选者转为激活状态
        m_tooltip_active_key = m_tooltip_candidate_key;
    }

    // 4. 为下一帧记录当前状态
    m_tooltip_previous_candidate_key = m_tooltip_candidate_key;
}

ImVec4 UIContext::GetThemeColor(ThemeColorID id) const
{
    const auto& t = m_theme;
    switch (id)
    {
    case ThemeColorID::Text:          return t.color_text;
    case ThemeColorID::TextHighlight: return t.color_text_highlight;
    case ThemeColorID::TextDisabled:  return t.color_text_disabled;
    case ThemeColorID::Border:        return t.color_border;
    case ThemeColorID::Accent:        return t.color_accent;
    default: return ImVec4(0, 0, 0, 0);
    }
}
void UIContext::SetFocus(UIElement* element) { m_focused_element = element; }
void UIContext::ClearFocus() { m_focused_element = nullptr; }
void UIContext::SetCapture(UIElement* element) { m_captured_element = element; }
void UIContext::ReleaseCapture() { m_captured_element = nullptr; }

// --- UIElement Implementation ---

void UIElement::AddChild(Ptr child)
{
    child->m_parent = this;
    m_children.push_back(child);
}

void UIElement::RemoveChild(Ptr child)
{
    if (!child) return;
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end())
    {
        (*it)->m_parent = nullptr; // 解绑
        m_children.erase(it);      // 安全移除
    }
}
void UIElement::ResetInteraction()
{
    m_clicked = false;
    m_hovered = false;
    // 递归重置子节点
    for (auto& child : m_children)
    {
        child->ResetInteraction();
    }
}
void UIElement::To(float* property, float target, float duration, EasingType easing, TweenCallback on_complete)
{
    m_tweens.erase(std::remove_if(m_tweens.begin(), m_tweens.end(),
        [property](const Tween& t) { return t.target == property; }), m_tweens.end());
    m_tweens.push_back({ property, *property, target, 0.0f, duration, easing, true, on_complete });
}
void UIElement::DrawGlassBackground(ImDrawList* draw_list, const ImVec2& p_min, const ImVec2& p_max, const ImVec4& BackCol)
{
    auto& ctx = UIContext::Get();
    ImTextureID blur_tex = ctx.m_scene_blur_texture;

    if (blur_tex != 0 && ctx.m_display_size.x > 0 && ctx.m_display_size.y > 0)
    {
        ImVec2 uv_min = ImVec2(p_min.x / ctx.m_display_size.x, p_min.y / ctx.m_display_size.y);
        ImVec2 uv_max = ImVec2(p_max.x / ctx.m_display_size.x, p_max.y / ctx.m_display_size.y);
        draw_list->AddImage(blur_tex, p_min, p_max, uv_min, uv_max);

        draw_list->AddRectFilled(
            p_min,
            p_max,
            GetColorWithAlpha(BackCol, 1.0f)
        );
    }
}

void UIElement::UpdateSelf(float dt, const ImVec2& parent_abs_pos)
{
    if (!m_visible) return;

    m_absolute_pos.x = parent_abs_pos.x + m_rect.x;
    m_absolute_pos.y = parent_abs_pos.y + m_rect.y;

    for (auto& t : m_tweens)
    {
        if (!t.active) continue;
        t.current_time += dt;
        float progress = t.current_time / t.duration;

        if (progress >= 1.0f)
        {
            progress = 1.0f;
            *t.target = t.end;
            t.active = false;
            if (t.on_complete) t.on_complete();
        }
        else
        {
            *t.target = t.start + (t.end - t.start) * AnimationUtils::Ease(progress, t.easing);
        }
    }
    m_tweens.erase(std::remove_if(m_tweens.begin(), m_tweens.end(),
        [](const Tween& t) { return !t.active; }), m_tweens.end());
}


void UIElement::Update(float dt, const ImVec2& parent_abs_pos)
{
    // 先更新自己
    UpdateSelf(dt, parent_abs_pos);

    if (!m_visible) return; 

    // 再递归子节点
    for (auto& child : m_children)
    {
        child->Update(dt, m_absolute_pos);
    }
}


void UIElement::Draw(ImDrawList* draw_list)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    bool font_pushed = false;
    ImFont* font = GetFont();
    // 只有当 GetFont 返回的字体与当前 ImGui 栈顶字体不同时，才有必要 Push，
    // 但为了保险和简单，通常直接 Push。
    // 注意：GetFont() 默认会返回 Regular。如果你的设计是 "nullptr 代表继承"，
    // 这里的逻辑需要调整。但根据我们之前的约定，组件应自治，所以这里必须 Push。
    if (font)
    {
        ImGui::PushFont(font);
        font_pushed = true;
    }

    for (auto& child : m_children)
    {
        child->Draw(draw_list);
    }

    if (font_pushed)
    {
        ImGui::PopFont();
    }
}

// [核心修改] 支持鼠标捕获和焦点的事件处理
// --- UIElement::HandleMouseEvent 实现 ---

void UIElement::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& external_handled)
{
    if (!m_visible || m_alpha <= 0.01f)
    {
        m_hovered = false;
        m_clicked = false;
        return;
    }

    // 1. 先递归处理子节点 (反向遍历：从最上层子节点开始)
    // 关键点：即使 handled 已经是 true，我们依然遍历子节点，
    // 这样子节点才有机会将自己的 m_hovered 设为 false。
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        (*it)->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, external_handled);
    }

    // 2. 如果事件已经被子节点（或者更上层的兄弟节点）处理了
    if (external_handled)
    {
        m_hovered = false;
        m_clicked = false;
        return; // 我没有机会交互了，直接返回
    }

    // 3. 只有当事件未被处理时，才检测自身
    Rect abs_rect = { m_absolute_pos.x, m_absolute_pos.y, m_rect.w, m_rect.h };
    bool inside = abs_rect.Contains(mouse_pos);

    // 捕获逻辑
    UIContext& ctx = UIContext::Get();
    if (ctx.m_captured_element == this) inside = true;
    if (mouse_released)
    {
        m_clicked = false;
    }
    if (inside)
    {
        if (!m_tooltip_key.empty())
        {
            UIContext::Get().RequestTooltip(m_tooltip_key);
        }
        // 只有在 block_input 时才算“消耗”了事件
        if (m_block_input)
        {
            m_hovered = true;
            external_handled = true; // 【关键】标记事件已被我消耗，后续遍历到的节点（底层的）将只能看不能动

            if (mouse_clicked)
            {
                m_clicked = true;
                if (m_focusable) ctx.SetFocus(this);
            }
        }
        else
        {
            // 不阻挡输入（如 Panel），虽然 inside，但不消耗 handled
            m_hovered = true;
        }
    }
    else
    {
        m_hovered = false;
        // 注意：这里不要设置 m_clicked = false，因为拖拽可能移出范围
        if (mouse_released) m_clicked = false;
    }
}

bool UIElement::HandleKeyboardEvent()
{
    // 默认不处理，子类 InputField 重写
    return false;
}

ImFont* UIElement::GetFont() const
{
    if (m_font) return m_font;

    // 如果没有指定，默认回退到全局常规字体
    // 确保你的 UIContext 里初始化了 m_font_regular
    return UIContext::Get().m_font_regular ? UIContext::Get().m_font_regular : ImGui::GetFont();
}

// --- API ---
ImU32 UIElement::GetColorWithAlpha(const ImVec4& col, float global_alpha) const
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4(col.x, col.y, col.z, col.w * m_alpha * global_alpha));
}
bool UIElement::IsFocused() const { return UIContext::Get().m_focused_element == this; }
void UIElement::RequestFocus() { UIContext::Get().SetFocus(this); }
void UIElement::CaptureMouse() { UIContext::Get().SetCapture(this); }
void UIElement::ReleaseMouse() { UIContext::Get().ReleaseCapture(); }

// --- Panel 实现 ---
void Panel::Draw(ImDrawList* draw_list)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    // 1. 获取屏幕尺寸用于计算 UV
    ImVec2 display_sz = UIContext::Get().m_display_size;

    // 2. 计算矩形范围
    ImVec2 p_min = TechUtils::Snap(m_absolute_pos);
    ImVec2 p_max = TechUtils::Snap(ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h));

    // --- [核心逻辑] 绘制毛玻璃背景 ---
    ImTextureID blur_tex = UIContext::Get().m_scene_blur_texture;

    if (m_use_glass_effect && blur_tex != 0)
    {
        // 计算 UV：将 UI 坐标映射到 [0,1] 范围
        // 这样纹理就像是“贴”在屏幕后面不动，而 Panel 只是一个窗口
        ImVec2 uv_min = ImVec2(p_min.x / display_sz.x, p_min.y / display_sz.y);
        ImVec2 uv_max = ImVec2(p_max.x / display_sz.x, p_max.y / display_sz.y);
        // 绘制模糊纹理
        draw_list->AddImage(
            blur_tex,
            p_min, p_max,
            uv_min, uv_max
        );

    }
    ImVec4 bg = m_bg_color.Resolve();

    draw_list->AddRectFilled(
        m_absolute_pos,
        ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h),
        GetColorWithAlpha(bg, 1.0f)
    );

    UIElement::Draw(draw_list);
}
// --- Image 实现 ---
void Image::Update(float dt, const ImVec2& parent_abs_pos)
{
    // [核心逻辑] 如果开启了自动高度，且宽高比有效
    if (m_auto_height && m_aspect_ratio > 0.001f)
    {
        // 高度 = 当前宽度 / 宽高比
        m_rect.h = m_rect.w / m_aspect_ratio;
    }

    // 调用基类更新位置和动画
    UIElement::Update(dt, parent_abs_pos);
}
void Image::Draw(ImDrawList* draw_list)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    draw_list->AddImage(
        m_texture_id,
        m_absolute_pos,
        ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h),
        m_uv0, m_uv1,
        GetColorWithAlpha(m_tint_col, 1.0f)
    );
}

// --- VBox 实现 (带 Alignment) ---
void VBox::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (!m_visible) return;

    // [关键修改] 只更新自身位置和动画，不调用基类 Update，避免触发默认的子节点递归
    UpdateSelf(dt, parent_abs_pos);

    // --- Pass 1: 测量 ---
    float fixed_height = 0.0f;
    int fill_count = 0;
    int visible_children = 0;

    for (const auto& child : m_children)
    {
        if (!child->m_visible) continue;
        visible_children++;
        if (child->m_fill_v) fill_count++;
        else fixed_height += child->m_rect.h;
    }

    float total_padding = (visible_children > 1) ? ((float)visible_children - 1.0f) * m_padding : 0.0f;

    // --- Pass 2: 分配与布局 ---
    float remaining_height = m_rect.h - fixed_height - total_padding;
    float fill_child_height = (fill_count > 0) ? std::max(0.0f, remaining_height / fill_count) : 0.0f;

    float current_y = 0.0f;
    for (size_t i = 0; i < m_children.size(); ++i)
    {
        auto& child = m_children[i];
        if (!child->m_visible) continue;

        // A. 确定高度
        if (child->m_fill_v) child->m_rect.h = fill_child_height;

        // B. 设置位置
        child->m_rect.y = current_y;

        // C. 水平对齐
        if (child->m_align_h == Alignment::Stretch) { child->m_rect.x = 0; child->m_rect.w = this->m_rect.w; }
        else if (child->m_align_h == Alignment::Center) { child->m_rect.x = (this->m_rect.w - child->m_rect.w) * 0.5f; }
        else if (child->m_align_h == Alignment::End) { child->m_rect.x = this->m_rect.w - child->m_rect.w; }
        else { child->m_rect.x = 0; }

        // D. [关键] 手动调用子节点 Update，这是该子节点在这一帧唯一的一次更新
        child->Update(dt, m_absolute_pos);

        // E. 移动游标
        current_y += child->m_rect.h;
        if (i < (size_t)visible_children - 1) current_y += m_padding;
    }

    if (fill_count == 0) this->m_rect.h = current_y;
}

// --- HBox 实现 (修复多重更新) ---
void HBox::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (!m_visible) return;

    // [关键修改] 只更新自身，接管子节点控制权
    UpdateSelf(dt, parent_abs_pos);

    // --- Pass 1: 测量 ---
    float fixed_width = 0.0f;
    int fill_count = 0;
    int visible_children = 0;

    for (const auto& child : m_children)
    {
        if (!child->m_visible) continue;
        visible_children++;
        if (child->m_fill_h) fill_count++;
        else fixed_width += child->m_rect.w;
    }

    float total_padding = (visible_children > 1) ? ((float)visible_children - 1.0f) * m_padding : 0.0f;

    // --- Pass 2: 分配与布局 ---
    float remaining_width = m_rect.w - fixed_width - total_padding;
    float fill_child_width = (fill_count > 0) ? std::max(0.0f, remaining_width / fill_count) : 0.0f;

    float current_x = 0.0f;
    float max_child_height = 0.0f;

    for (size_t i = 0; i < m_children.size(); ++i)
    {
        auto& child = m_children[i];
        if (!child->m_visible) continue;

        // A. 确定宽度
        if (child->m_fill_h) child->m_rect.w = fill_child_width;

        // B. 设置位置
        child->m_rect.x = current_x;

        // C. 垂直对齐
        if (child->m_align_v == Alignment::Stretch) { child->m_rect.y = 0; child->m_rect.h = this->m_rect.h; }
        else if (child->m_align_v == Alignment::Center) { child->m_rect.y = (this->m_rect.h - child->m_rect.h) * 0.5f; }
        else if (child->m_align_v == Alignment::End) { child->m_rect.y = this->m_rect.h - child->m_rect.h; }
        else { child->m_rect.y = 0; }

        // D. [关键] 手动更新子节点
        child->Update(dt, m_absolute_pos);

        if (child->m_rect.h > max_child_height) max_child_height = child->m_rect.h;

        current_x += child->m_rect.w;
        if (i < (size_t)visible_children - 1) current_x += m_padding;
    }

    if (m_rect.h <= 0.1f) m_rect.h = max_child_height;
    if (fill_count == 0) this->m_rect.w = current_x;
}

// --- ScrollView 实现 (修复多重更新) ---
void ScrollView::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (!m_visible) return;

    // [关键修改] 只更新自身
    UpdateSelf(dt, parent_abs_pos);

    // 滚动逻辑 (保留原样)
    float max_scroll = std::max(0.0f, m_content_height - m_rect.h);
    if (m_hovered)
    {
        float wheel = ImGui::GetIO().MouseWheel;
        if (std::abs(wheel) > 0.0f) m_target_scroll_y -= wheel * m_scroll_speed;
    }
    m_target_scroll_y = std::clamp(m_target_scroll_y, 0.0f, max_scroll);
    float diff = m_target_scroll_y - m_scroll_y;
    if (std::abs(diff) < 0.5f) m_scroll_y = m_target_scroll_y;
    else m_scroll_y += diff * std::min(1.0f, m_smoothing_speed * dt);

    // 布局子元素
    float current_max_bottom = 0.0f;
    float view_w = m_rect.w - (m_show_scrollbar ? 6.0f : 0.0f);

    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;
        if (child->m_fill_h) child->m_rect.w = view_w;
        child->m_rect.x = 0;
        child->m_rect.w = view_w;
        child->m_rect.y = -m_scroll_y;

        // [关键] 手动更新子节点
        child->Update(dt, m_absolute_pos);

        if (child->m_rect.h > current_max_bottom) current_max_bottom = child->m_rect.h;
    }

    m_content_height = current_max_bottom;

    // 再次 Clamp 逻辑 (保留原样)
    float new_max_scroll = std::max(0.0f, m_content_height - m_rect.h);
    if (m_target_scroll_y > new_max_scroll) m_target_scroll_y = new_max_scroll;
    if (m_scroll_y > new_max_scroll + 50.0f) m_scroll_y = new_max_scroll;
}


void ScrollView::Draw(ImDrawList* draw_list)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    ImVec2 clip_min = m_absolute_pos;
    ImVec2 clip_max = ImVec2(clip_min.x + m_rect.w, clip_min.y + m_rect.h);

    draw_list->PushClipRect(clip_min, clip_max, true);

    for (auto& child : m_children)
    {
        float child_abs_y = child->m_absolute_pos.y;
        float child_h = child->m_rect.h;
        if (child_abs_y + child_h < clip_min.y || child_abs_y > clip_max.y)
            continue;
        child->Draw(draw_list);
    }

    draw_list->PopClipRect();
}

void ScrollView::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    Rect abs_rect = { m_absolute_pos.x, m_absolute_pos.y, m_rect.w, m_rect.h };
    bool inside = abs_rect.Contains(mouse_pos);

    // 1. 如果被更上层（父级/遮挡层）处理了，或者根本不在范围内
    if (handled || !inside)
    {
        m_hovered = false;
        // 依然传递 dummy handled 给子节点以重置它们
        bool dummy_handled = true;
        for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
            (*it)->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, dummy_handled);
        return;
    }

    // [核心修复]：只要代码走到这里，说明鼠标在 ScrollView 范围内，且没有被上层遮挡。
    // 此时无论子节点是否消耗点击，ScrollView 都应该处于悬停状态，以便响应滚轮。
    m_hovered = true;

    // 2. 正常分发给子节点
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        (*it)->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);
    }

    // 3. 自身点击消耗逻辑
    // 如果子节点没有消耗点击，且 ScrollView 配置为阻挡输入，则标记 handled
    if (!handled)
    {
        if (m_block_input) handled = true;
    }
    // 注意：不要在这里写 else { m_hovered = false; }，这会导致子节点消耗事件后滚轮失效
}


void HorizontalScrollView::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (!m_visible) return;

    // 1. 基础位置更新
    UpdateSelf(dt, parent_abs_pos); // 使用修正后的 UpdateSelf

    // 2. 计算滚动边界
    float max_scroll = std::max(0.0f, m_content_width - m_rect.w);

    // 3. 滚动输入处理
    if (m_hovered)
    {
        // 鼠标滚轮同时控制水平滚动
        float wheel_x = ImGui::GetIO().MouseWheelH; // 水平滚轮 (通常是 Shift + 滚轮)
        float wheel_y = ImGui::GetIO().MouseWheel;  // 垂直滚轮

        if (std::abs(wheel_y) > std::abs(wheel_x))
        {
            m_target_scroll_x -= wheel_y * m_scroll_speed;
        }
        else
        {
            m_target_scroll_x -= wheel_x * m_scroll_speed;
        }
    }

    // 4. 限制目标值并平滑插值
    m_target_scroll_x = std::clamp(m_target_scroll_x, 0.0f, max_scroll);
    float diff = m_target_scroll_x - m_scroll_x;
    if (std::abs(diff) < 0.5f) m_scroll_x = m_target_scroll_x;
    else m_scroll_x += diff * std::min(1.0f, m_smoothing_speed * dt);

    float view_h = m_rect.h - (m_show_scrollbar ? 6.0f : 0.0f);
    // 5. 布局子元素
    float total_child_width = 0.0f;
    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;
        if (child->m_fill_v) child->m_rect.h = view_h;
        child->m_rect.x = -m_scroll_x;

        // 子元素垂直方向上可以居中或拉伸
        child->m_align_v = Alignment::Stretch;

        // 手动更新子节点
        child->Update(dt, m_absolute_pos);

        // 测量内容总宽度
        total_child_width = child->m_rect.x + child->m_rect.w; // HBox 只有一个
    }

    // 6. 更新内容总宽度
    m_content_width = total_child_width + m_scroll_x;

    // 再次 clamp 防止内容变少时悬空
    float new_max_scroll = std::max(0.0f, m_content_width - m_rect.w);
    if (m_target_scroll_x > new_max_scroll) m_target_scroll_x = new_max_scroll;
    if (m_scroll_x > new_max_scroll) m_scroll_x = new_max_scroll;
}


void HorizontalScrollView::Draw(ImDrawList* draw_list)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    // 1. 设置裁剪矩形
    ImVec2 clip_min = m_absolute_pos;
    ImVec2 clip_max = ImVec2(clip_min.x + m_rect.w, clip_min.y + m_rect.h);
    draw_list->PushClipRect(clip_min, clip_max, true);

    // 2. 遍历并剔除不可见的子元素
    for (auto& child : m_children)
    {
        // 计算子元素的绝对X坐标和宽度
        float child_abs_x = child->m_absolute_pos.x;
        float child_w = child->m_rect.w;

        // 如果子元素完全在可视区域的左侧或右侧，则跳过绘制
        if (child_abs_x + child_w < clip_min.x || child_abs_x > clip_max.x)
        {
            continue;
        }

        child->Draw(draw_list);
    }

    draw_list->PopClipRect();
}

// [修改] 返回值改为 void，新增 bool& handled 参数
void HorizontalScrollView::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    Rect abs_rect = { m_absolute_pos.x, m_absolute_pos.y, m_rect.w, m_rect.h };
    bool inside = abs_rect.Contains(mouse_pos);

    // 1. 遮挡/范围检测
    if (handled || !inside)
    {
        m_hovered = false;
        bool dummy_handled = true;
        for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
            (*it)->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, dummy_handled);
        return;
    }

    // [核心修复] 标记悬停，允许滚轮工作
    m_hovered = true;

    // 2. 分发给子节点
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        (*it)->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);
    }

    // 3. 自身消耗
    if (!handled)
    {
        if (m_block_input) handled = true; // 虽然你之前代码里这里写的是 if(m_hovered) handled=true，但逻辑是一样的
    }
}

// 在文件末尾，_UI_END 之前，添加 UIRoot 的实现

// --- UIRoot 实现 ---
UIRoot::UIRoot()
{
    ImGuiIO& io = ImGui::GetIO();
    m_rect = { 0, 0, io.DisplaySize.x, io.DisplaySize.y };
    m_block_input = false; // 关键：自身不阻挡输入，允许事件穿透
    m_visible = true;
    m_tooltip = std::make_shared<GlobalTooltip>();
}

void UIRoot::Update(float dt)
{
    // --- 1. 全局事件与状态管理 ---
    ImGuiIO& io = ImGui::GetIO();

    UIContext& ctx = UIContext::Get();
    ctx.NewFrame();

    m_rect = { 0, 0, ctx.m_display_size.x, ctx.m_display_size.y };
    UIElement* focused_element_before_events = ctx.m_focused_element;

    // --- 2. 鼠标事件分发 ---
    // [修改] 创建 handled 标记
    bool is_handled = false;
    UIElement* captured = ctx.m_captured_element;

    if (captured)
    {
        // 捕获模式下，直接传给捕获元素
        captured->HandleMouseEvent(io.MousePos, io.MouseDown[0], io.MouseClicked[0], io.MouseReleased[0], is_handled);
    }
    else
    {
        // 从根节点开始分发事件，传入 is_handled 引用
        HandleMouseEvent(io.MousePos, io.MouseDown[0], io.MouseClicked[0], io.MouseReleased[0], is_handled);
    }

    // 如果 is_handled 最终为 true，说明 UI 消耗了鼠标
    if (is_handled) io.WantCaptureMouse = true;

    // --- 4. 键盘事件分发 ---
    UIElement* focused = ctx.m_focused_element;
    if (focused)
    {
        if (focused->HandleKeyboardEvent())
        {
            io.WantCaptureKeyboard = true;
        }
    }

    // --- 5. 更新所有UI元素的状态和动画 ---
    UIElement::Update(dt, { 0,0 });
    ctx.UpdateTooltipLogic(dt);
    m_tooltip->Update(dt, { 0, 0 });
}
void UIRoot::Draw() {

    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    UIElement::Draw(draw_list);
    if (m_tooltip)
    {
        m_tooltip->Draw(draw_list);
    }
}

void UIRoot::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled)
{
    // 1. 先保存当前焦点状态
    UIContext& ctx = UIContext::Get();
    UIElement* focused_before = ctx.m_focused_element;

    // 2. 正常分发事件给子节点
    // 调用基类 UIElement::HandleMouseEvent，它会递归调用子节点
    // 如果某个子节点消耗了事件，handled 会变成 true
    UIElement::HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);

    // 3. [规范的失焦逻辑]
    // 如果发生了点击，并且没有任何子节点消耗这个点击 (!handled)...
    if (mouse_clicked && !handled)
    {
        if (focused_before)
        {
            ctx.ClearFocus();
        }
    }

}
_UI_END
_SYSTEM_END
_NPGS_END