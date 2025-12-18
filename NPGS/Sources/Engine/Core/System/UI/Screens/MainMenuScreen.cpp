#include "MainMenuScreen.h"
#include "../ScreenManager.h"
#include "../../../../../Program/Application.h"
#include "../UILoader.h" // [新增]
#include "../UIFactory.h" // [新增]

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

MainMenuScreen::MainMenuScreen(AppContext& context) : IScreen(context)
{
    m_blocks_update = true;

    // --- [修正后的代码] ---
    auto& factory = UIFactory::Get();

    // 对于有默认构造函数的组件，可以使用简单的模板版本
    factory.Register<UIRoot>("UIRoot");
    factory.Register<Panel>("Panel");
    factory.Register<VBox>("VBox");

    // 对于需要构造参数的组件，必须提供一个自定义的创建lambda
    // 我们传入一个空的占位符字符串，因为实际内容会从JSON加载
    factory.Register("TechText", []()
    {
        return std::make_shared<TechText>("");
    });

    factory.Register("TechButton", []()
    {
        return std::make_shared<TechButton>("");
    });
}

void MainMenuScreen::OnEnter()
{
    // 1. 创建加载器
    UILoader loader;

    // 2. 注册此屏幕需要的所有事件处理逻辑
    loader.RegisterAction("action.startGame", [this]()
    {
        m_screen_manager->RequestChangeScreen("Game");
    });

    loader.RegisterAction("action.quitGame", [this]()
    {
        if (m_context.Application)
        {
            m_context.Application->Quit();
        }
    });

    // 3. 从文件加载UI树
    // 假设你的资源路径是 Assets/UI/main_menu.json
    m_ui_root = loader.LoadFromFile("Assets/UI/main_menu.json");

    // 如果加载失败，可以创建一个空的根以防崩溃
    if (!m_ui_root)
    {
        m_ui_root = std::make_shared<UIRoot>();
    }
}

// OnExit, Update, Draw 保持不变
void MainMenuScreen::OnExit() { m_ui_root.reset(); }
void MainMenuScreen::Update(float dt) { if (m_ui_root) m_ui_root->Update(dt); }
void MainMenuScreen::Draw() { if (m_ui_root) m_ui_root->Draw(); }

_UI_END
_SYSTEM_END
_NPGS_END