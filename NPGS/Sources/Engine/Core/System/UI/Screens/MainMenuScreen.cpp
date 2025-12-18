#include "MainMenuScreen.h"
#include "../ScreenManager.h"
#include "../../../../../Program/Application.h"
#include "../UILoader.h"
#include "../UIFactory.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

MainMenuScreen::MainMenuScreen(AppContext& context) : IScreen(context)
{
    m_blocks_update = true;

    // 1. 注册本屏幕用到的所有UI组件类型
    auto& factory = UIFactory::Get();
    factory.Register<UIRoot>("UIRoot");
    factory.Register<Panel>("Panel");
    factory.Register<VBox>("VBox");
    factory.Register<HBox>("HBox");
    factory.Register<ScrollView>("ScrollView");
    factory.Register<TechBorderPanel>("TechBorderPanel");
    factory.Register<TechDivider>("TechDivider");
    factory.Register("TechText", []() { return std::make_shared<TechText>(""); });
    factory.Register("TechButton", []() { return std::make_shared<TechButton>(""); });
}

void MainMenuScreen::OnEnter()
{
    // 2. 创建加载器并注册事件
    UILoader loader;

    // --- 菜单导航事件 ---
    loader.RegisterAction("action.menu.toNewGame", [this]() { SwitchToPanel(PanelID::NewGame); });
    loader.RegisterAction("action.menu.toLoadGame", [this]() { /* SwitchToPanel(PanelID::LoadGame); */ }); // 暂未实现
    loader.RegisterAction("action.menu.toSettings", [this]() { SwitchToPanel(PanelID::Settings); });
    loader.RegisterAction("action.menu.backToMain", [this]() { SwitchToPanel(PanelID::Main); });

    // --- 游戏流程事件 ---
    loader.RegisterAction("action.startGame", [this]()
    {
        // 在这里可以收集 m_selectedGameMode 等配置
        m_screen_manager->RequestChangeScreen("Game");
    });
    loader.RegisterAction("action.quitGame", [this]()
    {
        if (m_context.Application) m_context.Application->Quit();
    });

    // --- “新游戏”面板内部事件 ---
    loader.RegisterAction("action.newgame.selectGodMode", [this]()
    {
        m_selectedGameMode = GameMode::God;
        if (m_btnModeGod) m_btnModeGod->SetSelected(true);
        if (m_btnModeEvo) m_btnModeEvo->SetSelected(false);
    });
    loader.RegisterAction("action.newgame.selectEvoMode", [this]()
    {
        m_selectedGameMode = GameMode::Evo;
        if (m_btnModeGod) m_btnModeGod->SetSelected(false);
        if (m_btnModeEvo) m_btnModeEvo->SetSelected(true);
    });

    // 3. 加载UI树
    m_ui_root = loader.LoadFromFile("Assets/UI/main_menu.json");
    if (!m_ui_root)
    {
        m_ui_root = std::make_shared<UIRoot>();
        return;
    }

    // 4. “连接”C++代码和加载的UI元素
    m_mainLevelPanel = m_ui_root->FindElementByID("mainMenuRoot.mainLevelPanel")->shared_from_this();
    m_newGamePanel = m_ui_root->FindElementByID("mainMenuRoot.newGamePanel")->shared_from_this();
    m_settingsPanel = m_ui_root->FindElementByID("mainMenuRoot.settingsPanel")->shared_from_this();

    m_btnModeGod = std::static_pointer_cast<TechButton>(m_ui_root->FindElementAs<TechButton>("mainMenuRoot.newGamePanel.btnModeGod")->shared_from_this());
    m_btnModeEvo = std::static_pointer_cast<TechButton>(m_ui_root->FindElementAs<TechButton>("mainMenuRoot.newGamePanel.btnModeEvo")->shared_from_this());

    // 5. 初始化UI状态
    SwitchToPanel(PanelID::Main);
}

void MainMenuScreen::SwitchToPanel(PanelID id)
{
    // 确保指针有效
    if (!m_mainLevelPanel || !m_newGamePanel || !m_settingsPanel) return;

    // 根据ID设置各个面板的可见性
    m_mainLevelPanel->m_visible = (id == PanelID::Main);
    m_newGamePanel->m_visible = (id == PanelID::NewGame);
    m_settingsPanel->m_visible = (id == PanelID::Settings);
    // m_loadGamePanel->m_visible = (id == PanelID::LoadGame);
}

// OnExit, Update, Draw 保持不变
void MainMenuScreen::OnExit() { m_ui_root.reset(); }
void MainMenuScreen::Update(float dt) { if (m_ui_root) m_ui_root->Update(dt); }
void MainMenuScreen::Draw() { if (m_ui_root) m_ui_root->Draw(); }

_UI_END
_SYSTEM_END
_NPGS_END