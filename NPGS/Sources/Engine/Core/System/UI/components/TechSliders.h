#pragma once
#include "../ui_framework.h"
#include "InputField.h"
#include "TechText.h" 
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <vector>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// BaseTechSlider, LinearTechSlider, ThrottleTechSlider

template<typename T>
class BaseTechSlider : public UIElement
{
public:
    std::string m_i1n_key;
private:
    std::string m_value_string_buffer;
    std::shared_ptr<InputField> m_value_input;
    std::shared_ptr<TechText> m_label_component;
    bool m_last_input_focused = false;
    bool TryParseScientific(const std::string& s, double& result);

public:
    T* m_target_value;
    bool m_is_dragging = false;
    bool m_is_rgb = false;
    float max_label_w = 100.0f;
    float value_box_w = 70.0f;
    float padding = 8.0f;

    // 缓存滑块轨道区域
    Rect m_slider_track_rect_local = { 0,0,0,0 };

    BaseTechSlider(const std::string& key, T* binding)
        : m_i1n_key(key), m_target_value(binding)
    {
        m_width = Length::Stretch();
        m_height = Length::Content();

        m_block_input = true;

        std::string initial_label = TR(m_i1n_key);
        if (initial_label == "R" || initial_label == "G" || initial_label == "B") m_is_rgb = true;

        m_label_component = std::make_shared<TechText>(m_i1n_key);
		m_label_component->SetName("label");
        m_label_component->SetSizing(TechTextSizingMode::AutoHeight);
        m_label_component->m_align_v = Alignment::Center;
        m_label_component->m_block_input = false;
        m_label_component->m_width = Length::Fixed(max_label_w);
        m_label_component->m_height = Length::Content();
        AddChild(m_label_component);

        m_value_input = std::make_shared<InputField>(&m_value_string_buffer);
		m_value_input->SetName("valueInput");
        m_value_input->m_underline_mode = UnderlineDisplayMode::OnHoverOrFocus;
        m_value_input->m_width = Length::Fixed(value_box_w);
        m_value_input->m_height = Length::Content();
        AddChild(m_value_input);

        m_value_input->on_commit = [this](const std::string& input)
        {
            double parsed_value;
            if (TryParseScientific(input, parsed_value))
            {
                this->SetValueFromParsedInput(parsed_value);
            }
        };
    }

    virtual void SetValueFromParsedInput(double parsed_value)
    {
        *this->m_target_value = static_cast<T>(parsed_value);
    }

    void Update(float dt) override;
    ImVec2 Measure(ImVec2 available_size) override;
    void Arrange(const Rect& final_rect) override;

    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled) override;
    void Draw(ImDrawList* dl) override;

    virtual float GetNormalizedVisualPos() const = 0;
    virtual float GetNeutralVisualPos() const { return 0.0f; }
    virtual void OnDrag(float rel_x) = 0;
};

// --- 实现 ---

template<typename T>
bool BaseTechSlider<T>::TryParseScientific(const std::string& s, double& result)
{
    if (s.empty()) return false;
    std::string temp = s;
    size_t pos;
    if ((pos = temp.find("*10^")) != std::string::npos)
    {
        temp.replace(pos, 4, "e");
    }
    else if ((pos = temp.find("x10^")) != std::string::npos)
    {
        temp.replace(pos, 4, "e");
    }

    try
    {
        result = std::stod(temp);
        return true;
    }
    catch (const std::invalid_argument&)
    {
        return false;
    }
    catch (const std::out_of_range&)
    {
        return false;
    }
    return false;
}

template<typename T>
void BaseTechSlider<T>::Update(float dt)
{
    if (!m_i1n_key.empty())
    {
        std::string current_label = TR(m_i1n_key);
        m_is_rgb = (current_label == "R" || current_label == "G" || current_label == "B");
    }

    if (m_value_input)
    {
        bool current_input_focused = m_value_input->IsFocused();
        if (!current_input_focused && !m_last_input_focused)
        {
            char buf[64];
            snprintf(buf, 64, "%.2e", (double)*m_target_value);
            if (m_value_string_buffer != buf)
            {
                m_value_string_buffer = buf;
                m_value_input->m_cursor_pos = m_value_string_buffer.length();
                m_value_input->ResetSelection();
            }
        }
        m_last_input_focused = current_input_focused;
    }

    UIElement::Update(dt);
}

template<typename T>
ImVec2 BaseTechSlider<T>::Measure(ImVec2 available_size)
{
    if (!m_visible)
    {
        m_desired_size = { 0,0 };
        return m_desired_size;
    }

    if (m_label_component) m_label_component->m_font = GetFont();
    if (m_value_input) m_value_input->m_font = GetFont();

    ImVec2 label_size = m_label_component->Measure({ max_label_w, available_size.y });
    ImVec2 input_size = m_value_input->Measure(available_size);

    if (m_height.IsFixed())
    {
        m_desired_size.y = m_height.value;
    }
    else
    {
        m_desired_size.y = std::max({ label_size.y, input_size.y, 16.0f });
    }

    float fixed_parts_w = max_label_w + value_box_w + (padding * 2);

    if (m_width.IsFixed())
    {
        m_desired_size.x = m_width.value;
    }
    else if (m_width.IsContent())
    {
        float default_track_w = 100.0f;
        m_desired_size.x = fixed_parts_w + default_track_w;
    }
    else
    {
        float min_track_w = 10.0f;
        m_desired_size.x = fixed_parts_w + min_track_w;
    }

    return m_desired_size;
}

template<typename T>
void BaseTechSlider<T>::Arrange(const Rect& final_rect)
{
    UIElement::Arrange(final_rect);

    if (!m_visible) return;

    float current_x = 0;

    Rect label_rect = { current_x, 0, max_label_w, m_rect.h };
    m_label_component->Arrange(label_rect);
    current_x += max_label_w;

    current_x += padding;
    float track_w = m_rect.w - max_label_w - value_box_w - (padding * 2);
    if (track_w < 10.0f) track_w = 10.0f;
    m_slider_track_rect_local = { current_x, 0, track_w, m_rect.h };
    current_x += track_w;

    current_x += padding;
    float input_y = (m_rect.h - m_value_input->m_desired_size.y) * 0.5f;
    Rect input_rect = { current_x, input_y, value_box_w, m_value_input->m_desired_size.y };
    m_value_input->Arrange(input_rect);
}

// [修改] 恢复旧版的事件处理逻辑：优先拖拽 -> 次优先子组件 -> 最后轨道点击
template<typename T>
void BaseTechSlider<T>::HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    // --- 1. 优先处理拖拽状态 (与旧版一致) ---
    // 如果当前正在拖拽滑块，则滑块独占所有鼠标事件，不分发给子元素。
    if (m_is_dragging)
    {
        // 结合 Layout 计算绝对坐标
        Rect track_abs = {
            m_absolute_pos.x + m_slider_track_rect_local.x,
            m_absolute_pos.y, // 整个高度区域都有效
            m_slider_track_rect_local.w,
            m_rect.h
        };

        float rel_x = (mouse_pos.x - track_abs.x) / track_abs.w;
        OnDrag(std::clamp(rel_x, 0.0f, 1.0f));

        if (mouse_released)
        {
            m_is_dragging = false;
            ReleaseMouse();
        }

        handled = true;
        return;
    }

    // --- 2. 让子元素（输入框）优先处理事件 (与旧版一致) ---
    // 只有没在拖拽时，才允许子元素响应
    for (size_t i = m_children.size(); i > 0; --i)
    {
        m_children[i - 1]->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);
    }


    if (handled)
    {
        m_hovered = false;
        return;
    }

    // --- 3. 检查滑块轨道区域的交互（开始新的拖拽） (与旧版一致) ---

    Rect track_abs = {
        m_absolute_pos.x + m_slider_track_rect_local.x,
        m_absolute_pos.y,
        m_slider_track_rect_local.w,
        m_rect.h
    };

    // 宽松的点击判定
    bool mouse_in_slider = (mouse_pos.x >= track_abs.x - 5.0f &&
        mouse_pos.x <= track_abs.x + track_abs.w + 5.0f &&
        mouse_pos.y >= m_absolute_pos.y &&
        mouse_pos.y <= m_absolute_pos.y + m_rect.h);

    m_hovered = mouse_in_slider;
    if (m_hovered && !m_tooltip_key.empty())
    {
        UIContext::Get().RequestTooltip(m_tooltip_key);
    }

    if (mouse_clicked && mouse_in_slider)
    {
        m_is_dragging = true;
        CaptureMouse();
        // 立即更新一次位置
        float rel_x = (mouse_pos.x - track_abs.x) / track_abs.w;
        OnDrag(std::clamp(rel_x, 0.0f, 1.0f));
        handled = true;
    }
}

template<typename T>
void BaseTechSlider<T>::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;
    ImFont* font = GetFont();
    if (font) ImGui::PushFont(font);

    const auto& theme = UIContext::Get().m_theme;

    ImVec4 accent_vec4 = theme.color_accent;
    if (m_is_rgb)
    {
        std::string current_label = TR(m_i1n_key);
        if (current_label == "R") accent_vec4 = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        else if (current_label == "G") accent_vec4 = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
        else if (current_label == "B") accent_vec4 = ImVec4(0.3f, 0.5f, 1.0f, 1.0f);
    }

    ImU32 col_accent = GetColorWithAlpha(accent_vec4, 1.0f);

    float h = m_rect.h;
    float y_center = m_absolute_pos.y + h * 0.5f;

    // 使用 Layout 计算出的轨道区域
    float track_x = m_absolute_pos.x + m_slider_track_rect_local.x;
    float track_w = m_slider_track_rect_local.w;

    float track_h = 1.0f;
    float track_y = y_center - track_h * 0.5f;
    ImVec4 track_bg_col = accent_vec4;
    track_bg_col.w = 0.5f;
    dl->AddRectFilled(
        ImVec2(track_x, track_y),
        ImVec2(track_x + track_w, track_y + track_h),
        GetColorWithAlpha(track_bg_col, 1.0f)
    );

    float t = GetNormalizedVisualPos();
    float grab_x = track_x + track_w * t;
    float grab_w = 6.0f;
    float grab_h = 14.0f;

    if (m_is_dragging)
    {
        grab_w = 8.0f;
        grab_h = 16.0f;
        col_accent = GetColorWithAlpha(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f);
    }
    else if (m_hovered)
    {
        col_accent = GetColorWithAlpha(accent_vec4, 0.9f);
    }
    dl->AddRectFilled(
        ImVec2(grab_x - grab_w * 0.5f, y_center - grab_h * 0.5f),
        ImVec2(grab_x + grab_w * 0.5f, y_center + grab_h * 0.5f),
        col_accent,
        0.0f
    );

    if (m_is_dragging || m_hovered)
    {
        dl->AddRect(
            ImVec2(grab_x - grab_w * 0.5f - 2, y_center - grab_h * 0.5f - 2),
            ImVec2(grab_x + grab_w * 0.5f + 2, y_center + grab_h * 0.5f + 2),
            GetColorWithAlpha(accent_vec4, 0.6f),
            0.0f,
            0,
            1.0f
        );
    }

    UIElement::Draw(dl);

    if (font) ImGui::PopFont();
}

template<typename T>
class LinearTechSlider : public BaseTechSlider<T>
{
public:
    T m_min, m_max;

    LinearTechSlider(const std::string& label, T* binding, T min_val, T max_val)
        : BaseTechSlider<T>(label, binding), m_min(min_val), m_max(max_val) {}

    void SetValueFromParsedInput(double parsed_value) override
    {
        T new_val = static_cast<T>(parsed_value);
        *this->m_target_value = std::clamp(new_val, this->m_min, this->m_max);
    }

    float GetNormalizedVisualPos() const override
    {
        float val = static_cast<float>(*this->m_target_value);
        float min_f = static_cast<float>(m_min);
        float max_f = static_cast<float>(m_max);
        if (std::abs(max_f - min_f) < 0.0001f) return 0.0f;
        return std::clamp((val - min_f) / (max_f - min_f), 0.0f, 1.0f);
    }

    float GetNeutralVisualPos() const override { return 0.0f; }

    void OnDrag(float rel_x) override
    {
        float range = static_cast<float>(m_max) - static_cast<float>(m_min);
        float new_val_f = static_cast<float>(m_min) + rel_x * range;

        if constexpr (std::is_integral_v<T>)
        {
            T new_val = static_cast<T>(std::round(new_val_f));
            *this->m_target_value = std::clamp(new_val, m_min, m_max);
        }
        else
        {
            T new_val = static_cast<T>(new_val_f);
            *this->m_target_value = std::clamp(new_val, m_min, m_max);
        }
    }
};

template<typename T>
class ThrottleTechSlider : public BaseTechSlider<T>
{
public:
    float m_throttle_val = 0.0f;
    T m_feature_a;
    double m_accumulator = 0.0;

    ThrottleTechSlider(const std::string& label, T* binding, T feature_size_a = 0)
        : BaseTechSlider<T>(label, binding), m_feature_a(feature_size_a) {}

    float GetNormalizedVisualPos() const override
    {
        return (m_throttle_val + 1.0f) * 0.5f;
    }

    float GetNeutralVisualPos() const override { return 0.5f; }

    void OnDrag(float rel_x) override
    {
        m_throttle_val = (rel_x - 0.5f) * 2.0f;
    }

    void Update(float dt) override
    {
        BaseTechSlider<T>::Update(dt);

        if (!this->m_is_dragging)
        {
            if (std::abs(m_throttle_val) > 0.001f)
            {
                m_throttle_val *= 0.85f;
                if (std::abs(m_throttle_val) < 0.001f) m_throttle_val = 0.0f;
            }
            return;
        }

        if (std::abs(m_throttle_val) < 0.001f)
        {
            m_accumulator = 0.0;
            return;
        }

        double val_d = static_cast<double>(*this->m_target_value);
        double a_d = static_cast<double>(m_feature_a);
        double speed_mult = 0.1 + 2.4 * pow(m_throttle_val, 2.0);

        double change_rate = sqrt(pow(val_d, 2.0) + pow(a_d, 2.0));
        double delta = m_throttle_val * dt * speed_mult * change_rate;

        if constexpr (std::is_floating_point_v<T>)
        {
            double new_val = val_d + delta;
            if (new_val < 0.0 && std::is_unsigned_v<T>) new_val = 0.0;
            *this->m_target_value = static_cast<T>(new_val);
        }
        else
        {
            m_accumulator += delta;
            long long int_change = static_cast<long long>(m_accumulator);

            if (int_change != 0)
            {
                if constexpr (std::is_unsigned_v<T>)
                {
                    if (int_change < 0)
                    {
                        if (static_cast<long long>(*this->m_target_value) < -int_change)
                            *this->m_target_value = 0;
                        else
                            *this->m_target_value += static_cast<T>(int_change);
                    }
                    else
                    {
                        *this->m_target_value += static_cast<T>(int_change);
                    }
                }
                else
                {
                    *this->m_target_value += static_cast<T>(int_change);
                }
                m_accumulator -= int_change;
            }
        }
    }
};

_UI_END
_SYSTEM_END
_NPGS_END