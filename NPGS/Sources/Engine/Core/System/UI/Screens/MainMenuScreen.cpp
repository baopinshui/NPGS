#include "MainMenuScreen.h"
#include "../ScreenManager.h"
#include "../../../../../Program/Application.h"
_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

MainMenuScreen::MainMenuScreen(AppContext& context) : IScreen(context)
{
    m_blocks_update = true;
}

void MainMenuScreen::OnEnter()
{
    m_ui_root = std::make_shared<UIRoot>();

    // 1. 背景面板：撑满整个屏幕
    auto bg_panel = std::make_shared<UI::Panel>();
    bg_panel->m_width = Length::Stretch(); // 宽度撑满
    bg_panel->m_height = Length::Stretch(); // 高度撑满
    bg_panel->m_use_glass_effect = true;
    bg_panel->m_bg_color = ImVec4(0, 0, 0, 0.4);
    bg_panel->m_block_input = true;
    m_ui_root->AddChild(bg_panel);

    // 2. 创建一个垂直布局容器来容纳所有居中内容
    auto main_layout = std::make_shared<UI::VBox>();
    main_layout->m_width = Length::Content();   // 宽度由内容决定
    main_layout->m_height = Length::Content();  // 高度由内容决定
    main_layout->m_align_h = UI::Alignment::Center; // 在父容器(bg_panel)中水平居中
    main_layout->m_align_v = UI::Alignment::Center; // 在父容器(bg_panel)中垂直居中
    main_layout->m_padding = 50.0f; // 标题和按钮组之间的间距
    bg_panel->AddChild(main_layout);

    // 3. 标题：尺寸自适应，在main_layout中水平居中
    m_title = std::make_shared<UI::TechText>("i18ntext.ui.menu.NPGS", ThemeColorID::TextHighlight, true, true);
    m_title->m_font = UIContext::Get().m_font_large;
    m_title->SetSizing(UI::TechTextSizingMode::AutoWidthHeight);
    m_title->m_align_h = UI::Alignment::Center; // 在VBox交叉轴上居中
    main_layout->AddChild(m_title);

    // 4. 按钮容器：固定宽度，高度由内容决定
    m_button_layout = std::make_shared<UI::VBox>();
    m_button_layout->m_width = Length::Fixed(300);
    m_button_layout->m_height = Length::Content();
    m_button_layout->m_padding = 15.0f;
    m_button_layout->m_align_h = UI::Alignment::Center; // 在VBox交叉轴上居中
    main_layout->AddChild(m_button_layout);

    // 5. 按钮：宽度撑满父容器(m_button_layout)，高度固定
    auto new_game_btn = std::make_shared<UI::TechButton>("i18ntext.ui.menu.new_game");
    new_game_btn->SetTooltip("i18ntext.tooltip.new_game");
    new_game_btn->m_width = Length::Stretch();
    new_game_btn->m_height = Length::Fixed(40.0f);
    new_game_btn->on_click = [this]()
    {
        m_screen_manager->RequestChangeScreen("Game");
    };
    m_button_layout->AddChild(new_game_btn);

    auto quit_btn = std::make_shared<UI::TechButton>("i18ntext.ui.menu.quit");
    quit_btn->SetTooltip("i18ntext.tooltip.test");
    quit_btn->m_width = Length::Stretch();
    quit_btn->m_height = Length::Fixed(40.0f);
    quit_btn->on_click = [this]()
    {
        if (m_context.Application)
        {
            m_context.Application->Quit();
        }
    };
    m_button_layout->AddChild(quit_btn);
}

void MainMenuScreen::OnExit()
{
    m_ui_root.reset();
}

void MainMenuScreen::Update(float dt)
{

    if (m_ui_root)
    {
        m_ui_root->Update(dt);
    }
}

void MainMenuScreen::Draw()
{
    if (m_ui_root)
    {
        m_ui_root->Draw();
    }
}

_UI_END
_SYSTEM_END
_NPGS_END