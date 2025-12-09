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
    enum class Style
    {
        Normal,     // [ ] 线框风格 (原 NeuralButton)
        Tab,        // 实心/透明切换风格 (原 TabButton)
        Vertical,   // 竖排旋转文字风格 (原 VerticalTextButton)
        Invisible   // 纯点击区域 (用于不需要视觉的交互)
    };

    // 回调
    std::function<void()> on_click;

    // 状态
    bool m_selected = false; // 用于 Tab 模式
    bool  m_use_glass = false;
    float m_anim_speed = 5.0f; // 动画速度
    std::string m_source_text;

    StyleColor m_color_bg;
    StyleColor m_color_bg_hover;
    StyleColor m_color_text;
    StyleColor m_color_text_hover;
    StyleColor m_color_border;
    StyleColor m_color_border_hover;
    StyleColor m_color_selected_bg;
    StyleColor m_color_selected_text;

private:
    uint32_t m_local_i18n_version = 0;
public:
    // 构造
    TechButton(const std::string& key_or_text, Style style = Style::Normal);

    void SetSourceText(const std::string& key_or_text, bool with_effect = false);

    TechButton* SetFont(ImFont* font);
    // 链式配置
    TechButton* SetSelected(bool v) { m_selected = v; return this; }
    TechButton* SetSpeed(float v) { m_anim_speed = v; return this; }
    TechButton* SetUseGlass(bool v) { m_use_glass = v; return this; }
    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* dl) override;
    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& external_handled) override;

private:
    std::string m_current_display_text;
    Style m_style;
    float m_hover_progress = 0.0f;

    // 内部文本组件 (用于 Normal 和 Tab 模式)
    std::shared_ptr<TechText> m_label_component;

    // 内部绘制辅助
    void DrawVerticalText(ImDrawList* dl, ImU32 col);
};

_UI_END
_SYSTEM_END
_NPGS_END