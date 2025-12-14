#include "GameScreen.h"
#include "../ScreenManager.h"
#include "../../../../../Program/Application.h" // For context types
#include "Engine/Core/Runtime/AssetLoaders/AssetManager.h"
#include "Engine/Core/Math/NumericConstants.h"
#include "Engine/Utils/TooolfuncForStarMap.h"
#include "Engine/Core/Runtime/Graphics/Vulkan/ShaderResourceManager.h"
#include <chrono>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

GameScreen::GameScreen(AppContext& context)
    : IScreen(context),
    m_beam_energy("1.919 E+30"),
    m_rkkv_mass("5.14 E+13")
{
    // GameScreen doesn't own the game world, so it shouldn't block its update
    m_blocks_update = false;
}

std::seed_seq seed{ std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
Npgs::System::Generator::FStellarGenerator Gen(seed, Npgs::System::Generator::FStellarGenerator::EStellarTypeGenerationOption::kRandom);

void GameScreen::OnEnter()
{
    // --- All UI creation code from FApplication::ExecuteMainRender is moved here ---
    m_ui_root = std::make_shared<UIRoot>();

    System::I18nManager::Get().RegisterCallback(this, [this]() { this->OnLanguageChanged(); });

    auto& ctx = UIContext::Get();

    m_neural_menu_controller = std::make_shared<UI::NeuralMenu>("ui.manage", "ui.network", "ui.settings", "ui.close_terminal");
    m_ui_root->AddChild(m_neural_menu_controller);

    m_beam_button = std::make_shared<UI::PulsarButton>("ui.status.target_locked", "ui.action.fire_beam", "☼", "ui.label.energy", &m_beam_energy, "ui.unit.joules", true, "beam");
    m_beam_button->m_width = Length::Fixed(40.0f);
    m_beam_button->m_height = Length::Fixed(40.0f);
    m_beam_button->SetAbsolutePos(50.0f, 360.0f);

    m_rkkv_button = std::make_shared<UI::PulsarButton>("ui.status.target_locked", "ui.action.launch_rkkv", m_context.RKKVID, "ui.label.mass", &m_rkkv_mass, "ui.unit.kg", true, "rkkv");
    m_rkkv_button->m_width = Length::Fixed(40.0f);
    m_rkkv_button->m_height = Length::Fixed(40.0f);
    m_rkkv_button->SetAbsolutePos(50.0f, 440.0f);

    m_VN_button = std::make_shared<UI::PulsarButton>("ui.status.target_locked", "ui.action.launch_vn", "⌘", "ui.label.mass", &m_VN_mass, "ui.unit.kg", true, "vn");
    m_VN_button->m_width = Length::Fixed(40.0f);
    m_VN_button->m_height = Length::Fixed(40.0f);
    m_VN_button->SetAbsolutePos(50.0f, 520.0f);

    m_message_button = std::make_shared<UI::PulsarButton>("ui.status.target_locked", "ui.action.send_message", "i", "ui.label.weight_time", &m_VN_mass, "ui.unit.years", false, "message");
    m_message_button->m_width = Length::Fixed(40.0f);
    m_message_button->m_text_label->SetTooltip("tooltip.test");
    m_message_button->m_height = Length::Fixed(40.0f);
    m_message_button->SetAbsolutePos(50.0f, 600.0f);

    // Setup callbacks (copy-pasted from Application.cpp)
    m_beam_button->on_toggle_callback = [this](bool want_expand)
    {
        // 简单的互斥逻辑：打开这个，关闭那个
        if (want_expand)
        {
            m_rkkv_button->SetActive(false);
            m_beam_button->SetActive(true);  // 打开自己
            m_VN_button->SetActive(false);
            m_message_button->SetActive(false);

            // [模拟业务逻辑] 检查是否有目标
            // 假设 _FreeCamera 看着某个方向或者有一个 SelectedTarget 变量

        }
        else
        {
            m_beam_button->SetActive(false);

            m_beam_button->SetExecutable(false); // <--- 关闭时总是设为不可执行
        }
    };

    // 2. 注册 Execute 回调 (按下“发射”文字)
    m_beam_button->on_execute_callback = [this](const std::string& id, const std::string& val)
    {


        NpgsCoreInfo("Command Received: ID={}, Value={}", id, val);


        {
            NpgsCoreInfo("FIRING DYSON BEAM with {} Joules!", val);
            // 触发游戏逻辑...
        }
    };


    m_rkkv_button->on_toggle_callback = [this](bool want_expand)
    {
        if (want_expand)
        {
            m_rkkv_button->SetActive(true);
            m_beam_button->SetActive(false);
            m_VN_button->SetActive(false);
            m_message_button->SetActive(false);

            // 假设 RKKV 总是可以发射
            m_rkkv_button->SetStatus("ui.status.target_locked");
            m_rkkv_button->SetExecutable(true);
        }
        else
        {
            m_rkkv_button->SetActive(false);
            m_rkkv_button->SetExecutable(false);
        }
    };

    m_rkkv_button->on_execute_callback = [this](const std::string& id, const std::string& val)
    {
        SimulateStarSelectionAndUpdateUI();
        m_log_panel->AddLog(System::UI::LogType::Info, Npgs::System::TR("log.event.scan_result_title"), Npgs::System::TR("log.event.scan_result_desc"));
        m_log_panel->AddLog(System::UI::LogType::Alert, Npgs::System::TR("log.event.critical_error_title"), Npgs::System::TR("log.event.critical_error_desc"));
        m_log_panel->SetAutoSaveTime(Npgs::System::TR("ui.log.autosave") + FormatTime(*m_context.GameTime));
        NpgsCoreInfo("LAUNCHING RKKV projectile. Mass: {}", val);
    };

    m_VN_button->on_toggle_callback = [this](bool want_expand)
    {
        if (want_expand)
        {
            m_VN_button->SetActive(true);
            m_beam_button->SetActive(false);
            m_rkkv_button->SetActive(false);
            m_message_button->SetActive(false);
            m_VN_button->SetStatus("ui.status.target_locked");
            m_VN_button->SetExecutable(true);
        }
        else
        {
            m_VN_button->SetActive(false);
            m_VN_button->SetExecutable(false);
        }
    };
    m_VN_button->on_execute_callback = [this](const std::string& id, const std::string& val)
    {

        NpgsCoreInfo("LAUNCHING 冯诺依曼探测器. Mass: {}", val);
    };

    m_message_button->on_toggle_callback = [this](bool want_expand)
    {
        if (want_expand)
        {
            m_message_button->SetActive(true);
            m_beam_button->SetActive(false);
            m_rkkv_button->SetActive(false);
            m_VN_button->SetActive(false);
            m_message_button->SetStatus("ui.status.target_locked");
            m_message_button->SetExecutable(true);
        }
        else
        {
            m_message_button->SetActive(false);
            m_message_button->SetExecutable(false); // <--- 关闭时重置
        }
    };
    m_message_button->on_execute_callback = [this](const std::string& id, const std::string& val)
    {
        if (System::I18nManager::Get().GetCurrentLanguage() == System::I18nManager::Language::English)
        {
            System::I18nManager::Get().SetLanguage(System::I18nManager::Language::Chinese);
        }
        else if (System::I18nManager::Get().GetCurrentLanguage() == System::I18nManager::Language::Chinese)
        {
            System::I18nManager::Get().SetLanguage(System::I18nManager::Language::English);
        }
        NpgsCoreInfo("传输意识至目标，用时: {}", val);
    };
    // ... all other button callbacks ...

    m_ui_root->AddChild(m_beam_button);
    m_ui_root->AddChild(m_rkkv_button);
    m_ui_root->AddChild(m_VN_button);
    m_ui_root->AddChild(m_message_button);

    m_celestial_info = std::make_shared<UI::CelestialInfoPanel>("ui.info", "ui.close_panel", "ui.celestial.progress_label", "ui.celestial.coil_label");
    m_ui_root->AddChild(m_celestial_info);

    m_top_Info = std::make_shared<System::UI::CinematicInfoPanel>(System::UI::CinematicInfoPanel::Position::Top);
    m_bottom_Info = std::make_shared<System::UI::CinematicInfoPanel>(System::UI::CinematicInfoPanel::Position::Bottom);
    m_ui_root->AddChild(m_top_Info);
    m_ui_root->AddChild(m_bottom_Info);

    m_time_control_panel = std::make_shared<System::UI::TimeControlPanel>(m_context.GameTime, m_context.TimeRate, "ui.time.pause", "ui.time.resume", "ui.time.reset_speed");
    m_ui_root->AddChild(m_time_control_panel);

    m_log_panel = std::make_shared<System::UI::LogPanel>("ui.log.system_scan", "ui.log.autosave");
    m_ui_root->AddChild(m_log_panel);

    SimulateStarSelectionAndUpdateUI();

    // Setup sliders (binding to context data)
    m_neural_menu_controller->AddLinear("R", &ctx.m_theme.color_accent.x, 0.0f, 1.0f);
    m_neural_menu_controller->AddLinear("G", &ctx.m_theme.color_accent.y, 0.0f, 1.0f);
    m_neural_menu_controller->AddLinear("B", &ctx.m_theme.color_accent.z, 0.0f, 1.0f);
    m_neural_menu_controller->AddThrottle("BlackHoleMassSol", &BlackHoleArgs.BlackHoleMassSol);
    m_neural_menu_controller->AddThrottle("Spin", &BlackHoleArgs.Spin);
    m_neural_menu_controller->AddThrottle("Mu", &BlackHoleArgs.Mu);
    m_neural_menu_controller->AddThrottle("AccretionRate", &BlackHoleArgs.AccretionRate);
    m_neural_menu_controller->AddThrottle("InterRadiusLy", &BlackHoleArgs.InterRadiusRs);
    m_neural_menu_controller->AddThrottle("OuterRadiusLy", &BlackHoleArgs.OuterRadiusRs);
    m_neural_menu_controller->AddThrottle("ThinLy", &BlackHoleArgs.ThinRs);
    m_neural_menu_controller->AddThrottle("Hopper", &BlackHoleArgs.Hopper, 0.01f);
    m_neural_menu_controller->AddThrottle("Brightmut", &BlackHoleArgs.Brightmut);
    m_neural_menu_controller->AddThrottle("Darkmut", &BlackHoleArgs.Darkmut);
    m_neural_menu_controller->AddThrottle("Reddening", &BlackHoleArgs.Reddening);
    m_neural_menu_controller->AddThrottle("Saturation", &BlackHoleArgs.Saturation, 0.1f);
    m_neural_menu_controller->AddThrottle("BlackbodyIntensityExponent", &BlackHoleArgs.BlackbodyIntensityExponent);
    m_neural_menu_controller->AddThrottle("RedShiftColorExponent", &BlackHoleArgs.RedShiftColorExponent);
    m_neural_menu_controller->AddThrottle("RedShiftIntensityExponent", &BlackHoleArgs.RedShiftIntensityExponent);
    m_neural_menu_controller->AddThrottle("JetRedShiftIntensityExponent", &BlackHoleArgs.JetRedShiftIntensityExponent);
    m_neural_menu_controller->AddThrottle("JetBrightmut", &BlackHoleArgs.JetBrightmut);
    m_neural_menu_controller->AddThrottle("JetSaturation", &BlackHoleArgs.JetSaturation);
    m_neural_menu_controller->AddThrottle("JetShiftMax", &BlackHoleArgs.JetShiftMax);
    // ... and all other sliders, binding to m_context.BlackHoleArgs->...
}

void GameScreen::OnExit()
{
    System::I18nManager::Get().UnregisterCallback(this);
    m_ui_root.reset();
}

void GameScreen::Update(float dt)
{
    // --- All UI update logic from FApplication's main loop is moved here ---
    if (m_ui_root)
    {
        m_ui_root->Update(dt);
    }

    // Update cinematic info panel
    int info_phase = int(*m_context.RealityTime / 2.50f + 1.0) % 3;
    // 阶段 0: 隐藏
    if (info_phase == 0)
    {
        // 传空字符串隐藏
        m_top_Info->SetCivilizationData("", "", "", "");
        m_bottom_Info->SetCelestialData("", "", "", "");
    }
    // 阶段 1: 奇点
    else if (info_phase == 1)
    {
        m_top_Info->SetCivilizationData(
            Npgs::System::TR("cinematic.title.singularity"),
            std::vformat(Npgs::System::TR("cinematic.stat.mass_dominated"), std::make_format_args("2.4e30 kg")),
            std::vformat(Npgs::System::TR("cinematic.stat.star_systems"), std::make_format_args("14201")),
            std::vformat(Npgs::System::TR("cinematic.stat.reward"), std::make_format_args("1.875e+21"))
        );

        m_bottom_Info->SetCelestialData(
            "191981", // ID
            Npgs::System::TR("cinematic.type.black_hole"),
            std::vformat(Npgs::System::TR("cinematic.stat.mass"), std::make_format_args("1.91E+36kg")),
            std::vformat(Npgs::System::TR("cinematic.stat.luminosity"), std::make_format_args("8.10E+30 W"))
        );
    }
    // 阶段 2: 文明
    else if (info_phase == 2)
    {
        m_top_Info->SetCivilizationData(
            Npgs::System::TR("cinematic.title.type2_civ"),
            std::vformat(Npgs::System::TR("cinematic.stat.output"), std::make_format_args("3.8e26 W")),
            std::vformat(Npgs::System::TR("cinematic.stat.progress"), std::make_format_args("84.2%")),
            Npgs::System::TR("cinematic.stat.status_warning")
        );

        m_bottom_Info->SetCelestialData(
            "000001",
            Npgs::System::TR("cinematic.type.red_giant"),
            std::vformat(Npgs::System::TR("cinematic.stat.mass"), std::make_format_args("1.91E+29kg")),
            std::vformat(Npgs::System::TR("cinematic.stat.luminosity"), std::make_format_args("8.10E+99 W"))
        );
    }
    // Update beam button executable state
    bool has_target = (int(0.33 * *m_context.RealityTime) % 2 == 1);
    m_beam_button->SetExecutable(has_target);
    if (has_target)
    {
        m_beam_button->SetStatus("ui.status.target_locked");
        m_beam_button->SetExecutable(true); // <--- 设置为可执行
    }
    else
    {
        m_beam_button->SetStatus("ui.status.no_target");
        m_beam_button->SetExecutable(false); // <--- 设置为不可执行
    }
}

void GameScreen::Draw()
{
    if (m_ui_root)
    {
        m_ui_root->Draw();
    }
}

// --- All helper functions from FApplication are moved here ---
void GameScreen::OnLanguageChanged()
{
    SimulateStarSelectionAndUpdateUI();
    m_log_panel->SetAutoSaveTime(Npgs::System::TR("ui.log.autosave") + FormatTime(*m_context.GameTime));
}

std::string GameScreen::FormatTime(double total_seconds)
{
    // ... same implementation as before ...
    const long SECONDS_PER_DAY = 86400;
    const long DAYS_PER_MONTH = 30;
    const long MONTHS_PER_YEAR = 12;
    const long DAYS_PER_YEAR = DAYS_PER_MONTH * MONTHS_PER_YEAR;

    long long t = static_cast<long long>(total_seconds);

    long long years = t / (DAYS_PER_YEAR * SECONDS_PER_DAY);
    long long rem_seconds = t % (DAYS_PER_YEAR * SECONDS_PER_DAY);
    long long months = rem_seconds / (DAYS_PER_MONTH * SECONDS_PER_DAY);
    rem_seconds %= (DAYS_PER_MONTH * SECONDS_PER_DAY);
    long long days = rem_seconds / SECONDS_PER_DAY;
    rem_seconds %= SECONDS_PER_DAY;
    long long hours = rem_seconds / 3600;
    rem_seconds %= 3600;
    long long minutes = rem_seconds / 60;
    long long seconds = rem_seconds % 60;

    char buf[64];
    std::snprintf(buf, sizeof(buf), "T+%04lld.%02lld.%02lld %02lld:%02lld:%02lld",
        years, months + 1, days + 1, hours, minutes, seconds);

    return std::string(buf);
}

void GameScreen::SimulateStarSelectionAndUpdateUI()
{
    Npgs::Astro::AStar myStar = Gen.GenerateStar();
    Gen.GenerateStar();
    Npgs::System::UI::CelestialData ui_data = Npgs::System::UI::AstroDataBuilder::BuildDataForObject(&myStar);

    std::string subtitle = Npgs::System::UI::AstroDataBuilder::StarPhaseToString(myStar.GetEvolutionPhase());
    m_celestial_info->SetTitle(subtitle + "-114514", subtitle);
    m_celestial_info->SetObjectImage(m_context.stage0ID, 1200, 800, { kelvin_to_rgb(myStar.GetTeff()).r ,kelvin_to_rgb(myStar.GetTeff()).g ,kelvin_to_rgb(myStar.GetTeff()).b ,1.0 });
    m_celestial_info->SetData(ui_data);
}


_UI_END
_SYSTEM_END
_NPGS_END