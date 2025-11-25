#include "PulsarButton.h"

#include "../TechUtils.h"
_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN
static void PushFont(ImFont* font)
{
    if (font) ImGui::PushFont(font);
}
static void PopFont(ImFont* font)
{
    if (font) ImGui::PopFont();
}
PulsarButton::PulsarButton(const std::string& id, const std::string& label, const std::string& icon_char, const std::string& stat_label, std::string* stat_value_ptr, const std::string& stat_unit, bool is_editable)
    : m_id(id), m_label(label), m_icon_char(icon_char), m_stat_label(stat_label), m_stat_unit(stat_unit), m_is_editable(is_editable)
{
    m_rect = { 0, 0, 40, 40 };
    m_block_input = true;

    if (is_editable && stat_value_ptr)
    {
        m_input_field = std::make_shared<InputField>(stat_value_ptr);
        m_input_field->m_visible = false;
        m_input_field->m_block_input = false;
        m_input_field->m_alpha = 0.0f;
        m_input_field->m_rect = { 135, 20, 100, 20 };
        const auto& theme = UIContext::Get().m_theme;
        m_input_field->m_text_color = theme.color_accent;
        m_input_field->m_border_color = theme.color_accent;
        m_input_field->on_change = [this](const std::string& val)
        {
            if (this->on_stat_change_callback)
            {
                this->on_stat_change_callback(this->m_id, val);
            }
        };
        AddChild(m_input_field);
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

void PulsarButton::SetActive(bool active)
{
    if (m_is_active == active) return;
    m_is_active = active;

    if (m_is_active)
    {
        if (m_anim_state == PulsarAnimState::Closed || m_anim_state == PulsarAnimState::Closing)
        {
            m_anim_state = PulsarAnimState::Opening;
            To(&m_anim_progress, 1.0f, 0.5f, EasingType::EaseOutQuad, [this]()
            {
                this->m_anim_state = PulsarAnimState::Open;
            });
            m_hacker_locked.Start("TARGET LOCKED", 0.0f);
            m_hacker_label.Start(m_label, 0.2f);
            if (!m_is_editable && m_input_field && m_input_field->m_target_string)
            {
                m_hacker_stat_value.Start(*m_input_field->m_target_string, 0.4f);
            }
        }
    }
    else
    {
        if (m_anim_state == PulsarAnimState::Open || m_anim_state == PulsarAnimState::Opening)
        {
            m_anim_state = PulsarAnimState::Closing;
            To(&m_anim_progress, 0.0f, 0.4f, EasingType::EaseInQuad, [this]()
            {
                this->m_anim_state = PulsarAnimState::Closed;
            });
            m_hacker_locked.Reset();
            m_hacker_label.Reset();
            m_hacker_stat_value.Reset();
        }
    }
}

void PulsarButton::Update(float dt, const ImVec2& parent_abs_pos)
{
    UIElement::Update(dt, parent_abs_pos); // Handles m_anim_progress tween and children update

    m_rotation_angle += 2.0f * Npgs::Math::kPi * dt / 3.0;
    if (m_rotation_angle > 2.0f * Npgs::Math::kPi) m_rotation_angle -= 2.0f * Npgs::Math::kPi;

    m_hacker_locked.Update(dt);
    m_hacker_label.Update(dt);
    m_hacker_stat_value.Update(dt);


    if (m_input_field)
    {
        float text_alpha = (m_anim_state == PulsarAnimState::Open) ? 1.0f : (m_anim_progress > 0.4f ? (m_anim_progress - 0.4f) / 0.6f : 0.0f);
        m_input_field->m_visible = (m_anim_state != PulsarAnimState::Closed);
        m_input_field->m_block_input = m_is_active;
        m_input_field->m_alpha = text_alpha;
    }
}
bool PulsarButton::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released)
{
    if (!m_visible || m_alpha <= 0.01f) return false;

    // 1. 先让子元素（如 InputField）处理事件
    // 因为 InputField 是挂在这个按钮下的，我们优先让它响应
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        if ((*it)->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released))
            return true;
    }

    // 2. [核心修改] 计算以中心向外扩散的判定区域
    // 基础尺寸 (Closed)
    float base_w = 40.0f;
    float base_h = 40.0f;

    // 当前动画下的尺寸 (插值)
    // 这里的 Lerp 可以简单手写： start + (end - start) * t
    float current_w = base_w + (m_expanded_width - base_w) * m_anim_progress;
    float current_h = base_h + (m_expanded_height - base_h) * m_anim_progress;

    // 计算中心点 (基于 m_rect 的中心，因为 m_rect 没变，所以中心点稳定)
    ImVec2 center = {
        m_absolute_pos.x + base_w * 0.5f,
        m_absolute_pos.y + base_h * 0.5f
    };

    // 生成“扩散后”的点击矩形
    Rect hit_rect = {
        center.x - current_w * 0.5f, // 左边缘 = 中心 - 半宽
        center.y - current_h * 0.5f, // 上边缘 = 中心 - 半高
        current_w,
        current_h
    };

    // 3. 执行判定
    bool inside = hit_rect.Contains(mouse_pos);

    if (inside && m_block_input)
    {
        m_hovered = true;
        if (mouse_clicked)
        {
            m_clicked = true;
            // 触发回调
            if (on_click_callback) on_click_callback();
        }

        // 如果松开鼠标，重置点击状态
        if (mouse_released) m_clicked = false;

        return true; // 消费事件
    }
    else if (inside && !m_block_input)
    {
        m_hovered = true;
        return false; // 悬停但不阻挡（视具体需求）
    }

    m_hovered = false;
    m_clicked = false;
    return false;
}
void PulsarButton::Draw(ImDrawList* draw_list)
{
    if (!m_visible) return;

    const auto& theme = UIContext::Get().m_theme;
    auto& ctx = UIContext::Get(); // 获取上下文以访问字体

    const ImU32 color_theme = GetColorWithAlpha(theme.color_accent, 1.0f);
    const ImU32 color_white = GetColorWithAlpha(theme.color_text_highlight, 1.0f);
    const ImU32 color_text_dim = GetColorWithAlpha(theme.color_text_highlight, 1.0f);


    ImFont* base_font = GetFont();
    if (base_font) ImGui::PushFont(base_font);
    // 1. 绘制原始图标按钮 (随动画缩小)
    float icon_scale = 1.0f - m_anim_progress;
    if (icon_scale > 0.01f)
    {
        ImVec2 center = { m_absolute_pos.x + 20, m_absolute_pos.y + 20 };
        ImVec2 p_min = { center.x - 20 * icon_scale, center.y - 20 * icon_scale };
        ImVec2 p_max = { center.x + 20 * icon_scale, center.y + 20 * icon_scale };

        draw_list->AddRectFilled(p_min, p_max, GetColorWithAlpha({ 0,0,0,0.5f }, icon_scale));

        ImVec2 icon_size = ImGui::CalcTextSize(m_icon_char.c_str());
        draw_list->AddText(
            ImVec2(center.x - icon_size.x * 0.5f, center.y - icon_size.y * 0.5f),
            GetColorWithAlpha(m_hovered && !m_is_active ? theme.color_accent : theme.color_text, icon_scale),
            m_icon_char.c_str()
        );
        float corner_len = 8.0f;
        float thickness = 2.0f;
        float half_t = -thickness * 0.5f; // 计算半宽，用于内缩偏移
        ImU32 border_col = GetColorWithAlpha(m_hovered && !m_is_active ? theme.color_accent : theme.color_border, icon_scale);

        // 左上 (Top Left) -> 向右(+), 向下(+)
        TechUtils::DrawCorner(draw_list,
            ImVec2(p_min.x + half_t, p_min.y + half_t),
            corner_len, corner_len, thickness, border_col);

        // 右上 (Top Right) -> 向左(-), 向下(+)
        TechUtils::DrawCorner(draw_list,
            ImVec2(p_max.x - half_t, p_min.y + half_t),
            -corner_len, corner_len, thickness, border_col);

        // 左下 (Bottom Left) -> 向右(+), 向上(-)
        TechUtils::DrawCorner(draw_list,
            ImVec2(p_min.x + half_t, p_max.y - half_t),
            corner_len, -corner_len, thickness, border_col);

        // 右下 (Bottom Right) -> 向左(-), 向上(-)
        TechUtils::DrawCorner(draw_list,
            ImVec2(p_max.x - half_t, p_max.y - half_t),
            -corner_len, -corner_len, thickness, border_col);
    }
    if (base_font) ImGui::PopFont();
    // 2. 绘制展开后的 Canvas 内容
    if (m_anim_progress > 0.01f)
    {
        // =========================================================
        // [统一布局系统] Unified Layout System
        // =========================================================

        // A. 定义绝对坐标基准点 (Pulsar 圆心)
        ImVec2 center_abs = {
            m_absolute_pos.x + pulsar_center_offset.x,
            m_absolute_pos.y + pulsar_center_offset.y
        };

        // B. 定义布局常量 (相对偏移量)
        const float LAYOUT_LINE_Y_OFFSET = -40.0f; // 分割线相对于圆心的 Y 偏移 (负数向上)
        const float LAYOUT_TEXT_START_X = 64.0f;  // 文字内容起始 X (相对于圆心)
        const float LAYOUT_ELBOW_X = 60.0f;  // 折线拐点 X (相对于圆心)
        const float LAYOUT_LINE_LENGTH = 130.0f; // 横线总长度

        // C. 计算关键绝对坐标 (所有的绘图都依赖这些计算出的值，不再出现魔术数字)
        float line_abs_y = center_abs.y + LAYOUT_LINE_Y_OFFSET;
        float text_abs_x = center_abs.x + LAYOUT_TEXT_START_X;
        float line_end_x = text_abs_x + LAYOUT_LINE_LENGTH;

        // 动画参数
        float expansion = m_anim_progress;
        float line_prog = std::max(0.0f, (m_anim_progress - 0.5f) * 2.0f);
        float pulsar_scale = std::min(1.0f, m_anim_progress * 1.5f);
        float text_alpha = (m_anim_state == PulsarAnimState::Open) ? 1.0f : (m_anim_progress > 0.4f ? (m_anim_progress - 0.4f) / 0.6f : 0.0f);

        // --- 绘制脉冲星核心与射线 (保持不变) ---
        if (pulsar_scale > 0.1f)
        {
            draw_list->AddCircleFilled(center_abs, 2.0f * pulsar_scale, color_white);
        }

        // 绘制射线
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

        if (line_prog > 0.01f)
        {
            // p1: 起点 (圆心)
            ImVec2 p1 = center_abs;
            // p2: 拐点 (向上折，X 稍微向右一点，Y 与分割线对齐)
            ImVec2 p2 = { center_abs.x + LAYOUT_ELBOW_X, line_abs_y };
            // p3: 终点 (水平延伸到最右侧)
            ImVec2 p3 = { line_end_x, line_abs_y };

            // 计算分段长度用于动画
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

        // --- 绘制文字与内容 ---
        if (text_alpha > 0.01f)
        {
            // 1. 顶部标题 (Locked) 
            ImVec2 pos_locked = { text_abs_x, line_abs_y - 18.0f };

            m_hacker_locked.Draw(draw_list, pos_locked, GetColorWithAlpha(theme.color_text_highlight, text_alpha), ctx.m_font_bold);


            // 2. 主标题 (Label) - 紧贴分割线上方
            ImVec2 pos_label = { text_abs_x, line_abs_y - 0.0f };

            m_hacker_label.Draw(draw_list, pos_label, GetColorWithAlpha(theme.color_accent, text_alpha), ctx.m_font_bold);


            // 3. 统计数据 / 输入框 - 位于分割线下方
            // LineY + 6px (顶部留白)
            if (!m_stat_label.empty())
            {
                ImVec2 pos_stats_row = { text_abs_x, line_abs_y + 26.0f };

                // (A) Label ("MASS:")
                PushFont(ctx.m_font_bold);
                std::string lbl_str = m_stat_label + ":";
                draw_list->AddText(pos_stats_row, color_text_dim, lbl_str.c_str());
                float lbl_w = ImGui::CalcTextSize(lbl_str.c_str()).x;
                PopFont(ctx.m_font_bold);

                // (B) Value / Input Field
                // 数值起始 X
                ImVec2 pos_value = { pos_stats_row.x + lbl_w, pos_stats_row.y };

                // [修正] 输入框位置计算
                // m_rect 是相对于父节点(this) absolute_pos 的偏移
                if (m_input_field)
                {
                    // 核心修正：rect.x = 绝对坐标.x - 父级绝对坐标.x
                    // Y轴微调：-2.0f 是为了让输入框文字与前面的 Label 文字基线对齐
                    m_input_field->m_rect.x = pos_value.x - m_absolute_pos.x;
                    m_input_field->m_rect.y = pos_value.y - m_absolute_pos.y - 0.0f;
                    m_input_field->m_rect.w = 100.0f; // 给予固定宽度
                }

                if (!m_is_editable)
                {
                    // 静态数值绘制
                    PushFont(ctx.m_font_large ? ctx.m_font_large : ctx.m_font_bold);
                    m_hacker_stat_value.Draw(draw_list, { pos_value.x, pos_value.y - 4.0f }, color_theme); // 大字体稍微上移修正视觉重心
                    float val_w = ImGui::CalcTextSize(m_hacker_stat_value.m_display_text.c_str()).x;
                    PopFont(ctx.m_font_large ? ctx.m_font_large : ctx.m_font_bold);

                    // 单位跟随
                    ImVec2 pos_unit = { pos_value.x + val_w + 5.0f, pos_stats_row.y };
                    PushFont(ctx.m_font_bold);
                    draw_list->AddText(pos_unit, color_text_dim, m_stat_unit.c_str());
                    PopFont(ctx.m_font_bold);
                }
                else
                {
                    // 编辑模式：单位跟在输入框后面 (假设输入框宽 80)
                    ImVec2 pos_unit = { pos_value.x + 103.0f, pos_stats_row.y };
                    PushFont(ctx.m_font_bold);
                    draw_list->AddText(pos_unit, color_text_dim, m_stat_unit.c_str());
                    PopFont(ctx.m_font_bold);
                }
            }
        }
    }

    // 绘制子组件 (InputField)
    UIElement::Draw(draw_list);
}

_UI_END
_SYSTEM_END
_NPGS_END
