#pragma once
#include "../ui_framework.h"
#include "HackerTextHelper.h" // 引入助手
#include <string>
#include <optional>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class TechText : public UIElement
{
public:
    std::string m_text; // 最终目标文字
    std::optional<ImVec4> m_color_override;

    // 集成 HackerTextHelper
    HackerTextHelper m_hacker_effect;
    bool m_use_effect = false; // 开关


    bool m_use_glow = false;                    // 荧光开关
    std::optional<ImVec4> m_glow_color;         // 荧光颜色 (若不设置则默认使用文字颜色)
    float m_glow_intensity = 1.0f;              // 荧光强度 (控制Alpha)
    float m_glow_spread = 2.0f;                 // 扩散范围 (像素)



    // 构造函数增加 use_effect 参数
    TechText(const std::string& text,
        const std::optional<ImVec4>& color = std::nullopt,
        bool use_hacker_effect = false,
        bool use_glow = false,
        const std::optional<ImVec4>& glow_color = std::nullopt);

    // 设置文本（支持重新触发动画）
    void SetText(const std::string& new_text);

    void RestartEffect();
    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* dl) override;

    TechText* SetColor(const ImVec4& col) { m_color_override = col; return this; }
    TechText* SetGlow(bool enable, const std::optional<ImVec4>& color = std::nullopt, float spread = 2.0f)
    {
        m_use_glow = enable;
        if (color.has_value()) m_glow_color = color;
        m_glow_spread = spread;
        return this;
    }
};

_UI_END
_SYSTEM_END
_NPGS_END