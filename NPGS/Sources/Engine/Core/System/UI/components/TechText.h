// --- START OF FILE TechText.h --- (修改部分)

#pragma once
#include "../ui_framework.h"
#include "HackerTextHelper.h"
#include <string>
#include <optional>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// [新增] 动画模式枚举
enum class TechTextAnimMode
{
    None,
    Hacker, // 原有的乱码特效
    Scroll  // [新增] 滚动淡入淡出特效
};

class TechText : public UIElement
{
public:
    std::string m_text; // 当前/目标文字
    std::optional<ImVec4> m_color_override;

    // Hacker 特效相关
    HackerTextHelper m_hacker_effect;

    // [修改] 替换原来的 bool m_use_effect
    TechTextAnimMode m_anim_mode = TechTextAnimMode::None;

    // [新增] Scroll 特效相关状态
    std::string m_old_text;       // 正在消失的旧文本
    float m_scroll_progress = 1.0f; // 0.0(开始切换) -> 1.0(完成)
    float m_scroll_duration = 0.3f; // 动画时长

    // 荧光相关
    bool m_use_glow = false;
    std::optional<ImVec4> m_glow_color;
    float m_glow_intensity = 1.0f;
    float m_glow_spread = 1.0f;

    // 构造函数更新
    TechText(const std::string& text,
        const std::optional<ImVec4>& color = std::nullopt,
        bool use_hacker_effect = false, // 兼容旧代码，true则设为 Hacker 模式
        bool use_glow = false,
        const std::optional<ImVec4>& glow_color = std::nullopt);

    // 设置模式的辅助函数
    TechText* SetAnimMode(TechTextAnimMode mode);

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

private:
    // [新增] 内部绘制辅助，用于复用发光和对齐逻辑
    void DrawTextContent(ImDrawList* dl, const std::string& text, float offset_y, float alpha_mult);
};

_UI_END
_SYSTEM_END
_NPGS_END