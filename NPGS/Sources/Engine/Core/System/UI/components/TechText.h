// --- START OF FILE TechText.h ---

#pragma once
#include "../ui_framework.h"
#include "HackerTextHelper.h"
#include <string>
#include <optional>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// [MODIFIED] 动画模式枚举保持不变
enum class TechTextAnimMode
{
    None,
    Hacker,
    Scroll
};

class TechText : public UIElement
{
public:
    // --- 视觉属性 ---
    StyleColor m_color;
    bool m_use_glow = false;
    StyleColor  m_glow_color;
    float m_glow_intensity = 1.0f;
    float m_glow_spread = 1.0f;

    // --- 内容与状态 ---
    std::string m_source_key_or_text;
    std::string m_current_display_text;
    uint32_t m_local_i18n_version = 0;

    // --- 动画状态 ---
    TechTextAnimMode m_anim_mode = TechTextAnimMode::None;
    HackerTextHelper m_hacker_effect;
    std::string m_old_text;
    float m_scroll_progress = 1.0f;
    float m_scroll_duration = 0.3f;

    // --- [MODIFIED] 构造函数简化 ---
    TechText(const std::string& text_or_key, const StyleColor& color = ThemeColorID::Text);

    // --- [MODIFIED] 核心生命周期方法 ---
    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* dl) override;
    ImVec2 Measure(const ImVec2& available_size) override;

    // --- [MODIFIED] API (链式调用) ---
    void SetSourceText(const std::string& key_or_text);
    TechText* SetColor(const StyleColor& col) { m_color = col; return this; }
    TechText* SetAnimMode(TechTextAnimMode mode);
    TechText* SetGlow(bool enable, const StyleColor& color = ThemeColorID::None, float intensity = 1.0f,float spread = 2.0f);
    void RestartEffect();

private:
    // 内部绘制辅助，用于复用发光和对齐逻辑
    void DrawTextContent(ImDrawList* dl, const std::string& text, float offset_y, float alpha_mult);
};

_UI_END
_SYSTEM_END
_NPGS_END