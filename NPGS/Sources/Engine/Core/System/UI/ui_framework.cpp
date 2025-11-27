#include "ui_framework.h"

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

// --- UIContext Implementation ---
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
    auto it = std::remove(m_children.begin(), m_children.end(), child);
    if (it != m_children.end())
    {
        (*it)->m_parent = nullptr;
        m_children.erase(it, m_children.end());
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
void UIElement::DrawGlassBackground(ImDrawList* draw_list, const ImVec2& p_min, const ImVec2& p_max)
{
    auto& ctx = UIContext::Get();
    ImTextureID blur_tex = ctx.m_scene_blur_texture;

    if (blur_tex != 0 && ctx.m_display_size.x > 0 && ctx.m_display_size.y > 0)
    {
        ImVec2 uv_min = ImVec2(p_min.x / ctx.m_display_size.x, p_min.y / ctx.m_display_size.y);
        ImVec2 uv_max = ImVec2(p_max.x / ctx.m_display_size.x, p_max.y / ctx.m_display_size.y);
        draw_list->AddImage(blur_tex, p_min, p_max, uv_min, uv_max);
    }
}
void UIElement::Update(float dt, const ImVec2& parent_abs_pos)
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

    // 递归子节点
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
bool UIElement::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released)
{
    if (!m_visible || m_alpha <= 0.01f)
    {
        m_hovered = false;
        m_clicked = false;
        return false;
    }

    UIContext& ctx = UIContext::Get();

    // 1. 检查是否有全局捕获
    // 如果当前有元素捕获了鼠标，且不是我自己，也不是我的子节点（简化处理：只看是不是我自己）
    // 在递归模型中，我们通常只在 Root 处做捕获分发。
    // 但为了保持兼容性，我们在 UIElement 内部判断：
    // 如果当前处于捕获模式，且捕获者不是我，且我不是捕获者的祖先(这个判断复杂)，
    // 这里采用了简化的逻辑：如果处于捕获模式，递归逻辑会在 Root 处被截断直接发给捕获者，
    // 或者我们在这里通过判断。

    // 为了实现简单，我们在 Root Controller 层面处理分发更好。
    // 但既然必须在这里实现，我们假设调用此函数时，分发逻辑已经处理好了（或者是标准的冒泡）。
    // 这里维持递归逻辑。

    // 2. 递归子节点 (反向遍历)
    // 如果处于捕获状态，只有捕获者及其子树应该响应，这里暂不深度实现子树判断。
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        if ((*it)->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released))
            return true;
    }

    // 3. 自身处理
    Rect abs_rect = { m_absolute_pos.x, m_absolute_pos.y, m_rect.w, m_rect.h };
    bool inside = abs_rect.Contains(mouse_pos);

    // 如果我是捕获者，我强制认为 inside = true (或者是逻辑上的 inside)
    // 这样即使鼠标移出屏幕，我也能收到事件
    if (ctx.m_captured_element == this)
    {
        inside = true;
    }

    // 焦点逻辑
    if (inside && mouse_clicked && m_focusable)
    {
        ctx.SetFocus(this);
    }
    // 点击空白处失去焦点逻辑通常由 Root 处理

    if (m_clicked)
    {
        if (mouse_released)
        {
            m_clicked = false;
            // 自动释放捕获 (如果是由点击触发的简单捕获)
            // 但通常 SlideBar 会手动管理 ReleaseMouse
        }
        return true;
    }

    if (inside && m_block_input)
    {
        m_hovered = true;
        if (mouse_clicked) m_clicked = true;
        return true;
    }

    if (inside && !m_block_input)
    {
        m_hovered = true;
        return false;
    }

    m_hovered = false;
    return false;
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
    ImVec2 p_min = m_absolute_pos;
    ImVec2 p_max = ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h);

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
    ImVec4 bg = m_bg_color.value_or(UIContext::Get().m_theme.color_panel_bg);

    draw_list->AddRectFilled(
        m_absolute_pos,
        ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h),
        GetColorWithAlpha(bg, 1.0f)
    );

    UIElement::Draw(draw_list);
}
// --- Image 实现 ---
//void Image::Draw(ImDrawList* draw_list)
//{
//    if (!m_visible || m_alpha <= 0.01f) return;
//
//    draw_list->AddImage(
//        m_texture_id,
//        m_absolute_pos,
//        ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h),
//        m_uv0, m_uv1,
//        GetColorWithAlpha(m_tint_col, 1.0f)
//    );
//}

// --- VBox 实现 (带 Alignment) ---
void VBox::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (!m_visible) return;

    // 基础更新
    m_absolute_pos.x = parent_abs_pos.x + m_rect.x;
    m_absolute_pos.y = parent_abs_pos.y + m_rect.y;
    UIElement::Update(dt, parent_abs_pos); // 更新 Tween 等

    // --- Pass 1: 测量 ---
    float fixed_height = 0.0f;
    int fill_count = 0;
    int visible_children = 0;

    for (const auto& child : m_children)
    {
        if (!child->m_visible) continue;
        visible_children++;
        if (child->m_fill_v)
        {
            fill_count++;
        }
        else
        {
            fixed_height += child->m_rect.h;
        }
    }

    // 计算所有间距占用的总高度
    float total_padding = (visible_children > 1) ? ((float)visible_children - 1.0f) * m_padding : 0.0f;

    // --- Pass 2: 分配与布局 ---
    float remaining_height = m_rect.h - fixed_height - total_padding;
    float fill_child_height = (fill_count > 0) ? std::max(0.0f, remaining_height / fill_count) : 0.0f;

    float current_y = 0.0f;
    for (size_t i = 0; i < m_children.size(); ++i)
    {
        auto& child = m_children[i];
        if (!child->m_visible) continue;

        // A. 确定当前子元素的高度
        if (child->m_fill_v)
        {
            child->m_rect.h = fill_child_height;
        }

        // B. 设置垂直位置
        child->m_rect.y = current_y;

        // C. 处理水平对齐 (逻辑不变)
        if (child->m_align_h == Alignment::Stretch)
        {
            child->m_rect.x = 0;
            child->m_rect.w = this->m_rect.w;
        }
        else if (child->m_align_h == Alignment::Center)
        {
            child->m_rect.x = (this->m_rect.w - child->m_rect.w) * 0.5f;
        }
        else if (child->m_align_h == Alignment::End)
        {
            child->m_rect.x = this->m_rect.w - child->m_rect.w;
        }
        else // Start
        {
            child->m_rect.x = 0;
        }

        // D. 递归更新子元素
        child->Update(dt, m_absolute_pos);

        // E. 移动Y坐标，为下一个元素做准备
        current_y += child->m_rect.h;
        if (i < (size_t)visible_children - 1) // 最后一个可见元素后面不加 padding
        {
            current_y += m_padding;
        }
    }

    // --- [核心修正] Pass 3: 更新自身高度 (如果需要) ---
    // 如果 fill_count 为 0，意味着这个VBox是内容自适应模式。
    // 它的高度应该被其内容撑开。
    if (fill_count == 0)
    {
        this->m_rect.h = current_y;
    }
    // 否则，如果 fill_count > 0，意味着这个VBox是尺寸分配模式。
    // 它的高度由它的父级决定，它不应该改变自己的高度。
}
// --- HBox 实现 (新增) ---
void HBox::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (!m_visible) return;

    // 基础更新
    m_absolute_pos.x = parent_abs_pos.x + m_rect.x;
    m_absolute_pos.y = parent_abs_pos.y + m_rect.y;
    UIElement::Update(dt, parent_abs_pos);

    // --- Pass 1: 测量 (主轴：水平) ---
    float fixed_width = 0.0f;
    int fill_count = 0;
    int visible_children = 0;

    for (const auto& child : m_children)
    {
        if (!child->m_visible) continue;
        visible_children++;
        if (child->m_fill_h)
        {
            fill_count++;
        }
        else
        {
            fixed_width += child->m_rect.w;
        }
    }

    float total_padding = (visible_children > 1) ? ((float)visible_children - 1.0f) * m_padding : 0.0f;

    // --- Pass 2: 分配与布局 ---
    float remaining_width = m_rect.w - fixed_width - total_padding;
    float fill_child_width = (fill_count > 0) ? std::max(0.0f, remaining_width / fill_count) : 0.0f;

    float current_x = 0.0f;
    float max_child_height = 0.0f; // 用于计算HBox的自适应高度

    for (size_t i = 0; i < m_children.size(); ++i)
    {
        auto& child = m_children[i];
        if (!child->m_visible) continue;

        // A. 确定宽度 (主轴尺寸)
        if (child->m_fill_h)
        {
            child->m_rect.w = fill_child_width;
        }

        // B. 设置水平位置 (主轴定位)
        child->m_rect.x = current_x;

        // C. 处理垂直对齐 (次轴定位)
        if (child->m_align_v == Alignment::Stretch)
        {
            child->m_rect.y = 0;
            child->m_rect.h = this->m_rect.h; // 子元素高度被拉伸到与HBox相同
        }
        else if (child->m_align_v == Alignment::Center)
        {
            child->m_rect.y = (this->m_rect.h - child->m_rect.h) * 0.5f;
        }
        else if (child->m_align_v == Alignment::End)
        {
            child->m_rect.y = this->m_rect.h - child->m_rect.h;
        }
        else // Start
        {
            child->m_rect.y = 0;
        }

        // D. 递归更新
        child->Update(dt, m_absolute_pos);

        // 在递归更新后，记录子元素最终的高度，用于HBox自身的自适应高度计算
        if (child->m_rect.h > max_child_height)
        {
            max_child_height = child->m_rect.h;
        }

        // E. 移动X坐标
        current_x += child->m_rect.w;
        if (i < (size_t)visible_children - 1) current_x += m_padding;
    }

    // --- Pass 3: 更新自身高度 (次轴自适应) ---
    // 如果HBox的高度没有被外部（例如父VBox或用户代码）设置一个明确的值，
    // 那么它的高度就应该由其内部最高的子元素决定。
    // 我们用一个很小的值来判断“高度是否未被设置”。
    if (m_rect.h <= 0.1f)
    {
        m_rect.h = max_child_height;
    }
}

// --- ScrollView 实现 (修复过滚闪烁问题) ---
void ScrollView::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (!m_visible) return;

    // 1. 基础位置更新
    m_absolute_pos.x = parent_abs_pos.x + m_rect.x;
    m_absolute_pos.y = parent_abs_pos.y + m_rect.y;

    // --- 计算滚动边界 ---
    // 这是为了确保目标值不会超出范围
    float max_scroll = std::max(0.0f, m_content_height - m_rect.h);

    // 2. 滚动输入处理
    if (m_hovered)
    {
        float wheel = ImGui::GetIO().MouseWheel;
        if (std::abs(wheel) > 0.0f)
        {
            // 修改目标值，而不是直接修改当前值
            m_target_scroll_y -= wheel * m_scroll_speed;
        }
    }

    // --- [核心逻辑 1] 限制目标值范围 ---
    // 无论是否有输入，都要时刻修正目标值，防止内容变少时目标值悬空
    m_target_scroll_y = std::clamp(m_target_scroll_y, 0.0f, max_scroll);

    // --- [核心逻辑 2] 平滑插值 (Smooth Damp) ---
    // 使用这种公式可以保证帧率波动时动画速度一致: current += (target - current) * (1 - exp(-speed * dt))
    // 或者使用简化的 Lerp 近似：current += (target - current) * speed * dt

    float diff = m_target_scroll_y - m_scroll_y;

    // 如果差距很小，直接吸附，避免浮点数抖动
    if (std::abs(diff) < 0.5f)
    {
        m_scroll_y = m_target_scroll_y;
    }
    else
    {
        // 这里的 dt * m_smoothing_speed 决定了跟手的速度
        m_scroll_y += diff * std::min(1.0f, m_smoothing_speed * dt);
    }

    // --- Pass 2: 布局子元素 (逻辑不变，但使用平滑后的 m_scroll_y) ---
    float current_max_bottom = 0.0f;
    float view_w = m_rect.w - (m_show_scrollbar ? 6.0f : 0.0f);

    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;

        // 传递宽度约束
        if (child->m_fill_h)
        {
            child->m_rect.w = view_w;
        }
        child->m_rect.x = 0;
        child->m_rect.w = view_w;

        // 应用平滑后的 m_scroll_y
        child->m_rect.y = -m_scroll_y;

        child->Update(dt, m_absolute_pos);

        // 计算内容高度
        if (child->m_rect.h > current_max_bottom)
        {
            current_max_bottom = child->m_rect.h;
        }
    }

    // --- Pass 3: 更新内容高度 ---
    m_content_height = current_max_bottom;

    // 再次检查 clamp (防止这一帧内容突然变少导致 scroll_y 悬空)
    // 注意：这里同时限制 target 和 current，确保逻辑稳健
    float new_max_scroll = std::max(0.0f, m_content_height - m_rect.h);
    if (m_target_scroll_y > new_max_scroll) m_target_scroll_y = new_max_scroll;
    // 如果当前位置已经超出很多（比如列表被清空），则不需要平滑回去，直接归位，体验更好
    // 如果只是超出一点点，则通过上面的插值慢慢回去
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

bool ScrollView::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released)
{
    if (!m_visible || m_alpha <= 0.01f) return false;

    Rect abs_rect = { m_absolute_pos.x, m_absolute_pos.y, m_rect.w, m_rect.h };
    bool inside = abs_rect.Contains(mouse_pos);

    m_hovered = inside && m_block_input;

    if (mouse_clicked && !inside) return false;
    if (!inside)
    {
        return false;
    }

    // 如果有捕获，优先捕获逻辑（在 Root 分发层处理更好，这里保留递归）
    bool child_consumed = false;
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        if ((*it)->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released))
        {
            child_consumed = true;
            break;
        }
    }

    if (child_consumed) return true;
    return inside;
}
// 在文件末尾，_UI_END 之前，添加 UIRoot 的实现

// --- UIRoot 实现 ---
UIRoot::UIRoot()
{
    ImGuiIO& io = ImGui::GetIO();
    m_rect = { 0, 0, io.DisplaySize.x, io.DisplaySize.y };
    m_block_input = false; // 关键：自身不阻挡输入，允许事件穿透
    m_visible = true;
}

void UIRoot::Update(float dt)
{
    // --- 1. 全局事件与状态管理 ---
    ImGuiIO& io = ImGui::GetIO();
    UIContext& ctx = UIContext::Get();

    UIElement* focused_element_before_events = ctx.m_focused_element;

    // --- 2. 鼠标事件分发 ---
    bool ui_consumed_mouse = false;
    UIElement* captured = ctx.m_captured_element;

    if (captured)
    {
        ui_consumed_mouse = captured->HandleMouseEvent(io.MousePos, io.MouseDown[0], io.MouseClicked[0], io.MouseReleased[0]);
    }
    else
    {
        // 从根节点开始分发事件
        ui_consumed_mouse = HandleMouseEvent(io.MousePos, io.MouseDown[0], io.MouseClicked[0], io.MouseReleased[0]);
    }

 

    if (ui_consumed_mouse) io.WantCaptureMouse = true;

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
}
void UIRoot::Draw() {

    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    UIElement::Draw(draw_list);
}
bool UIRoot::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released)
{
    // 1. 先保存当前焦点状态
    UIContext& ctx = UIContext::Get();
    UIElement* focused_before = ctx.m_focused_element;

    // 2. 正常分发事件给子节点
    // 如果子节点处理了（比如点到了按钮），consumed_by_child 会是 true
    bool consumed_by_child = UIElement::HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released);

    // 3. [规范的失焦逻辑]
    // 如果发生了点击，并且没有任何子节点消耗这个点击...
    if (mouse_clicked && !consumed_by_child)
    {
        // ...说明点击落在了“空地”（即游戏画面）上。
        // 此时，如果之前有元素拥有焦点，我们需要判断是否应该清除它。
        if (focused_before)
        {
            // 这里其实可以直接 ClearFocus()，因为我们已经确认点击没有被任何UI元素（包括焦点元素自己）消耗。
            // 只要 HandleMouseEvent 实现正确（点击焦点元素内部会返回 true），这个逻辑就是严密的。
            ctx.ClearFocus();
        }
    }

    // 4. 返回 false，允许事件穿透给游戏层
    return consumed_by_child; ; // UIRoot 永远不阻挡输入
}
_UI_END
_SYSTEM_END
_NPGS_END