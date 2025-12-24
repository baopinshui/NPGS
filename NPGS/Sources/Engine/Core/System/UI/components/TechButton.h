
#pragma once
#include "../ui_framework.h"
#include "TechText.h"
#include <string>
#include <functional>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class TechButton : public UIElement
{
public:
    enum class Style { Normal, Tab, Vertical, Invisible };


    bool m_selected = false;
    bool m_use_glass = false;
    float m_anim_speed = 5.0f;
    std::string m_source_text;

    // [新增] 内边距，用于计算尺寸
    ImVec2 m_padding = { 0.0f, 0.0f };

    StyleColor m_color_bg;
    StyleColor m_color_bg_hover;
    StyleColor m_color_text;
    StyleColor m_color_text_hover;
    StyleColor m_color_border;
    StyleColor m_color_border_hover;
    StyleColor m_color_selected_bg;
    StyleColor m_color_selected_text;

public:
    TechButton(const std::string& key_or_text, Style style = Style::Normal);

    void SetSourceText(const std::string& key_or_text, bool with_effect = false);
    TechButton* SetFont(ImFont* font);
    TechButton* SetSelected(bool v) { m_selected = v; return this; }
    TechButton* SetSpeed(float v) { m_anim_speed = v; return this; }
    TechButton* SetUseGlass(bool v) { m_use_glass = v; return this; }
    TechButton* SetPadding(const ImVec2& padding) { m_padding = padding; return this; }

    // --- [核心修改] 重写新的生命周期函数 ---
    void Update(float dt) override;
    ImVec2 Measure(ImVec2 available_size) override;
    // Arrange 使用基类的实现，因为它会正确地居中我们的 m_label_component
    void Draw(ImDrawList* dl) override;

private:
    std::string m_current_display_text; // 仅用于 Vertical 模式
    Style m_style;
    float m_hover_progress = 0.0f;
    std::shared_ptr<TechText> m_label_component;
    void DrawVerticalText(ImDrawList* dl, ImU32 col);
};

_UI_END
_SYSTEM_END
_NPGS_END