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

    OnToggleCallback on_toggle_callback;
    OnExecuteCallback on_execute_callback;

    enum class PulsarAnimState { Closed, Opening, Open, Closing };
    PulsarAnimState m_anim_state = PulsarAnimState::Closed;
    bool m_is_active = false;
    bool m_can_execute = false;

    // [修改] 构造函数参数名保持不变，但内部逻辑会改变
    PulsarButton(
        const std::string& status_key,
        const std::string& label_key,
        const std::string& icon_char,
        const std::string& stat_label_key,
        std::string* stat_value_ptr,
        const std::string& stat_unit_key,
        bool is_editable,
        const std::string& id = ""
    );

    PulsarButton(
        const std::string& status_key,
        const std::string& label_key,
        ImTextureID icon_texture,
        const std::string& stat_label_key,
        std::string* stat_value_ptr,
        const std::string& stat_unit_key,
        bool is_editable,
        const std::string& id = ""
    );

    void SetActive(bool active);
    void SetI18nKey(const std::string& status_key);

    // [弃用] 但保留用于向后兼容
    void SetStatusText(const std::string& text);

    void SetExecutable(bool can_execute);

    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* draw_list) override;
    void HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release, bool& handled) override;

private:
    void InitCommon(const std::string& status_key, const std::string& label_key, const std::string& stat_label_key, std::string* stat_value_ptr, const std::string& stat_unit_key);
    void SetIconColor(const ImVec4& color);

    // [新增] 内部存储 Key，用于手动控制 I18n 更新
    std::string m_status_key;
    std::string m_label_key;
    // 统计标签也可能需要更新
    std::string m_stat_label_key;
    std::string m_stat_unit_key;
    uint32_t m_local_i18n_version = 0;

    bool m_core_hovered = false;
    float m_core_hover_progress = 0.0f;
    float m_hover_energy_progress = 0.0f;

    float current_box_size = 0.0f;
    float m_anim_progress = 0.0f;
    float line_prog = 0.0f;
    float m_rotation_angle = 0.0f;

    // 暂存文本：当需要伸长横线时，新文本存在这里
    std::string m_pending_status_text;
    std::string m_pending_label_text; // [新增] Label 也可能变长
    bool m_has_pending_text = false;  // [修改] 统一标记

    std::string* m_stat_value_ptr = nullptr;
    float m_current_line_len = 130.0f;
    float m_target_line_len = 130.0f;

    std::shared_ptr<TechText> m_text_status;
    std::shared_ptr<TechText> m_text_label;
    std::shared_ptr<TechText> m_text_stat_label;
    std::shared_ptr<TechText> m_text_stat_value;
    std::shared_ptr<TechText> m_text_stat_unit;

    std::shared_ptr<TechText> m_text_icon;
    std::shared_ptr<Image>    m_image_icon;

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

    // [新增] 辅助函数：检查文本长度并决定是否暂存
    void CheckAndSetText(std::shared_ptr<TechText> comp, std::string& pending_str, const std::string& new_text);
};

_UI_END
_SYSTEM_END
_NPGS_END