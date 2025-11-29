#pragma once
#include "../ui_framework.h"
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
    std::string m_label;
    T* m_target_value;
    bool m_is_dragging = false;
    bool m_is_rgb = false;

    BaseTechSlider(const std::string& label, T* binding)
        : m_label(label), m_target_value(binding)
    {
        // 增加默认高度，以便容纳两行文字 (如果换行的话)
        m_rect.h = 32.0f;
        m_block_input = true;
        if (m_label == "R" || m_label == "G" || m_label == "B") m_is_rgb = true;
    }

    // 子类实现：当前归一化位置 [0, 1]
    virtual float GetNormalizedVisualPos() const = 0;

    // 子类实现：视觉中性点/原点 [0, 1] (线性=0.0, 油门=0.5)
    virtual float GetNeutralVisualPos() const { return 0.0f; }

    // 子类实现：拖拽逻辑
    virtual void OnDrag(float rel_x) = 0;

    // 通用的鼠标事件处理
    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled) override
    {
        // 如果已经被外部遮挡，且不在拖拽状态，直接放弃
        // 注意：如果正在 m_is_dragging，即使外部 handled=true 我们也要继续处理（比如鼠标移出了窗口）
        if (handled && !m_is_dragging)
        {
            m_hovered = false;
            return;
        }

        // 布局参数 (必须与 Draw 严格一致)
        float max_label_w = 70.0f;
        float value_box_w = 90.0f;
        float padding = 8.0f;

        float slider_start_x = m_absolute_pos.x + max_label_w + padding;
        float slider_w = m_rect.w - max_label_w - value_box_w - (padding * 2);

        // 简单的布局防崩坏逻辑
        if (slider_w < 10.0f) { slider_w = 10.0f; slider_start_x = m_absolute_pos.x + m_rect.w - slider_w; }

        // 判定鼠标是否在滑条的有效可点击区域内
        bool mouse_in_slider = (mouse_pos.x >= slider_start_x - 5.0f && mouse_pos.x <= slider_start_x + slider_w + 5.0f &&
            mouse_pos.y >= m_absolute_pos.y && mouse_pos.y <= m_absolute_pos.y + m_rect.h);

        // 1. 开始拖拽逻辑
        // 只有当事件未被处理 (!handled) 时才允许发起新的拖拽
        if (!m_is_dragging && !handled && mouse_clicked && mouse_in_slider)
        {
            m_is_dragging = true;
            CaptureMouse();
            handled = true; // [关键] 标记事件被消耗，开始拖拽
        }

        // 2. 结束拖拽逻辑
        if (m_is_dragging && mouse_released)
        {
            m_is_dragging = false;
            ReleaseMouse();
            handled = true; // 释放动作本身也是一种交互，应该被标记处理
        }

        // 3. 拖拽过程逻辑
        if (m_is_dragging)
        {
            float rel_x = (mouse_pos.x - slider_start_x) / slider_w;
            OnDrag(std::clamp(rel_x, 0.0f, 1.0f));
            handled = true; // [关键] 拖拽过程中始终消耗事件
            return; // 拖拽时不需要再走基类的通用悬停逻辑
        }

        // 4. 如果没有发生拖拽相关的交互，调用基类处理通用悬停 (Hover)
        // 这里的 handled 传进去，如果上面逻辑都没触发，handled 依然是 false (或外部传入的状态)
        UIElement::HandleMouseEvent(mouse_pos, mouse_down, mouse_clicked, mouse_released, handled);
    }

    // [视觉升级] 重写 Draw 方法
    void Draw(ImDrawList* dl) override
    {
        if (!m_visible || m_alpha <= 0.01f) return;
        ImFont* font = GetFont();
        if (font) ImGui::PushFont(font);

        const auto& theme = UIContext::Get().m_theme;

        // --- 1. 颜色配置 ---
        ImVec4 accent_vec4 = theme.color_accent;
        if (m_is_rgb)
        {
            if (m_label == "R") accent_vec4 = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            else if (m_label == "G") accent_vec4 = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
            else if (m_label == "B") accent_vec4 = ImVec4(0.3f, 0.5f, 1.0f, 1.0f);
        }

        ImU32 col_text = GetColorWithAlpha(theme.color_text, 1.0f);
        ImU32 col_accent = GetColorWithAlpha(accent_vec4, 1.0f);
        ImU32 col_accent_dim = GetColorWithAlpha(accent_vec4, 0.4f);

        // 基础滑轨背景 (半透明可见)
        ImVec4 track_bg_vec = theme.color_border;
        track_bg_vec.w = 0.3f;
        ImU32 col_track_bg = GetColorWithAlpha(track_bg_vec, 1.0f);

        // --- 2. 布局计算 ---
        float h = m_rect.h;
        float y_center = m_absolute_pos.y + h * 0.5f;

        float max_label_w = 100.0f;
        float value_box_w = 70.0f; // 宽度增加以适应 1.2345e+02
        float padding = 8.0f;

        float track_x = m_absolute_pos.x + max_label_w + padding;
        float track_w = m_rect.w - max_label_w - value_box_w - (padding * 2);

        if (track_w < 10.0f) track_w = 10.0f;

        // --- 3. 绘制 Label (手动计算换行) ---
        // 修复：不使用 ImGui::Text，而是直接计算分割点并用 dl->AddText 绘制
        {
            const char* text_begin = m_label.c_str();
            const char* text_end = text_begin + m_label.size();
            float font_size = font ? font->FontSize : 13.0f;
            float line_height = font_size; // 行高

            // 计算文字总尺寸（含换行）以便垂直居中
            ImVec2 total_size = ImGui::CalcTextSize(text_begin, text_end, false, max_label_w);
            float text_start_y = y_center - total_size.y * 0.5f;

            const char* line_ptr = text_begin;
            float current_y = text_start_y;

            // 循环绘制每一行
            while (line_ptr < text_end)
            {
                // 计算当前行在 max_label_w 宽度下应该在哪里断开
                const char* next_line_ptr = font->CalcWordWrapPositionA(1.0f, line_ptr, text_end, max_label_w);

                // 如果一行都放不下（next == line），强制进一个字符防止死循环
                if (next_line_ptr == line_ptr) next_line_ptr++;

                dl->AddText(ImVec2(m_absolute_pos.x, current_y), col_text, line_ptr, next_line_ptr);

                line_ptr = next_line_ptr;
                // 跳过空格/换行符（简单的处理）
                while (line_ptr < text_end && (*line_ptr == ' ' || *line_ptr == '\n')) line_ptr++;

                current_y += line_height;
            }
        }

        // --- 4. 绘制轨道 (Track) ---
        float track_h = 1.0f; // 极细
        float track_y = y_center - track_h * 0.5f;

        // 轨道背景：不再使用灰色，使用主题色但极低透明度 (15%)
        ImVec4 track_bg_col = accent_vec4;
        track_bg_col.w = 0.5f;

        // 绘制轨道背景
        dl->AddRectFilled(
            ImVec2(track_x, track_y),
            ImVec2(track_x + track_w, track_y + track_h),
            GetColorWithAlpha(track_bg_col, 1.0f)
        );

        // --- 5. 绘制高亮填充 (Fill) ---
        float t = GetNormalizedVisualPos();       // 当前位置 0~1
       // float t_neutral = GetNeutralVisualPos();  // 中性点 (0.0 或 0.5)
       //
       // // 计算填充范围 (从中心向目标延伸)
       // float t_min = std::min(t, t_neutral);
       // float t_max = std::max(t, t_neutral);
       //
       // float fill_start_x = track_x + track_w * t_min;
       // float fill_end_x = track_x + track_w * t_max;
       //
       // // 只有当偏离中性点时才绘制
       // if (t_max - t_min > 0.001f)
       // {
       //     dl->AddRectFilled(
       //         ImVec2(fill_start_x, track_y),
       //         ImVec2(fill_end_x, track_y + track_h),
       //         col_accent_dim,
       //         2.0f
       //     );
       // }

        // --- 6. 绘制游标 (Grabber) ---
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

        // 光晕
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

        // --- 7. 绘制数值 (科学计数法) ---
        char buf[64];
        // 强制转换为 float 使用 %.4e (保留4位小数的科学计数法)
        snprintf(buf, 64, "%.2e", (float)*m_target_value);

        ImVec2 val_size = ImGui::CalcTextSize(buf);
        // 右对齐
        float val_x = m_absolute_pos.x + m_rect.w - val_size.x;

        dl->AddText(ImVec2(val_x, y_center - val_size.y * 0.5f), col_text, buf);

        if (font) ImGui::PopFont();
    }
};

// ==========================================
// 1. 线性滑块 (Linear Slider)
// ==========================================
template<typename T>
class LinearTechSlider : public BaseTechSlider<T>
{
public:
    T m_min, m_max;

    LinearTechSlider(const std::string& label, T* binding, T min_val, T max_val)
        : BaseTechSlider<T>(label, binding), m_min(min_val), m_max(max_val) {}

    float GetNormalizedVisualPos() const override
    {
        float val = static_cast<float>(*this->m_target_value);
        float min_f = static_cast<float>(m_min);
        float max_f = static_cast<float>(m_max);
        if (std::abs(max_f - min_f) < 0.0001f) return 0.0f;
        return std::clamp((val - min_f) / (max_f - min_f), 0.0f, 1.0f);
    }

    // 线性滑块从中点 0.0 (左侧) 开始填充
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

// ==========================================
// 2. 油门滑块 (Throttle Slider)
// ==========================================
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

    // 油门滑块从 0.5 (中间) 开始填充
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
                m_throttle_val *= 0.85f; // 回弹速度
                if (std::abs(m_throttle_val) < 0.001f) m_throttle_val = 0.0f;
            }

            // [关键修改]：松手后（非拖拽状态），只更新视觉位置(m_throttle_val)，
            // 不再更新绑定的目标数值，直接返回。
            return;
        }


        if (std::abs(m_throttle_val) < 0.001f)
        {
            m_accumulator = 0.0;
            return;
        }

        double val_d = static_cast<double>(*this->m_target_value);
        double a_d = static_cast<double>(m_feature_a);
        double speed_mult = 1.0+pow(m_throttle_val,2.0);

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