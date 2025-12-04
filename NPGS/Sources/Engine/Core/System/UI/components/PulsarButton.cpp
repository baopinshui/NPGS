#include "PulsarButton.h"
#include "../TechUtils.h"
#include "TechBorderPanel.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

static void PushFont(ImFont* font) { if (font) ImGui::PushFont(font); }
static void PopFont(ImFont* font) { if (font) ImGui::PopFont(); }

PulsarButton::PulsarButton(const std::string& id, const std::string& label, const std::string& icon_char, const std::string& stat_label, std::string* stat_value_ptr, const std::string& stat_unit, bool is_editable)
    : m_id(id), m_is_editable(is_editable)
{
    m_rect = { 0, 0, 40, 40 };
    m_block_input = true;

    auto& ctx = UIContext::Get();
    const auto& theme = UIContext::Get().m_theme;
    //  0. 创建背景面板 
    m_bg_panel = std::make_shared<TechBorderPanel>();
    m_bg_panel->m_rect = { 0, 0, 40, 40 }; // 初始大小
    m_bg_panel->m_use_glass_effect = true; // 启用毛玻璃
    m_bg_panel->m_thickness = 2.0f;        // 边框厚度
    m_bg_panel->m_block_input = false;     // 背景不拦截输入
    // 背景色设为半透明黑 (原本 Draw 里写的 0.6f)
    m_bg_panel->m_bg_color = ImVec4(0.0f, 0.0f, 0.0f, 0.6f);
    AddChild(m_bg_panel); // 最先添加，即在最底层绘制
    // --- 1. 图标 (Icon) ---
    // [修复] 必须设置 rect 为 40x40，否则 Center 对齐找不到中心
    m_text_icon = std::make_shared<TechText>(icon_char);
    m_text_icon->m_rect = { 0, 0, 40, 40 };
    m_text_icon->m_align_h = Alignment::Center;
    m_text_icon->m_align_v = Alignment::Center;
    m_text_icon->m_block_input = false; // 图标文字不阻挡父级点击
    AddChild(m_text_icon);

    // --- 2. 状态文本 (Status: SYSTEM READY) ---
    m_text_status = std::make_shared<TechText>("SYSTEM READY", theme.color_text_highlight, true);
    m_text_status->m_font = ctx.m_font_bold;
    m_text_status->m_block_input = false;
    AddChild(m_text_status);

    // --- 3. 主标签 (Label: EXECUTE) ---
    m_text_label = std::make_shared<TechText>(label, theme.color_text_highlight, true);
    m_text_label->m_font = ctx.m_font_bold;
    m_text_label->m_block_input = false;
    AddChild(m_text_label);

    // --- 4. 统计信息行 (Stats) ---
    if (!stat_label.empty())
    {
        m_text_stat_label = std::make_shared<TechText>(stat_label + ":", theme.color_text);
        m_text_stat_label->m_font = ctx.m_font_bold;
        m_text_stat_label->m_block_input = false;
        AddChild(m_text_stat_label);
    }

    if (!stat_unit.empty())
    {
        m_text_stat_unit = std::make_shared<TechText>(stat_unit, theme.color_text);
        m_text_stat_unit->m_font = ctx.m_font_bold;
        m_text_stat_unit->m_block_input = false;
        AddChild(m_text_stat_unit);
    }

    if (is_editable && stat_value_ptr)
    {
        m_input_field = std::make_shared<InputField>(stat_value_ptr);
        m_input_field->m_text_color = theme.color_accent;
        m_input_field->m_border_color = theme.color_accent;
        AddChild(m_input_field);
    }
    else if (stat_value_ptr)
    {
        m_text_stat_value = std::make_shared<TechText>(*stat_value_ptr, theme.color_accent, true);
        m_text_stat_value->m_font = ctx.m_font_large ? ctx.m_font_large : ctx.m_font_bold;
        m_text_stat_value->m_block_input = false;
        AddChild(m_text_stat_value);
    }

    // 预生成射线数据
    m_rays.resize(24);
    for (auto& ray : m_rays)
    {
        ray.theta = (float(rand()) / RAND_MAX) * 2.0f * Npgs::Math::kPi;
        ray.phi = ((float(rand()) / RAND_MAX) * Npgs::Math::kPi) - Npgs::Math::kPi / 2.0f;
        ray.len = 0.4f + (float(rand()) / RAND_MAX) * 0.6f;
        int seg_count = 5 + rand() % 5;
        for (int i = 0; i < seg_count; ++i)
        {
            RaySegment seg;
            seg.offset = float(rand()) / RAND_MAX;
            seg.has_bump = (float(rand()) / RAND_MAX) > 0.8f;
            seg.bump_height = ((float(rand()) / RAND_MAX) > 0.5f ? 1.0f : -1.0f) * (1.5f + (float(rand()) / RAND_MAX) * 1.0f);
            seg.is_gap = false;
            ray.segments.push_back(seg);
        }
        std::sort(ray.segments.begin(), ray.segments.end(), [](const auto& a, const auto& b) { return a.offset < b.offset; });
    }
}

void PulsarButton::SetStatusText(const std::string& text)
{
    if (!m_text_status) return;
    if (m_text_status->m_text == text && !m_has_pending_status) return;

    // 1. 获取字体进行测量
    ImFont* font = m_text_status->GetFont();
    if (!font) font = UIContext::Get().m_font_bold; // 兜底

    float new_text_w = 0.0f;
    if (font)
    {
        new_text_w = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text.c_str()).x;
    }

    float required_len = new_text_w + 30.0f; // 文字宽 + 缓冲

    // 2. 核心判断：当前线长是否足够？
    // 如果当前视觉长度 (m_current_line_len) 小于 新文字所需长度
    // 说明是“突然变长”，需要先缓冲
    if (m_current_line_len < required_len)
    {
        m_pending_status_text = text;
        m_has_pending_status = true;
        // 注意：这里不调用 m_text_status->SetText(text)，保持旧文字显示
    }
    else
    {
        // 如果是变短，或者当前本来就很长，直接更新
        m_text_status->SetText(text);
        m_has_pending_status = false;
        m_pending_status_text.clear();
    }
}
void PulsarButton::SetActive(bool active)
{
    if (m_is_active == active) return;
    m_is_active = active;

    if (m_is_active)
    {
        if (m_anim_state == PulsarAnimState::Closed || m_anim_state == PulsarAnimState::Closing)
        {
            m_anim_state = PulsarAnimState::Opening;
            To(&m_anim_progress, 1.0f, 0.5f, EasingType::EaseInOutQuad, [this]() { this->m_anim_state = PulsarAnimState::Open; });

            // 重启特效
            m_text_status->RestartEffect();
            m_text_label->RestartEffect();
            if (m_text_stat_value) m_text_stat_value->RestartEffect();
        }
    }
    else
    {
        if (m_anim_state == PulsarAnimState::Open || m_anim_state == PulsarAnimState::Opening)
        {
            m_anim_state = PulsarAnimState::Closing;
            To(&m_anim_progress, 0.0f, 0.4f, EasingType::EaseInOutQuad, [this]() { this->m_anim_state = PulsarAnimState::Closed; });
        }
    }
}
void PulsarButton::SetExecutable(bool can_execute) { m_can_execute = can_execute; }

void PulsarButton::Update(float dt, const ImVec2& parent_abs_pos)
{
    // 基础旋转动画
    m_rotation_angle += 2.0f * Npgs::Math::kPi * dt / 3.0f;
    if (m_rotation_angle > 2.0f * Npgs::Math::kPi) m_rotation_angle -= 2.0f * Npgs::Math::kPi;

    // --- 1. 计算折线生长进度 ---
    float line_prog = std::max(0.0f, (m_anim_progress - 0.5f) * 2.0f);

    // --- 2. 优化文字显现时机 ---
    float text_fade_start = 0.75f;
    float text_alpha = 0.0f;
    if (m_anim_state == PulsarAnimState::Open) text_alpha = 1.0f;
    else text_alpha = std::clamp((line_prog - text_fade_start) / (1.0f - text_fade_start), 0.0f, 1.0f);

    const auto& theme = UIContext::Get().m_theme;

    // --- 3. [核心修改] 动态测量与平滑过渡逻辑 ---

    float max_text_w = 0.0f;

    // A. 计算 Label 宽度
    if (m_text_label)
    {
        ImFont* font = m_text_label->GetFont();
        if (font)
        {
            float w = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, m_text_label->m_text.c_str()).x;
            if (w > max_text_w) max_text_w = w;
        }
    }

    // B. 计算 Status 宽度 (关键逻辑)
    // 如果有暂存的（更长的）文本，我们用暂存文本来计算目标长度，以便让线先伸长
    std::string status_str_for_calc;
    if (m_has_pending_status) status_str_for_calc = m_pending_status_text;
    else if (m_text_status) status_str_for_calc = m_text_status->m_text;

    if (!status_str_for_calc.empty() && m_text_status)
    {
        ImFont* font = m_text_status->GetFont();
        if (font)
        {
            float w = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, status_str_for_calc.c_str()).x;
            if (w > max_text_w) max_text_w = w;
        }
    }

    // C. 计算目标长度并触发动画
    float calculated_target_len = std::max(130.0f, max_text_w + 30.0f);

    if (std::abs(calculated_target_len - m_target_line_len) > 1.0f)
    {
        m_target_line_len = calculated_target_len;
        // 触发伸缩动画
        To(&m_current_line_len, m_target_line_len, 0.3f, EasingType::EaseOutQuad);
    }

    // D. [新增] 检查并提交暂存文本
    // 如果有待更新的文本，且当前视觉线长 (m_current_line_len) 已经接近目标长度
    // 则说明线已经伸长到位了，可以安全地替换文字了
    if (m_has_pending_status && m_text_status)
    {
        // 允许 2.0px 的误差，或者你可以判断 m_current_line_len >= calculated_target_len - 2.0f
        if (std::abs(m_current_line_len - m_target_line_len) < 5.0f || m_current_line_len >= calculated_target_len)
        {
            m_text_status->SetText(m_pending_status_text);
            m_has_pending_status = false;
            m_pending_status_text.clear();
        }
    }

    // --- 4. 动态计算布局与路径 (逻辑不变，使用 m_current_line_len) ---

    ImVec2 p_start = { 20.0f, 20.0f };
    float rel_line_y = 20.0f - 40.0f;

    ImVec2 p_elbow = { 20.0f + 60.0f, rel_line_y };
    ImVec2 p_end = { 20.0f + 64.0f + m_current_line_len, rel_line_y };

    float dist_1 = std::sqrt(std::pow(p_elbow.x - p_start.x, 2) + std::pow(p_elbow.y - p_start.y, 2));
    float dist_2 = std::sqrt(std::pow(p_end.x - p_elbow.x, 2) + std::pow(p_end.y - p_elbow.y, 2));
    float total_dist = dist_1 + dist_2;

    // --- 5. 图标位置 ---
    float current_dist_on_path = total_dist * line_prog;
    ImVec2 icon_current_center = p_start;

    if (current_dist_on_path <= dist_1)
    {
        float t = (dist_1 > 0.001f) ? (current_dist_on_path / dist_1) : 0.0f;
        icon_current_center.x = p_start.x + (p_elbow.x - p_start.x) * t;
        icon_current_center.y = p_start.y + (p_elbow.y - p_start.y) * t;
    }
    else
    {
        float t = (dist_2 > 0.001f) ? ((current_dist_on_path - dist_1) / dist_2) : 0.0f;
        icon_current_center.x = p_elbow.x + (p_end.x - p_elbow.x) * t;
        icon_current_center.y = p_elbow.y + (p_end.y - p_elbow.y) * t;
    }
    icon_current_center = TechUtils::Snap(icon_current_center);
    // --- 6. 更新背景面板 ---
    if (m_bg_panel)
    {
        float box_size = 40.0f;
        m_bg_panel->m_rect.x = icon_current_center.x - box_size * 0.5f;
        m_bg_panel->m_rect.y = icon_current_center.y - box_size * 0.5f;
        m_bg_panel->m_rect.w = box_size;
        m_bg_panel->m_rect.h = box_size;
        m_bg_panel->m_visible = true;
        m_bg_panel->m_alpha = 1.0f;
    }

    // --- 7. 更新图标 ---
    if (m_text_icon)
    {
        m_text_icon->m_rect = m_bg_panel->m_rect;
        m_text_icon->m_alpha = 1.0f;
        m_text_icon->m_visible = true;
        m_text_icon->SetColor((m_hovered || m_is_active) ? theme.color_accent : theme.color_text);
    }

    // --- 8. 下方文本位置更新 ---
    const float LAYOUT_TEXT_START_X = 64.0f;
    float rel_text_x = pulsar_center_offset.x + LAYOUT_TEXT_START_X;

    // 状态文本
    m_text_status->m_rect.x = rel_text_x;
    m_text_status->m_rect.y = rel_line_y - 18.0f;
    m_text_status->m_alpha = text_alpha;
    m_text_status->m_visible = (text_alpha > 0.01f);

    // 主标签
    m_text_label->m_rect.x = rel_text_x;
    m_text_label->m_rect.y = rel_line_y;
    m_text_label->m_alpha = text_alpha;
    m_text_label->m_visible = (text_alpha > 0.01f);
    ImVec4 label_color = m_can_execute ? (m_label_hovered ? theme.color_accent : theme.color_text_highlight) : theme.color_text_disabled;
    m_text_label->SetColor(label_color);

    // 统计信息行
    if (m_text_stat_label)
    {
        float stat_y = rel_line_y + 26.0f;
        m_text_stat_label->m_rect.x = rel_text_x;
        m_text_stat_label->m_rect.y = stat_y;
        m_text_stat_label->m_alpha = text_alpha;
        m_text_stat_label->m_visible = (text_alpha > 0.01f);

        ImFont* font = m_text_stat_label->GetFont();
        float lbl_w = 0.0f;
        if (font) lbl_w = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, m_text_stat_label->m_text.c_str()).x;

        float val_x = rel_text_x + lbl_w + 5.0f;

        if (m_input_field)
        {
            m_input_field->m_rect.x = val_x;
            m_input_field->m_rect.y = stat_y - 2.0f;
            m_input_field->m_rect.w = 100.0f;
            m_input_field->m_alpha = text_alpha;
            m_input_field->m_visible = (text_alpha > 0.01f);
            m_input_field->m_block_input = m_is_active;
        }
        if (m_text_stat_value)
        {
            m_text_stat_value->m_rect.x = val_x;
            m_text_stat_value->m_rect.y = stat_y - 4.0f;
            m_text_stat_value->m_alpha = text_alpha;
            m_text_stat_value->m_visible = (text_alpha > 0.01f);
        }
        if (m_text_stat_unit)
        {
            float val_w = 0.0f;
            if (m_is_editable) val_w = 100.0f;
            else
            {
                ImFont* lg_font = UIContext::Get().m_font_large;
                if (lg_font && m_text_stat_value) val_w = lg_font->CalcTextSizeA(lg_font->FontSize, FLT_MAX, 0.0f, m_text_stat_value->m_hacker_effect.m_display_text.c_str()).x;
            }

            m_text_stat_unit->m_rect.x = val_x + val_w + 5.0f;
            m_text_stat_unit->m_rect.y = stat_y;
            m_text_stat_unit->m_alpha = text_alpha;
            m_text_stat_unit->m_visible = (text_alpha > 0.01f);
        }
    }

    UIElement::Update(dt, parent_abs_pos);
}

void PulsarButton::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    // 1. 优先让子元素（如 InputField）处理事件
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        (*it)->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);
    }

    // 如果事件已经被子元素（比如 InputField）处理了，或者被上层元素遮挡
    if (handled)
    {
        m_hovered = false;
        m_label_hovered = false;
        return;
    }

    // --- [核心实现] 交互区域判定 ---

    // A. 计算核心区域 (Pulsar Core) 的 Rect
    //    这里的逻辑只负责左侧的圆形脉冲星
    const float closed_w = 40.0f;
    const float closed_h = 40.0f;
    const float open_w = pulsar_radius; // 60.0f
    const float open_h = pulsar_radius;

    // 插值计算当前尺寸
    float current_w = closed_w + (open_w - closed_w) * m_anim_progress;
    float current_h = closed_h + (open_h - closed_h) * m_anim_progress;

    ImVec2 center = { m_absolute_pos.x + 20.0f, m_absolute_pos.y + 20.0f };

    Rect core_hit_rect = {
        center.x - current_w * 0.5f,
        center.y - current_h * 0.5f,
        current_w,
        current_h
    };

    bool inside_core = core_hit_rect.Contains(mouse_pos);

    // B. [关键修复] 独立计算 Label (发射按钮) 的悬停状态
    //    Label 在核心区域之外，必须单独检测，且只在展开(Active)时有效
    m_label_hovered = false;
    if (m_is_active && m_anim_progress > 0.8f && m_can_execute)
    {
        // 获取 Label 的字体尺寸来生成精确的包围盒
        ImFont* font = UIContext::Get().m_font_bold;
        ImVec2 txt_sz = { 100, 20 };
        // 这里的 m_text_label->m_text 应该是 "EXECUTE" 或其他
        if (font && m_text_label)
            txt_sz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, m_text_label->m_text.c_str());

        // 使用组件当前的绝对位置 (由 Update 计算)
        if (m_text_label)
        {
            Rect label_abs_rect = {
                m_text_label->m_absolute_pos.x - 10.0f, // 稍微扩大一点点击范围
                m_text_label->m_absolute_pos.y - 5.0f,
                txt_sz.x + 20.0f,
                txt_sz.y + 10.0f
            };

            if (label_abs_rect.Contains(mouse_pos))
            {
                m_label_hovered = true;
            }
        }
    }

    // C. 综合判定：只要在 核心内 或 Label内，都算悬停/交互
    bool is_interactive_area = inside_core || m_label_hovered;

    if (is_interactive_area && m_block_input)
    {
        m_hovered = true; // 让按钮保持高亮
        handled = true;   // [关键] 标记事件已被消耗

        if (mouse_clicked)
        {
            m_clicked = true;

            // 分支逻辑：点 Label 是执行，点核心是折叠
            if (m_label_hovered)
            {
                if (on_execute_callback)
                {
                    std::string val = "";
                    if (m_input_field && m_input_field->m_target_string) val = *m_input_field->m_target_string;
                    on_execute_callback(m_id, val);
                }
            }
            else // 点在了 inside_core
            {
                if (on_toggle_callback) on_toggle_callback(!m_is_active);
                else SetActive(!m_is_active);
            }
        }
    }
    else
    {
        m_hovered = false;
        // 注意：m_label_hovered 已在上面 B 步骤重置
    }

    if (mouse_released) m_clicked = false;
}
void PulsarButton::Draw(ImDrawList* draw_list)
{
    if (!m_visible) return;

    const auto& theme = UIContext::Get().m_theme;
    

    // 2. 绘制展开后的图形 (连接线、脉冲星)
    // 所有的文字都由 UIElement::Draw -> 子元素 Draw 完成
    if (m_anim_progress > 0.01f)
    {
        ImVec2 center_abs = { m_absolute_pos.x + 20.0f, m_absolute_pos.y + 20.0f };
        float line_prog = std::max(0.0f, (m_anim_progress - 0.5f) * 2.0f);
        float pulsar_scale = std::min(1.0f, m_anim_progress * 1.5f);
        const ImU32 color_theme = GetColorWithAlpha(theme.color_accent, 1.0f);
        const ImU32 color_white = GetColorWithAlpha(theme.color_text_highlight, 1.0f);

        // 脉冲星核心
        if (pulsar_scale > 0.1f)
        {
            draw_list->AddCircleFilled(center_abs, 2.0f * pulsar_scale, color_white);
        }
        // 射线
        for (const auto& ray : m_rays)
        {
            float current_len = std::max(0.0f, pulsar_scale * ray.len);
            if (current_len <= 0.05f) continue;

            float cosT = std::cos(ray.theta + m_rotation_angle);
            float sinT = std::sin(ray.theta + m_rotation_angle);
            float cosP = std::cos(ray.phi);
            float sinP = std::sin(ray.phi);

            float x3d = cosT * cosP * pulsar_radius;
            float y3d = sinP * pulsar_radius;
            float z3d = sinT * cosP * pulsar_radius;

            float scale = 200.0f / (200.0f + z3d);
            ImVec2 p_end = { center_abs.x + x3d * scale * current_len, center_abs.y + y3d * scale * current_len };

            ImVec2 delta = { p_end.x - center_abs.x, p_end.y - center_abs.y };
            float total_dist = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (total_dist < 0.1f) continue;
            ImVec2 unit_v = { delta.x / total_dist, delta.y / total_dist };
            ImVec2 perp_v = { -unit_v.y, unit_v.x };

            ImVec2 current_pos = center_abs;
            for (const auto& seg : ray.segments)
            {
                if (seg.offset > current_len / ray.len) break;
                ImVec2 next_pos = { center_abs.x + delta.x * (seg.offset / (current_len / ray.len)), center_abs.y + delta.y * (seg.offset / (current_len / ray.len)) };
                draw_list->AddLine(current_pos, next_pos, color_theme);
                if (seg.has_bump)
                {
                    ImVec2 bump_pos = { next_pos.x + perp_v.x * seg.bump_height, next_pos.y + perp_v.y * seg.bump_height };
                    ImVec2 bump_end = { next_pos.x + unit_v.x * 0.5f + perp_v.x * seg.bump_height, next_pos.y + unit_v.y * 0.5f + perp_v.y * seg.bump_height };
                    ImVec2 back_pos = { next_pos.x + unit_v.x * 0.5f, next_pos.y + unit_v.y * 0.5f };
                    draw_list->AddLine(next_pos, bump_pos, color_theme);
                    draw_list->AddLine(bump_pos, bump_end, color_theme);
                    draw_list->AddLine(bump_end, back_pos, color_theme);
                    current_pos = back_pos;
                }
                else
                {
                    current_pos = next_pos;
                }
            }
            draw_list->AddLine(current_pos, p_end, color_theme);
        }

        // 装饰连线
        if (line_prog > 0.01f)
        {
            float rel_text_x = 20.0f + 64.0f;
            float rel_line_y = 20.0f - 40.0f;
            float line_abs_y = m_absolute_pos.y + rel_line_y;
            float text_abs_x = m_absolute_pos.x + rel_text_x;

            ImVec2 p1 = center_abs;
            ImVec2 p2 = { center_abs.x + 60.0f, line_abs_y };// 修改后：使用当前动态长度
            ImVec2 p3 = { text_abs_x + m_current_line_len, line_abs_y };

            ImVec2 d1 = { p2.x - p1.x, p2.y - p1.y }; float l1 = std::sqrt(d1.x * d1.x + d1.y * d1.y);
            ImVec2 d2 = { p3.x - p2.x, p3.y - p2.y }; float l2 = std::sqrt(d2.x * d2.x + d2.y * d2.y);
            float total_len = l1 + l2;
            float draw_len = total_len * line_prog;

            if (draw_len <= l1)
            {
                ImVec2 end_p = { p1.x + d1.x * (draw_len / l1), p1.y + d1.y * (draw_len / l1) };
                draw_list->AddLine(p1, end_p, GetColorWithAlpha(theme.color_accent, 0.7f));
            }
            else
            {
                draw_list->AddLine(p1, p2, GetColorWithAlpha(theme.color_accent, 0.7f));
                float rem_len = draw_len - l1;
                ImVec2 end_p = { p2.x + d2.x * (rem_len / l2), p2.y + d2.y * (rem_len / l2) };
                draw_list->AddLine(p2, end_p, GetColorWithAlpha(theme.color_accent, 0.7f));
            }
        }
    }

    // 3. 绘制子元素
    UIElement::Draw(draw_list);
}

_UI_END
_SYSTEM_END
_NPGS_END