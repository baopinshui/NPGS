#pragma once
#include "../ui_framework.h"
#include "TechText.h"
#include "TechButton.h"
#include "TechSliders.h"
#include <string>
#include <cstdio>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class TimeControlPanel : public UIElement
{
public:
    TimeControlPanel(double* current_time_ptr, double* time_scale_ptr);
    void Update(float dt, const ImVec2& parent_abs_pos) override;

private:
    double* m_time_ptr = nullptr;
    double* m_scale_ptr = nullptr;

    // [新增] 视觉上的目标倍率。滑条绑定到这里，而不是直接绑定到 m_scale_ptr
    double m_visual_target_scale = 1.0;

    // [移除] m_stored_scale 不再需要，因为 m_visual_target_scale 充当了记忆存储且可编辑
    bool m_is_paused = false;

    std::shared_ptr<TechText> m_text_display;
    std::shared_ptr<TechButton> m_pause_btn;
    std::shared_ptr<TechButton> m_1x_btn;
    // 滑条绑定的是 double 类型
    std::shared_ptr<ThrottleTechSlider<double>> m_speed_slider;

    std::string FormatTime(double total_seconds);
};

_UI_END
_SYSTEM_END
_NPGS_END