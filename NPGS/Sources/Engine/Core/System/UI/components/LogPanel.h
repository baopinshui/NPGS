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

    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* dl) override;

    static constexpr float CARD_WIDTH = 260.0f;
    static constexpr float CARD_HEIGHT = 58.0f;

private:
    LogType m_type;
    std::shared_ptr<TechText> m_title_text;
    std::shared_ptr<TechText> m_msg_text;
    float m_pulse_timer = 0.0f;
};

class LogPanel : public UIElement // [REFACTOR] 保持继承自 UIElement
{
public:
    LogPanel(const std::string& sys_key, const std::string& save_key);

    void AddLog(LogType type, const std::string& title, const std::string& message);
    void SetSystemStatus(const std::string& text);
    void SetAutoSaveTime(const std::string& text);

    void Update(float dt, const ImVec2& parent_abs_pos) override;
    void Draw(ImDrawList* dl) override;
    ImVec2 Measure(const ImVec2& available_size) override;

private:
    // [REFACTOR] 新的结构
    std::shared_ptr<VBox> m_root_vbox;         // 主布局容器
    std::shared_ptr<Panel> m_clipping_area;      // 列表的裁剪区域
    std::shared_ptr<VBox> m_log_container;     // 真正存放 LogCard 的容器

    std::shared_ptr<TechDivider> m_divider;
    std::shared_ptr<TechText> m_system_text;
    std::shared_ptr<TechText> m_autosave_text;

    const size_t MAX_LOG_COUNT = 4;
    // [REFACTOR] 列表区高度现在只用于设置 clipping_area 的高度
    const float LIST_AREA_HEIGHT = MAX_LOG_COUNT * LogCard::CARD_HEIGHT;

    // 滑动控制 (保持不变)
    float m_slide_offset = 0.0f;
};

_UI_END
_SYSTEM_END
_NPGS_END