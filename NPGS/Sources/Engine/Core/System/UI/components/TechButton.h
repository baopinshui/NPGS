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
        Vertical,   // 竖排旋转文字风格 (原 VerticalTextButton) - 自行绘制，不使用 TechText
        Invisible   // 纯点击区域
    };

    // 回调
    std::function<void()> on_click;

    // 状态
    bool m_selected = false; // 用于 Tab 模式
    bool m_use_glass = false;
    float m_anim_speed = 5.0f; // 动画速度
    std::string m_source_text;

    // 样式颜色
    StyleColor m_color_bg;
    StyleColor m_color_bg_hover;
    StyleColor m_color_text;
    StyleColor m_color_text_hover;
    StyleColor m_color_border;
    StyleColor m_color_border_hover;
    StyleColor m_color_selected_bg;
    StyleColor m_color_selected_text;

    // 构造
    TechButton(const std::string& key_or_text, Style style = Style::Normal);

    void SetSourceText(const std::string& key_or_text, bool with_effect = false);
    TechButton* SetFont(ImFont* font);
    TechButton* SetSelected(bool v) { m_selected = v; return this; }
    TechButton* SetSpeed(float v) { m_anim_speed = v; return this; }
    TechButton* SetUseGlass(bool v) { m_use_glass = v; return this; }

    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* dl) override;
    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& external_handled) override;

    // [MODIFIED] Measure 默认返回固定尺寸或 0，对于 Button 来说通常由 SetFixedSize 设置
    // 如果需要 Button 自适应文本大小，可以在这里实现
    ImVec2 Measure(const ImVec2& available_size) override;

private:
    std::string m_current_display_text; // 仅用于 Vertical 模式
    Style m_style;
    float m_hover_progress = 0.0f;
    uint32_t m_local_i18n_version = 0;

    // 内部文本组件 (用于 Normal 和 Tab 模式)
    std::shared_ptr<TechText> m_label_component;

    // 内部绘制辅助 (仅 Vertical 模式使用)
    void DrawVerticalText(ImDrawList* dl, ImU32 col);
};

_UI_END
_SYSTEM_END
_NPGS_END