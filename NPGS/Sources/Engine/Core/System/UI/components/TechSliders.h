#pragma once
#include "../ui_framework.h"
#include "InputField.h" // [新增] 引入 InputField
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
    std::string m_translated_label;
private:
    uint32_t m_local_i1n_version = 0;
    std::string m_value_string_buffer;
    std::shared_ptr<InputField> m_value_input;

    bool TryParseScientific(const std::string& s, double& result)
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

public:
    T* m_target_value;
    bool m_is_dragging = false;
    bool m_is_rgb = false;
    float max_label_w = 100.0f;
    float value_box_w = 70.0f;
    float padding = 8.0f;

    BaseTechSlider(const std::string& key, T* binding)
        : m_i1n_key(key), m_target_value(binding)
    {
        m_rect.h = 32.0f;
        m_block_input = true;
        m_translated_label = TR(m_i1n_key);
        if (m_translated_label == "R" || m_translated_label == "G" || m_translated_label == "B") m_is_rgb = true;

        m_value_input = std::make_shared<InputField>(&m_value_string_buffer);
        m_value_input->m_underline_mode = UnderlineDisplayMode::OnHoverOrFocus;
        AddChild(m_value_input);

        // [核心修正] Lambda 现在调用虚函数，而不是进行类型检查
        m_value_input->on_commit = [this](const std::string& input)
        {
            double parsed_value;
            if (TryParseScientific(input, parsed_value))
            {
                // 调用虚函数更新数值
                this->SetValueFromParsedInput(parsed_value);
            }
            // 如果解析失败（格式不对），什么都不做，下次刷新会变回旧数值
        };
    }

    // [新增] 虚函数，用于子类重写数值设置逻辑
    virtual void SetValueFromParsedInput(double parsed_value)
    {
        // 基类默认行为：直接赋值
        *this->m_target_value = static_cast<T>(parsed_value);
    }

    void Update(float dt, const ImVec2& parent_abs_pos) override
    {
        if (!m_i1n_key.empty())
        {
            // [修改] 简化
            m_translated_label = TR(m_i1n_key);
            if (m_translated_label == "R" || m_translated_label == "G" || m_translated_label == "B") m_is_rgb = true; else m_is_rgb = false;
        }
        UIElement::Update(dt, parent_abs_pos);
        if (m_value_input)
        {
            ImFont* font = GetFont();

            // [修改] 只有当输入框 **没有焦点** 时，才从底层数据同步显示文本
            // 这样当用户正在打字（有焦点）时，我们不会用底层数据覆盖用户正在输入的内容
            if (!m_value_input->IsFocused())
            {
                char buf[64];
                snprintf(buf, 64, "%.2e", (double)*m_target_value);

                // 只有当显示的文本和目标值不一致时才更新，避免每帧都在改 string
                if (m_value_string_buffer != buf)
                {
                    m_value_string_buffer = buf;
                    m_value_input->m_cursor_pos = m_value_string_buffer.length();
                    m_value_input->ResetSelection();
                }
            }
            float h_up = m_rect.h;
            float input_h = font ? font->FontSize : 13.0f;
            m_value_input->m_rect.w = value_box_w;
            m_value_input->m_rect.h = input_h;
            m_value_input->m_rect.x = m_rect.w - value_box_w;
            m_value_input->m_rect.y = (h_up - input_h) * 0.5f;
            m_value_input->m_font = font;
        }

    }

    virtual float GetNormalizedVisualPos() const = 0;
    virtual float GetNeutralVisualPos() const { return 0.0f; }
    virtual void OnDrag(float rel_x) = 0;

    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled) override
    {
        if (!m_visible || m_alpha <= 0.01f) return;

        // [核心修复] 优先处理拖拽状态
        // 如果当前正在拖拽滑块，则滑块独占所有鼠标事件，不分发给子元素。
        if (m_is_dragging)
        {
            // 拖拽过程中，更新滑块位置
            float slider_start_x = m_absolute_pos.x + max_label_w + padding;
            float slider_w = m_rect.w - max_label_w - value_box_w - (padding * 2);
            if (slider_w < 10.0f) slider_w = 10.0f;
            float rel_x = (mouse_pos.x - slider_start_x) / slider_w;
            OnDrag(std::clamp(rel_x, 0.0f, 1.0f));

            // 如果鼠标释放，则结束拖拽
            if (mouse_released)
            {
                m_is_dragging = false;
                ReleaseMouse();
            }

            handled = true; // 拖拽期间，事件被完全消耗
            return;         // 直接返回，不执行后续逻辑
        }

        // --- 如果没有在拖拽，则执行正常事件分发 ---

        // 1. 让子元素（输入框）优先处理事件
        for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
        {
            (*it)->HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);
        }

        // 如果子元素已经消耗了事件，则滑块自身不进行交互判断
        if (handled)
        {
            m_hovered = false;
            return;
        }

        // 2. 如果子元素未处理，则检查滑块轨道区域的交互（开始新的拖拽）
        float slider_start_x = m_absolute_pos.x + max_label_w + padding;
        float slider_w = m_rect.w - max_label_w - value_box_w - (padding * 2);
        if (slider_w < 10.0f) { slider_w = 10.0f; }

        bool mouse_in_slider = (mouse_pos.x >= slider_start_x - 5.0f && mouse_pos.x <= slider_start_x + slider_w + 5.0f &&
            mouse_pos.y >= m_absolute_pos.y && mouse_pos.y <= m_absolute_pos.y + m_rect.h);

        m_hovered = mouse_in_slider;
        if (m_hovered && !m_tooltip_key.empty())
        {
            UIContext::Get().RequestTooltip(m_tooltip_key);
        }

        if (mouse_clicked && mouse_in_slider)
        {
            m_is_dragging = true;
            CaptureMouse();
            handled = true;
        }
    }

    void Draw(ImDrawList* dl) override
    {
        if (!m_visible || m_alpha <= 0.01f) return;
        ImFont* font = GetFont();
        if (font) ImGui::PushFont(font);

        const auto& theme = UIContext::Get().m_theme;

        ImVec4 accent_vec4 = theme.color_accent;
        if (m_is_rgb)
        {
            if (m_translated_label == "R") accent_vec4 = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            else if (m_translated_label == "G") accent_vec4 = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
            else if (m_translated_label == "B") accent_vec4 = ImVec4(0.3f, 0.5f, 1.0f, 1.0f);
        }

        ImU32 col_text = GetColorWithAlpha(theme.color_text, 1.0f);
        ImU32 col_accent = GetColorWithAlpha(accent_vec4, 1.0f);

        float h = m_rect.h;
        float y_center = m_absolute_pos.y + h * 0.5f;
        float track_x = m_absolute_pos.x + max_label_w + padding;
        float track_w = m_rect.w - max_label_w - value_box_w - (padding * 2);
        if (track_w < 10.0f) track_w = 10.0f;

        {
            const char* text_begin = m_translated_label.c_str();
            const char* text_end = text_begin + m_translated_label.size();
            float font_size = font ? font->FontSize : 13.0f;
            float line_height = font_size;

            ImVec2 total_size = ImGui::CalcTextSize(text_begin, text_end, false, max_label_w);
            float text_start_y = y_center - total_size.y * 0.5f;

            const char* line_ptr = text_begin;
            float current_y = text_start_y;
            while (line_ptr < text_end)
            {
                const char* next_line_ptr = font->CalcWordWrapPositionA(1.0f, line_ptr, text_end, max_label_w);
                if (next_line_ptr == line_ptr) next_line_ptr++;
                dl->AddText(ImVec2(m_absolute_pos.x, current_y), col_text, line_ptr, next_line_ptr);
                line_ptr = next_line_ptr;
                while (line_ptr < text_end && (*line_ptr == ' ' || *line_ptr == '\n')) line_ptr++;
                current_y += line_height;
            }
        }

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
};

template<typename T>
class LinearTechSlider : public BaseTechSlider<T>
{
public:
    T m_min, m_max;

    LinearTechSlider(const std::string& label, T* binding, T min_val, T max_val)
        : BaseTechSlider<T>(label, binding), m_min(min_val), m_max(max_val) {}

    // [新增] 重写基类方法，加入范围限制逻辑
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

    void Update(float dt, const ImVec2& parent_abs_pos) override
    {
        BaseTechSlider<T>::Update(dt, parent_abs_pos);

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
        double speed_mult = 1.0 + pow(m_throttle_val, 2.0);

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