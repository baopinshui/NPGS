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

    m_neural_menu_controller = std::make_shared<UI::NeuralMenu>("i18ntext.ui.manage", "i18ntext.ui.network", "i18ntext.ui.settings", "i18ntext.ui.close_terminal");
    m_neural_menu_controller->SetAnchor(UI::AnchorPoint::TopLeft, { 20.0f, 20.0f });
    m_ui_root->AddChild(m_neural_menu_controller);



    m_neural_menu_controller->GetExitButton()->on_click = [this]()
    {
        m_screen_manager->ScreenManager::RequestChangeScreen("MainMenu");// OnExit();
    };
    m_beam_button = std::make_shared<UI::PulsarButton>("i18ntext.ui.status.target_locked", "i18ntext.ui.action.fire_beam", "☼", "i18ntext.ui.label.energy", &m_beam_energy, "i18ntext.ui.unit.joules", true, "beam");
    m_beam_button->m_width = Length::Fixed(40.0f);
    m_beam_button->m_height = Length::Fixed(40.0f);
    m_beam_button->SetAnchor(UI::AnchorPoint::MiddleLeft, { 50.0f, 360.0f-450.0f });

    m_rkkv_button = std::make_shared<UI::PulsarButton>("i18ntext.ui.status.target_locked", "i18ntext.ui.action.launch_rkkv", m_context.RKKVID, "i18ntext.ui.label.mass", &m_rkkv_mass, "i18ntext.ui.unit.kg", true, "rkkv");
    m_rkkv_button->m_width = Length::Fixed(40.0f);
    m_rkkv_button->m_height = Length::Fixed(40.0f);
    m_rkkv_button->SetAnchor(UI::AnchorPoint::MiddleLeft, { 50.0f, 440.0f - 450.0f });

    m_VN_button = std::make_shared<UI::PulsarButton>("i18ntext.ui.status.target_locked", "i18ntext.ui.action.launch_vn", "⌘", "i18ntext.ui.label.mass", &m_VN_mass, "i18ntext.ui.unit.kg", true, "vn");
    m_VN_button->m_width = Length::Fixed(40.0f);
    m_VN_button->m_height = Length::Fixed(40.0f);
    m_VN_button->SetAnchor(UI::AnchorPoint::MiddleLeft, { 50.0f, 520.0f - 450.0f });

    m_message_button = std::make_shared<UI::PulsarButton>("i18ntext.ui.status.target_locked", "i18ntext.ui.action.send_message", "i", "i18ntext.ui.label.weight_time", &m_VN_mass, "i18ntext.ui.unit.years", false, "message");
    m_message_button->m_width = Length::Fixed(40.0f);
    m_message_button->m_text_label->SetTooltip("i18ntext.tooltip.test");
    m_message_button->m_height = Length::Fixed(40.0f);
    m_message_button->SetAnchor(UI::AnchorPoint::MiddleLeft, { 50.0f, 600.0f - 450.0f });

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
            m_rkkv_button->SetStatus("i18ntext.ui.status.target_locked");
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
        m_log_panel->AddLog(System::UI::LogType::Info, Npgs::System::TR("i18ntext.log.event.scan_result_title"), Npgs::System::TR("i18ntext.log.event.scan_result_desc"));
        m_log_panel->AddLog(System::UI::LogType::Alert, Npgs::System::TR("i18ntext.log.event.critical_error_title"), Npgs::System::TR("i18ntext.log.event.critical_error_desc"));
        m_log_panel->SetAutoSaveTime(Npgs::System::TR("i18ntext.ui.log.autosave") + FormatTime(*m_context.GameTime));
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
            m_VN_button->SetStatus("i18ntext.ui.status.target_locked");
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
            m_message_button->SetStatus("i18ntext.ui.status.target_locked");
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

    m_celestial_info = std::make_shared<UI::CelestialInfoPanel>("i18ntext.ui.info", "i18ntext.ui.close_panel", "i18ntext.ui.celestial.progress_label", "i18ntext.ui.celestial.coil_label");
    // [修改] CelestialInfoPanel 也使用锚点定位
    m_celestial_info->SetAnchor(UI::AnchorPoint::MiddleRight, { 0.0f, 0.0f });
    m_ui_root->AddChild(m_celestial_info);

    m_top_Info = std::make_shared<System::UI::CinematicInfoPanel>(System::UI::CinematicInfoPanel::Position::Top);
    m_bottom_Info = std::make_shared<System::UI::CinematicInfoPanel>(System::UI::CinematicInfoPanel::Position::Bottom);
    // [修改] 为 CinematicInfoPanel 设置锚点
    m_top_Info->SetAnchor(UI::AnchorPoint::TopCenter, { 0.0f, 10.0f });
    m_bottom_Info->SetAnchor(UI::AnchorPoint::BottomCenter, { 0.0f, 10.0f });
    m_ui_root->AddChild(m_top_Info);
    m_ui_root->AddChild(m_bottom_Info);

    m_time_control_panel = std::make_shared<System::UI::TimeControlPanel>(m_context.GameTime, m_context.TimeRate, "i18ntext.ui.time.pause", "i18ntext.ui.time.resume", "i18ntext.ui.time.reset_speed");
    // [修改] 为 TimeControlPanel 设置锚点
    m_time_control_panel->SetAnchor(UI::AnchorPoint::TopRight, { 20.0f, 20.0f });
    m_ui_root->AddChild(m_time_control_panel);

    m_log_panel = std::make_shared<System::UI::LogPanel>("i18ntext.ui.log.system_scan", "i18ntext.ui.log.autosave");
    // [修改] 为 LogPanel 设置锚点
    m_log_panel->SetAnchor(UI::AnchorPoint::BottomLeft, { 20.0f, 20.0f });
    m_ui_root->AddChild(m_log_panel);
    m_intro_anim_states.clear();
    m_is_intro_playing = true;

    // 辅助 lambda：生成 0.0 ~ 0.5 之间的随机延迟，让它们不同步
    auto random_delay = []() { return (rand() % 50) / 100.0f; };

    // 注册核心组件
    // 参数: (元素指针, 延迟时间, 闪烁持续时间)

    // 菜单稍微早一点出现
    RegisterIntroEffect(m_neural_menu_controller, 0.1f, 0.8f);

    // 按钮组带有不同的随机延迟
    RegisterIntroEffect(m_beam_button, 0.2f + random_delay(), 1.0f);
    RegisterIntroEffect(m_rkkv_button, 0.2f + random_delay(), 1.0f);
    RegisterIntroEffect(m_VN_button, 0.2f + random_delay(), 1.0f);
    RegisterIntroEffect(m_message_button, 0.2f + random_delay(), 1.0f);

    // 信息面板稍晚一点
    RegisterIntroEffect(m_celestial_info, 0.5f, 0.6f);

    // 上下电影黑边和时间控制
    RegisterIntroEffect(m_top_Info, 0.0f, 1.5f);
    RegisterIntroEffect(m_bottom_Info, 0.0f, 1.5f);
    RegisterIntroEffect(m_time_control_panel, 0.8f, 0.5f);
    RegisterIntroEffect(m_log_panel, 1.0f, 0.5f);

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
    
}

void GameScreen::RegisterIntroEffect(std::shared_ptr<UIElement> el, float delay, float duration)
{
    if (!el) return;

    // 初始设为不可见
    el->m_alpha = 0.0f;

    IntroAnimState state;
    state.element = el;
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

            state.timer += dt;

            // 1. 等待阶段
            if (state.timer < state.delay_time)
            {
                state.element->m_alpha = 0.0f;
                all_finished = false;
            }
            // 2. 闪烁阶段
            else if (state.timer < (state.delay_time + state.flicker_duration))
            {
                all_finished = false;

                // [核心修改] 累加当前闪烁状态的计时
                state.current_flicker_timer += dt;

                // 只有当计时超过了设定的间隔，才改变状态
                // 这样无论 FPS 是 30 还是 144，闪烁的频率都由时间决定
                if (state.current_flicker_timer >= state.time_until_next_change)
                {
                    // 重置计时器
                    state.current_flicker_timer = 0.0f;

                    // 随机生成下一次状态改变的时间间隔 (0.02s ~ 0.08s)
                    // 这个范围决定了闪烁的"节奏感"，0.02-0.08 是比较典型的科幻故障频率
                    state.time_until_next_change =0.1*( 0.02f + (static_cast<float>(rand() % 60) / 1000.0f));

                    // --- 原有的概率计算逻辑 ---
                    float progress = (state.timer - state.delay_time) / state.flicker_duration;

                    // 增加一点非线性，让最后阶段更稳定
                    // progress * progress 会让前期更难亮起，后期迅速变亮
                    float probability = progress * progress;

                    float dice = (rand() % 100) / 100.0f;

                    if (dice < probability)
                    {
                        // 亮起：随机亮度 (0.5 ~ 1.0) 模拟电压不稳
                        state.element->m_alpha = 0.5f + ((rand() % 50) / 100.0f);
                    }
                    else
                    {
                        // 熄灭
                        state.element->m_alpha = 0.0f;
                    }
                }
                // 重点：如果 if 条件不满足（时间没到），什么都不做。
                // 这样 Alpha 值会保持上一帧的状态，直到下一次随机时间点到来。
                // 这就是 "Sample and Hold" 机制，保证了视觉频率与帧率解耦。
            }
            // 3. 完成阶段
            else
            {
                state.element->m_alpha = 1.0f;
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
            Npgs::System::TR("i18ntext.cinematic.title.singularity"),
            std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.mass_dominated"), std::make_format_args("2.4e30 kg")),
            std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.star_systems"), std::make_format_args("14201")),
            std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.reward"), std::make_format_args("1.875e+21"))
        );

        m_bottom_Info->SetCelestialData(
            "191981", // ID
            Npgs::System::TR("i18ntext.cinematic.type.black_hole"),
            std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.mass"), std::make_format_args("1.91E+36kg")),
            std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.luminosity"), std::make_format_args("8.10E+30 W"))
        );
    }
    // 阶段 2: 文明
    else if (info_phase == 2)
    {
        m_top_Info->SetCivilizationData(
            Npgs::System::TR("i18ntext.cinematic.title.type2_civ"),
            std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.output"), std::make_format_args("3.8e26 W")),
            std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.progress"), std::make_format_args("84.2%")),
            Npgs::System::TR("i18ntext.cinematic.stat.status_warning")
        );

        m_bottom_Info->SetCelestialData(
            "000001",
            Npgs::System::TR("i18ntext.cinematic.type.red_giant"),
            std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.mass"), std::make_format_args("1.91E+29kg")),
            std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.luminosity"), std::make_format_args("8.10E+99 W"))
        );
    }
    // Update beam button executable state
    bool has_target = (int(0.33 * *m_context.RealityTime) % 2 == 1);
    m_beam_button->SetExecutable(has_target);
    if (has_target)
    {
        m_beam_button->SetStatus("i18ntext.ui.status.target_locked");
        m_beam_button->SetExecutable(true); // <--- 设置为可执行
    }
    else
    {
        m_beam_button->SetStatus("i18ntext.ui.status.no_target");
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
    m_log_panel->SetAutoSaveTime(Npgs::System::TR("i18ntext.ui.log.autosave") + FormatTime(*m_context.GameTime));
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