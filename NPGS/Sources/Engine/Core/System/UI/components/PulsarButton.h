#pragma once
#include "../ui_framework.h"
#include <string>
#include <variant>
#include "HackerTextHelper.h"
#include "InputField.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class PulsarButton : public UIElement
{
public:
    // --- 外部接口 ---
    std::string m_id;
    std::function<void()> on_click_callback;
    std::function<void(const std::string& id, const std::string& value)> on_stat_change_callback;

    // --- 状态 ---
    enum class PulsarAnimState { Closed, Opening, Open, Closing };
    PulsarAnimState m_anim_state = PulsarAnimState::Closed;
    bool m_is_active = false;

    // --- 构造函数 ---
    PulsarButton(
        const std::string& id,
        const std::string& label,
        const std::string& icon_char,
        const std::string& stat_label,
        std::string* stat_value_ptr, // 绑定外部 string
        const std::string& stat_unit,
        bool is_editable
    );

    // --- 核心方法 ---
    void SetActive(bool active);
    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* draw_list) override;
    bool HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release) override;

private:
    // --- 内部数据 ---
    // 动画
    float m_anim_progress = 0.0f; // 0.0=closed, 1.0=open
    float m_rotation_angle = 0.0f;
    HackerTextHelper m_hacker_locked;
    HackerTextHelper m_hacker_label;
    HackerTextHelper m_hacker_stat_value;

    // 内容
    std::string m_label;
    std::string m_icon_char;
    std::string m_stat_label;
    std::string m_stat_unit;
    bool m_is_editable;
    bool m_use_glass_effect = true;
    // 子控件
    std::shared_ptr<InputField> m_input_field;

    // 脉冲星射线几何数据
    struct RaySegment { float offset, bump_height; bool has_bump, is_gap;float thickness;  };
    struct PulsarRay { float theta, phi, len; std::vector<RaySegment> segments;};
    std::vector<PulsarRay> m_rays;

    // 绘制常量
    const float m_expanded_width = 80.0f;
    const float m_expanded_height = 80.0f;
    const ImVec2 pulsar_center_offset = { 20.0f, 20.0f };
    const float pulsar_radius = 60.0f;
};


_UI_END
_SYSTEM_END
_NPGS_END