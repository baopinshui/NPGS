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

};

_UI_END
_SYSTEM_END
_NPGS_END