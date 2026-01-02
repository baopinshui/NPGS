#include "ui_framework.h"
#include "TechUtils.h"
#include "components/GlobalTooltip.h"
//#define _DEBUG // 用于开启调试功能，如绘制边框

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
    default: return t; // Linear
    }
}

// --- Rect ---
bool Rect::Contains(const ImVec2& p) const
{
    bool inside = p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;

#ifdef _DEBUG
    if (true) // 调试开关：绘制所有元素的包围盒
    {
        ImDrawList* fg_draw = ImGui::GetForegroundDrawList();
        ImU32 col = inside ? IM_COL32(0, 255, 0, 200) : IM_COL32(255, 0, 0, 100);
        fg_draw->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), col);
        if (inside) fg_draw->AddLine(ImVec2(x, y), ImVec2(x + w, y + h), col);
    }
#endif
    return inside;
}

// --- StyleColor ---
ImVec4 StyleColor::Resolve() const
{
    ImVec4 base_color;
    if (id == ThemeColorID::Custom)
    {
        base_color = custom_value;
    }
    else if (id != ThemeColorID::None)
    {
        base_color = UIContext::Get().GetThemeColor(id);
    }
    else
    {
        return ImVec4(0, 0, 0, 0); // 无效颜色
    }

    if (alpha_override >= 0.0f)
    {
        base_color.w = alpha_override;
    }
    return base_color;
}

// --- UIContext ---
void UIContext::NewFrame() { m_tooltip_candidate_key.clear(); }

void UIContext::RequestTooltip(const std::string& key)
{
    if (m_tooltip_candidate_key.empty() && !key.empty())
        m_tooltip_candidate_key = key;
}

void UIContext::UpdateTooltipLogic(float dt)
{
    if (m_tooltip_candidate_key != m_tooltip_previous_candidate_key)
    {
        m_tooltip_timer = 0.0f;
        m_tooltip_active_key.clear();
    }
    if (!m_tooltip_candidate_key.empty())
    {
        m_tooltip_timer += dt;
    }
    if (m_tooltip_timer > 0.2f)
    { // 悬停0.2秒后激活Tooltip
        m_tooltip_active_key = m_tooltip_candidate_key;
    }
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


// =================================================================================
// --- UIElement Implementation ---
// =================================================================================

// --- ID Management ---
void UIElement::SetName(const std::string& name)
{
    if (m_name != name)
    {
        m_name = name;
        InvalidateIDCache(); // 名字变了，ID缓存必须失效
    }
}

std::string& UIElement::GetID()
{
    if (m_id_dirty)
    {
        if (m_parent)
        {
            const std::string& parent_id = m_parent->GetID();
            // 规则：如果自身name为空，则为“透明”容器，ID直接继承父ID
            if (m_name.empty())
            {
                m_cached_id = parent_id;
            }
            // 规则：如果父ID为空（例如父是Root或也是透明容器），则不加点
            else if (parent_id.empty())
            {
                m_cached_id = m_name;
            }
            else
            {
                m_cached_id = parent_id + "." + m_name;
            }
        }
        else // 没有父节点（通常是UIRoot），ID就是自己的名字
        {
            m_cached_id = m_name;
        }

        m_id_dirty = false; // 计算完毕，清除脏标记
    }
    return m_cached_id;
}

void UIElement::InvalidateIDCache()
{
    if (!m_id_dirty) // 避免不必要的递归
    {
        m_id_dirty = true;
        if (m_root)
        {
            m_root->MarkIDMapDirty();
        }
        // 递归标记所有子节点为脏，因为它们的ID路径也已改变
        for (auto& child : m_children)
        {
            child->InvalidateIDCache();
        }
    }
}

// --- Hierarchy Management ---
void UIElement::AddChild(Ptr child)
{
    child->m_parent = this;
    child->m_root = this->m_root;
    m_children.push_back(child);

    // 子元素的父节点变了，其ID路径需要重新计算
    child->InvalidateIDCache();
}

void UIElement::RemoveChild(Ptr child)
{
    if (!child) return;
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end())
    {
        (*it)->InvalidateIDCache();
        (*it)->m_parent = nullptr;
        (*it)->m_root = nullptr;
        m_children.erase(it);
    }
}

void UIElement::ResetInteraction()
{
    m_clicked = false;
    m_hovered = false;
    for (auto& child : m_children) child->ResetInteraction();
}

// --- Animation ---
void UIElement::To(float* property, float target, float duration, EasingType easing, TweenCallback on_complete)
{
    // 如果已存在针对该属性的动画，先移除
    m_tweens.erase(std::remove_if(m_tweens.begin(), m_tweens.end(),
        [property](const Tween& t) { return t.target == property; }), m_tweens.end());
    // 添加新的动画
    m_tweens.push_back({ property, *property, target, 0.0f, duration, easing, true, on_complete });
}

// --- Drawing Helpers ---
void UIElement::DrawGlassBackground(ImDrawList* draw_list, const ImVec2& p_min, const ImVec2& p_max, const ImVec4& BackCol)
{
    auto& ctx = UIContext::Get();
    ImTextureID blur_tex = ctx.m_scene_blur_texture;

    if (blur_tex != 0 && ctx.m_display_size.x > 0 && ctx.m_display_size.y > 0)
    {
        ImVec2 uv_min = ImVec2(p_min.x / ctx.m_display_size.x, p_min.y / ctx.m_display_size.y);
        ImVec2 uv_max = ImVec2(p_max.x / ctx.m_display_size.x, p_max.y / ctx.m_display_size.y);
        ImDrawList* bg_list = ImGui::GetBackgroundDrawList();
        ImVec2 clip_min = draw_list->GetClipRectMin();
        ImVec2 clip_max = draw_list->GetClipRectMax();

        bg_list->PushClipRect(clip_min, clip_max, true);
        bg_list->AddImage(blur_tex, p_min, p_max, uv_min, uv_max);
        bg_list->PopClipRect();

        draw_list->AddRectFilled(p_min, p_max, GetColorWithAlpha(BackCol, 1.0f));
    }
}

// --- Core Update & Layout ---
void UIElement::UpdateSelf(float dt)
{
    if (!m_visible) return;

    // 更新所有活动的动画
    for (auto& t : m_tweens)
    {
        if (!t.active) continue;
        t.current_time += dt;
        float progress = std::min(1.0f, t.current_time / t.duration);

        *t.target = t.start + (t.end - t.start) * AnimationUtils::Ease(progress, t.easing);

        if (progress >= 1.0f)
        {
            t.active = false;
            if (t.on_complete) t.on_complete();
        }
    }
    // 移除已完成的动画
    m_tweens.erase(std::remove_if(m_tweens.begin(), m_tweens.end(),
        [](const Tween& t) { return !t.active; }), m_tweens.end());
}

void UIElement::Update(float dt)
{
    UpdateSelf(dt);
    if (!m_visible) return;
    for (auto& child : m_children)
    {
        child->Update(dt);
    }
}

ImVec2 UIElement::Measure(ImVec2 available_size)
{
    if (!m_visible) return { 0, 0 };

    // 默认的Measure行为：期望尺寸由子元素撑开（如果是Content），或为0（如果是Stretch）。
    // 具体容器（VBox/HBox）会重写此逻辑。
    float max_child_w = 0.0f;
    float max_child_h = 0.0f;

    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;
        ImVec2 child_size = child->Measure(available_size);
        max_child_w = std::max(max_child_w, child_size.x);
        max_child_h = std::max(max_child_h, child_size.y);
    }

    float target_w = 0.0f;
    if (m_width.IsFixed()) target_w = m_width.value;
    else if (m_width.IsContent()) target_w = max_child_w;

    float target_h = 0.0f;
    if (m_height.IsFixed()) target_h = m_height.value;
    else if (m_height.IsContent()) target_h = max_child_h;

    m_desired_size = { target_w, target_h };
    return m_desired_size;
}

void UIElement::Arrange(const Rect& final_rect)
{
    // 1. 记录自身的位置和大小
    m_rect = final_rect;
    if (m_parent)
    {
        m_absolute_pos.x = m_parent->m_absolute_pos.x + m_rect.x;
        m_absolute_pos.y = m_parent->m_absolute_pos.y + m_rect.y;
    }
    else // 根节点
    {
        m_absolute_pos.x = m_rect.x;
        m_absolute_pos.y = m_rect.y;
    }

    if (!m_visible) return;

    // 2. 默认的Arrange行为：将子元素根据其尺寸模式和对齐方式，放置在自身区域内。
    // Panel等简单容器会使用此逻辑。VBox/HBox等会重写。
    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;

        // 分配尺寸
        float child_w = child->m_width.IsStretch() ? m_rect.w : child->m_desired_size.x;
        float child_h = child->m_height.IsStretch() ? m_rect.h : child->m_desired_size.y;

        // 计算对齐偏移
        float offset_x = 0.0f;
        if (child_w < m_rect.w)
        {
            if (child->m_align_h == Alignment::Center) offset_x = (m_rect.w - child_w) * 0.5f;
            else if (child->m_align_h == Alignment::End) offset_x = m_rect.w - child_w;
        }
        float offset_y = 0.0f;
        if (child_h < m_rect.h)
        {
            if (child->m_align_v == Alignment::Center) offset_y = (m_rect.h - child_h) * 0.5f;
            else if (child->m_align_v == Alignment::End) offset_y = m_rect.h - child_h;
        }

        // 递归排列子元素
        child->Arrange({ offset_x, offset_y, child_w, child_h });
    }
}

void UIElement::Draw(ImDrawList* draw_list)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    // 设置字体
    ImFont* font = GetFont();
    bool font_pushed = (font != nullptr);
    if (font_pushed) ImGui::PushFont(font);

    // 递归绘制子元素
    for (auto& child : m_children)
    {
        child->Draw(draw_list);
    }

    if (font_pushed) ImGui::PopFont();
}

// --- Event Handling ---
void UIElement::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& external_handled)
{
    if (!m_visible || m_alpha <= 0.01f)
    {
        m_hovered = false;
        m_clicked = false;
        return;
    }

    UIContext& ctx = UIContext::Get();

    // 如果当前元素正在捕获鼠标（例如按钮被按住）
    if (ctx.m_captured_element == this)
    {
        external_handled = true; // 独占事件
        Rect abs_rect = { m_absolute_pos.x, m_absolute_pos.y, m_rect.w, m_rect.h };
        bool inside = abs_rect.Contains(mouse_pos);
        m_hovered = inside;

        // 处理鼠标释放
        if (mouse_released)
        {
            if (inside) OnClick(); // 只有在元素内部释放才算有效点击
            ctx.ReleaseCapture();
            m_clicked = false;
        }
        return;
    }

    // --- 常规事件处理流程 ---

    // 1. 倒序遍历子元素，让顶层的子元素先处理事件
    for (size_t i = m_children.size(); i > 0; --i)
    {
        m_children[i - 1]->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, external_handled);
    }

    // 如果事件已被子元素处理，则自身不再处理
    if (external_handled)
    {
        m_hovered = false;
        m_clicked = false;
        return;
    }

    // 2. 自身命中测试
    Rect abs_rect = { m_absolute_pos.x, m_absolute_pos.y, m_rect.w, m_rect.h };
    bool inside = abs_rect.Contains(mouse_pos);

    if (mouse_released) m_clicked = false;

    if (inside)
    {
        // 请求显示Tooltip
        if (!m_tooltip_key.empty()) ctx.RequestTooltip(m_tooltip_key);

        m_hovered = true;
        // 如果元素需要阻挡输入
        if (m_block_input)
        {
            external_handled = true; // 标记事件已被处理
            if (mouse_clicked)
            {
                m_clicked = true;
                if (m_focusable) ctx.SetFocus(this);
                // 关键：按下时开始捕获鼠标
                ctx.SetCapture(this);
            }
        }
    }
    else
    {
        m_hovered = false;
    }
}

bool UIElement::HandleKeyboardEvent() { return false; /* 默认不处理键盘事件 */ }

// --- Getters & Helpers ---
ImFont* UIElement::GetFont() const
{
    if (m_font) return m_font;
    return UIContext::Get().m_font_regular ? UIContext::Get().m_font_regular : ImGui::GetFont();
}

ImU32 UIElement::GetColorWithAlpha(const ImVec4& col, float global_alpha) const
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4(col.x, col.y, col.z, col.w * m_alpha * global_alpha));
}

void UIElement::OnClick()
{
    if (on_click) on_click();
}

bool UIElement::IsFocused() const { return UIContext::Get().m_focused_element == this; }
void UIElement::RequestFocus() { UIContext::Get().SetFocus(this); }
void UIElement::CaptureMouse() { UIContext::Get().SetCapture(this); }
void UIElement::ReleaseMouse() { UIContext::Get().ReleaseCapture(); }


// =================================================================================
// --- Panel Implementation ---
// =================================================================================

void Panel::Draw(ImDrawList* draw_list)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    ImVec2 p_min = TechUtils::Snap(m_absolute_pos);
    ImVec2 p_max = TechUtils::Snap(ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h));
    ImVec4 bg = m_bg_color.Resolve();

    if (m_use_glass_effect && UIContext::Get().m_scene_blur_texture != 0)
    {
        DrawGlassBackground(draw_list, p_min, p_max, bg);
    }
    else if (bg.w > 0.0f) // 透明背景不绘制
    {
        draw_list->AddRectFilled(p_min, p_max, GetColorWithAlpha(bg, 1.0f));
    }

    // 调用基类Draw来绘制子元素
    UIElement::Draw(draw_list);
}


// =================================================================================
// --- Image Implementation ---
// =================================================================================

ImVec2 Image::Measure(ImVec2 available_size)
{
    if (!m_visible) return m_desired_size = { 0, 0 };

    // 如果没有有效的宽高比，退化为普通元素
    if (m_aspect_ratio <= 0.001f) return UIElement::Measure(available_size);

    // 根据固定/拉伸属性和可用空间，计算期望尺寸
    if (m_width.IsFixed() && m_height.IsFixed())
    {
        m_desired_size = { m_width.value, m_height.value };
    }
    else if (m_width.IsFixed())
    {
        m_desired_size = { m_width.value, m_width.value / m_aspect_ratio };
    }
    else if (m_height.IsFixed())
    {
        m_desired_size = { m_height.value * m_aspect_ratio, m_height.value };
    }
    else // 宽高都未固定
    {
        // 核心逻辑：利用父容器提供的有限空间来确定尺寸
        // 场景A (VBox中的拉伸图片): 宽度受限，根据宽度计算高度
        if (m_width.IsStretch() && available_size.x < FLT_MAX)
        {
            float w = available_size.x;
            float h = w / m_aspect_ratio;
            m_desired_size = { w, h };
        }
        // 场景B (HBox中的拉伸图片): 高度受限，根据高度计算宽度
        else if (m_height.IsStretch() && available_size.y < FLT_MAX)
        {
            float h = available_size.y;
            float w = h * m_aspect_ratio;
            m_desired_size = { w, h };
        }
        // 场景C (无限空间): 无法确定大小
        else
        {
            m_desired_size = { 0, 0 };
        }
    }

    return m_desired_size;
}

void Image::Arrange(const Rect& final_rect)
{
    // 如果没有有效的宽高比，退化为普通元素
    if (m_aspect_ratio <= 0.001f)
    {
        UIElement::Arrange(final_rect);
        return;
    }

    // 1. 在父级分配的空间(final_rect)内，计算保持宽高比所能达到的最大尺寸
    float target_w, target_h;
    if (final_rect.w <= 0 || final_rect.h <= 0)
    {
        target_w = 0;
        target_h = 0;
    }
    else
    {
        float container_aspect = final_rect.w / final_rect.h;
        if (container_aspect > m_aspect_ratio) // 容器更"宽"，以高度为准
        {
            target_h = final_rect.h;
            target_w = target_h * m_aspect_ratio;
        }
        else // 容器更"高"，以宽度为准
        {
            target_w = final_rect.w;
            target_h = target_w / m_aspect_ratio;
        }
    }

    // 2. 根据对齐方式，计算偏移量
    float offset_x = 0.0f;
    if (m_align_h == Alignment::Center) offset_x = (final_rect.w - target_w) * 0.5f;
    else if (m_align_h == Alignment::End) offset_x = final_rect.w - target_w;

    float offset_y = 0.0f;
    if (m_align_v == Alignment::Center) offset_y = (final_rect.h - target_h) * 0.5f;
    else if (m_align_v == Alignment::End) offset_y = final_rect.h - target_h;

    // 3. 设置最终的相对矩形和绝对坐标
    m_rect.x = final_rect.x + offset_x;
    m_rect.y = final_rect.y + offset_y;
    m_rect.w = target_w;
    m_rect.h = target_h;

    if (m_parent)
    {
        m_absolute_pos.x = m_parent->m_absolute_pos.x + m_rect.x;
        m_absolute_pos.y = m_parent->m_absolute_pos.y + m_rect.y;
    }
    else
    {
        m_absolute_pos = { m_rect.x, m_rect.y };
    }
}

void Image::Draw(ImDrawList* draw_list)
{
    if (!m_visible || m_alpha <= 0.01f || m_texture_id == 0) return;

    draw_list->AddImage(
        m_texture_id,
        m_absolute_pos,
        ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h),
        m_uv0, m_uv1,
        GetColorWithAlpha(m_tint_col, 1.0f)
    );
}

// =================================================================================
// --- VBox Implementation ---
// =================================================================================

ImVec2 VBox::Measure(ImVec2 available_size)
{
    if (!m_visible) return { 0,0 };

    float total_h = 0.0f;
    float max_w = 0.0f;
    int visible_children = 0;

    // 测量所有子元素，累加高度，取最大宽度
    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;
        visible_children++;
        // VBox中，子元素的宽度受限，高度无限
        ImVec2 child_size = child->Measure(available_size);
        total_h += child_size.y;
        max_w = std::max(max_w, child_size.x);
    }

    if (visible_children > 1) total_h += (visible_children - 1) * m_padding;

    // 根据自身尺寸模式，确定最终期望大小
    float my_w = m_width.IsFixed() ? m_width.value : max_w;
    float my_h = m_height.IsFixed() ? m_height.value : total_h;

    return m_desired_size = { my_w, my_h };
}

void VBox::Arrange(const Rect& final_rect)
{
    UIElement::Arrange(final_rect); // 首先设置自身位置
    if (!m_visible) return;

    // 1. 统计信息：计算固定高度总和与拉伸权重总和
    float fixed_h_used = 0.0f;
    float total_flex = 0.0f;
    int visible_children = 0;

    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;
        visible_children++;
        if (child->m_height.IsStretch()) total_flex += child->m_height.value;
        else fixed_h_used += child->m_desired_size.y;
    }

    float total_padding = (visible_children > 1) ? ((float)visible_children - 1.0f) * m_padding : 0.0f;
    float remaining_h = std::max(0.0f, m_rect.h - fixed_h_used - total_padding);

    // 2. 布局：遍历子元素，分配位置和大小
    float current_y = 0.0f;
    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;

        // 计算高度
        float child_h = 0.0f;
        if (child->m_height.IsStretch())
        {
            if (total_flex > 0.0f) child_h = (child->m_height.value / total_flex) * remaining_h;
        }
        else
        {
            child_h = child->m_desired_size.y;
        }

        // 计算宽度和水平位置（交叉轴）
        float child_w = child->m_width.IsStretch() ? m_rect.w : child->m_desired_size.x;
        float child_x = 0.0f;
        if (child_w < m_rect.w)
        {
            if (child->m_align_h == Alignment::Center) child_x = (m_rect.w - child_w) * 0.5f;
            else if (child->m_align_h == Alignment::End) child_x = m_rect.w - child_w;
        }

        // 递归排列子元素
        child->Arrange({ child_x, current_y, child_w, child_h });
        current_y += child_h + m_padding;
    }
}

// =================================================================================
// --- HBox Implementation ---
// =================================================================================

ImVec2 HBox::Measure(ImVec2 available_size)
{
    // 逻辑与VBox类似，但主轴是水平方向
    if (!m_visible) return { 0,0 };

    float total_w = 0.0f;
    float max_h = 0.0f;
    int visible_children = 0;

    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;
        visible_children++;
        ImVec2 child_size = child->Measure(available_size);
        total_w += child_size.x;
        max_h = std::max(max_h, child_size.y);
    }

    if (visible_children > 1) total_w += (visible_children - 1) * m_padding;

    float my_w = m_width.IsFixed() ? m_width.value : total_w;
    float my_h = m_height.IsFixed() ? m_height.value : max_h;

    return m_desired_size = { my_w, my_h };
}

void HBox::Arrange(const Rect& final_rect)
{
    // 逻辑与VBox类似，但主轴是水平方向
    UIElement::Arrange(final_rect);
    if (!m_visible) return;

    // 1. 统计
    float fixed_w_used = 0.0f;
    float total_flex = 0.0f;
    int visible_children = 0;
    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;
        visible_children++;
        if (child->m_width.IsStretch()) total_flex += child->m_width.value;
        else fixed_w_used += child->m_desired_size.x;
    }
    float total_padding = (visible_children > 1) ? ((float)visible_children - 1.0f) * m_padding : 0.0f;
    float remaining_w = std::max(0.0f, m_rect.w - fixed_w_used - total_padding);

    // 2. 布局
    float current_x = 0.0f;
    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;
        // Width
        float child_w = 0.0f;
        if (child->m_width.IsStretch())
        {
            if (total_flex > 0.0f) child_w = (child->m_width.value / total_flex) * remaining_w;
        }
        else
        {
            child_w = child->m_desired_size.x;
        }
        // Height & Y Pos
        float child_h = child->m_height.IsStretch() ? m_rect.h : child->m_desired_size.y;
        float child_y = 0.0f;
        if (child_h < m_rect.h)
        {
            if (child->m_align_v == Alignment::Center) child_y = (m_rect.h - child_h) * 0.5f;
            else if (child->m_align_v == Alignment::End) child_y = m_rect.h - child_h;
        }

        child->Arrange({ current_x, child_y, child_w, child_h });
        current_x += child_w + m_padding;
    }
}


// =================================================================================
// --- ScrollView Implementation ---
// =================================================================================

void ScrollView::Update(float dt)
{
    UpdateSelf(dt);
    if (!m_visible) return;

    // 处理滚动动画
    float max_scroll = std::max(0.0f, m_content_height - m_rect.h);
    if (m_hovered)
    {
        float wheel = ImGui::GetIO().MouseWheel;
        if (std::abs(wheel) > 0.0f) m_target_scroll_y -= wheel * m_scroll_speed;
    }
    m_target_scroll_y = std::clamp(m_target_scroll_y, 0.0f, max_scroll);

    // 平滑滚动
    float diff = m_target_scroll_y - m_scroll_y;
    if (std::abs(diff) < 0.5f) m_scroll_y = m_target_scroll_y;
    else m_scroll_y += diff * std::min(1.0f, m_smoothing_speed * dt);

    m_scroll_y = std::clamp(m_scroll_y, 0.0f, max_scroll);
    float view_min_y = m_absolute_pos.y;
    float view_max_y = view_min_y + m_rect.h;

    for (auto& child : m_children)
    {
        // 计算子元素在屏幕上的垂直范围
        float child_min_y = child->m_absolute_pos.y;
        float child_max_y = child_min_y + child->m_rect.h;

        // AABB 相交测试：只有当子元素在可视范围内时才更新逻辑
        // child_bottom > view_top && child_top < view_bottom
        if (child_max_y >= view_min_y && child_min_y <= view_max_y)
        {
            child->Update(dt);
        }
    }
}

ImVec2 ScrollView::Measure(ImVec2 available_size)
{
    if (!m_visible) return { 0,0 };

    // ScrollView的核心：在测量子元素时，提供无限的高度，以获取真实的内容总高度
    ImVec2 infinite_h_size = { available_size.x, FLT_MAX };
    float content_h = 0.0f;
    float content_w = 0.0f;
    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;
        ImVec2 size = child->Measure(infinite_h_size);
        content_h = std::max(content_h, size.y); // 通常ScrollView只有一个子元素
        content_w = std::max(content_w, size.x);
    }
    m_content_height = content_h;

    // 自身的期望尺寸：宽度由内容决定，高度通常是拉伸或固定的
    float my_w = m_width.IsFixed() ? m_width.value : content_w;
    float my_h = m_height.IsFixed() ? m_height.value : 0.0f;

    return m_desired_size = { my_w, my_h };
}

void ScrollView::Arrange(const Rect& final_rect)
{
    UIElement::Arrange(final_rect);
    if (!m_visible) return;

    // 排列子元素时，关键在于施加一个负的滚动偏移
    float view_w = m_rect.w - (m_show_scrollbar ? 6.0f : 0.0f);
    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;
        float child_w = child->m_width.IsFixed() ? child->m_desired_size.x : view_w;
        float child_h = child->m_desired_size.y;
        // 关键：Y坐标为负的滚动值
        child->Arrange({ 0, -m_scroll_y, child_w, child_h });
    }
}

void ScrollView::Draw(ImDrawList* draw_list)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    // 开启裁剪矩形，只绘制可视区域内的内容
    ImVec2 clip_min = m_absolute_pos;
    ImVec2 clip_max = ImVec2(clip_min.x + m_rect.w, clip_min.y + m_rect.h);
    draw_list->PushClipRect(clip_min, clip_max, true);

    // 绘制子元素 (带剔除)
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

    // 如果事件已被处理或鼠标不在区域内，则仅更新子元素状态
    if (handled || !inside)
    {
        m_hovered = false;
        bool dummy_handled = true; // 确保子元素不响应点击，但能更新状态
        for (size_t i = m_children.size(); i > 0; --i)
        {
            m_children[i - 1]->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, dummy_handled);
        }
        return;
    }

    m_hovered = true; // 只要在内部就允许滚动

    // 将事件传递给子元素
    for (size_t i = m_children.size(); i > 0; --i)
    {
        m_children[i - 1]->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);
    }

    if (!handled && m_block_input) handled = true;
}


// =================================================================================
// --- HorizontalScrollView Implementation ---
// =================================================================================
// (逻辑与ScrollView完全对应，只是轴向不同)

void HorizontalScrollView::Update(float dt)
{
    UpdateSelf(dt);
    if (!m_visible) return;

    float max_scroll = std::max(0.0f, m_content_width - m_rect.w);
    if (m_hovered)
    {
        float wheel_x = ImGui::GetIO().MouseWheelH;
        float wheel_y = ImGui::GetIO().MouseWheel;
        // 优先响应垂直滚轮
        float wheel = (std::abs(wheel_y) > std::abs(wheel_x)) ? wheel_y : wheel_x;
        m_target_scroll_x -= wheel * m_scroll_speed;
    }
    m_target_scroll_x = std::clamp(m_target_scroll_x, 0.0f, max_scroll);

    float diff = m_target_scroll_x - m_scroll_x;
    if (std::abs(diff) < 0.5f) m_scroll_x = m_target_scroll_x;
    else m_scroll_x += diff * std::min(1.0f, m_smoothing_speed * dt);

    m_scroll_x = std::clamp(m_scroll_x, 0.0f, max_scroll);
    float view_min_x = m_absolute_pos.x;
    float view_max_x = view_min_x + m_rect.w;

    for (auto& child : m_children)
    {
        // 计算子元素在屏幕上的水平范围
        float child_min_x = child->m_absolute_pos.x;
        float child_max_x = child_min_x + child->m_rect.w;

        // AABB 相交测试：只有当子元素在可视范围内时才更新逻辑
        if (child_max_x >= view_min_x && child_min_x <= view_max_x)
        {
            child->Update(dt);
        }
    }
}

ImVec2 HorizontalScrollView::Measure(ImVec2 available_size)
{
    if (!m_visible) return { 0,0 };
    ImVec2 infinite_w_size = { FLT_MAX, available_size.y };
    float content_w = 0.0f, content_h = 0.0f;
    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;
        ImVec2 size = child->Measure(infinite_w_size);
        content_w = std::max(content_w, size.x);
        content_h = std::max(content_h, size.y);
    }
    m_content_width = content_w;

    float my_w = m_width.IsFixed() ? m_width.value : 0.0f;
    float my_h = m_height.IsFixed() ? m_height.value : content_h;
    return m_desired_size = { my_w, my_h };
}

void HorizontalScrollView::Arrange(const Rect& final_rect)
{
    UIElement::Arrange(final_rect);
    if (!m_visible) return;

    float view_h = m_rect.h - (m_show_scrollbar ? 6.0f : 0.0f);
    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;
        float child_w = child->m_desired_size.x;
        float child_h = child->m_height.IsFixed() ? child->m_desired_size.y : view_h;
        child->Arrange({ -m_scroll_x, 0, child_w, child_h });
    }
}

void HorizontalScrollView::Draw(ImDrawList* draw_list)
{
    if (!m_visible || m_alpha <= 0.01f) return;
    ImVec2 clip_min = m_absolute_pos;
    ImVec2 clip_max = ImVec2(clip_min.x + m_rect.w, clip_min.y + m_rect.h);
    draw_list->PushClipRect(clip_min, clip_max, true);
    for (auto& child : m_children)
    {
        float child_abs_x = child->m_absolute_pos.x;
        float child_w = child->m_rect.w;
        if (child_abs_x + child_w < clip_min.x || child_abs_x > clip_max.x) continue;
        child->Draw(draw_list);
    }
    draw_list->PopClipRect();
}

void HorizontalScrollView::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled)
{
    if (!m_visible || m_alpha <= 0.01f) return;
    Rect abs_rect = { m_absolute_pos.x, m_absolute_pos.y, m_rect.w, m_rect.h };
    bool inside = abs_rect.Contains(mouse_pos);
    if (handled || !inside)
    {
        m_hovered = false;
        bool dummy = true;
        for (size_t i = m_children.size(); i > 0; --i)
            m_children[i - 1]->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, dummy);
        return;
    }
    m_hovered = true;
    for (size_t i = m_children.size(); i > 0; --i)
        m_children[i - 1]->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);
    if (!handled && m_block_input) handled = true;
}


// =================================================================================
// --- UIRoot Implementation ---
// =================================================================================

UIRoot::UIRoot()
{
    ImGuiIO& io = ImGui::GetIO();
    m_rect = { 0, 0, io.DisplaySize.x, io.DisplaySize.y };
    m_block_input = false;
    m_visible = true;
    m_root = this;
    m_tooltip = std::make_shared<GlobalTooltip>();
}

void UIRoot::Arrange(const Rect& final_rect)
{
    // 1. 设置UIRoot自身的位置和大小 (通常是全屏)
    m_rect = final_rect;
    m_absolute_pos = { final_rect.x, final_rect.y };

    // 2. 遍历所有直接子元素，根据其锚点(Anchor)属性进行特殊布局
    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;

        const ImVec2& desired_size = child->m_desired_size;
        Rect child_rect = { 0, 0, desired_size.x, desired_size.y };

        // 如果子元素没有设置锚点，则使用默认的流式布局行为
        if (child->m_anchor == AnchorPoint::None)
        {
            child_rect.w = child->m_width.IsStretch() ? m_rect.w : desired_size.x;
            child_rect.h = child->m_height.IsStretch() ? m_rect.h : desired_size.y;
            child->Arrange(child_rect);
            continue;
        }

        // --- 锚点布局逻辑 ---
        switch (child->m_anchor)
        {
        case AnchorPoint::TopLeft:      child_rect.x = child->m_margin.x; child_rect.y = child->m_margin.y; break;
        case AnchorPoint::TopCenter:    child_rect.x = (m_rect.w - desired_size.x) * 0.5f + child->m_margin.x; child_rect.y = child->m_margin.y; break;
        case AnchorPoint::TopRight:     child_rect.x = m_rect.w - desired_size.x - child->m_margin.x; child_rect.y = child->m_margin.y; break;
        case AnchorPoint::MiddleLeft:   child_rect.x = child->m_margin.x; child_rect.y = (m_rect.h - desired_size.y) * 0.5f + child->m_margin.y; break;
        case AnchorPoint::Center:       child_rect.x = (m_rect.w - desired_size.x) * 0.5f + child->m_margin.x; child_rect.y = (m_rect.h - desired_size.y) * 0.5f + child->m_margin.y; break;
        case AnchorPoint::MiddleRight:  child_rect.x = m_rect.w - desired_size.x - child->m_margin.x; child_rect.y = (m_rect.h - desired_size.y) * 0.5f + child->m_margin.y; break;
        case AnchorPoint::BottomLeft:   child_rect.x = child->m_margin.x; child_rect.y = m_rect.h - desired_size.y - child->m_margin.y; break;
        case AnchorPoint::BottomCenter: child_rect.x = (m_rect.w - desired_size.x) * 0.5f + child->m_margin.x; child_rect.y = m_rect.h - desired_size.y - child->m_margin.y; break;
        case AnchorPoint::BottomRight:  child_rect.x = m_rect.w - desired_size.x - child->m_margin.x; child_rect.y = m_rect.h - desired_size.y - child->m_margin.y; break;
        default: break;
        }

        // 3. 将计算好的矩形传递给子元素，让它继续排列自己的子孙
        child->Arrange(child_rect);
    }
}

void UIRoot::Update(float dt)
{
    ImGuiIO& io = ImGui::GetIO();
    UIContext& ctx = UIContext::Get();

    // --- [帧开始] 准备工作 ---
    ctx.NewFrame();
    ctx.m_display_size = io.DisplaySize;
    m_rect = { 0, 0, io.DisplaySize.x, io.DisplaySize.y };

    // --- [PASS 1] 事件处理 ---
    ImVec2 mouse_pos = io.MousePos;
    bool mouse_down = io.MouseDown[0];
    bool mouse_clicked = io.MouseClicked[0];
    bool mouse_released = io.MouseReleased[0];

    // 如果输入被外部逻辑（如摄像机）阻塞，则模拟鼠标在屏幕外且未按下
    if (ctx.m_input_blocked)
    {
        mouse_pos = ImVec2(-FLT_MAX, -FLT_MAX);
        mouse_down = mouse_clicked = mouse_released = false;
        if (ctx.m_captured_element) ctx.ReleaseCapture();
    }

    bool is_handled = false;
    if (ctx.m_captured_element)
        ctx.m_captured_element->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, is_handled);
    else
        HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, is_handled);

    if (is_handled) io.WantCaptureMouse = true;

    // 键盘事件
    if (ctx.m_focused_element && ctx.m_focused_element->HandleKeyboardEvent())
    {
        io.WantCaptureKeyboard = true;
    }

    // --- [PASS 2] 状态更新 (动画等) ---
    UIElement::Update(dt);

    // --- [PASS 3] 测量 (自下而上) ---
    UIElement::Measure(io.DisplaySize);

    // --- [PASS 4] 排列 (自上而下) ---
    Arrange({ 0, 0, io.DisplaySize.x, io.DisplaySize.y });

    // --- [帧结束] 后处理 ---
    ctx.UpdateTooltipLogic(dt);
    m_tooltip->Update(dt);
}

void UIRoot::Draw()
{
    Draw(ImGui::GetForegroundDrawList());
}

void UIRoot::Draw(ImDrawList* draw_list)
{
    UIElement::Draw(draw_list);
    if (m_tooltip) m_tooltip->Draw(draw_list); // 最后绘制Tooltip，确保在最上层
}

void UIRoot::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled)
{
    // 如果在UI元素之外点击，则清除焦点
    UIContext& ctx = UIContext::Get();
    UIElement* focused_before = ctx.m_focused_element;

    UIElement::HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);

    if (mouse_clicked && !handled && focused_before)
    {
        ctx.ClearFocus();
    }
}

// --- ID Map Management ---
void UIRoot::MarkIDMapDirty() { m_id_map_dirty = true; }

void UIRoot::BuildIDMapRecursive(UIElement* element)
{
    // 只有具名且ID非空的元素才会被添加到映射表中
    if (!element->GetName().empty() && !element->GetID().empty())
    {
        const std::string& id = element->GetID();
        if (m_id_map.count(id))
        {
            NpgsCoreWarn("Duplicate UI ID detected: {}", id);
        }
        m_id_map[id] = element;
    }

    for (auto& child : element->m_children)
    {
        BuildIDMapRecursive(child.get());
    }
}

void UIRoot::RebuildIDMap()
{
    m_id_map.clear();
    BuildIDMapRecursive(this); // 从根节点开始递归构建
    m_id_map_dirty = false;

#ifdef _DEBUG
    if (true) // 调试开关：打印重建后的ID映射表
    {
        std::vector<std::string> sorted_keys;
        for (const auto& kv : m_id_map) sorted_keys.push_back(kv.first);
        std::sort(sorted_keys.begin(), sorted_keys.end());

        NpgsCoreInfo("========= UI ID MAP REBUILT (Total: {}) =========", m_id_map.size());
        for (const auto& key : sorted_keys)
        {
            UIElement* el = m_id_map[key];
            std::string type_name = typeid(*el).name();
            size_t class_prefix = type_name.find("class ");
            if (class_prefix != std::string::npos) type_name = type_name.substr(class_prefix + 6);

            NpgsCoreInfo("  ID: {:<40} | Type: {:<20} | Ptr: {}", key, type_name, (void*)el);
        }
        NpgsCoreInfo("=================================================");
    }
#endif
}

UIElement* UIRoot::FindElementByID(const std::string& id)
{
    // 如果映射表是脏的（例如UI结构发生了变化），先重建它
    if (m_id_map_dirty)
    {
        RebuildIDMap();
    }

    auto it = m_id_map.find(id);
    return (it != m_id_map.end()) ? it->second : nullptr;
}

_UI_END
_SYSTEM_END
_NPGS_END