#pragma once
#include "../IScreen.h"
#include "../neural_ui.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class MainMenuScreen : public IScreen
{
public:
    MainMenuScreen(AppContext& context);

    void OnEnter() override;
    void OnExit() override;
    void Update(float dt) override;
    void Draw() override;

private:
    std::shared_ptr<UI::TechText> m_title;
    std::shared_ptr<UI::VBox> m_button_layout;
};

_UI_END
_SYSTEM_END
_NPGS_END