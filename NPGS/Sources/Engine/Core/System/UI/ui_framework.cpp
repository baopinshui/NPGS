#include "ui_framework.h"
#include "TechUtils.h"
#include "components/GlobalTooltip.h"
//#define _DEBUG

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

// --- Rect ---
bool Rect::Contains(const ImVec2& p) const
{
    bool inside = p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;

    // DEBUG 绘制
#ifdef _DEBUG
    if (true)
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
    if (id == ThemeColorID::Custom) base_color = custom_value;
    else if (id != ThemeColorID::None) base_color = UIContext::Get().GetThemeColor(id);
    else return ImVec4(0, 0, 0, 0);

    if (alpha_override >= 0.0f) base_color.w = alpha_override;
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
    if (!m_tooltip_candidate_key.empty()) m_tooltip_timer += dt;
    if (m_tooltip_timer > 0.2f) m_tooltip_active_key = m_tooltip_candidate_key;
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

// [NEW] ID Management Implementation
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
        // 如果父节点存在，ID路径基于父节点
        if (m_parent)
        {
            const std::string& parent_id = m_parent->GetID();

            // 规则：如果自身 name 为空，则为“透明”容器，ID直接继承父ID
            if (m_name.empty())
            {
                m_cached_id = parent_id;
            }
            else
            {
                // 如果父ID为空（例如父节点是根或也是透明容器），则不加点
                if (parent_id.empty())
                {
                    m_cached_id = m_name;
                }
                else
                {
                    m_cached_id = parent_id + "." + m_name;
                }
            }
        }
        else // 如果没有父节点，ID就是自己的名字
        {
            m_cached_id = m_name;
        }

        m_id_dirty = false; // 计算完毕，清除脏标记
    }
    return m_cached_id;
}

void UIElement::InvalidateIDCache()
{
    // 标记自己为脏
    m_id_dirty = true;
    if (m_root)
    {
        m_root->MarkIDMapDirty();
    }
    // 递归标记所有子节点为脏，因为它们的父路径已经改变
    for (auto& child : m_children)
    {
        child->InvalidateIDCache();
    }
}


// [MODIFIED] AddChild
void UIElement::AddChild(Ptr child)
{
    child->m_parent = this;
    child->m_root = this->m_root;
    m_children.push_back(child);

    // [NEW] 子元素的父节点变了，其ID路径需要重新计算
    child->InvalidateIDCache();
}

// [MODIFIED] RemoveChild
void UIElement::RemoveChild(Ptr child)
{
    if (!child) return;
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end())
    {
        (*it)->InvalidateIDCache();

        (*it)->m_parent = nullptr;
        (*it)->m_root = nullptr; // [新增] 解除与根节点的关联

        m_children.erase(it);
    }
}
void UIElement::ResetInteraction()
{
    m_clicked = false;
    m_hovered = false;
    for (auto& child : m_children) child->ResetInteraction();
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
        ImDrawList* bg_list = ImGui::GetBackgroundDrawList();
        ImVec2 clip_min = draw_list->GetClipRectMin();
        ImVec2 clip_max = draw_list->GetClipRectMax();
        bg_list->PushClipRect(clip_min, clip_max, true);
        bg_list->AddImage(blur_tex, p_min, p_max, uv_min, uv_max);
        bg_list->PopClipRect();
        draw_list->AddRectFilled(p_min, p_max, GetColorWithAlpha(BackCol, 1.0f));
    }
}

void UIElement::UpdateSelf(float dt)
{
    if (!m_visible) return;

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

void UIElement::Update(float dt)
{
    UpdateSelf(dt);
    if (!m_visible) return;
    for (auto& child : m_children)
    {
        child->Update(dt);
    }
}

// [NEW] 默认测量实现
ImVec2 UIElement::Measure(ImVec2 available_size)
{
    if (!m_visible) return { 0, 0 };

    // 1. 先递归测量所有子元素，获取它们期望的大小
    //    对于普通容器，我们通常给子元素完整的可用空间
    float max_child_w = 0.0f;
    float max_child_h = 0.0f;

    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;

        // 递归调用关键点：让子树更新 m_desired_size
        ImVec2 child_size = child->Measure(available_size);

        if (child_size.x > max_child_w) max_child_w = child_size.x;
        if (child_size.y > max_child_h) max_child_h = child_size.y;
    }

    float target_w = 0.0f;
    float target_h = 0.0f;

    // 2. 宽度处理
    if (m_width.IsFixed())
    {
        target_w = m_width.value;
    }
    else if (m_width.IsStretch())
    {
        // Stretch 在测量阶段通常不占空间（由父级在 Arrange 时拉伸），
        // 但如果有最小宽度需求可以在这里设置
        target_w = 0.0f;
    }
    else // Content
    {
        // 现在 Content 可以正确包裹子元素了
        target_w = max_child_w;
    }

    // 3. 高度处理
    if (m_height.IsFixed())
    {
        target_h = m_height.value;
    }
    else if (m_height.IsStretch())
    {
        target_h = 0.0f;
    }
    else // Content
    {
        target_h = max_child_h;
    }

    m_desired_size = { target_w, target_h };
    return m_desired_size;
}

// [NEW] 默认排列实现
void UIElement::Arrange(const Rect& final_rect)
{
    // 记录相对位置
    m_rect = final_rect;
    // 计算绝对位置
    if (m_parent)
    {
        m_absolute_pos.x = m_parent->m_absolute_pos.x + m_rect.x;
        m_absolute_pos.y = m_parent->m_absolute_pos.y + m_rect.y;
    }
    else
    {
        // 根节点
        m_absolute_pos.x = m_rect.x;
        m_absolute_pos.y = m_rect.y;
    }

    if (!m_visible) return;

    // 默认容器行为：如果子元素是 Stretch，则填满当前容器；否则按对齐放置
    // 注意：VBox/HBox 会重写这个，这里主要用于普通 Panel
    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;

        float child_w = 0.0f;
        float child_h = 0.0f;

        // --- 宽度分配 ---
        if (child->m_width.IsStretch()) child_w = m_rect.w; // 占满整个宽
        else child_w = child->m_desired_size.x;

        // --- 高度分配 ---
        if (child->m_height.IsStretch()) child_h = m_rect.h; // 占满整个高
        else child_h = child->m_desired_size.y;

        Rect child_rect = { 0, 0, child_w, child_h };

        // --- 对齐处理 (当子元素小于容器时) ---
        // X轴对齐
        if (child_w < m_rect.w)
        {
            if (child->m_align_h == Alignment::Center) child_rect.x = (m_rect.w - child_w) * 0.5f;
            else if (child->m_align_h == Alignment::End) child_rect.x = m_rect.w - child_w;
            else child_rect.x = 0.0f;
        }

        // Y轴对齐
        if (child_h < m_rect.h)
        {
            if (child->m_align_v == Alignment::Center) child_rect.y = (m_rect.h - child_h) * 0.5f;
            else if (child->m_align_v == Alignment::End) child_rect.y = m_rect.h - child_h;
            else child_rect.y = 0.0f;
        }

        child->Arrange(child_rect);
    }
}

void UIElement::Draw(ImDrawList* draw_list)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    bool font_pushed = false;
    ImFont* font = GetFont();
    if (font)
    {
        ImGui::PushFont(font);
        font_pushed = true;
    }

    for (auto& child : m_children)
    {
        child->Draw(draw_list);
    }

    if (font_pushed) ImGui::PopFont();
}

void UIElement::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& external_handled)
{
    if (!m_visible || m_alpha <= 0.01f)
    {
        m_hovered = false;
        m_clicked = false;
        return;
    }

    // 1. 递归子节点
    for (size_t i = m_children.size(); i > 0; --i)
    {
        m_children[i - 1]->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, external_handled);
    }

    if (external_handled)
    {
        m_hovered = false;
        m_clicked = false;
        return;
    }

    Rect abs_rect = { m_absolute_pos.x, m_absolute_pos.y, m_rect.w, m_rect.h };
    bool inside = abs_rect.Contains(mouse_pos);

    UIContext& ctx = UIContext::Get();
    if (ctx.m_captured_element == this) inside = true;
    if (mouse_released) m_clicked = false;

    if (inside)
    {
        if (!m_tooltip_key.empty()) ctx.RequestTooltip(m_tooltip_key);
        if (m_block_input)
        {
            m_hovered = true;
            external_handled = true;
            if (mouse_clicked)
            {
                m_clicked = true;
                if (m_focusable) ctx.SetFocus(this);
            }
        }
        else
        {
            m_hovered = true;
        }
    }
    else
    {
        m_hovered = false;
        if (mouse_released) m_clicked = false;
    }
}

bool UIElement::HandleKeyboardEvent() { return false; }

ImFont* UIElement::GetFont() const
{
    if (m_font) return m_font;
    return UIContext::Get().m_font_regular ? UIContext::Get().m_font_regular : ImGui::GetFont();
}

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

    ImVec2 p_min = TechUtils::Snap(m_absolute_pos);
    ImVec2 p_max = TechUtils::Snap(ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h));

    ImTextureID blur_tex = UIContext::Get().m_scene_blur_texture;
    ImVec4 bg = m_bg_color.Resolve();
    if (m_use_glass_effect && blur_tex != 0)
    {
        DrawGlassBackground(draw_list, p_min, p_max, bg);
    }
    else
    {
        draw_list->AddRectFilled(m_absolute_pos, p_max, GetColorWithAlpha(bg, 1.0f));
    }
    UIElement::Draw(draw_list);
}

// --- Image 实现 ---
// in ui_framework.cpp

ImVec2 Image::Measure(ImVec2 available_size)
{
    if (!m_visible)
    {
        m_desired_size = { 0, 0 };
        return m_desired_size;
    }

    // 如果宽高比无效，则退化为普通UIElement的行为
    if (m_aspect_ratio <= 0.001f)
    {
        return UIElement::Measure(available_size);
    }

    // 情况1: 宽度和高度都已固定。
    if (m_width.IsFixed() && m_height.IsFixed())
    {
        m_desired_size = { m_width.value, m_height.value };
    }
    // 情况2: 宽度固定，高度可变。
    else if (m_width.IsFixed())
    {
        m_desired_size = { m_width.value, m_width.value / m_aspect_ratio };
    }
    // 情况3: 高度固定，宽度可变。
    else if (m_height.IsFixed())
    {
        m_desired_size = { m_height.value * m_aspect_ratio, m_height.value };
    }
    // 情况4: 宽度和高度都未固定 (例如 Stretch, Content)。
    // 这是最需要智能处理的情况。
    else
    {
        // [核心修复]
        // 场景A (VBox中的拉伸图片): 宽度是 Stretch，但父容器在 Measure 时提供了有限的宽度。
        // 我们可以根据这个宽度，计算出期望的高度。
        if (m_width.IsStretch() && available_size.x < FLT_MAX)
        {
            float w = available_size.x;
            float h = w / m_aspect_ratio;
            m_desired_size = { w, h };
        }
        // 场景B (HBox中的拉伸图片): 高度是 Stretch，但父容器提供了有限的高度。
        else if (m_height.IsStretch() && available_size.y < FLT_MAX)
        {
            float h = available_size.y;
            float w = h * m_aspect_ratio;
            m_desired_size = { w, h };
        }
        // 场景C (在无限空间中): 如果父容器未提供任何尺寸限制（例如在一个ScrollView里），
        // 且图片本身尺寸也不固定，那在Measure阶段确实无法确定大小。
        else
        {
            m_desired_size = { 0, 0 };
        }
    }

    return m_desired_size;
}
// in ui_framework.cpp

// [新增] Image 需要自己的 Arrange 来保持宽高比
void Image::Arrange(const Rect& final_rect)
{
    // 如果宽高比无效，则退化为普通UIElement的行为
    if (m_aspect_ratio <= 0.001f)
    {
        UIElement::Arrange(final_rect);
        return;
    }

    // 1. 计算在父级分配的 final_rect 空间内，保持宽高比所能达到的最大尺寸
    float target_w, target_h;

    // 检查 final_rect 是否有效
    if (final_rect.w <= 0 || final_rect.h <= 0)
    {
        target_w = 0;
        target_h = 0;
    }
    else
    {
        float container_aspect = final_rect.w / final_rect.h;
        if (container_aspect > m_aspect_ratio)
        {
            // 容器更“宽”，高度是限制
            target_h = final_rect.h;
            target_w = target_h * m_aspect_ratio;
        }
        else
        {
            // 容器更“高”，宽度是限制
            target_w = final_rect.w;
            target_h = target_w / m_aspect_ratio;
        }
    }


    // 2. 根据对齐方式，计算该尺寸的图像在 final_rect 中的偏移量
    float offset_x = 0.0f;
    if (m_align_h == Alignment::Center) offset_x = (final_rect.w - target_w) * 0.5f;
    else if (m_align_h == Alignment::End) offset_x = final_rect.w - target_w;

    float offset_y = 0.0f;
    if (m_align_v == Alignment::Center) offset_y = (final_rect.h - target_h) * 0.5f;
    else if (m_align_v == Alignment::End) offset_y = final_rect.h - target_h;

    // 3. 设置最终的相对矩形 (m_rect)
    // 注意：这里的坐标是相对于父元素的
    m_rect.x = final_rect.x + offset_x;
    m_rect.y = final_rect.y + offset_y;
    m_rect.w = target_w;
    m_rect.h = target_h;

    // 4. 计算并缓存最终的绝对屏幕坐标
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

    // Image 控件没有子元素，所以不需要递归调用 Arrange
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

// --- VBox 实现 ---

ImVec2 VBox::Measure(ImVec2 available_size)
{
    if (!m_visible) return { 0,0 };

    float total_h = 0.0f;
    float max_w = 0.0f;
    int visible_children = 0;

    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;
        visible_children++;

        // 递归测量子元素
        ImVec2 child_size = child->Measure(available_size); // VBox 中宽可以继承 available，高则是无限的

        // 垂直方向累加 (Stretch 元素在这里贡献 0)
        total_h += child_size.y;

        // 水平方向取最大值 (包括 Stretch 元素若能算出大小)
        if (child_size.x > max_w) max_w = child_size.x;
    }

    if (visible_children > 1) total_h += (visible_children - 1) * m_padding;

    // 自身的最终期望大小
    // 如果自身是 Fixed，则忽略计算结果；如果是 Content，则用计算结果
    // 如果是 Stretch，在 Measure 阶段返回计算出的最小内容大小 (用于 ScrollView 等计算内容)
    float my_w = m_width.IsFixed() ? m_width.value : max_w;
    float my_h = m_height.IsFixed() ? m_height.value : total_h;

    m_desired_size = { my_w, my_h };
    return m_desired_size;
}

void VBox::Arrange(const Rect& final_rect)
{
    // 设置自身位置
    UIElement::Arrange(final_rect);

    if (!m_visible) return;

    // 1. 统计 Flex 信息
    float fixed_h_used = 0.0f;
    float total_flex = 0.0f;
    int visible_children = 0;

    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;
        visible_children++;

        if (child->m_height.IsStretch())
        {
            total_flex += child->m_height.value;
        }
        else
        {
            fixed_h_used += child->m_desired_size.y;
        }
    }

    float total_padding = (visible_children > 1) ? ((float)visible_children - 1.0f) * m_padding : 0.0f;
    float remaining_h = std::max(0.0f, m_rect.h - fixed_h_used - total_padding);

    // 2. 布局
    float current_y = 0.0f;

    for (size_t i = 0; i < m_children.size(); ++i)
    {
        auto& child = m_children[i];
        if (!child->m_visible) continue;

        // 计算子元素高度
        float child_h = 0.0f;
        if (child->m_height.IsStretch())
        {
            if (total_flex > 0.0f)
                child_h = (child->m_height.value / total_flex) * remaining_h;
        }
        else
        {
            child_h = child->m_desired_size.y;
        }

        // 计算子元素宽度 (和位置)
        float child_w = 0.0f;
        float child_x = 0.0f;

        if (child->m_width.IsStretch())
        {
            child_w = m_rect.w;
            child_x = 0.0f;
        }
        else
        {
            child_w = child->m_desired_size.x;
            // 处理交叉轴对齐
            if (child->m_align_h == Alignment::Center) child_x = (m_rect.w - child_w) * 0.5f;
            else if (child->m_align_h == Alignment::End) child_x = m_rect.w - child_w;
            else child_x = 0.0f;
        }

        Rect child_rect = { child_x, current_y, child_w, child_h };
        child->Arrange(child_rect);

        current_y += child_h + m_padding;
    }
}

// --- HBox 实现 ---

ImVec2 HBox::Measure(ImVec2 available_size)
{
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
        if (child_size.y > max_h) max_h = child_size.y;
    }

    if (visible_children > 1) total_w += (visible_children - 1) * m_padding;

    float my_w = m_width.IsFixed() ? m_width.value : total_w;
    float my_h = m_height.IsFixed() ? m_height.value : max_h;

    m_desired_size = { my_w, my_h };
    return m_desired_size;
}

void HBox::Arrange(const Rect& final_rect)
{
    UIElement::Arrange(final_rect); // Setup m_rect, m_abs_pos

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

    for (size_t i = 0; i < m_children.size(); ++i)
    {
        auto& child = m_children[i];
        if (!child->m_visible) continue;

        // Width
        float child_w = 0.0f;
        if (child->m_width.IsStretch())
        {
            if (total_flex > 0.0f)
                child_w = (child->m_width.value / total_flex) * remaining_w;
        }
        else
        {
            child_w = child->m_desired_size.x;
        }

        // Height & Y Pos
        float child_h = 0.0f;
        float child_y = 0.0f;

        if (child->m_height.IsStretch())
        {
            child_h = m_rect.h;
            child_y = 0.0f;
        }
        else
        {
            child_h = child->m_desired_size.y;
            // Cross axis align
            if (child->m_align_v == Alignment::Center) child_y = (m_rect.h - child_h) * 0.5f;
            else if (child->m_align_v == Alignment::End) child_y = m_rect.h - child_h;
            else child_y = 0.0f;
        }

        Rect child_rect = { current_x, child_y, child_w, child_h };
        child->Arrange(child_rect);

        current_x += child_w + m_padding;
    }
}

// --- ScrollView 实现 ---
void ScrollView::Update(float dt)
{
    // ScrollView 的 Update 只处理滚动动画
    UIElement::Update(dt);
    if (!m_visible) return;

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

    // Clamp again just in case content shrank
    if (m_scroll_y > max_scroll) m_scroll_y = max_scroll;
}

ImVec2 ScrollView::Measure(ImVec2 available_size)
{
    // ScrollView 在 Measure 阶段需要测量子元素在无限高度下的尺寸
    // 从而得知 ContentHeight
    if (!m_visible) return { 0,0 };

    float content_h = 0.0f;
    float content_w = 0.0f;

    // 给子元素一个无限高、宽度受限的空间进行测量
    ImVec2 infinite_h_size = { available_size.x, FLT_MAX };

    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;
        ImVec2 size = child->Measure(infinite_h_size);
        content_h = std::max(content_h, size.y); // 这里简化为取最大高，通常 ScrollView 只有一个 VBox 子节点
        content_w = std::max(content_w, size.x);
    }

    m_content_height = content_h;

    // 自身的 Desired Size
    float my_w = m_width.IsFixed() ? m_width.value : content_w;
    float my_h = m_height.IsFixed() ? m_height.value : 0.0f; // ScrollView 高度通常由父级决定 (Stretch) 或 Fixed

    m_desired_size = { my_w, my_h };
    return m_desired_size;
}

void ScrollView::Arrange(const Rect& final_rect)
{
    UIElement::Arrange(final_rect);
    if (!m_visible) return;

    float view_w = m_rect.w - (m_show_scrollbar ? 6.0f : 0.0f);

    // 排列子元素时，赋予它们整个内容高度，但 Y 坐标偏移 -scroll_y
    // 注意：这里的 Arrange 传递的是相对于 ScrollView 左上角的坐标

    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;

        // 子元素宽度通常撑满 ViewWidth
        float child_w = child->m_width.IsFixed() ? child->m_desired_size.x : view_w;
        // 子元素高度使用它 Measure 出来的真实高度
        float child_h = child->m_desired_size.y;

        // 关键：位置是负的滚动值
        Rect child_rect = { 0, -m_scroll_y, child_w, child_h };
        child->Arrange(child_rect);
    }
}

void ScrollView::Draw(ImDrawList* draw_list)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    ImVec2 clip_min = m_absolute_pos;
    ImVec2 clip_max = ImVec2(clip_min.x + m_rect.w, clip_min.y + m_rect.h);

    draw_list->PushClipRect(clip_min, clip_max, true);

    for (auto& child : m_children)
    {
        // 简单的剔除优化
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

    if (handled || !inside)
    {
        m_hovered = false;
        bool dummy_handled = true;
        for (size_t i = m_children.size(); i > 0; --i)
        {
            m_children[i - 1]->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, dummy_handled);
        }
        return;
    }

    m_hovered = true; // 允许滚动

    for (size_t i = m_children.size(); i > 0; --i)
    {
        m_children[i - 1]->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);
    }

    if (!handled && m_block_input) handled = true;
}

// --- HorizontalScrollView 实现 (同理) ---
void HorizontalScrollView::Update(float dt)
{
    UIElement::Update(dt);
    if (!m_visible) return;

    float max_scroll = std::max(0.0f, m_content_width - m_rect.w);
    if (m_hovered)
    {
        float wheel_x = ImGui::GetIO().MouseWheelH;
        float wheel_y = ImGui::GetIO().MouseWheel;
        if (std::abs(wheel_y) > std::abs(wheel_x)) m_target_scroll_x -= wheel_y * m_scroll_speed;
        else m_target_scroll_x -= wheel_x * m_scroll_speed;
    }
    m_target_scroll_x = std::clamp(m_target_scroll_x, 0.0f, max_scroll);
    float diff = m_target_scroll_x - m_scroll_x;
    if (std::abs(diff) < 0.5f) m_scroll_x = m_target_scroll_x;
    else m_scroll_x += diff * std::min(1.0f, m_smoothing_speed * dt);

    if (m_scroll_x > max_scroll) m_scroll_x = max_scroll;
}

ImVec2 HorizontalScrollView::Measure(ImVec2 available_size)
{
    if (!m_visible) return { 0,0 };

    float content_w = 0.0f;
    float content_h = 0.0f;
    ImVec2 infinite_w_size = { FLT_MAX, available_size.y };

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
    m_desired_size = { my_w, my_h };
    return m_desired_size;
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
        Rect child_rect = { -m_scroll_x, 0, child_w, child_h };
        child->Arrange(child_rect);
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
        {
            m_children[i - 1]->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, dummy);
        }
        return;
    }
    m_hovered = true;
    for (size_t i = m_children.size(); i > 0; --i)
    {
        m_children[i - 1]->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);
    }
    if (!handled && m_block_input) handled = true;
}

// --- UIRoot 实现 ---
UIRoot::UIRoot()
{
    ImGuiIO& io = ImGui::GetIO();
    m_rect = { 0, 0, io.DisplaySize.x, io.DisplaySize.y };
    m_block_input = false;
    m_visible = true;
    m_root = this;
    m_tooltip = std::make_shared<GlobalTooltip>();
}

// [MODIFIED] UIRoot Update 驱动整个布局流程
void UIRoot::Arrange(const Rect& final_rect)
{
    // 1. 设置 UIRoot 自身的位置和大小 (通常是全屏)
    m_rect = final_rect;
    m_absolute_pos = { final_rect.x, final_rect.y };

    // 2. 遍历所有直接子元素，并根据其锚点属性进行布局
    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;

        // 获取子元素期望的尺寸 (必须在 Measure 阶段计算好)
        const ImVec2& desired_size = child->m_desired_size;
        Rect child_rect = { 0, 0, desired_size.x, desired_size.y };

        // 如果子元素没有设置锚点，则使用默认的拉伸行为
        if (child->m_anchor == AnchorPoint::None)
        {
            // 对于 UIRoot 的子元素，默认行为是撑满全屏
            child_rect.w = child->m_width.IsStretch() ? m_rect.w : desired_size.x;
            child_rect.h = child->m_height.IsStretch() ? m_rect.h : desired_size.y;
            child->Arrange(child_rect);
            continue; // 处理下一个子元素
        }

        // --- [NEW] 锚点布局逻辑 ---
        switch (child->m_anchor)
        {
        case AnchorPoint::TopLeft:
            child_rect.x = child->m_margin.x;
            child_rect.y = child->m_margin.y;
            break;
        case AnchorPoint::TopCenter:
            child_rect.x = (m_rect.w - desired_size.x) * 0.5f + child->m_margin.x;
            child_rect.y = child->m_margin.y;
            break;
        case AnchorPoint::TopRight:
            child_rect.x = m_rect.w - desired_size.x - child->m_margin.x;
            child_rect.y = child->m_margin.y;
            break;
        case AnchorPoint::MiddleLeft:
            child_rect.x = child->m_margin.x;
            child_rect.y = (m_rect.h - desired_size.y) * 0.5f + child->m_margin.y;
            break;
        case AnchorPoint::Center:
            child_rect.x = (m_rect.w - desired_size.x) * 0.5f + child->m_margin.x;
            child_rect.y = (m_rect.h - desired_size.y) * 0.5f + child->m_margin.y;
            break;
        case AnchorPoint::MiddleRight:
            child_rect.x = m_rect.w - desired_size.x - child->m_margin.x;
            child_rect.y = (m_rect.h - desired_size.y) * 0.5f + child->m_margin.y;
            break;
        case AnchorPoint::BottomLeft:
            child_rect.x = child->m_margin.x;
            child_rect.y = m_rect.h - desired_size.y - child->m_margin.y;
            break;
        case AnchorPoint::BottomCenter:
            child_rect.x = (m_rect.w - desired_size.x) * 0.5f + child->m_margin.x;
            child_rect.y = m_rect.h - desired_size.y - child->m_margin.y;
            break;
        case AnchorPoint::BottomRight:
            child_rect.x = m_rect.w - desired_size.x - child->m_margin.x;
            child_rect.y = m_rect.h - desired_size.y - child->m_margin.y;
            break;
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
    ctx.NewFrame();

    // 1. 事件处理前置逻辑 (设置 display size 等)
    ctx.m_display_size = io.DisplaySize;
    m_rect = { 0, 0, io.DisplaySize.x, io.DisplaySize.y };
    UIElement* focused_element_before_events = ctx.m_focused_element;
    // 2. 鼠标事件处理

    bool is_handled = false;
    UIElement* captured = ctx.m_captured_element;
    if (captured)
        captured->HandleMouseEvent(io.MousePos, io.MouseDown[0], io.MouseClicked[0], io.MouseReleased[0], is_handled);
    else
        HandleMouseEvent(io.MousePos, io.MouseDown[0], io.MouseClicked[0], io.MouseReleased[0], is_handled);

    if (is_handled) io.WantCaptureMouse = true;

    // 3. 键盘事件
    UIElement* focused = ctx.m_focused_element;
    if (focused && focused->HandleKeyboardEvent()) io.WantCaptureKeyboard = true;

    // 4. [PASS 1] 状态更新 (动画)
    // 递归调用子元素 Update (只做动画，不设位置)
    UIElement::Update(dt);

    // 5. [PASS 2] 测量 (自下而上)
    UIElement::Measure(io.DisplaySize);

    // 6. [PASS 3] 排列 (自上而下)
    Arrange({ 0, 0, io.DisplaySize.x, io.DisplaySize.y });

    // 7. Tooltip 逻辑
    ctx.UpdateTooltipLogic(dt);
    m_tooltip->Update(dt); // Tooltip 内部有自己的定位逻辑
}

void UIRoot::Draw()
{
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    Draw(draw_list);
}

void UIRoot::Draw(ImDrawList* draw_list)
{
    UIElement::Draw(draw_list);
    if (m_tooltip) m_tooltip->Draw(draw_list);
}

void UIRoot::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled)
{
    UIContext& ctx = UIContext::Get();
    UIElement* focused_before = ctx.m_focused_element;
    UIElement::HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);
    if (mouse_clicked && !handled && focused_before) ctx.ClearFocus();
}

void UIRoot::MarkIDMapDirty()
{
    m_id_map_dirty = true;
}

void UIRoot::BuildIDMapRecursive(UIElement* element)
{
    // 1. 获取当前元素的完整ID
    const std::string& id = element->GetID();

    // 2. 只有非空ID才会被添加到映射表中
    // 这自然地处理了“透明”的匿名排版容器
    if (!element->GetName().empty() && !id.empty())
    {
        // [健壮性检查] 在开发模式下，检查ID是否重复
        if (m_id_map.count(id))
        {
             NpgsCoreWarn("Duplicate UI ID detected: {}", id);
        }
        m_id_map[id] = element;
    }

    // 3. 递归遍历所有子元素
    for (auto& child : element->m_children)
    {
        BuildIDMapRecursive(child.get());
    }
}

void UIRoot::RebuildIDMap()
{
    m_id_map.clear();
    BuildIDMapRecursive(this); // 从根节点开始递归构建
    m_id_map_dirty = false;    // 重建完成，清除脏标记
#ifdef _DEBUG // 建议只在 Debug 模式下开启，避免 Release 刷屏
    if (true) // 控制开关
    {
        // 提取所有 Key 以便排序（unordered_map 本身是乱序的，不便阅读）
        std::vector<std::string> sorted_keys;
        sorted_keys.reserve(m_id_map.size());
        for (const auto& kv : m_id_map)
        {
            sorted_keys.push_back(kv.first);
        }

        // 字母排序，这样 "Menu" 和 "Menu.Button" 会挨在一起显示
        std::sort(sorted_keys.begin(), sorted_keys.end());

        NpgsCoreInfo("========= UI ID MAP REBUILT (Total: {}) =========", m_id_map.size());
        for (const auto& key : sorted_keys)
        {
            UIElement* el = m_id_map[key];

            // 获取组件的具体类型名 (例如 class NPGS::UI::Button)
            // 注意：typeid().name() 的输出格式依赖编译器
            std::string type_name = typeid(*el).name();

            // 简单的清理一下类型名显示 (可选，针对 MSVC 去掉 "class " 前缀)
            size_t class_prefix = type_name.find("class ");
            if (class_prefix != std::string::npos) type_name = type_name.substr(class_prefix + 6);

            NpgsCoreInfo("  ID: {:<40} | Type: {:<15} | Ptr: {}",
                key,
                type_name,
                (void*)el
            );
        }
        NpgsCoreInfo("=================================================");
    }
#endif
}

UIElement* UIRoot::FindElementByID(const std::string& id)
{
    // 如果映射表是脏的，先重建它
    if (m_id_map_dirty)
    {
        RebuildIDMap();
    }

    auto it = m_id_map.find(id);
    if (it != m_id_map.end())
    {
        return it->second;
    }

    return nullptr;
}
_UI_END
_SYSTEM_END
_NPGS_END