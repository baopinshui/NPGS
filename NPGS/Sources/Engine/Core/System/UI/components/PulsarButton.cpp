#include "PulsarButton.h"
#include "../TechUtils.h"
#include "TechBorderPanel.h"
#include "../I18n/I18nManager.h" 

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

void PulsarButton::SetIconColor(const ImVec4& color)
{
    if (m_text_icon)
    {
        m_text_icon->SetColor(color);
    }
    if (m_image_icon)
    {
        m_image_icon->m_tint_col = color;
    }
}

void PulsarButton::InitCommon(const std::string& status_key, const std::string& label_key, const std::string& stat_label_key, std::string* stat_value_ptr, const std::string& stat_unit_key)
{
    m_width = Length::Content();
    m_height = Length::Content();
    m_block_input = true;

    m_status_key = status_key;
    m_label_key = label_key;
    m_stat_label_key = stat_label_key;

    auto& ctx = UIContext::Get();
    const auto& theme = UIContext::Get().m_theme;

    // 0. 创建背景面板 
    m_bg_panel = std::make_shared<TechBorderPanel>();
    m_bg_panel->SetName("background"); // [新增] 命名背景
    m_bg_panel->m_rect = { 0, 0, 40, 40 };
    m_bg_panel->m_use_glass_effect = true;
    m_bg_panel->m_thickness = 2.0f;
    m_bg_panel->m_block_input = false;
    AddChild(m_bg_panel);

    // --- 2. 状态文本 ---
    m_text_status = std::make_shared<TechText>(TR(status_key), ThemeColorID::TextHighlight, true);
    m_text_status->SetName("status"); // [新增] 命名状态文本
    m_text_status->SetSizing(TechTextSizingMode::AutoWidthHeight);
    m_text_status->m_font = ctx.m_font_bold;
    m_text_status->m_block_input = false;
    AddChild(m_text_status);

    // --- 3. 主标签 ---
    m_text_label = std::make_shared<TechText>(TR(label_key), ThemeColorID::TextHighlight, true);
    m_text_label->SetName("label"); // [新增] 命名主标签
    m_text_label->SetSizing(TechTextSizingMode::AutoWidthHeight);
    m_text_label->m_font = ctx.m_font_bold;
    m_text_label->m_block_input = false;
    AddChild(m_text_label);

    // --- 4. 统计信息行 ---
    if (!stat_label_key.empty())
    {
        m_text_stat_label = std::make_shared<TechText>(TR(stat_label_key) + ":", ThemeColorID::Text);
        m_text_stat_label->SetName("statLabel"); // [新增] 命名统计标签
        m_text_stat_label->SetSizing(TechTextSizingMode::AutoWidthHeight);

        m_text_stat_label->m_font = ctx.m_font_bold;
        m_text_stat_label->m_block_input = false;
        AddChild(m_text_stat_label);
    }

    if (!stat_unit_key.empty())
    {
        m_text_stat_unit = std::make_shared<TechText>(stat_unit_key, ThemeColorID::Text);
        m_text_stat_unit->SetName("statUnit"); // [新增] 命名统计单位
        m_text_stat_unit->SetSizing(TechTextSizingMode::AutoWidthHeight);

        m_text_stat_unit->m_font = ctx.m_font_bold;
        m_text_stat_unit->m_block_input = false;
        AddChild(m_text_stat_unit);
    }

    m_stat_value_ptr = stat_value_ptr;
    if (m_is_editable && stat_value_ptr)
    {
        m_input_field = std::make_shared<InputField>(stat_value_ptr);
        m_input_field->SetName("statValueInput"); // [新增] 命名输入字段
        m_input_field->m_font = ctx.m_font_bold;
        m_input_field->m_text_color = ThemeColorID::Accent;
        m_input_field->m_border_color = ThemeColorID::Accent;
        AddChild(m_input_field);
    }
    else if (stat_value_ptr)
    {
        m_text_stat_value = std::make_shared<TechText>(*stat_value_ptr, ThemeColorID::Accent, true);
        m_text_stat_value->SetName("statValue"); // [新增] 命名统计数值
        m_text_stat_value->SetSizing(TechTextSizingMode::AutoWidthHeight);

        m_text_stat_value->m_font = ctx.m_font_bold;
        m_text_stat_value->m_block_input = false;
        AddChild(m_text_stat_value);
    }

    // 初始化射线 (逻辑不变)
    m_rays.resize(24);
    for (auto& ray : m_rays)
    {
        ray.theta = (float(rand()) / RAND_MAX) * 2.0f * Npgs::Math::kPi;
        ray.phi = ((float(rand()) / RAND_MAX) * Npgs::Math::kPi) - Npgs::Math::kPi / 2.0f;
        ray.len = 0.4f + (float(rand()) / RAND_MAX) * 0.6f;
        ray.shrink_factor = -0.2f;
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

PulsarButton::PulsarButton(const std::string& status_key, const std::string& label_key, const std::string& icon_char, const std::string& stat_label_key, std::string* stat_value_ptr, const std::string& stat_unit_key, bool is_editable)
    : m_is_editable(is_editable)
{
    InitCommon(status_key, label_key, stat_label_key, stat_value_ptr, stat_unit_key);


    m_text_icon = std::make_shared<TechText>(icon_char);
    m_text_icon->SetName("icon"); // [新增] 命名图标
    m_text_icon->SetSizing(TechTextSizingMode::Fixed);
    m_text_icon->m_rect = { 0, 0, 40, 40 };
    m_text_icon->m_align_h = Alignment::Center;
    m_text_icon->m_align_v = Alignment::Center;
    m_text_icon->m_block_input = false;
    AddChild(m_text_icon);
}

PulsarButton::PulsarButton(const std::string& status_key, const std::string& label_key, ImTextureID icon_texture, const std::string& stat_label_key, std::string* stat_value_ptr, const std::string& stat_unit_key, bool is_editable)
    : m_is_editable(is_editable)
{
    InitCommon(status_key, label_key, stat_label_key, stat_value_ptr, stat_unit_key);


    m_image_icon = std::make_shared<Image>(icon_texture);
    m_image_icon->SetName("icon"); // [新增] 命名图标
    m_image_icon->m_rect = { 0, 0, 40, 40 };
    m_image_icon->m_block_input = false;
    m_image_icon->m_tint_col = UIContext::Get().m_theme.color_text;
    AddChild(m_image_icon);
}
// --- PulsarButton.cpp 新增部分 ---

void PulsarButton::UpdateCommonData(const std::string& status_key, const std::string& label_key, const std::string& stat_label_key, std::string* stat_value_ptr, const std::string& stat_unit_key, bool is_editable)
{
    auto& ctx = UIContext::Get();

    // 1. 更新 Status
    m_status_key = status_key;
    if (m_text_status)
    {
        m_text_status->SetSourceText(TR(m_status_key));
    }

    // 2. 更新 Label
    m_label_key = label_key;
    if (m_text_label)
    {
        m_text_label->SetSourceText(TR(m_label_key));
    }

    // 3. 更新 Stat Label
    m_stat_label_key = stat_label_key;
    if (m_stat_label_key.empty())
    {
        if (m_text_stat_label)
        {
            RemoveChild(m_text_stat_label);
            m_text_stat_label = nullptr; // 重要：置空指针以便后续判断
        }
    }
    else
    {
        if (!m_text_stat_label)
        {
            m_text_stat_label = std::make_shared<TechText>(TR(m_stat_label_key) + ":", ThemeColorID::Text);
            m_text_stat_label->SetName("statLabel");
            m_text_stat_label->SetSizing(TechTextSizingMode::AutoWidthHeight);
            m_text_stat_label->m_font = ctx.m_font_bold;
            m_text_stat_label->m_block_input = false;
            AddChild(m_text_stat_label);
        }
        else
        {
            m_text_stat_label->SetSourceText(TR(m_stat_label_key) + ":");
        }
    }

    // 4. 更新 Stat Unit
    if (stat_unit_key.empty())
    {
        if (m_text_stat_unit)
        {
            RemoveChild(m_text_stat_unit);
            m_text_stat_unit = nullptr;
        }
    }
    else
    {
        if (!m_text_stat_unit)
        {
            m_text_stat_unit = std::make_shared<TechText>(stat_unit_key, ThemeColorID::Text);
            m_text_stat_unit->SetName("statUnit");
            m_text_stat_unit->SetSizing(TechTextSizingMode::AutoWidthHeight);
            m_text_stat_unit->m_font = ctx.m_font_bold;
            m_text_stat_unit->m_block_input = false;
            AddChild(m_text_stat_unit);
        }
        else
        {
            m_text_stat_unit->SetSourceText(stat_unit_key);
        }
    }

    // 5. 更新 Value (处理 Editable 模式切换)
    bool mode_changed = (m_is_editable != is_editable);
    m_is_editable = is_editable;
    m_stat_value_ptr = stat_value_ptr;

    // 如果模式改变，或者当前需要的组件不存在（且有数据指针），则需要重建
    if (mode_changed || (m_is_editable && !m_input_field && m_stat_value_ptr) || (!m_is_editable && !m_text_stat_value && m_stat_value_ptr))
    {
        // 先清理旧的组件
        if (m_input_field)
        {
            RemoveChild(m_input_field);
            m_input_field = nullptr;
        }
        if (m_text_stat_value)
        {
            RemoveChild(m_text_stat_value);
            m_text_stat_value = nullptr;
        }

        // 根据新模式创建组件
        if (m_stat_value_ptr)
        {
            if (m_is_editable)
            {
                m_input_field = std::make_shared<InputField>(m_stat_value_ptr);
                m_input_field->SetName("statValueInput");
                m_input_field->m_font = ctx.m_font_bold;
                m_input_field->m_text_color = ThemeColorID::Accent;
                m_input_field->m_border_color = ThemeColorID::Accent;
                AddChild(m_input_field);
            }
            else
            {
                m_text_stat_value = std::make_shared<TechText>(*m_stat_value_ptr, ThemeColorID::Accent, true);
                m_text_stat_value->SetName("statValue");
                m_text_stat_value->SetSizing(TechTextSizingMode::AutoWidthHeight);
                m_text_stat_value->m_font = ctx.m_font_bold;
                m_text_stat_value->m_block_input = false;
                AddChild(m_text_stat_value);
            }
        }
    }
    else
    {
        // 模式未变，仅更新数据绑定
        if (m_input_field) m_input_field->m_target_string = m_stat_value_ptr;
        if (m_text_stat_value && m_stat_value_ptr) m_text_stat_value->SetSourceText(*m_stat_value_ptr);
    }
}

void PulsarButton::SetData(const std::string& status_key, const std::string& label_key, const std::string& icon_char, const std::string& stat_label_key, std::string* stat_value_ptr, const std::string& stat_unit_key, bool is_editable)
{
    UpdateCommonData(status_key, label_key, stat_label_key, stat_value_ptr, stat_unit_key, is_editable);

    // 切换到 Text Icon 模式
    if (m_image_icon)
    {
        RemoveChild(m_image_icon);
        m_image_icon = nullptr;
    }

    if (!m_text_icon)
    {
        m_text_icon = std::make_shared<TechText>(icon_char);
        m_text_icon->SetName("icon");
        m_text_icon->SetSizing(TechTextSizingMode::Fixed);
        m_text_icon->m_rect = { 0, 0, 40, 40 };
        m_text_icon->m_align_h = Alignment::Center;
        m_text_icon->m_align_v = Alignment::Center;
        m_text_icon->m_block_input = false;
        AddChild(m_text_icon);
    }
    else
    {
        m_text_icon->SetSourceText(icon_char);
    }

    // 刷新颜色
    const auto& theme = UIContext::Get().m_theme;
    ImVec4 col = m_is_active ? (m_label_hovered ? theme.color_accent : theme.color_text_highlight) : (m_hovered ? theme.color_accent : theme.color_text);
    SetIconColor(col);
}

void PulsarButton::SetData(const std::string& status_key, const std::string& label_key, ImTextureID icon_texture, const std::string& stat_label_key, std::string* stat_value_ptr, const std::string& stat_unit_key, bool is_editable)
{
    UpdateCommonData(status_key, label_key, stat_label_key, stat_value_ptr, stat_unit_key, is_editable);

    // 切换到 Image Icon 模式
    if (m_text_icon)
    {
        RemoveChild(m_text_icon);
        m_text_icon = nullptr;
    }

    if (!m_image_icon)
    {
        m_image_icon = std::make_shared<Image>(icon_texture);
        m_image_icon->SetName("icon");
        m_image_icon->m_rect = { 0, 0, 40, 40 };
        m_image_icon->m_block_input = false;
        m_image_icon->m_tint_col = UIContext::Get().m_theme.color_text;
        AddChild(m_image_icon);
    }
    else
    {
        m_image_icon->m_texture_id = icon_texture;
    }

    // 刷新颜色
    const auto& theme = UIContext::Get().m_theme;
    ImVec4 col = m_is_active ? (m_label_hovered ? theme.color_accent : theme.color_text_highlight) : (m_hovered ? theme.color_accent : theme.color_text);
    SetIconColor(col);
}
void PulsarButton::SetStatus(const std::string& status_key)
{
    if (m_status_key == status_key) return;
    m_status_key = status_key;

    // 立即更新文本
    if (m_text_status)
    {
        m_text_status->SetSourceText(TR(m_status_key));
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
            To(&m_anim_progress, 1.0f, 0.8f, EasingType::Linear, [this]() { this->m_anim_state = PulsarAnimState::Open; });

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
            To(&m_anim_progress, 0.0f, 0.6f, EasingType::Linear, [this]() { this->m_anim_state = PulsarAnimState::Closed; });
        }
    }
}

void PulsarButton::SetExecutable(bool can_execute) { m_can_execute = can_execute; }

void PulsarButton::Update(float dt)
{
    UIElement::Update(dt);
    // --- 0. I18n 更新检查 ---
    // 一旦版本变化，立即更新所有文本组件
    auto& i18n = System::I18nManager::Get();
    if (m_local_i18n_version != i18n.GetVersion())
    {
        m_local_i18n_version = i18n.GetVersion();

        if (!m_status_key.empty() && m_text_status)
            m_text_status->SetSourceText(i18n.Get(m_status_key));

        if (!m_label_key.empty() && m_text_label)
            m_text_label->SetSourceText(i18n.Get(m_label_key));

        if (!m_stat_label_key.empty() && m_text_stat_label)
            m_text_stat_label->SetSourceText(i18n.Get(m_stat_label_key) + ":");
    }

    // --- 动画逻辑开始 ---
    float target_core_hover = (m_is_active && m_core_hovered) ? 1.0f : 0.0f;
    float speed = 6.0f * dt;
    if (m_core_hover_progress < target_core_hover)
        m_core_hover_progress = std::min(m_core_hover_progress + speed, target_core_hover);
    else
        m_core_hover_progress = std::max(m_core_hover_progress - speed, target_core_hover);

    const float normal_speed = 2.0f * Npgs::Math::kPi / 3.0f;
    const float fast_speed = 6.0 * normal_speed;
    float current_rot_speed = normal_speed + (fast_speed - normal_speed) * std::sin(Npgs::Math::kPi * m_core_hover_progress);

    m_rotation_angle += current_rot_speed * dt;
    if (m_rotation_angle > 2.0f * Npgs::Math::kPi) m_rotation_angle -= 2.0f * Npgs::Math::kPi;

    // 检查动态数值更新
    if (!m_is_editable && m_stat_value_ptr && m_text_stat_value)
    {
        if (m_text_stat_value->m_source_key_or_text != *m_stat_value_ptr)
        {
            m_text_stat_value->SetSourceText(*m_stat_value_ptr);
        }
    }

    float t = std::clamp((m_anim_progress - 0.25f) * 2.0f, 0.0f, 1.0f);
    line_prog = t * t * (3.0f - 2.0f * t);

    float text_fade_start = 0.75f;
    float text_alpha = 0.0f;
    if (m_anim_state == PulsarAnimState::Open) text_alpha = 1.0f;
    else text_alpha = std::clamp((line_prog - text_fade_start) / (1.0f - text_fade_start), 0.0f, 1.0f);

    const auto& theme = UIContext::Get().m_theme;

    // --- 3. [核心逻辑] 实时计算目标线长并同步驱动 ---
    float max_text_w = 0.0f;

    // 测量 Label 宽度
    if (m_text_label)
    {
        ImFont* font = m_text_label->GetFont();
        if (font)
        {
            float w = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, m_text_label->m_current_display_text.c_str()).x;
            if (w > max_text_w) max_text_w = w;
        }
    }
    // 测量 Status 宽度
    if (m_text_status)
    {
        ImFont* font = m_text_status->GetFont();
        if (font)
        {
            float w = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, m_text_status->m_current_display_text.c_str()).x;
            if (w > max_text_w) max_text_w = w;
        }
    }

    // 计算目标长度，无论变长还是变短都直接设置目标
    float calculated_target_len = std::max(130.0f, max_text_w + 30.0f);

    // 如果目标长度发生变化，触发平滑过渡
    if (std::abs(calculated_target_len - m_target_line_len) > 1.0f)
    {
        m_target_line_len = calculated_target_len;
        To(&m_current_line_len, m_target_line_len, 0.2f, EasingType::EaseOutQuad);
    }



    ImVec4 final_action_color;
    if (m_can_execute)
    {
        final_action_color = m_label_hovered ? theme.color_accent : theme.color_text_highlight;
    }
    else
    {
        final_action_color = theme.color_text_disabled;
    }

    if (m_bg_panel)
    {
        m_bg_panel->m_force_accent_color = (m_anim_progress > 0.6f);
        m_bg_panel->m_show_flow_border = (m_anim_progress > 0.6f);

        bool is_high_energy = m_can_execute && m_label_hovered;
        const float total_duration = 0.5f;
        const float spd = 1.0f / total_duration;

        if (is_high_energy)
        {
            m_hover_energy_progress += spd * dt;
            if (m_hover_energy_progress > 1.0f) m_hover_energy_progress = 1.0f;
        }
        else
        {
            m_hover_energy_progress -= spd * dt;
            if (m_hover_energy_progress < 0.0f) m_hover_energy_progress = 0.0f;
        }

        float freq_t = std::clamp(m_hover_energy_progress / 0.6f, 0.0f, 1.0f);
        const float min_freq = 0.5f;
        const float max_freq = 10.0f;
        float current_freq = min_freq + (max_freq - min_freq) * freq_t;
        m_bg_panel->m_flow_period = 1.0f / std::max(0.01f, current_freq);

        float len_t = m_hover_energy_progress;
        const float min_len = 0.3f;
        const float max_len = 1.0f;
        m_bg_panel->m_flow_length_ratio = min_len + (max_len - min_len) * len_t;
    }

    // --- 7. 更新图标 ---
    Rect icon_rect = m_bg_panel->m_rect;
    if (m_text_icon)
    {
        m_text_icon->m_visible = true;
        if (m_is_active)
        {
            m_text_icon->SetColor(final_action_color);
            if (m_label_hovered) m_bg_panel->m_hovered = true;
        }
        else
        {
            m_text_icon->SetColor(m_hovered ? theme.color_accent : theme.color_text);
        }
    }
    if (m_image_icon)
    {
        m_image_icon->m_visible = true;
        if (m_is_active)
        {
            m_image_icon->m_tint_col = final_action_color;
            if (m_label_hovered) m_bg_panel->m_hovered = true;
        }
        else
        {
            m_image_icon->m_tint_col = m_hovered ? theme.color_accent : theme.color_text;
        }
    }

    // --- 8. 下方文本位置更新 ---
    const float LAYOUT_TEXT_START_X = 64.0f;
    float rel_text_x = pulsar_center_offset.x + LAYOUT_TEXT_START_X;

    // 状态文本
    m_text_status->m_alpha = text_alpha;
    m_text_status->m_visible = (text_alpha > 0.01f);

    // 主标签
    m_text_label->m_alpha = text_alpha;
    m_text_label->m_visible = (text_alpha > 0.01f);
    m_text_label->SetColor(final_action_color);

    // 统计信息行
    if (m_text_stat_label)
    {
        m_text_stat_label->m_alpha = text_alpha;
        m_text_stat_label->m_visible = (text_alpha > 0.01f);

        ImFont* font = m_text_stat_label->GetFont();
        float lbl_w = 0.0f;
        if (font) lbl_w = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, m_text_stat_label->m_source_key_or_text.c_str()).x;

        float val_x = rel_text_x + lbl_w + 5.0f;

        if (m_input_field)
        {
            m_input_field->m_alpha = text_alpha;
            m_input_field->m_visible = (text_alpha > 0.01f);
            m_input_field->m_block_input = m_is_active;
        }
        if (m_text_stat_value)
        {
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

            m_text_stat_unit->m_alpha = text_alpha;
            m_text_stat_unit->m_visible = (text_alpha > 0.01f);
        }
    }

    if (!m_visible) return;
 

    m_core_hovered = false;

    if (!m_can_execute && m_anim_progress > 0.5)
    {
        m_bg_panel->m_hovered = false;
    }
}

ImVec2 PulsarButton::Measure(ImVec2 available_size)
{
    if (!m_visible)
    {
        m_desired_size = { 0, 0 };
        return m_desired_size;
    }

    // 1. 测量所有子组件，以确保它们的 desired_size 是最新的
    //    (虽然我们是手动布局，但子组件内部可能需要这个信息)
    for (auto& child : m_children)
    {
        child->Measure(available_size);
    }

    float closed_w = 40.0f, closed_h = 40.0f;


    m_desired_size = { closed_w, closed_h };
    return m_desired_size;
}

void PulsarButton::Arrange(const Rect& final_rect)
{
    // 1. 设置 PulsarButton 自身的位置和大小
    UIElement::Arrange(final_rect);

    // 2. [从旧 Update 迁移] 动态计算布局与路径
 // --- 4. 动态计算布局与路径 (逻辑不变) ---
    ImVec2 p_start = { 20.0f, 20.0f };
    float rel_line_y = 20.0f - 40.0f;

    ImVec2 p_elbow = { 20.0f + 60.0f, rel_line_y };
    ImVec2 p_end = { 20.0f + 64.0f + m_current_line_len, rel_line_y };

    float dist_1 = std::sqrt(std::pow(p_elbow.x - p_start.x, 2) + std::pow(p_elbow.y - p_start.y, 2));
    float dist_2 = std::sqrt(std::pow(p_end.x - p_elbow.x, 2) + std::pow(p_end.y - p_elbow.y, 2));
    float total_dist = dist_1 + dist_2;

    float current_dist_on_path = total_dist * line_prog;
    ImVec2 icon_current_center = p_start;

    if (current_dist_on_path <= dist_1)
    {
        float t_path = (dist_1 > 0.001f) ? (current_dist_on_path / dist_1) : 0.0f;
        icon_current_center.x = p_start.x + (p_elbow.x - p_start.x) * t_path;
        icon_current_center.y = p_start.y + (p_elbow.y - p_start.y) * t_path;
    }
    else
    {
        float t_path = (dist_2 > 0.001f) ? ((current_dist_on_path - dist_1) / dist_2) : 0.0f;
        icon_current_center.x = p_elbow.x + (p_end.x - p_elbow.x) * t_path;
        icon_current_center.y = p_elbow.y + (p_end.y - p_elbow.y) * t_path;
    }
    icon_current_center = TechUtils::Snap(icon_current_center);
    // ... (计算 icon_current_center 的逻辑保持不变) ...

    // 3. [从旧 Update 迁移] 更新背景面板的位置和大小
    if (m_bg_panel)
    {
        float base_size = 40.0f;
        float current_scale = 0.0f;

        if (m_anim_progress < 0.2f)
        {
            float t_sub = (m_anim_progress) / 0.2f;
            current_scale = 1.0f - (0.4f * AnimationUtils::Ease(t_sub, EasingType::EaseInOutQuad));
        }
        else if (m_anim_progress < 0.8f)
        {
            current_scale = 0.6f;
        }
        else
        {
            float t_sub = (m_anim_progress - 0.8f) / 0.2f;
            current_scale = 0.6f + (0.4f * AnimationUtils::Ease(t_sub, EasingType::EaseInOutQuad));
        }

        current_box_size = base_size * current_scale;

        m_bg_panel->m_visible = true;
        m_bg_panel->m_alpha = 1.0f;

        Rect bg_rect;
        bg_rect.w = current_box_size;
        bg_rect.h = current_box_size;
        bg_rect.x = icon_current_center.x - bg_rect.w * 0.5f;
        bg_rect.y = icon_current_center.y - bg_rect.h * 0.5f;
        m_bg_panel->Arrange(bg_rect); // [修改] 调用子元素的 Arrange
    }


    // 4. [从旧 Update 迁移] 更新图标的位置和大小
    // 图标和背景面板位置大小完全一样
    if (m_bg_panel)
    {
        if (m_text_icon) m_text_icon->Arrange(m_bg_panel->m_rect);
        if (m_image_icon) m_image_icon->Arrange(m_bg_panel->m_rect);
    }

    // 5. [从旧 Update 迁移] 下方文本位置更新
    const float LAYOUT_TEXT_START_X = 64.0f;
    float rel_text_x = pulsar_center_offset.x + LAYOUT_TEXT_START_X;

    // 状态文本
    if (m_text_status)
    {
        Rect status_rect = { rel_text_x, rel_line_y - 18.0f, m_text_status->m_desired_size.x, m_text_status->m_desired_size.y };
        m_text_status->Arrange(status_rect);
    }

    // 主标签
    if (m_text_label)
    {
        Rect label_rect = { rel_text_x, rel_line_y, m_text_label->m_desired_size.x, m_text_label->m_desired_size.y };
        m_text_label->Arrange(label_rect);
    }

    // 统计信息行
    if (m_text_stat_label)
    {
        float stat_y = rel_line_y + 26.0f;
        Rect stat_label_rect = { rel_text_x, stat_y, m_text_stat_label->m_desired_size.x, m_text_stat_label->m_desired_size.y };
        m_text_stat_label->Arrange(stat_label_rect);

        float val_x = rel_text_x + m_text_stat_label->m_desired_size.x + 5.0f;

        if (m_input_field)
        {
            Rect input_rect = { val_x, stat_y - 0.0f, 100.0f, m_input_field->m_desired_size.y };
            m_input_field->Arrange(input_rect);
        }
        if (m_text_stat_value)
        {
            Rect value_rect = { val_x, stat_y, m_text_stat_value->m_desired_size.x, m_text_stat_value->m_desired_size.y };
            m_text_stat_value->Arrange(value_rect);
        }
        if (m_text_stat_unit)
        {
            float val_w = 0.0f;
            if (m_is_editable) val_w = 100.0f;
            else if (m_text_stat_value) val_w = m_text_stat_value->m_desired_size.x;

            Rect unit_rect = { val_x + val_w + 5.0f, stat_y, m_text_stat_unit->m_desired_size.x, m_text_stat_unit->m_desired_size.y };
            m_text_stat_unit->Arrange(unit_rect);
        }
    }
}
void PulsarButton::Draw(ImDrawList* draw_list)
{
    if (!m_visible) return;

    const auto& theme = UIContext::Get().m_theme;

    // 2. 绘制展开后的图形 (连接线、脉冲星)
    if (m_anim_progress > 0.01f)
    {
        ImVec2 center_abs = { m_absolute_pos.x + 20.0f, m_absolute_pos.y + 20.0f };
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
            float hover_shrink_mult = 1.0f - (m_core_hover_progress * ray.shrink_factor);

            float current_len = std::max(0.0f, pulsar_scale * ray.len * hover_shrink_mult);

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

            ImVec2 p1 = TechUtils::Snap(center_abs);
            ImVec2 p2 = TechUtils::Snap({ center_abs.x + 60.0f, line_abs_y });
            ImVec2 p3 = TechUtils::Snap({ text_abs_x + m_current_line_len, line_abs_y });

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
                ImVec2 end_p = { p2.x + std::max(d2.x * (rem_len / l2) - 2.0f - current_box_size * 0.5f,0.0f), p2.y + d2.y * (rem_len / l2) };
                draw_list->AddLine(p2, end_p, GetColorWithAlpha(theme.color_accent, 0.7f));
            }
        }
    }

    // --- [核心修改] 手动绘制子元素以应用局部裁剪 ---
    // 为了防止文字超出当前的 m_current_line_len，我们对 Status 和 Label 应用裁剪。
    // 裁剪区域：X 轴从文字起点开始，最大宽度为当前装饰线长度。


    // 垂直方向覆盖足够的范围即可
    ImVec2 clip_min = { -10.0f, m_absolute_pos.y - 100.0f };
    ImVec2 clip_max = { m_bg_panel->m_rect.x+ m_absolute_pos.x-9.0f, m_absolute_pos.y + 100.0f };

    bool font_pushed = false;
    ImFont* font = GetFont();
    if (font)
    {
        ImGui::PushFont(font);
        font_pushed = true;
    }

    for (auto& child : m_children)
    {
        // 仅对 Label 和 Status 应用裁剪，防止其向右侧侵入未延伸到的区域
        bool need_clip = (child == m_text_status || child == m_text_label);

        if (need_clip)
        {
            draw_list->PushClipRect(clip_min, clip_max, true);
        }

        child->Draw(draw_list);

        if (need_clip)
        {
            draw_list->PopClipRect();
        }
    }

    if (font_pushed)
    {
        ImGui::PopFont();
    }
}

void PulsarButton::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    // 1. 子元素优先 (例如 InputField)
    for (size_t i = m_children.size(); i > 0; --i)
    {
        m_children[i - 1]->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);
    }

    if (handled)
    {
        m_hovered = false;
        m_core_hovered = false;
        m_label_hovered = false;
        return;
    }

    // --- 2. 准备热区数据 (使用原先的设计) ---

    // A. 核心区域 (Core)
    const float closed_w = 40.0f;
    const float closed_h = 40.0f;
    const float open_w = pulsar_radius;
    const float open_h = pulsar_radius;

    float current_w = closed_w + (open_w - closed_w) * m_anim_progress;
    float current_h = closed_h + (open_h - closed_h) * m_anim_progress;
    ImVec2 center = { m_absolute_pos.x + 20.0f, m_absolute_pos.y + 20.0f };
    Rect core_hit_rect = { center.x - current_w * 0.5f, center.y - current_h * 0.5f, current_w, current_h };

    // B. 操作区域 (Action Area) - 分别计算 Icon 和 Label 的矩形
    Rect icon_abs_rect = { 0,0,0,0 };
    Rect label_abs_rect = { 0,0,0,0 };
    bool action_area_active = (m_is_active && m_anim_progress > 0.8f);

    if (action_area_active)
    {
        // 计算 Icon 矩形
        if (m_bg_panel)
        {
            icon_abs_rect = { m_bg_panel->m_absolute_pos.x, m_bg_panel->m_absolute_pos.y, m_bg_panel->m_rect.w, m_bg_panel->m_rect.h };
        }

        // 计算 Label 矩形 (保留原有的 Padding 逻辑)
        if (m_text_label)
        {
            ImFont* font = UIContext::Get().m_font_bold;
            ImVec2 txt_sz = { 100, 20 };
            if (font) txt_sz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, m_text_label->m_current_display_text.c_str());

            label_abs_rect = { m_text_label->m_absolute_pos.x - 5.0f, m_text_label->m_absolute_pos.y - 5.0f, txt_sz.x + 10.0f, txt_sz.y + 10.0f };
        }
    }

    // 定义检测是否在 Action 区域内的 Helper Lambda
    // 逻辑：只要在 Icon 内 或者 在 Label 内，就算命中
    auto IsInActionArea = [&](const ImVec2& p) -> bool
    {
        if (!action_area_active) return false;
        return icon_abs_rect.Contains(p) || label_abs_rect.Contains(p);
    };

    // --- 3. 状态判定逻辑 ---

    UIContext& ctx = UIContext::Get();
    bool is_captured_by_me = (ctx.m_captured_element == this);

    // [逻辑分支 A]：已被捕获 (正在拖拽/按住中)
    if (is_captured_by_me)
    {
        handled = true; // 独占输入

        // 重置悬停状态，重新计算
        m_core_hovered = false;
        m_label_hovered = false;
        m_hovered = false;

        bool is_inside_target = false;

        // 根据之前锁定的目标，只判定该目标是否悬停
        if (m_interaction_target == InteractionTarget::Core)
        {
            if (core_hit_rect.Contains(mouse_pos))
            {
                m_core_hovered = true;
                m_hovered = true;
                is_inside_target = true;
            }
        }
        else if (m_interaction_target == InteractionTarget::Action)
        {
            // 使用 Helper 检查是否在 Icon 或 Label 内
            if (IsInActionArea(mouse_pos))
            {
                m_label_hovered = true;
                m_hovered = true;
                is_inside_target = true;
            }
        }

        // 释放处理
        if (mouse_released)
        {
            if (is_inside_target)
            {
                // 触发逻辑
                if (m_interaction_target == InteractionTarget::Core)
                {
                    if (on_toggle_callback) on_toggle_callback(!m_is_active);
                    else SetActive(!m_is_active);
                }
                else if (m_interaction_target == InteractionTarget::Action && m_can_execute)
                {
                    if (on_execute_callback)
                    {
                        std::string val = "";
                        if (m_input_field && m_input_field->m_target_string) val = *m_input_field->m_target_string;
                        on_execute_callback(GetName(), val);
                    }
                }
            }

            // 复位状态
            ctx.ReleaseCapture();
            m_interaction_target = InteractionTarget::None;
            m_clicked = false;
        }
        return;
    }

    // [逻辑分支 B]：未被捕获 (常规悬停与点击检测)

    bool hit_core = core_hit_rect.Contains(mouse_pos);
    bool hit_action = IsInActionArea(mouse_pos); // 使用 Helper

    // 更新悬停状态 (视觉反馈)
    m_core_hovered = hit_core;
    m_label_hovered = hit_action && m_can_execute;

    if (hit_core || hit_action)
    {
        m_hovered = true;

        if (m_block_input)
        {
            handled = true;

            if (mouse_clicked)
            {
                m_clicked = true;
                // 确定交互目标并捕获
                if (hit_core)
                {
                    m_interaction_target = InteractionTarget::Core;
                    ctx.SetCapture(this);
                }
                else if (hit_action)
                {
                    m_interaction_target = InteractionTarget::Action;
                    ctx.SetCapture(this);
                }
            }
        }
    }
    else
    {
        m_hovered = false;
        m_core_hovered = false;
        m_label_hovered = false;
    }

    if (mouse_released) m_clicked = false;
}

_UI_END
_SYSTEM_END
_NPGS_END