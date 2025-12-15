#pragma once

#include "../IScreen.h"
#include "../neural_ui.h"

#include "../../../../../Program/DataStructures.h"
#include <string>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

struct IntroAnimState
{
    std::shared_ptr<UIElement> element;
    float delay_time;      // 延迟多久开始闪烁
    float flicker_duration;// 闪烁持续多久
    float timer;           // 内部计时器
    bool is_finished;      // 是否完成
    float current_flicker_timer = 0.0f;      // 当前闪烁状态已经维持了多久
    float time_until_next_change = 0.0f;     // 当前闪烁状态应该维持多久（随机生成）
};

class GameScreen : public IScreen
{
public:
    GameScreen(AppContext& context);

    void OnEnter() override;
    void OnExit() override;
    void Update(float dt) override;
    void Draw() override;

private:
    // Helper functions moved from Application
    void OnLanguageChanged();
    void SimulateStarSelectionAndUpdateUI();
    std::string FormatTime(double total_seconds);
    // [新增] 注册组件参与入场动画的辅助函数
    void RegisterIntroEffect(std::shared_ptr<UIElement> el, float delay, float duration);

    // [新增] 动画状态列表
    std::vector<IntroAnimState> m_intro_anim_states;
    bool m_is_intro_playing = false;
    // UI component pointers
    std::shared_ptr<UI::NeuralMenu> m_neural_menu_controller;
    std::shared_ptr<UI::PulsarButton> m_beam_button;
    std::shared_ptr<UI::PulsarButton> m_rkkv_button;
    std::shared_ptr<UI::PulsarButton> m_VN_button;
    std::shared_ptr<UI::PulsarButton> m_message_button;
    std::shared_ptr<UI::CelestialInfoPanel> m_celestial_info;
    std::shared_ptr<UI::CinematicInfoPanel> m_top_Info;
    std::shared_ptr<UI::CinematicInfoPanel> m_bottom_Info;
    std::shared_ptr<UI::TimeControlPanel> m_time_control_panel;
    std::shared_ptr<UI::LogPanel> m_log_panel;

    // UI internal state
    std::string m_beam_energy;
    std::string m_rkkv_mass;
    std::string m_VN_mass;
};

_UI_END
_SYSTEM_END
_NPGS_END