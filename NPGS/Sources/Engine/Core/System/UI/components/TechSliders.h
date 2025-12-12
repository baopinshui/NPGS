// --- START OF FILE TechSliders.h ---

#pragma once
#include "../ui_framework.h"
#include "InputField.h"
#include "TechText.h"
#include "SliderTrack.h" // 需确保此前已创建 SliderTrack.h/cpp
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <vector>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// =================================================================================
// BaseTechSlider: 基础滑块容器 (HBox 布局)
// =================================================================================
template<typename T>
class BaseTechSlider : public HBox
{
public:
    // --- 子组件引用 ---
    std::shared_ptr<TechText> m_label_component;
    std::shared_ptr<SliderTrack> m_track_component;
    std::shared_ptr<InputField> m_value_input;

    // --- 数据绑定与状态 ---
    T* m_target_value;
    std::string m_value_string_buffer;
    bool m_last_input_focused = false;
    std::string m_i1n_key;

    // --- 辅助工具: 科学计数法解析 ---
    bool TryParseScientific(const std::string& s, double& result)
    {
        if (s.empty()) return false;
        std::string temp = s;
        size_t pos;
        if ((pos = temp.find("*10^")) != std::string::npos) temp.replace(pos, 4, "e");
        else if ((pos = temp.find("x10^")) != std::string::npos) temp.replace(pos, 4, "e");

        try
        {
            result = std::stod(temp);
            return true;
        }
        catch (...) { return false; }
    }

public:
    BaseTechSlider(const std::string& key, T* binding)
        : m_target_value(binding), m_i1n_key(key)
    {
        // 1. 配置 HBox 布局策略
        SetWidth(Length::Fill());       // 宽度填满父容器
        SetHeight(Length::Content());   // 高度由子元素决定
        m_padding = 8.0f;               // 子元素间距

        // 2. 创建并配置 Label
        m_label_component = std::make_shared<TechText>(key);
        // 标签固定宽 100，高度填满以垂直居中
        m_label_component->SetWidth(Length::Fix(100.0f))
            ->SetHeight(Length::Fill())
            ->SetAlignment(Alignment::Start, Alignment::Center);

        // 3. 创建并配置 Track
        m_track_component = std::make_shared<SliderTrack>();
        // 轨道宽度填满 HBox 剩余空间，高度固定
        m_track_component->SetWidth(Length::Fill())
            ->SetHeight(Length::Fix(16.0f))
            ->SetAlignment(Alignment::Center, Alignment::Center);

        // 4. 创建并配置 InputField
        m_value_input = std::make_shared<InputField>(&m_value_string_buffer);
        // 输入框固定宽 70，高度固定
        m_value_input->SetWidth(Length::Fix(70.0f))
            ->SetHeight(Length::Fix(16.0f))
            ->SetAlignment(Alignment::End, Alignment::Center);
        m_value_input->m_underline_mode = UnderlineDisplayMode::OnHoverOrFocus;

        // 5. 组装组件
        AddChild(m_label_component);
        AddChild(m_track_component);
        AddChild(m_value_input);

        // 6. 绑定输入框提交事件 (通用逻辑)
        m_value_input->on_commit = [this](const std::string& input)
        {
            double parsed_value;
            if (TryParseScientific(input, parsed_value))
            {
                this->SetValueFromParsedInput(parsed_value);
                // 提交后强制同步一次UI，防止非法输入
                this->SyncTrackFromValue();
                this->SyncInputFromValue();
            }
        };
    }

    void Update(float dt, const ImVec2& parent_abs_pos) override
    {
        // 1. 同步逻辑 (由子类决定是否需要每帧同步轨道)
        UpdateSyncLogic(dt);

        // 2. 特殊视觉处理 (RGB 颜色检测)
        // 这里的逻辑保持原样，检测 key 是否为 R/G/B 来改变轨道颜色
        std::string current_label = TR(m_i1n_key); // 获取翻译后的文本
        bool is_rgb = (current_label == "R" || current_label == "G" || current_label == "B");
        m_track_component->m_is_rgb_mode = is_rgb;
        if (is_rgb)
        {
            if (current_label == "R") m_track_component->m_grab_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            else if (current_label == "G") m_track_component->m_grab_color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
            else if (current_label == "B") m_track_component->m_grab_color = ImVec4(0.3f, 0.5f, 1.0f, 1.0f);
        }
        else
        {
            m_track_component->m_grab_color = ThemeColorID::Accent;
        }

        // 3. 执行 HBox 布局
        HBox::Update(dt, parent_abs_pos);
    }

    // --- 虚接口：供子类实现特定的数值逻辑 ---
    virtual void SetValueFromParsedInput(double parsed_value) = 0;
    virtual void UpdateSyncLogic(float dt) = 0; // 替代原本的 SyncUIFromValue，给予子类更多控制权
    virtual void SyncTrackFromValue() = 0;

protected:
    // 通用输入框同步逻辑
    void SyncInputFromValue()
    {
        bool current_input_focused = m_value_input->IsFocused();
        if (!current_input_focused && !m_last_input_focused)
        {
            char buf[64];
            snprintf(buf, 64, "%.2e", (double)*m_target_value);
            if (m_value_string_buffer != buf)
            {
                m_value_string_buffer = buf;
                m_value_input->m_cursor_pos = (int)m_value_string_buffer.length();
                m_value_input->ResetSelection();
            }
        }
        m_last_input_focused = current_input_focused;
    }
};

// =================================================================================
// LinearTechSlider: 线性映射滑块
// =================================================================================
template<typename T>
class LinearTechSlider : public BaseTechSlider<T>
{
public:
    T m_min, m_max;

    LinearTechSlider(const std::string& label, T* binding, T min_val, T max_val)
        : BaseTechSlider<T>(label, binding), m_min(min_val), m_max(max_val)
    {
        // 绑定轨道回调：线性滑块直接映射位置到数值
        this->m_track_component->on_value_changed = [this](float norm_val)
        {
            this->SetValueFromNormalized(norm_val);
        };
    }

    void SetValueFromParsedInput(double parsed_value) override
    {
        *this->m_target_value = std::clamp(static_cast<T>(parsed_value), m_min, m_max);
    }

    void SyncTrackFromValue() override
    {
        float range = static_cast<float>(m_max) - static_cast<float>(m_min);
        if (std::abs(range) < 1e-6)
        {
            this->m_track_component->SetValue(0.0f);
            return;
        }
        float norm = std::clamp((static_cast<float>(*this->m_target_value) - static_cast<float>(m_min)) / range, 0.0f, 1.0f);
        this->m_track_component->SetValue(norm);
    }

    void UpdateSyncLogic(float dt) override
    {
        // 线性滑块每帧都需要双向检查（为了响应外部数据变更）
        // 只有当轨道没有被拖拽时，才从数据同步到轨道
        if (UIContext::Get().m_captured_element != this->m_track_component.get())
        {
            SyncTrackFromValue();
        }
        this->SyncInputFromValue();
    }

private:
    void SetValueFromNormalized(float norm_val)
    {
        float range = static_cast<float>(m_max) - static_cast<float>(m_min);
        float new_val_f = static_cast<float>(m_min) + norm_val * range;

        if constexpr (std::is_integral_v<T>)
        {
            *this->m_target_value = static_cast<T>(std::round(new_val_f));
        }
        else
        {
            *this->m_target_value = static_cast<T>(new_val_f);
        }
        *this->m_target_value = std::clamp(*this->m_target_value, m_min, m_max);
    }
};

// =================================================================================
// ThrottleTechSlider: 油门/加速度控制滑块
// =================================================================================
template<typename T>
class ThrottleTechSlider : public BaseTechSlider<T>
{
public:
    float m_throttle_val = 0.0f; // -1.0 to 1.0
    T m_feature_a;
    double m_accumulator = 0.0;

    ThrottleTechSlider(const std::string& label, T* binding, T feature_a = 0)
        : BaseTechSlider<T>(label, binding), m_feature_a(feature_a)
    {
        // 绑定轨道回调：不直接修改 m_target_value，而是更新油门值
        this->m_track_component->on_value_changed = [this](float norm_val)
        {
            // 将 0.0~1.0 映射回 -1.0~1.0
            m_throttle_val = (norm_val - 0.5f) * 2.0f;
        };
    }

    void SetValueFromParsedInput(double parsed_value) override
    {
        *this->m_target_value = static_cast<T>(parsed_value);
    }

    void SyncTrackFromValue() override
    {
        // 油门滑块的轨道位置不代表 value，而是代表 m_throttle_val
        // (m_throttle_val + 1.0) / 2.0  -> 映射回 0.0~1.0
        float visual_pos = (m_throttle_val + 1.0f) * 0.5f;
        this->m_track_component->SetValue(visual_pos);
    }

    void UpdateSyncLogic(float dt) override
    {
        // 1. 检测是否正在拖拽轨道
        // 使用 UIContext 判断 SliderTrack 是否捕获了鼠标
        bool is_dragging = (UIContext::Get().m_captured_element == this->m_track_component.get());

        // 2. 如果未拖拽，执行阻尼归零逻辑 (完全保留原逻辑)
        if (!is_dragging)
        {
            if (std::abs(m_throttle_val) > 0.001f)
            {
                m_throttle_val *= 0.85f; // 衰减
                if (std::abs(m_throttle_val) < 0.001f) m_throttle_val = 0.0f;
            }
            // 归位时，重置累加器
            if (std::abs(m_throttle_val) < 0.001f)
            {
                m_accumulator = 0.0;
            }
            // 强制更新轨道显示位置 (回弹动画)
            SyncTrackFromValue();
        }

        // 3. 执行物理计算逻辑 (完全保留原逻辑)
        if (std::abs(m_throttle_val) > 0.001f)
        {
            double val_d = static_cast<double>(*this->m_target_value);
            double a_d = static_cast<double>(m_feature_a);

            // 非线性速度倍率
            double speed_mult = 0.1 + 2.4 * pow(m_throttle_val, 2.0);

            // 变化率计算
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

        // 4. 同步输入框显示
        this->SyncInputFromValue();
    }
};

_UI_END
_SYSTEM_END
_NPGS_END