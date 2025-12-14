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

    auto bg_panel = std::make_shared<UI::Panel>();
    bg_panel->m_rect = { 0, 0, 1920, 1080 };
    bg_panel->m_use_glass_effect = true;
    bg_panel->m_bg_color = ImVec4(0, 0, 0, 0.4);
    bg_panel->m_block_input = true;
    m_ui_root->AddChild(bg_panel);

    m_title = std::make_shared<UI::TechText>("ui.menu.NPGS", ThemeColorID::TextHighlight, true, true);
    m_title->m_font = UIContext::Get().m_font_large;
    m_title->SetSizing(UI::TechTextSizingMode::AutoWidthHeight);
    m_title->m_align_h = UI::Alignment::Center;
    bg_panel->AddChild(m_title);

    m_button_layout = std::make_shared<UI::VBox>();
    m_button_layout->m_rect = { 0, 0, 300, 200 };
    m_button_layout->m_padding = 15.0f;
    m_button_layout->m_align_h = UI::Alignment::Center;
    bg_panel->AddChild(m_button_layout);

    auto new_game_btn = std::make_shared<UI::TechButton>("ui.menu.new_game");

    new_game_btn->SetTooltip("tooltip.new_game");
    new_game_btn->m_rect.h = 40.0f;
    new_game_btn->on_click = [this]()
    {
        m_screen_manager->RequestChangeScreen("Game");
    };
    m_button_layout->AddChild(new_game_btn);

    auto quit_btn = std::make_shared<UI::TechButton>("ui.menu.quit");
    quit_btn->m_rect.h = 40.0f;
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
    auto& display_size = UIContext::Get().m_display_size;

    if (m_title)
    {
        m_title->m_rect.x = (display_size.x - m_title->m_rect.w) * 0.5f;
        m_title->m_rect.y = display_size.y * 0.3f;
    }
    if (m_button_layout)
    {
        m_button_layout->m_rect.x = (display_size.x - m_button_layout->m_rect.w) * 0.5f;
        m_button_layout->m_rect.y = display_size.y * 0.5f;
    }

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