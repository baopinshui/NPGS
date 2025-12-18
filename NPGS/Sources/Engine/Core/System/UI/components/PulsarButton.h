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
    using OnExecuteCallback = std::function<void(const std::string& name, const std::string& current_value)>;

    OnToggleCallback on_toggle_callback;
    OnExecuteCallback on_execute_callback;

    enum class PulsarAnimState { Closed, Opening, Open, Closing };
    PulsarAnimState m_anim_state = PulsarAnimState::Closed;
    bool m_is_active = false;
    bool m_can_execute = false;


    std::shared_ptr<TechText> m_text_status;
    std::shared_ptr<TechText> m_text_label;
    std::shared_ptr<TechText> m_text_stat_label;
    std::shared_ptr<TechText> m_text_stat_value;
    std::shared_ptr<TechText> m_text_stat_unit;

    std::shared_ptr<TechText> m_text_icon;
    std::shared_ptr<Image>    m_image_icon;


    PulsarButton(
        const std::string& status_key,
        const std::string& label_key,
        const std::string& icon_char,
        const std::string& stat_label_key,
        std::string* stat_value_ptr,
        const std::string& stat_unit_key,
        bool is_editable
    );

    PulsarButton(
        const std::string& status_key,
        const std::string& label_key,
        ImTextureID icon_texture,
        const std::string& stat_label_key,
        std::string* stat_value_ptr,
        const std::string& stat_unit_key,
        bool is_editable
    );

    void SetActive(bool active);
    void SetStatus(const std::string& status_key);
    void SetExecutable(bool can_execute);

    void Update(float dt) override;
    ImVec2 Measure(ImVec2 available_size) override;
    void Arrange(const Rect& final_rect) override;

    void Draw(ImDrawList* draw_list) override;
    void HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release, bool& handled) override;
private:
    void InitCommon(const std::string& status_key, const std::string& label_key, const std::string& stat_label_key, std::string* stat_value_ptr, const std::string& stat_unit_key);
    void SetIconColor(const ImVec4& color);

    // 内部存储 Key，用于 I18n 更新时的手动翻译
    std::string m_status_key;
    std::string m_label_key;
    std::string m_stat_label_key;
    // 注意：m_stat_unit_key 不需要存，因为它直接交给 TechText 自动管理

    uint32_t m_local_i18n_version = 0;

    bool m_core_hovered = false;
    float m_core_hover_progress = 0.0f;
    float m_hover_energy_progress = 0.0f;

    float current_box_size = 0.0f;
    float m_anim_progress = 0.0f;
    float line_prog = 0.0f;
    float m_rotation_angle = 0.0f;

    std::string* m_stat_value_ptr = nullptr;
    float m_current_line_len = 130.0f;
    float m_target_line_len = 130.0f;

    bool m_is_editable;
    std::shared_ptr<InputField> m_input_field;
    std::shared_ptr<TechBorderPanel> m_bg_panel;
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