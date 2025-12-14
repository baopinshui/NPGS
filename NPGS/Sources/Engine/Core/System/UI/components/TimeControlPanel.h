#pragma once
#include "../ui_framework.h"
#include "TechText.h"
#include "TechButton.h"
#include "TechSliders.h"
#include <string>
#include <cstdio>
#include <memory>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class TimeControlPanel : public UIElement
{
public:
    TimeControlPanel(
        double* current_time_ptr,
        double* time_scale_ptr,
        const std::string& pause_key,
        const std::string& resume_key,
        const std::string& reset_key
    );

    // [修正] Update 只负责业务逻辑
    void Update(float dt) override;

    // [标准布局流程]
    ImVec2 Measure(ImVec2 available_size) override;
    void Arrange(const Rect& final_rect) override;

private:
    double* m_time_ptr = nullptr;
    double* m_scale_ptr = nullptr;

    double m_visual_target_scale = 1.0;
    bool m_is_paused = false;

    std::string m_pause_key;
    std::string m_resume_key;

    std::shared_ptr<VBox> m_main_layout;
    std::shared_ptr<TechText> m_text_display;
    std::shared_ptr<TechButton> m_pause_btn;
    std::shared_ptr<TechButton> m_1x_btn;
    std::shared_ptr<ThrottleTechSlider<double>> m_speed_slider;

    std::string FormatTime(double total_seconds);
};

_UI_END
_SYSTEM_END
_NPGS_END