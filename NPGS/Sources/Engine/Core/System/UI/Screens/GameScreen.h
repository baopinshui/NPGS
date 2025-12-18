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
    // [修改] 不再存储指针，而是存储ID
    std::string element_id;
    float delay_time;
    float flicker_duration;
    float timer;
    bool is_finished;
    float current_flicker_timer = 0.0f;
    float time_until_next_change = 0.0f;
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

    void RegisterIntroEffect(const std::shared_ptr<UIElement>& el, float delay, float duration);
    // [新增] 动画状态列表
    std::vector<IntroAnimState> m_intro_anim_states;
    bool m_is_intro_playing = false;

    // UI internal state
    std::string m_beam_energy;
    std::string m_rkkv_mass;
    std::string m_VN_mass;
};

_UI_END
_SYSTEM_END
_NPGS_END