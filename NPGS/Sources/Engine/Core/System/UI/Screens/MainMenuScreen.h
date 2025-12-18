#pragma once
#include "../IScreen.h"
#include "../neural_ui.h" // 引入所有UI组件

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
    // 定义面板ID，方便管理
    enum class PanelID { Main, NewGame, LoadGame, Settings };

    // 切换面板的辅助函数
    void SwitchToPanel(PanelID id);

    // 指向关键UI元素的指针，用于逻辑控制
    std::shared_ptr<UIElement> m_mainLevelPanel;
    std::shared_ptr<UIElement> m_newGamePanel;
    std::shared_ptr<UIElement> m_settingsPanel;
    // ... 可以添加更多，比如 m_loadGamePanel

    // “新游戏”面板的状态
    std::shared_ptr<TechButton> m_btnModeGod;
    std::shared_ptr<TechButton> m_btnModeEvo;
    enum class GameMode { God, Evo };
    GameMode m_selectedGameMode = GameMode::God;
};

_UI_END
_SYSTEM_END
_NPGS_END