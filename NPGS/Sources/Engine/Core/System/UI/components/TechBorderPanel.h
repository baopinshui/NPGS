#pragma once
#include "../ui_framework.h"
#include <string>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class TechBorderPanel : public Panel
{
public:
    StyleColor m_glass_col = ImVec4{ 0.0f,0.0f,0.0f,0.3f };
    float m_thickness = 2.0f;
    bool m_force_accent_color = false;

    // [New Features: Dual Orbit Flow]
    bool m_show_flow_border = false;

    // 参数控制
    float m_flow_period = 2.0f;       // 外层周期
    float m_flow_length_ratio = 0.2f; // 总长度占比
    int m_flow_segment_count = 1;     // 分段数量
    float m_flow_randomness = 0.0f;   // 随机性强度
    bool m_flow_use_gradient = false;     // 渐变



    TechBorderPanel();

    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled) override;

    void Update(float dt) override;
    void Draw(ImDrawList* draw_list) override;

private:
    float m_progress_outer = 0.0f;
    float m_progress_inner = 0.0f;
};

_UI_END
_SYSTEM_END
_NPGS_END