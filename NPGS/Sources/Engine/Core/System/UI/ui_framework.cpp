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
    m_absolute_pos.x = parent_abs_pos.x + m_rect.x;
    m_absolute_pos.y = parent_abs_pos.y + m_rect.y;

    // 更新自身动画
    UIElement::Update(dt, parent_abs_pos); // 这里只跑 Tween，不递归子节点，因为我们要手动布局

    float current_y = 0.0f;
    for (size_t i = 0; i < m_children.size(); i++)
    {
        auto& child = m_children[i];
        if (!child->m_visible) continue;

        // 垂直堆叠
        child->m_rect.y = current_y;

        // 水平处理 (保持原有的对齐逻辑)
        if (child->m_align_h == Alignment::Stretch)
        {
            child->m_rect.x = 0;
            child->m_rect.w = this->m_rect.w; // 继承 VBox 的宽度
        }
        else if (child->m_align_h == Alignment::Center)
        {
            child->m_rect.x = (this->m_rect.w - child->m_rect.w) * 0.5f;
        }
        else if (child->m_align_h == Alignment::End)
        {
            child->m_rect.x = this->m_rect.w - child->m_rect.w;
        }
        else
        {
            child->m_rect.x = 0;
        }

        child->Update(dt, m_absolute_pos);

        current_y += child->m_rect.h;
        // 最后一个元素后面不加 padding
        if (i < m_children.size() - 1) current_y += m_padding;
    }

    // [新增] VBox 根据内容自动计算自身高度
    // 这对 ScrollView 非常重要
    this->m_rect.h = current_y;
}

// --- HBox 实现 (新增) ---
void HBox::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (!m_visible) return;
    m_absolute_pos.x = parent_abs_pos.x + m_rect.x;
    m_absolute_pos.y = parent_abs_pos.y + m_rect.y;

    UIElement::Update(dt, parent_abs_pos); // 跑 Tween

    float current_x = 0.0f;
    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;

        // 水平方向：堆叠
        child->m_rect.x = current_x;

        // 垂直方向：根据 Alignment 计算 Y 和 H
        if (child->m_align_v == Alignment::Stretch)
        {
            child->m_rect.y = 0;
            child->m_rect.h = this->m_rect.h;
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

        child->Update(dt, m_absolute_pos);
        current_x += child->m_rect.w + m_padding;
    }
}

// --- ScrollView 实现 ---
void ScrollView::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (!m_visible) return;

    // 1. 基础位置更新
    m_absolute_pos.x = parent_abs_pos.x + m_rect.x;
    m_absolute_pos.y = parent_abs_pos.y + m_rect.y;

    // 2. 滚动输入处理
    if (m_hovered)
    {
        float wheel = ImGui::GetIO().MouseWheel;
        if (std::abs(wheel) > 0.0f) m_scroll_y -= wheel * m_scroll_speed;
    }

    // 3. 布局子元素
    // ScrollView 逻辑变为：强制子元素的 Y 坐标偏移，并让子元素的宽等于 ScrollView 的宽
    float max_child_bottom = 0.0f;
    float view_w = m_rect.w - (m_show_scrollbar ? 6.0f : 0.0f); // 预留滚动条空间

    for (auto& child : m_children)
    {
        if (!child->m_visible) continue;

        // [关键] ScrollView 将自身的宽度约束传递给子容器 (比如 VBox)
        // 子容器负责在 update 里根据这个宽度去布局它自己的子元素
        child->m_rect.x = 0; // 默认左对齐，内容容器通常填满宽
        child->m_rect.w = view_w;

        // 应用滚动偏移
        // 注意：这里我们其实是在修改子元素的相对坐标。
        // 子元素的 Update 会基于这个 m_rect.y 计算 absolute_pos
        // VBox 的 Update 会重置它子元素的 Y，所以通常 ScrollView 内部只放 *一个* VBox 是最佳实践
        // 如果放了多个子元素，ScrollView 不会帮它们堆叠，它们会重叠在一起，除非它们自己有坐标。

        // 为了兼容性，我们假设 ScrollView 内部只有一个主要的内容容器 (VBox)
        // 或者我们依然允许 ScrollView 简单地偏移所有子元素

        // 这里有一个 tricky 的点：VBox 的 Update 会把自己的 children 的 Y 重置。
        // 所以 ScrollView 只需要控制 VBox 本身的 Y 即可。

        // 也就是： VBox.y = -m_scroll_y
        child->m_rect.y = -m_scroll_y;

        child->Update(dt, m_absolute_pos);

        // 计算内容总高度 (取子元素的最底部)
        // VBox 会在 Update 后计算好自己的 m_rect.h (如果 VBox 实现了自动高度)
        // 但目前的 VBox 实现并没有自动计算自身高度，我们需要让 VBox 计算自身高度。

        // [补丁] 让我们去修改一下 VBox，让它能算出自己的 Height
        if (child->m_rect.h > max_child_bottom) max_child_bottom = child->m_rect.h;
    }

    m_content_height = max_child_bottom;

    // 4. 限制滚动范围
    float max_scroll = std::max(0.0f, m_content_height - m_rect.h);
    m_scroll_y = std::clamp(m_scroll_y, 0.0f, max_scroll);
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