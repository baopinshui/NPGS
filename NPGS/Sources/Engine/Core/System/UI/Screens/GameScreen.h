#pragma once

#include "../IScreen.h"
#include "../neural_ui.h"

#include "../../../../../Program/DataStructures.h"
#include <string>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

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