// --- START OF FILE PulsarButton.h ---
#pragma once
#include "../ui_framework.h"
#include <string>
#include <functional>
#include "HackerTextHelper.h"
#include "InputField.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class PulsarButton : public UIElement
{
public:
    // --- 定义回调类型 ---
    // 点击图标展开/收起的回调
    using OnToggleCallback = std::function<void(bool is_expanding)>;
    // 点击主标题（发射）的回调，返回当前的ID和输入框的值
    using OnExecuteCallback = std::function<void(const std::string& id, const std::string& current_value)>;

    // --- 外部接口 ---
    std::string m_id;

    OnToggleCallback on_toggle_callback;   // [修改] 明确用途
    OnExecuteCallback on_execute_callback; // [新增] 执行回调

    // --- 状态 ---
    enum class PulsarAnimState { Closed, Opening, Open, Closing };
    PulsarAnimState m_anim_state = PulsarAnimState::Closed;
    bool m_is_active = false;
    bool m_can_execute = false; // [新增] 是否可执行（即是否锁定目标）
    std::string m_current_status_text = "SYSTEM READY"; // 默认状态文本

    // --- 构造函数 ---
    PulsarButton(
        const std::string& id,
        const std::string& label, // 这将是“发射按钮”的文字
        const std::string& icon_char,
        const std::string& stat_label,
        std::string* stat_value_ptr,
        const std::string& stat_unit,
        bool is_editable
    );

    // --- 核心方法 ---
    void SetActive(bool active);

    // [新增] 动态设置状态文本 (例如 "TARGET LOCKED" -> "NO TARGET")
    void SetStatusText(const std::string& text);

    void SetExecutable(bool can_execute); // [新增] 设置可执行状态的接口


    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* draw_list) override;
    bool HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release) override;

private:
    // --- 内部数据 ---
    float m_anim_progress = 0.0f;
    float m_rotation_angle = 0.0f;

    // HackerTextHelper 管理器
    HackerTextHelper m_hacker_status; // [重命名] 原 m_hacker_locked
    HackerTextHelper m_hacker_label;  // 主标题/发射按钮
    HackerTextHelper m_hacker_stat_value;

    std::string m_label;
    std::string m_icon_char;
    std::string m_stat_label;
    std::string m_stat_unit;

    bool m_is_editable;
    bool m_use_glass_effect = true;

    // 子控件
    std::shared_ptr<InputField> m_input_field;

    // [新增] Label（发射按钮）的点击区域检测
    Rect m_label_hit_rect = { 0,0,0,0 };
    bool m_label_hovered = false; // Label 是否被悬停

    // 脉冲星射线几何数据 (保持不变)
    struct RaySegment { float offset, bump_height; bool has_bump, is_gap; float thickness; };
    struct PulsarRay { float theta, phi, len; std::vector<RaySegment> segments; };
    std::vector<PulsarRay> m_rays;

    // 布局常量
    const float m_expanded_width = 80.0f;
    const float m_expanded_height = 80.0f;
    const ImVec2 pulsar_center_offset = { 20.0f, 20.0f };
    const float pulsar_radius = 60.0f;

    // 布局计算辅助
    void UpdateLayoutRects();
};

_UI_END
_SYSTEM_END
_NPGS_END