// --- START OF FILE LogPanel.h ---
#pragma once
#include "../ui_framework.h"
#include "TechText.h"
#include "TechDivider.h"
#include <string>
#include <vector>
#include <memory>
#include <list>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

enum class LogType { Info, Alert };

class LogCard : public UIElement
{
public:
    LogCard(LogType type, const std::string& title, const std::string& message);

    void Update(float dt) override;
    ImVec2 Measure(ImVec2 available_size) override;
    void Arrange(const Rect& final_rect) override;
    void Draw(ImDrawList* dl) override;

    static constexpr float BASE_HEIGHT = 58.0f;

private:
    LogType m_type;
    std::shared_ptr<VBox> m_content_box;
    std::shared_ptr<TechText> m_title_text;
    std::shared_ptr<TechText> m_msg_text;
    float m_pulse_timer = 0.0f;

    float m_current_height = BASE_HEIGHT;
    float m_target_height = BASE_HEIGHT;
    const float m_expand_speed = 15.0f;
};

class LogPanel : public UIElement
{
public:
    LogPanel(const std::string& sys_key, const std::string& save_key);

    void AddLog(LogType type, const std::string& title, const std::string& message);
    void SetSystemStatus(const std::string& text);
    void SetAutoSaveTime(const std::string& text);

    // 生命周期重写
    void Update(float dt) override;
    ImVec2 Measure(ImVec2 available_size) override;
    void Arrange(const Rect& final_rect) override;
    void Draw(ImDrawList* dl) override;

private:
    std::shared_ptr<ScrollView> m_scroll_view;
    std::shared_ptr<VBox> m_list_content;
    std::shared_ptr<UIElement> m_top_spacer;

    std::shared_ptr<VBox> m_footer_box;
    std::shared_ptr<TechDivider> m_divider;

    std::shared_ptr<TechText> m_system_text;
    std::shared_ptr<TechText> m_autosave_text;

    bool m_auto_scroll = true;

    // 视觉补偿偏移量 (渲染层面的偏移)
    float m_visual_offset_y = 0.0f;

    const float LIST_VISIBLE_HEIGHT = 4.0f * LogCard::BASE_HEIGHT;
    const float DIVIDER_AREA_HEIGHT = 10.0f;
};

_UI_END
_SYSTEM_END
_NPGS_END