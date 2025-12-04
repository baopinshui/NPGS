#pragma once
#include "../ui_framework.h"
#include <string>
#include <functional>
#include "HackerTextHelper.h"
#include "InputField.h"
#include "TechText.h" 
#include "TechBorderPanel.h" 

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class PulsarButton : public UIElement
{
public:
    using OnToggleCallback = std::function<void(bool is_expanding)>;
    using OnExecuteCallback = std::function<void(const std::string& id, const std::string& current_value)>;

    std::string m_id;
    OnToggleCallback on_toggle_callback;
    OnExecuteCallback on_execute_callback;

    enum class PulsarAnimState { Closed, Opening, Open, Closing };
    PulsarAnimState m_anim_state = PulsarAnimState::Closed;
    bool m_is_active = false;
    bool m_can_execute = false;

    // [旧构造函数] 使用字符图标
    PulsarButton(
        const std::string& id,
        const std::string& label,
        const std::string& icon_char,
        const std::string& stat_label,
        std::string* stat_value_ptr,
        const std::string& stat_unit,
        bool is_editable
    );

    // [新构造函数] 使用图片(ImTextureID)图标
    PulsarButton(
        const std::string& id,
        const std::string& label,
        ImTextureID icon_texture,
        const std::string& stat_label,
        std::string* stat_value_ptr,
        const std::string& stat_unit,
        bool is_editable
    );

    void SetActive(bool active);
    void SetStatusText(const std::string& text);
    void SetExecutable(bool can_execute);

    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* draw_list) override;
    void HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release, bool& handled) override;

private:
    // [新增] 内部通用初始化逻辑，减少代码重复
    void InitCommon(const std::string& label, const std::string& stat_label, std::string* stat_value_ptr, const std::string& stat_unit);
    // [新增] 统一设置图标颜色的辅助函数
    void SetIconColor(const ImVec4& color);

    bool m_core_hovered = false;
    float m_core_hover_progress = 0.0f;

    float m_anim_progress = 0.0f;
    float m_rotation_angle = 0.0f;
    std::string m_pending_status_text;
    bool m_has_pending_status = false;
    float m_current_line_len = 130.0f;
    float m_target_line_len = 130.0f;

    std::shared_ptr<TechText> m_text_status;
    std::shared_ptr<TechText> m_text_label;
    std::shared_ptr<TechText> m_text_stat_label;
    std::shared_ptr<TechText> m_text_stat_value;
    std::shared_ptr<TechText> m_text_stat_unit;

    // 图标现在可能是文字，也可能是图片
    std::shared_ptr<TechText> m_text_icon;
    std::shared_ptr<Image>    m_image_icon; // [新增]

    bool m_is_editable;
    std::shared_ptr<InputField> m_input_field;
    std::shared_ptr<TechBorderPanel> m_bg_panel;
    Rect m_label_hit_rect = { 0,0,0,0 };
    bool m_label_hovered = false;

    struct RaySegment { float offset, bump_height; bool has_bump, is_gap; float thickness; };
    struct PulsarRay
    {
        float theta, phi, len;
        float shrink_factor;
        std::vector<RaySegment> segments;
    };
    std::vector<PulsarRay> m_rays;

    const ImVec2 pulsar_center_offset = { 20.0f, 20.0f };
    const float pulsar_radius = 60.0f;
};

_UI_END
_SYSTEM_END
_NPGS_END