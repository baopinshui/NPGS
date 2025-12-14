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

// LogCard: 固定高度，仅负责渲染内容
class LogCard : public UIElement
{
public:
    LogCard(LogType type, const std::string& title, const std::string& message);

    // [MODIFIED] 使用新的生命周期函数
    void Update(float dt) override;
    void Arrange(const Rect& final_rect) override;
    void Draw(ImDrawList* dl) override;

    static constexpr float CARD_HEIGHT = 58.0f;

private:
    LogType m_type;
    std::shared_ptr<VBox> m_content_box; // [NEW] 使用VBox进行内部布局
    std::shared_ptr<TechText> m_title_text;
    std::shared_ptr<TechText> m_msg_text;
    float m_pulse_timer = 0.0f; // 仅用于 Alert 闪烁
};

class LogPanel : public UIElement
{
public:
    LogPanel(const std::string& sys_key, const std::string& save_key);

    void AddLog(LogType type, const std::string& title, const std::string& message);
    void SetSystemStatus(const std::string& text);
    void SetAutoSaveTime(const std::string& text);

    // [MODIFIED] 使用新的生命周期函数
    void Update(float dt) override;
    ImVec2 Measure(ImVec2 available_size) override;
    void Arrange(const Rect& final_rect) override;
    void Draw(ImDrawList* dl) override;

private:
    std::shared_ptr<VBox> m_list_box;
    std::shared_ptr<VBox> m_footer_box;
    std::shared_ptr<TechDivider> m_divider;

    std::shared_ptr<TechText> m_system_text;
    std::shared_ptr<TechText> m_autosave_text;
    const size_t MAX_LOG_COUNT = 4;
    // 列表可视区域固定高度
    const float LIST_AREA_HEIGHT = MAX_LOG_COUNT * LogCard::CARD_HEIGHT;
    const float DIVIDER_AREA_HEIGHT = 10.0f;

    // 滑动控制
    float m_slide_offset = 0.0f;
    // [REMOVED] m_is_sliding is no longer needed
};

_UI_END
_SYSTEM_END
_NPGS_END