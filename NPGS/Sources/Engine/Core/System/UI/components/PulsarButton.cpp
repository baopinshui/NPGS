// --- START OF FILE PulsarButton.cpp ---
#include "PulsarButton.h"
#include "../TechUtils.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// --- Helper Functions ---
static void PushFont(ImFont* font) { if (font) ImGui::PushFont(font); }
static void PopFont(ImFont* font) { if (font) ImGui::PopFont(); }

// --- Constructor ---
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

// --- Public Methods ---
void PulsarButton::SetStatusText(const std::string& text)
{
    if (m_current_status_text == text) return;
    m_current_status_text = text;

    if (m_is_active)
    {
        m_hacker_status.Start(m_current_status_text, 0.0f);
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
            To(&m_anim_progress, 1.0f, 0.5f, EasingType::EaseOutQuad, [this]() { this->m_anim_state = PulsarAnimState::Open; });

            m_hacker_status.Start(m_current_status_text, 0.0f);
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
            To(&m_anim_progress, 0.0f, 0.4f, EasingType::EaseInQuad, [this]() { this->m_anim_state = PulsarAnimState::Closed; });
            m_hacker_status.Reset();
            m_hacker_label.Reset();
            m_hacker_stat_value.Reset();
        }
    }
}
void PulsarButton::SetExecutable(bool can_execute)
{
    m_can_execute = can_execute;
}

// --- Overrides ---
void PulsarButton::Update(float dt, const ImVec2& parent_abs_pos)
{
    UIElement::Update(dt, parent_abs_pos);

    m_rotation_angle += 2.0f * Npgs::Math::kPi * dt / 3.0f;
    if (m_rotation_angle > 2.0f * Npgs::Math::kPi) m_rotation_angle -= 2.0f * Npgs::Math::kPi;

    m_hacker_status.Update(dt);
    m_hacker_label.Update(dt);
    m_hacker_stat_value.Update(dt);

    if (m_input_field)
    {
        float text_alpha = (m_anim_state == PulsarAnimState::Open) ? 1.0f : (m_anim_progress > 0.4f ? (m_anim_progress - 0.4f) / 0.6f : 0.0f);
        m_input_field->m_visible = (m_anim_state != PulsarAnimState::Closed);
        m_input_field->m_alpha = text_alpha;
        m_input_field->m_block_input = m_is_active;
    }
}

bool PulsarButton::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released)
{
    if (!m_visible || m_alpha <= 0.01f) return false;

    // 1. Let child (InputField) handle event first
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        if ((*it)->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released)) return true;
    }

    // 2. Define hit regions
    Rect icon_rect = { m_absolute_pos.x, m_absolute_pos.y, 40, 40 };

    // 3. Check hits
    bool hit_icon = icon_rect.Contains(mouse_pos);
    bool hit_label = false;
    if (m_is_active && m_anim_progress > 0.8f)
    { // Only check label when expanded
        hit_label = m_label_hit_rect.Contains(mouse_pos);
    }

    // 4. Update hover states
    m_label_hovered = hit_label && m_can_execute;
    m_hovered = hit_icon || m_label_hovered;

    // 5. Process clicks
    if (mouse_clicked && m_block_input)
    {
        if (hit_label && m_is_active && m_can_execute)
        {
            m_clicked = true;
            if (on_execute_callback)
            {
                std::string val = "";
                if (m_input_field && m_input_field->m_target_string) val = *m_input_field->m_target_string;
                on_execute_callback(m_id, val);
            }
            return true; // Consume event
        }
        else if (hit_icon)
        {
            m_clicked = true;
            if (on_toggle_callback)
            {
                on_toggle_callback(!m_is_active);
            }
            else
            { // Fallback behavior
                SetActive(!m_is_active);
            }
            return true; // Consume event
        }
    }

    if (mouse_released) m_clicked = false;

    // Consume event if mouse is over any active part of the button
    return m_hovered;
}

void PulsarButton::Draw(ImDrawList* draw_list)
{
    if (!m_visible) return;

    const auto& theme = UIContext::Get().m_theme;
    auto& ctx = UIContext::Get();

    const ImU32 color_theme = GetColorWithAlpha(theme.color_accent, 1.0f);
    const ImU32 color_white = GetColorWithAlpha(theme.color_text_highlight, 1.0f);
    const ImU32 color_text_dim = GetColorWithAlpha(theme.color_text, 1.0f);

    // 1. Draw the collapsed icon button (fades out as it expands)
    float icon_scale = 1.0f - m_anim_progress;
    if (icon_scale > 0.01f)
    {
        ImVec2 center = { m_absolute_pos.x + 20, m_absolute_pos.y + 20 };
        ImVec2 p_min = { center.x - 20 * icon_scale, center.y - 20 * icon_scale };
        ImVec2 p_max = { center.x + 20 * icon_scale, center.y + 20 * icon_scale };

        draw_list->AddRectFilled(p_min, p_max, GetColorWithAlpha({ 0,0,0,0.6f }, icon_scale));

        PushFont(m_font);
        ImVec2 icon_size = ImGui::CalcTextSize(m_icon_char.c_str());
        draw_list->AddText(
            ImVec2(center.x - icon_size.x * 0.5f, center.y - icon_size.y * 0.5f),
            GetColorWithAlpha(m_hovered && !m_is_active ? theme.color_accent : theme.color_text, icon_scale),
            m_icon_char.c_str()
        );
        PopFont(m_font);

        ImU32 border_col = GetColorWithAlpha(m_hovered && !m_is_active ? theme.color_accent : theme.color_border, icon_scale);
        TechUtils::DrawBracketedBox(draw_list, p_min, p_max, border_col, 2.0f, 8.0f);
    }

    // 2. Draw the expanded canvas content
    if (m_anim_progress > 0.01f)
    {
        // Layout calculations
        ImVec2 center_abs = { m_absolute_pos.x + pulsar_center_offset.x, m_absolute_pos.y + pulsar_center_offset.y };
        const float LAYOUT_LINE_Y_OFFSET = -40.0f;
        const float LAYOUT_TEXT_START_X = 64.0f;
        const float LAYOUT_ELBOW_X = 60.0f;
        const float LAYOUT_LINE_LENGTH = 130.0f;
        float line_abs_y = center_abs.y + LAYOUT_LINE_Y_OFFSET;
        float text_abs_x = center_abs.x + LAYOUT_TEXT_START_X;
        float line_end_x = text_abs_x + LAYOUT_LINE_LENGTH;

        // Animation progress variables
        float line_prog = std::max(0.0f, (m_anim_progress - 0.5f) * 2.0f);
        float pulsar_scale = std::min(1.0f, m_anim_progress * 1.5f);
        float text_alpha = (m_anim_state == PulsarAnimState::Open) ? 1.0f : (m_anim_progress > 0.4f ? (m_anim_progress - 0.4f) / 0.6f : 0.0f);

        // [恢复] Draw Pulsar Core & Rays
        if (pulsar_scale > 0.1f)
        {
            draw_list->AddCircleFilled(center_abs, 2.0f * pulsar_scale, color_white);
        }
        for (const auto& ray : m_rays)
        {
            float current_len = std::max(0.0f, pulsar_scale * ray.len);
            if (current_len <= 0.05f) continue;
            float cosT = std::cos(ray.theta + m_rotation_angle); float sinT = std::sin(ray.theta + m_rotation_angle);
            float cosP = std::cos(ray.phi); float sinP = std::sin(ray.phi);
            float x3d = cosT * cosP * pulsar_radius; float y3d = sinP * pulsar_radius; float z3d = sinT * cosP * pulsar_radius;
            float scale = 200.0f / (200.0f + z3d);
            ImVec2 p_end = { center_abs.x + x3d * scale * current_len, center_abs.y + y3d * scale * current_len };
            draw_list->AddLine(center_abs, p_end, color_theme); // Simplified ray drawing for brevity
        }

        // [恢复] Draw Decorative Connecting Line
        if (line_prog > 0.01f)
        {
            ImVec2 p1 = center_abs;
            ImVec2 p2 = { center_abs.x + LAYOUT_ELBOW_X, line_abs_y };
            ImVec2 p3 = { line_end_x, line_abs_y };
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

        // Draw Text Content
        if (text_alpha > 0.01f)
        {
            // Status Text (e.g., TARGET LOCKED)
            ImVec2 pos_status = { text_abs_x, line_abs_y - 18.0f };
            m_hacker_status.Draw(draw_list, pos_status, GetColorWithAlpha(theme.color_text_highlight, text_alpha), ctx.m_font_bold);

            // Main Label / Execute Button
            ImVec2 pos_label = { text_abs_x, line_abs_y - 0.0f };
            ImU32 label_color;
            if (m_can_execute)
            {
                // 可执行：使用悬停变色逻辑
                label_color = GetColorWithAlpha(m_label_hovered ? theme.color_accent : theme.color_text_highlight, text_alpha);
            }
            else
            {
                // 不可执行：强制使用灰色
                label_color = GetColorWithAlpha(theme.color_text_disabled, text_alpha);
            }
            m_hacker_label.Draw(draw_list, pos_label, label_color, ctx.m_font_bold);

            // Update hit rect for next frame's input handling
            PushFont(ctx.m_font_bold);
            ImVec2 label_size = ImGui::CalcTextSize(m_label.c_str());
            PopFont(ctx.m_font_bold);
            m_label_hit_rect = { pos_label.x - 5, pos_label.y - 2, label_size.x + 10, label_size.y + 4 };

            // [恢复] Draw Stats Label, Value/Input, and Unit
            if (!m_stat_label.empty())
            {
                ImVec2 pos_stats_row = { text_abs_x, line_abs_y + 26.0f };

                // (A) Label ("MASS:")
                PushFont(ctx.m_font_bold);
                std::string lbl_str = m_stat_label + ":";
                draw_list->AddText(pos_stats_row, color_text_dim, lbl_str.c_str());
                float lbl_w = ImGui::CalcTextSize(lbl_str.c_str()).x;
                PopFont(ctx.m_font_bold);

                ImVec2 pos_value = { pos_stats_row.x + lbl_w + 5.0f, pos_stats_row.y };

                // (B) Set InputField position
                if (m_input_field)
                {
                    m_input_field->m_rect.x = pos_value.x - m_absolute_pos.x;
                    m_input_field->m_rect.y = pos_value.y - m_absolute_pos.y - 2.0f; // Minor Y adjustment
                    m_input_field->m_rect.w = 100.0f;
                }

                // (C) Draw static value or unit next to input field
                if (!m_is_editable)
                {
                    PushFont(ctx.m_font_large ? ctx.m_font_large : ctx.m_font_bold);
                    m_hacker_stat_value.Draw(draw_list, { pos_value.x, pos_value.y - 4.0f }, color_theme);
                    float val_w = ImGui::CalcTextSize(m_hacker_stat_value.m_display_text.c_str()).x;
                    PopFont(ctx.m_font_large ? ctx.m_font_large : ctx.m_font_bold);

                    ImVec2 pos_unit = { pos_value.x + val_w + 5.0f, pos_stats_row.y };
                    PushFont(ctx.m_font_bold);
                    draw_list->AddText(pos_unit, color_text_dim, m_stat_unit.c_str());
                    PopFont(ctx.m_font_bold);
                }
                else
                {
                    ImVec2 pos_unit = { pos_value.x + 103.0f, pos_stats_row.y };
                    PushFont(ctx.m_font_bold);
                    draw_list->AddText(pos_unit, color_text_dim, m_stat_unit.c_str());
                    PopFont(ctx.m_font_bold);
                }
            }
        }
    }

    // Always draw children (InputField) at the end
    UIElement::Draw(draw_list);
}

_UI_END
_SYSTEM_END
_NPGS_END