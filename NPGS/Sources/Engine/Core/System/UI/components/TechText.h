// --- START OF FILE TechText.h --- (修改后)

#pragma once
#include "../ui_framework.h"
#include "HackerTextHelper.h"
#include <string>
#include <optional>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

enum class TechTextAnimMode { None, Hacker, Scroll };

enum class TechTextSizingMode
{
    Fixed,
    AutoWidthHeight,
    AutoHeight
};

class TechText : public UIElement
{
public:
    StyleColor m_color;

    std::string m_source_key_or_text;
    std::string m_current_display_text;
    uint32_t m_local_i18n_version = 0;

    HackerTextHelper m_hacker_effect;
    TechTextAnimMode m_anim_mode = TechTextAnimMode::None;

    std::string m_old_text;
    float m_scroll_progress = 1.0f;
    float m_scroll_duration = 0.3f;

    bool m_use_glow = false;
    StyleColor m_glow_color;
    float m_glow_intensity = 1.0f;
    float m_glow_spread = 1.0f;

    TechTextSizingMode m_sizing_mode = TechTextSizingMode::AutoWidthHeight;

    TechText(const std::string& text_or_key,
        const StyleColor& color = ThemeColorID::Text,
        bool use_hacker_effect = false,
        bool use_glow = false,
        const StyleColor& glow_color = ThemeColorID::Accent);

    // --- API (保持不变) ---
    TechText* SetAnimMode(TechTextAnimMode mode);
    void SetSourceText(const std::string& key_or_text);
    TechText* SetColor(const StyleColor& col) { m_color = col; return this; }
    TechText* SetSizing(TechTextSizingMode mode)
    {
        m_sizing_mode = mode;
        if (mode == TechTextSizingMode::Fixed)
        {
            // 当用户要求 Fixed 时，我们不知道具体值，可以保留旧值或设为0
            // 如果 m_width/m_height 之前不是 Fixed，需要给一个默认值
            if (!m_width.IsFixed()) m_width = Length::Fixed(100.0f);
            if (!m_height.IsFixed()) m_height = Length::Fixed(20.0f);
        }
        else
        {
            // 当用户要求自适应时，将 Length 类型设为 Content
            m_width = Length::Content();
            m_height = Length::Content();
        }
        return this;
    }

    TechText* SetGlow(bool enable, const StyleColor& color = ThemeColorID::None, float spread = 2.0f);
    void RestartEffect();

    // --- [核心修改] 重写新的生命周期函数 ---
    void Update(float dt) override; // 不再接收 parent_abs_pos
    ImVec2 Measure(ImVec2 available_size) override;
    // Arrange 使用基类实现即可，除非有特殊需求
    void Draw(ImDrawList* dl) override;

private:
    // RecomputeSize 被 Measure 替代，可以移除
    // void RecomputeSize(); 
    void DrawTextContent(ImDrawList* dl, const std::string& text, float offset_y, float alpha_mult);
};

_UI_END
_SYSTEM_END
_NPGS_END