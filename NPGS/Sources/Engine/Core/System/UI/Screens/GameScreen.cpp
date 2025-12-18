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
    m_ui_root = std::make_shared<UIRoot>();
    //m_ui_root->SetName("gameScreenRoot"); // [新增] 为根命名

    System::I18nManager::Get().RegisterCallback(this, [this]() { this->OnLanguageChanged(); });

    auto& ctx = UIContext::Get();

    // [修改] 创建组件时使用局部变量，然后命名并添加到UI树中
    auto neural_menu_controller = std::make_shared<UI::NeuralMenu>("i18ntext.ui.manage", "i18ntext.ui.network", "i18ntext.ui.settings", "i18ntext.ui.close_terminal");
    neural_menu_controller->SetName("neuralMenu");
    neural_menu_controller->SetAnchor(UI::AnchorPoint::TopLeft, { 20.0f, 20.0f });
    m_ui_root->AddChild(neural_menu_controller);

    neural_menu_controller->GetExitButton()->on_click = [this]()
    {
        m_screen_manager->ScreenManager::RequestChangeScreen("MainMenu");
    };

    // [修改] 对所有 PulsarButton 进行命名
    auto beam_button = std::make_shared<UI::PulsarButton>("i18ntext.ui.status.target_locked", "i18ntext.ui.action.fire_beam", "☼", "i18ntext.ui.label.energy", &m_beam_energy, "i18ntext.ui.unit.joules", true);
    beam_button->SetName("beamButton");
    beam_button->m_width = Length::Fixed(40.0f);
    beam_button->m_height = Length::Fixed(40.0f);
    beam_button->SetAnchor(UI::AnchorPoint::MiddleLeft, { 50.0f, 360.0f - 450.0f });

    auto rkkv_button = std::make_shared<UI::PulsarButton>("i18ntext.ui.status.target_locked", "i18ntext.ui.action.launch_rkkv", m_context.RKKVID, "i18ntext.ui.label.mass", &m_rkkv_mass, "i18ntext.ui.unit.kg", true);
    rkkv_button->SetName("rkkvButton");
    rkkv_button->m_width = Length::Fixed(40.0f);
    rkkv_button->m_height = Length::Fixed(40.0f);
    rkkv_button->SetAnchor(UI::AnchorPoint::MiddleLeft, { 50.0f, 440.0f - 450.0f });

    auto VN_button = std::make_shared<UI::PulsarButton>("i18ntext.ui.status.target_locked", "i18ntext.ui.action.launch_vn", "⌘", "i18ntext.ui.label.mass", &m_VN_mass, "i18ntext.ui.unit.kg", true);
    VN_button->SetName("vnButton");
    VN_button->m_width = Length::Fixed(40.0f);
    VN_button->m_height = Length::Fixed(40.0f);
    VN_button->SetAnchor(UI::AnchorPoint::MiddleLeft, { 50.0f, 520.0f - 450.0f });

    auto message_button = std::make_shared<UI::PulsarButton>("i18ntext.ui.status.target_locked", "i18ntext.ui.action.send_message", "i", "i18ntext.ui.label.weight_time", &m_VN_mass, "i18ntext.ui.unit.years", false);
    message_button->SetName("messageButton");
    message_button->m_text_label->SetTooltip("i18ntext.tooltip.test");
    message_button->m_width = Length::Fixed(40.0f);
    message_button->m_height = Length::Fixed(40.0f);
    message_button->SetAnchor(UI::AnchorPoint::MiddleLeft, { 50.0f, 600.0f - 450.0f });

    // [修改] 在回调函数中通过ID查找组件
    beam_button->on_toggle_callback = [this](bool want_expand)
    {
        if (want_expand)
        {
            if (auto rkkv = m_ui_root->FindElementAs<UI::PulsarButton>("rkkvButton")) rkkv->SetActive(false);
            if (auto beam = m_ui_root->FindElementAs<UI::PulsarButton>("beamButton")) beam->SetActive(true);
            if (auto vn = m_ui_root->FindElementAs<UI::PulsarButton>("vnButton")) vn->SetActive(false);
            if (auto msg = m_ui_root->FindElementAs<UI::PulsarButton>("messageButton")) msg->SetActive(false);
        }
        else
        {
            if (auto beam = m_ui_root->FindElementAs<UI::PulsarButton>("beamButton"))
            {
                beam->SetActive(false);
                beam->SetExecutable(false);
            }
        }
    };
    beam_button->on_execute_callback = [this](const std::string& id, const std::string& val)
    {
        NpgsCoreInfo("Command Received: ID={}, Value={}", id, val);
        NpgsCoreInfo("FIRING DYSON BEAM with {} Joules!", val);
    };

    rkkv_button->on_toggle_callback = [this](bool want_expand)
    {
        if (want_expand)
        {
            if (auto rkkv = m_ui_root->FindElementAs<UI::PulsarButton>("rkkvButton")) { rkkv->SetActive(true); rkkv->SetStatus("i18ntext.ui.status.target_locked"); rkkv->SetExecutable(true); }
            if (auto beam = m_ui_root->FindElementAs<UI::PulsarButton>("beamButton")) beam->SetActive(false);
            if (auto vn = m_ui_root->FindElementAs<UI::PulsarButton>("vnButton")) vn->SetActive(false);
            if (auto msg = m_ui_root->FindElementAs<UI::PulsarButton>("messageButton")) msg->SetActive(false);
        }
        else
        {
            if (auto rkkv = m_ui_root->FindElementAs<UI::PulsarButton>("rkkvButton")) { rkkv->SetActive(false); rkkv->SetExecutable(false); }
        }
    };
    rkkv_button->on_execute_callback = [this](const std::string& id, const std::string& val)
    {
        SimulateStarSelectionAndUpdateUI();
        if (auto log_panel = m_ui_root->FindElementAs<UI::LogPanel>("logPanel"))
        {
            log_panel->AddLog(System::UI::LogType::Info, Npgs::System::TR("i18ntext.log.event.scan_result_title"), Npgs::System::TR("i18ntext.log.event.scan_result_desc"));
            log_panel->AddLog(System::UI::LogType::Alert, Npgs::System::TR("i18ntext.log.event.critical_error_title"), Npgs::System::TR("i18ntext.log.event.critical_error_desc"));
            log_panel->SetAutoSaveTime(Npgs::System::TR("i18ntext.ui.log.autosave") + FormatTime(*m_context.GameTime));
        }
        NpgsCoreInfo("LAUNCHING RKKV projectile. Mass: {}", val);
    };

    // ... 其他按钮回调也做类似修改 ...
    VN_button->on_toggle_callback = [this](bool want_expand)
    {
        if (want_expand)
        {
            // 展开自己，关闭其他
            if (auto vn = m_ui_root->FindElementAs<UI::PulsarButton>("vnButton"))
            {
                vn->SetActive(true);
                vn->SetStatus("i18ntext.ui.status.target_locked");
                vn->SetExecutable(true);
            }
            if (auto beam = m_ui_root->FindElementAs<UI::PulsarButton>("beamButton")) beam->SetActive(false);
            if (auto rkkv = m_ui_root->FindElementAs<UI::PulsarButton>("rkkvButton")) rkkv->SetActive(false);
            if (auto msg = m_ui_root->FindElementAs<UI::PulsarButton>("messageButton")) msg->SetActive(false);
        }
        else
        {
            // 关闭自己
            if (auto vn = m_ui_root->FindElementAs<UI::PulsarButton>("vnButton"))
            {
                vn->SetActive(false);
                vn->SetExecutable(false);
            }
        }
    };
    VN_button->on_execute_callback = [this](const std::string& id, const std::string& val)
    {
        // 此回调不与其他UI组件交互，逻辑保持不变
        NpgsCoreInfo("LAUNCHING 冯诺依曼探测器. Mass: {}", val);
    };

    // [完整实现] Message 按钮回调
    message_button->on_toggle_callback = [this](bool want_expand)
    {
        if (want_expand)
        {
            // 展开自己，关闭其他
            if (auto msg = m_ui_root->FindElementAs<UI::PulsarButton>("messageButton"))
            {
                msg->SetActive(true);
                msg->SetStatus("i18ntext.ui.status.target_locked");
                msg->SetExecutable(true);
            }
            if (auto beam = m_ui_root->FindElementAs<UI::PulsarButton>("beamButton")) beam->SetActive(false);
            if (auto rkkv = m_ui_root->FindElementAs<UI::PulsarButton>("rkkvButton")) rkkv->SetActive(false);
            if (auto vn = m_ui_root->FindElementAs<UI::PulsarButton>("vnButton")) vn->SetActive(false);
        }
        else
        {
            // 关闭自己
            if (auto msg = m_ui_root->FindElementAs<UI::PulsarButton>("messageButton"))
            {
                msg->SetActive(false);
                msg->SetExecutable(false);
            }
        }
    };
    message_button->on_execute_callback = [this](const std::string& id, const std::string& val)
    {
        // 此回调不与其他UI组件交互，逻辑保持不变
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
    m_ui_root->AddChild(beam_button);
    m_ui_root->AddChild(rkkv_button);
    m_ui_root->AddChild(VN_button);
    m_ui_root->AddChild(message_button);

    // [修改] 创建并命名其他组件
    auto celestial_info = std::make_shared<UI::CelestialInfoPanel>("i18ntext.ui.info", "i18ntext.ui.close_panel", "i18ntext.ui.celestial.progress_label", "i18ntext.ui.celestial.coil_label");
    celestial_info->SetName("celestialInfoPanel");
    celestial_info->SetAnchor(UI::AnchorPoint::MiddleRight, { 0.0f, 0.0f });
    m_ui_root->AddChild(celestial_info);

    auto top_Info = std::make_shared<System::UI::CinematicInfoPanel>(System::UI::CinematicInfoPanel::Position::Top);
    top_Info->SetName("topCinematicInfo");
    top_Info->SetAnchor(UI::AnchorPoint::TopCenter, { 0.0f, 10.0f });
    m_ui_root->AddChild(top_Info);

    auto bottom_Info = std::make_shared<System::UI::CinematicInfoPanel>(System::UI::CinematicInfoPanel::Position::Bottom);
    bottom_Info->SetName("bottomCinematicInfo");
    bottom_Info->SetAnchor(UI::AnchorPoint::BottomCenter, { 0.0f, 10.0f });
    m_ui_root->AddChild(bottom_Info);

    auto time_control_panel = std::make_shared<System::UI::TimeControlPanel>(m_context.GameTime, m_context.TimeRate, "i18ntext.ui.time.pause", "i18ntext.ui.time.resume", "i18ntext.ui.time.reset_speed");
    time_control_panel->SetName("timeControlPanel");
    time_control_panel->SetAnchor(UI::AnchorPoint::TopRight, { 20.0f, 20.0f });
    m_ui_root->AddChild(time_control_panel);

    auto log_panel = std::make_shared<System::UI::LogPanel>("i18ntext.ui.log.system_scan", "i18ntext.ui.log.autosave");
    log_panel->SetName("logPanel");
    log_panel->SetAnchor(UI::AnchorPoint::BottomLeft, { 20.0f, 20.0f });
    m_ui_root->AddChild(log_panel);

    m_intro_anim_states.clear();
    m_is_intro_playing = true;
    auto random_delay = []() { return (rand() % 50) / 100.0f; };

    // [修改] RegisterIntroEffect 现在接收指针，但内部会存储ID
    RegisterIntroEffect(neural_menu_controller, 0.1f, 0.8f);
    RegisterIntroEffect(beam_button, 0.2f + random_delay(), 1.0f);
    RegisterIntroEffect(rkkv_button, 0.2f + random_delay(), 1.0f);
    RegisterIntroEffect(VN_button, 0.2f + random_delay(), 1.0f);
    RegisterIntroEffect(message_button, 0.2f + random_delay(), 1.0f);
    RegisterIntroEffect(celestial_info, 0.5f, 0.6f);
    RegisterIntroEffect(top_Info, 0.0f, 1.5f);
    RegisterIntroEffect(bottom_Info, 0.0f, 1.5f);
    RegisterIntroEffect(time_control_panel, 0.8f, 0.5f);
    RegisterIntroEffect(log_panel, 1.0f, 0.5f);

    SimulateStarSelectionAndUpdateUI();

    // Setup sliders (binding to context data)

    auto neural_menu = m_ui_root->FindElementAs<UI::NeuralMenu>("neuralMenu");
    if (neural_menu)
    {
        neural_menu->AddLinear("R", &ctx.m_theme.color_accent.x, 0.0f, 1.0f);
        neural_menu->AddLinear("G", &ctx.m_theme.color_accent.y, 0.0f, 1.0f);
        neural_menu->AddLinear("B", &ctx.m_theme.color_accent.z, 0.0f, 1.0f);
        neural_menu->AddThrottle("BlackHoleMassSol", &BlackHoleArgs.BlackHoleMassSol);
        neural_menu->AddThrottle("Spin", &BlackHoleArgs.Spin);
        neural_menu->AddThrottle("Mu", &BlackHoleArgs.Mu);
        neural_menu->AddThrottle("AccretionRate", &BlackHoleArgs.AccretionRate);
        neural_menu->AddThrottle("InterRadiusLy", &BlackHoleArgs.InterRadiusRs);
        neural_menu->AddThrottle("OuterRadiusLy", &BlackHoleArgs.OuterRadiusRs);
        neural_menu->AddThrottle("ThinLy", &BlackHoleArgs.ThinRs);
        neural_menu->AddThrottle("Hopper", &BlackHoleArgs.Hopper, 0.01f);
        neural_menu->AddThrottle("Brightmut", &BlackHoleArgs.Brightmut);
        neural_menu->AddThrottle("Darkmut", &BlackHoleArgs.Darkmut);
        neural_menu->AddThrottle("Reddening", &BlackHoleArgs.Reddening);
        neural_menu->AddThrottle("Saturation", &BlackHoleArgs.Saturation, 0.1f);
        neural_menu->AddThrottle("BlackbodyIntensityExponent", &BlackHoleArgs.BlackbodyIntensityExponent);
        neural_menu->AddThrottle("RedShiftColorExponent", &BlackHoleArgs.RedShiftColorExponent);
        neural_menu->AddThrottle("RedShiftIntensityExponent", &BlackHoleArgs.RedShiftIntensityExponent);
        neural_menu->AddThrottle("JetRedShiftIntensityExponent", &BlackHoleArgs.JetRedShiftIntensityExponent);
        neural_menu->AddThrottle("JetBrightmut", &BlackHoleArgs.JetBrightmut);
        neural_menu->AddThrottle("JetSaturation", &BlackHoleArgs.JetSaturation);
        neural_menu->AddThrottle("JetShiftMax", &BlackHoleArgs.JetShiftMax);
    }
    
}

void GameScreen::RegisterIntroEffect(const std::shared_ptr<UIElement>& el, float delay, float duration)
{
    if (!el) return;
    el->m_alpha = 0.0f;
    IntroAnimState state;
    state.element_id = el->GetID(); // [修改] 存储ID而不是指针
    state.delay_time = delay;
    state.flicker_duration = duration;
    state.timer = 0.0f;
    state.is_finished = false;
    state.current_flicker_timer = 0.0f;
    state.time_until_next_change = 0.0f; // 设为0确保第一帧立即进行一次判定
    m_intro_anim_states.push_back(state);
}

void GameScreen::OnExit()
{
    System::I18nManager::Get().UnregisterCallback(this);
    m_ui_root.reset();
}

void GameScreen::Update(float dt)
{
    if (m_is_intro_playing)
    {
        bool all_finished = true;
        for (auto& state : m_intro_anim_states)
        {
            if (state.is_finished) continue;

            // [核心修改 1] 通过 ID 动态查找元素，而不是使用存储的指针
            auto element = m_ui_root->FindElementByID(state.element_id);
            if (!element)
            {
                // 如果找不到元素（例如，它被移除了），就将此动画标记为完成
                state.is_finished = true;
                continue;
            }

            state.timer += dt;

            // 1. 等待阶段
            if (state.timer < state.delay_time)
            {
                // [核心修改 2] 使用查找到的 element 指针
                element->m_alpha = 0.0f;
                all_finished = false;
            }
            // 2. 闪烁阶段
            else if (state.timer < (state.delay_time + state.flicker_duration))
            {
                all_finished = false;

                // 累加当前闪烁状态的计时
                state.current_flicker_timer += dt;

                // 只有当计时超过了设定的间隔，才改变状态
                if (state.current_flicker_timer >= state.time_until_next_change)
                {
                    // 重置计时器
                    state.current_flicker_timer = 0.0f;

                    // 随机生成下一次状态改变的时间间隔
                    state.time_until_next_change = 0.1 * (0.02f + (static_cast<float>(rand() % 60) / 1000.0f));

                    // --- 原有的概率计算逻辑 ---
                    float progress = (state.timer - state.delay_time) / state.flicker_duration;
                    float probability = progress * progress;
                    float dice = (rand() % 100) / 100.0f;

                    if (dice < probability)
                    {
                        // 亮起：随机亮度
                        // [核心修改 2] 使用查找到的 element 指针
                        element->m_alpha = 0.5f + ((rand() % 50) / 100.0f);
                    }
                    else
                    {
                        // 熄灭
                        // [核心修改 2] 使用查找到的 element 指针
                        element->m_alpha = 0.0f;
                    }
                }
                // 如果时间没到，则保持上一帧的 Alpha 值不变
            }
            // 3. 完成阶段
            else
            {
                // [核心修改 2] 使用查找到的 element 指针
                element->m_alpha = 1.0f;
                state.is_finished = true;
            }
        }

        if (all_finished)
        {
            m_is_intro_playing = false;
            m_intro_anim_states.clear();
        }
    }
    // --- All UI update logic from FApplication's main loop is moved here ---
    if (m_ui_root)
    {
        m_ui_root->Update(dt);
    }

    // Update cinematic info panel
    int info_phase = int(*m_context.RealityTime / 2.50f + 1.0) % 3;
    auto top_info = m_ui_root->FindElementAs<UI::CinematicInfoPanel>("topCinematicInfo");
    auto bottom_info = m_ui_root->FindElementAs<UI::CinematicInfoPanel>("bottomCinematicInfo");
    // 阶段 0: 隐藏
    if (top_info != nullptr && bottom_info != nullptr)
    {
        if (info_phase == 0)
        {
            // 传空字符串隐藏
            top_info->SetCivilizationData("", "", "", "");
            bottom_info->SetCelestialData("", "", "", "");
        }
        // 阶段 1: 奇点
        else if (info_phase == 1)
        {
            top_info->SetCivilizationData(
                Npgs::System::TR("i18ntext.cinematic.title.singularity"),
                std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.mass_dominated"), std::make_format_args("2.4e30 kg")),
                std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.star_systems"), std::make_format_args("14201")),
                std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.reward"), std::make_format_args("1.875e+21"))
            );

            bottom_info->SetCelestialData(
                "191981", // ID
                Npgs::System::TR("i18ntext.cinematic.type.black_hole"),
                std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.mass"), std::make_format_args("1.91E+36kg")),
                std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.luminosity"), std::make_format_args("8.10E+30 W"))
            );
        }
        // 阶段 2: 文明
        else if (info_phase == 2)
        {
            top_info->SetCivilizationData(
                Npgs::System::TR("i18ntext.cinematic.title.type2_civ"),
                std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.output"), std::make_format_args("3.8e26 W")),
                std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.progress"), std::make_format_args("84.2%")),
                Npgs::System::TR("i18ntext.cinematic.stat.status_warning")
            );

            bottom_info->SetCelestialData(
                "000001",
                Npgs::System::TR("i18ntext.cinematic.type.red_giant"),
                std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.mass"), std::make_format_args("1.91E+29kg")),
                std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.luminosity"), std::make_format_args("8.10E+99 W"))
            );
        }
    }
    // Update beam button executable state
    if (auto beam_button = m_ui_root->FindElementAs<UI::PulsarButton>("beamButton"))
    {
        bool has_target = (int(0.33 * *m_context.RealityTime) % 2 == 1);
        beam_button->SetExecutable(has_target);
        if (has_target)
        {
            beam_button->SetStatus("i18ntext.ui.status.target_locked");
            beam_button->SetExecutable(true);
        }
        else
        {
            beam_button->SetStatus("i18ntext.ui.status.no_target");
            beam_button->SetExecutable(false);
        }
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
    if (auto log_panel = m_ui_root->FindElementAs<UI::LogPanel>("logPanel"))
    {
        log_panel->SetAutoSaveTime(Npgs::System::TR("i18ntext.ui.log.autosave") + FormatTime(*m_context.GameTime));
    }
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

    // [修改] 使用ID查找
    if (auto panel = m_ui_root->FindElementAs<UI::CelestialInfoPanel>("celestialInfoPanel"))
    {
        std::string subtitle = Npgs::System::UI::AstroDataBuilder::StarPhaseToString(myStar.GetEvolutionPhase());
        panel->SetTitle(subtitle + "-114514", subtitle);
        panel->SetObjectImage(m_context.stage0ID, 1200, 800, { kelvin_to_rgb(myStar.GetTeff()).r ,kelvin_to_rgb(myStar.GetTeff()).g ,kelvin_to_rgb(myStar.GetTeff()).b ,1.0 });
        panel->SetData(ui_data);
    }
}


_UI_END
_SYSTEM_END
_NPGS_END