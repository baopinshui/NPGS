#pragma once
#include "../ui_framework.h"
#include "TechText.h"
#include "TechDivider.h"
#include <string>
#include <vector>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class CinematicInfoPanel : public UIElement
{
public:
    enum class Position { Top, Bottom };

    CinematicInfoPanel(Position pos);

    void SetCivilizationData(const std::string& name,
        const std::string& mass_label,
        const std::string& count_label,
        const std::string& reward_label);

    void SetCelestialData(const std::string& id,
        const std::string& type,
        const std::string& mass_label,
        const std::string& lum_label);

    void Update(float dt, const ImVec2& parent_abs_pos) override;

private:
    Position m_position;

    enum class State { Hidden, Expanding, Visible, Contracting };
    State m_state = State::Hidden;

    float m_anim_progress = 0.0f; // 控制横线长度 0.0 -> 1.0
    float m_text_alpha = 0.0f;    // 独立控制文字透明度
    bool m_text_anim_triggered = false; // 标记文字动画是否已触发

    float m_max_width = 800.0f; // 面板总宽

    // 容器与组件
    std::shared_ptr<VBox> m_layout_vbox;
    std::shared_ptr<TechDivider> m_divider;
    std::shared_ptr<TechText> m_title_text;

    // Top
    std::shared_ptr<HBox> m_top_stats_box;
    std::shared_ptr<TechText> m_top_stat_1;
    std::shared_ptr<TechText> m_top_stat_2;
    std::shared_ptr<TechText> m_top_stat_3;

    // Bottom
    std::shared_ptr<TechText> m_bot_type_text;
    std::shared_ptr<HBox> m_bot_stats_box;
    std::shared_ptr<TechText> m_bot_stat_1;
    std::shared_ptr<TechText> m_bot_stat_2;

    void CheckStateTransition(bool has_content);

    // 辅助：计算并设置文字宽度
    void UpdateTextWidth(std::shared_ptr<TechText> text_elem, const std::string& content);
};

_UI_END
_SYSTEM_END
_NPGS_END