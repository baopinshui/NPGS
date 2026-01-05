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
    m_rkkv_mass("5.14 E+13"),
    m_VN_mass("1.0 E+4") // 假设一个初始值
{
    // GameScreen 不拥有游戏世界，所以它不应该阻塞其更新
    m_blocks_update = false;

    // 在构造时注册所有此屏幕需要的UI组件
    RegisterUIComponents();
}

void GameScreen::RegisterUIComponents()
{
    auto& factory = UIFactory::Get();

    // 注册所有 GameScreen 需要的自定义组件
    // 对于有复杂构造函数的，我们提供一个lambda来创建默认实例。
    // 真正的参数将在OnEnter中通过代码设置。
    factory.Register<UIRoot>("UIRoot");

    factory.Register("NeuralMenu", []()
    {
        return std::make_shared<UI::NeuralMenu>(
            "i18ntext.ui.manage",    // Key1: 主按钮文字1
            "i18ntext.ui.network",   // Key2: 主按钮文字2
            "i18ntext.ui.settings",  // Settings Key
            "i18ntext.ui.close_terminal" // Close Key
        );
    });

    // PulsarButton现在可以创建一个“空白”版本，稍后用SetData填充
    factory.Register("PulsarButton", []()
    {
        return std::make_shared<UI::PulsarButton>("", "", " ", "", nullptr, "", false);
    });

    factory.Register("CelestialInfoPanel", []()
    {
        return std::make_shared<UI::CelestialInfoPanel>("i18ntext.ui.info", "i18ntext.ui.close_panel", "", "");
    });

    factory.Register("CinematicInfoPanel_Top", []()
    {
        return std::make_shared<System::UI::CinematicInfoPanel>(System::UI::CinematicInfoPanel::Position::Top);
    });
    factory.Register("CinematicInfoPanel_Bottom", []()
    {
        return std::make_shared<System::UI::CinematicInfoPanel>(System::UI::CinematicInfoPanel::Position::Bottom);
    });

    factory.Register("TimeControlPanel", [this]()
    {
        // 构造函数需要指针，这里在创建时直接传入真实的指针
        return std::make_shared<System::UI::TimeControlPanel>(m_context.GameTime, m_context.TimeRate, "i18ntext.ui.time.pause", "i18ntext.ui.time.resume", "i18ntext.ui.time.reset_speed");
    });

    factory.Register("LogPanel", []()
    {
        return std::make_shared<System::UI::LogPanel>("i18ntext.ui.log.system_scan", "i18ntext.ui.log.autosave");
    });
}

// 随机数生成器 (与原版保持一致)
std::seed_seq seed{ std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() };
Npgs::System::Generator::FStellarGenerator Gen(seed, Npgs::System::Generator::FStellarGenerator::EStellarTypeGenerationOption::kRandom);

void GameScreen::OnEnter()
{
    // --- 1. 从JSON加载UI骨架 ---
    UILoader loader;
    m_ui_root = loader.LoadFromFile("Assets/UI/game_screen.json");

    if (!m_ui_root)
    {
        NpgsCoreError("Failed to load GameScreen UI from JSON, creating empty root.");
        m_ui_root = std::make_shared<UIRoot>();
        return;
    }

    System::I18nManager::Get().RegisterCallback(this, [this]() { this->OnLanguageChanged(); });
    auto& ctx = UIContext::Get();

    // --- 2. 在C++中进行“连接”和动态设置 ---
    // 通过名称查找加载的UI元素，并为其注入数据和逻辑

    // 连接 NeuralMenu
    if (auto menu = m_ui_root->FindElementAs<UI::NeuralMenu>("gameScreenRoot.neuralMenu"))
    {
 
        menu->GetExitButton()->on_click = [this]()
        {
            m_screen_manager->RequestChangeScreen("MainMenu");
        };

        // 添加滑块 (动态数据绑定)
        menu->AddLinear("R", &ctx.m_theme.color_accent.x, 0.0f, 1.0f);
        menu->AddLinear("G", &ctx.m_theme.color_accent.y, 0.0f, 1.0f);
        menu->AddLinear("B", &ctx.m_theme.color_accent.z, 0.0f, 1.0f);
        menu->AddThrottle("Fov", &cfov);
        menu->AddThrottle("BlackHoleMassSol", &BlackHoleArgs.BlackHoleMassSol);
        menu->AddThrottle("Spin", &BlackHoleArgs.Spin,0.1f);
        menu->AddThrottle("Mu", &BlackHoleArgs.Mu);
        menu->AddThrottle("AccretionRate", &BlackHoleArgs.AccretionRate);
        menu->AddThrottle("InterRadiusLy", &BlackHoleArgs.InterRadiusRs);
        menu->AddThrottle("OuterRadiusLy", &BlackHoleArgs.OuterRadiusRs);
        menu->AddThrottle("ThinLy", &BlackHoleArgs.ThinRs);
        menu->AddThrottle("Hopper", &BlackHoleArgs.Hopper, 0.01f);
        menu->AddThrottle("Brightmut", &BlackHoleArgs.Brightmut);
        menu->AddThrottle("Darkmut", &BlackHoleArgs.Darkmut);
        menu->AddThrottle("Reddening", &BlackHoleArgs.Reddening);
        menu->AddThrottle("Saturation", &BlackHoleArgs.Saturation, 0.1f);
        menu->AddThrottle("BlackbodyIntensityExponent", &BlackHoleArgs.BlackbodyIntensityExponent);
        menu->AddThrottle("RedShiftColorExponent", &BlackHoleArgs.RedShiftColorExponent);
        menu->AddThrottle("RedShiftIntensityExponent", &BlackHoleArgs.RedShiftIntensityExponent);
        menu->AddThrottle("JetRedShiftIntensityExponent", &BlackHoleArgs.JetRedShiftIntensityExponent);
        menu->AddThrottle("JetBrightmut", &BlackHoleArgs.JetBrightmut);
        menu->AddThrottle("JetSaturation", &BlackHoleArgs.JetSaturation);
        menu->AddThrottle("JetShiftMax", &BlackHoleArgs.JetShiftMax);
    }

    // 连接 PulsarButtons
    if (auto beam_button = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.beamButton"))
    {
        beam_button->SetData("i18ntext.ui.status.target_locked", "i18ntext.ui.action.fire_beam", "☼", "i18ntext.ui.label.energy", &m_beam_energy, "i18ntext.ui.unit.joules", true);
        beam_button->on_toggle_callback = [this](bool want_expand)
        {
            if (want_expand)
            {
                if (auto rkkv = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.rkkvButton")) rkkv->SetActive(false);
                if (auto beam = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.beamButton")) beam->SetActive(true);
                if (auto vn = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.vnButton")) vn->SetActive(false);
                if (auto msg = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.messageButton")) msg->SetActive(false);
            }
            else
            {
                if (auto beam = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.beamButton"))
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
    }

    if (auto rkkv_button = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.rkkvButton"))
    {
        rkkv_button->SetData("i18ntext.ui.status.target_locked", "i18ntext.ui.action.launch_rkkv", m_context.RKKVID, "i18ntext.ui.label.mass", &m_rkkv_mass, "i18ntext.ui.unit.kg", true);
        rkkv_button->on_toggle_callback = [this](bool want_expand)
        {
            if (want_expand)
            {
                if (auto rkkv = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.rkkvButton")) { rkkv->SetActive(true); rkkv->SetStatus("i18ntext.ui.status.target_locked"); rkkv->SetExecutable(true); }
                if (auto beam = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.beamButton")) beam->SetActive(false);
                if (auto vn = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.vnButton")) vn->SetActive(false);
                if (auto msg = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.messageButton")) msg->SetActive(false);
            }
            else
            {
                if (auto rkkv = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.rkkvButton")) { rkkv->SetActive(false); rkkv->SetExecutable(false); }
            }
        };
        rkkv_button->on_execute_callback = [this](const std::string& id, const std::string& val)
        {
            SimulateStarSelectionAndUpdateUI();
            if (auto log_panel = m_ui_root->FindElementAs<UI::LogPanel>("gameScreenRoot.logPanel"))
            {
                log_panel->AddLog(System::UI::LogType::Info, Npgs::System::TR("i18ntext.log.event.scan_result_title"), Npgs::System::TR("i18ntext.log.event.scan_result_desc"));
                log_panel->AddLog(System::UI::LogType::Alert, Npgs::System::TR("i18ntext.log.event.critical_error_title"), Npgs::System::TR("i18ntext.log.event.critical_error_desc"));
                log_panel->SetAutoSaveTime(Npgs::System::TR("i18ntext.ui.log.autosave") + FormatTime(*m_context.GameTime));
            }
            NpgsCoreInfo("LAUNCHING RKKV projectile. Mass: {}", val);
        };

        if (auto clickable_text = m_ui_root->FindElementAs<UI::TechText>("gameScreenRoot.rkkvButton.label")) {
            clickable_text->SetTooltip("i18ntext.tooltip.test");
        }
    }

    if (auto vn_button = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.vnButton"))
    {
        vn_button->SetData("i18ntext.ui.status.target_locked", "i18ntext.ui.action.launch_vn", "⌘", "i18ntext.ui.label.mass", &m_VN_mass, "i18ntext.ui.unit.kg", true);
        vn_button->on_toggle_callback = [this](bool want_expand)
        {
            if (want_expand)
            {
                if (auto vn = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.vnButton")) { vn->SetActive(true); vn->SetStatus("i18ntext.ui.status.target_locked"); vn->SetExecutable(true); }
                if (auto beam = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.beamButton")) beam->SetActive(false);
                if (auto rkkv = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.rkkvButton")) rkkv->SetActive(false);
                if (auto msg = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.messageButton")) msg->SetActive(false);
            }
            else
            {
                if (auto vn = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.vnButton")) { vn->SetActive(false); vn->SetExecutable(false); }
            }
        };
        vn_button->on_execute_callback = [this](const std::string& id, const std::string& val)
        {
            NpgsCoreInfo("LAUNCHING 冯诺依曼探测器. Mass: {}", val);
        };
    }

    if (auto message_button = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.messageButton"))
    {
        message_button->SetData("i18ntext.ui.status.target_locked", "i18ntext.ui.action.send_message", "i", "i18ntext.ui.label.weight_time", &m_VN_mass, "i18ntext.ui.unit.years", false);
        message_button->on_toggle_callback = [this](bool want_expand)
        {
            if (want_expand)
            {
                if (auto msg = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.messageButton")) { msg->SetActive(true); msg->SetStatus("i18ntext.ui.status.target_locked"); msg->SetExecutable(true); }
                if (auto beam = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.beamButton")) beam->SetActive(false);
                if (auto rkkv = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.rkkvButton")) rkkv->SetActive(false);
                if (auto vn = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.vnButton")) vn->SetActive(false);
            }
            else
            {
                if (auto msg = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.messageButton")) { msg->SetActive(false); msg->SetExecutable(false); }
            }
        };
        message_button->on_execute_callback = [this](const std::string& id, const std::string& val)
        {
            auto& i18n = System::I18nManager::Get();
            const auto& available_languages = i18n.GetAvailableLanguages();

            // 确保至少有两种语言可供切换
            if (available_languages.size() < 2)
            {
                return; // 或者记录一条日志
            }

            const std::string& current_lang_code = i18n.GetCurrentLanguageCode();

            // 2. 查找当前语言在列表中的索引
            size_t current_index = 0;
            for (size_t i = 0; i < available_languages.size(); ++i)
            {
                if (available_languages[i].code == current_lang_code)
                {
                    current_index = i;
                    break;
                }
            }

            // 3. 计算下一个语言的索引，使用取模运算实现循环
            size_t next_index = (current_index + 1) % available_languages.size();

            // 4. 获取下一个语言的 code 并设置
            const std::string& next_lang_code = available_languages[next_index].code;
            i18n.SetLanguage(next_lang_code);
            NpgsCoreInfo("传输意识至目标，用时: {}", val);
            NpgsCoreInfo("切换语言至{}", next_lang_code);
        };
    }


    // --- 3. 注册入场动画 ---
    m_intro_anim_states.clear();
    m_is_intro_playing = true;
    auto random_delay = []() { return (rand() % 50) / 100.0f; };

    // 辅助lambda，用于简化查找和注册的重复代码
    auto find_and_register = [&](const std::string& name, float delay, float duration)
    {
        if (auto element = m_ui_root->FindElementByID(name))
        {
            RegisterIntroEffect(element->shared_from_this(), delay, duration);
        }
        else
        {
            NpgsCoreWarn("GameScreen::OnEnter - Could not find element '{}' to register for intro animation.", name);
        }
    };

    find_and_register("gameScreenRoot.neuralMenu", 0.1f, 0.8f);
    find_and_register("gameScreenRoot.beamButton", 0.2f + random_delay(), 1.0f);
    find_and_register("gameScreenRoot.rkkvButton", 0.2f + random_delay(), 1.0f);
    find_and_register("gameScreenRoot.vnButton", 0.2f + random_delay(), 1.0f);
    find_and_register("gameScreenRoot.messageButton", 0.2f + random_delay(), 1.0f);
    find_and_register("gameScreenRoot.celestialInfoPanel", 0.5f, 0.6f);
    find_and_register("gameScreenRoot.topCinematicInfo", 0.0f, 1.5f);
    find_and_register("gameScreenRoot.bottomCinematicInfo", 0.0f, 1.5f);
    find_and_register("gameScreenRoot.timeControlPanel", 0.8f, 0.5f);
    find_and_register("gameScreenRoot.logPanel", 1.0f, 0.5f);


    // --- 4. 首次数据填充 ---
    SimulateStarSelectionAndUpdateUI();
}

void GameScreen::RegisterIntroEffect(const std::shared_ptr<UIElement>& el, float delay, float duration)
{
    if (!el) return;
    el->m_alpha = 0.0f;
    IntroAnimState state;
    state.element_id = el->GetID(); // 存储ID而不是指针
    state.delay_time = delay;
    state.flicker_duration = duration;
    state.timer = 0.0f;
    state.is_finished = false;
    state.current_flicker_timer = 0.0f;
    state.time_until_next_change = 0.0f;
    m_intro_anim_states.push_back(state);
}

void GameScreen::OnExit()
{
    System::I18nManager::Get().UnregisterCallback(this);
    m_ui_root.reset();
}

void GameScreen::Update(float dt)
{
    if (!m_ui_root) return;

    if (m_is_intro_playing)
    {
        bool all_finished = true;
        for (auto& state : m_intro_anim_states)
        {
            if (state.is_finished) continue;

            auto element = m_ui_root->FindElementByID(state.element_id);
            if (!element)
            {
                state.is_finished = true;
                continue;
            }

            state.timer += dt;

            if (state.timer < state.delay_time)
            {
                element->m_alpha = 0.0f;
                all_finished = false;
            }
            else if (state.timer < (state.delay_time + state.flicker_duration))
            {
                all_finished = false;
                state.current_flicker_timer += dt;

                if (state.current_flicker_timer >= state.time_until_next_change)
                {
                    state.current_flicker_timer = 0.0f;
                    state.time_until_next_change = 0.1f * (0.02f + (static_cast<float>(rand() % 60) / 1000.0f));
                    float progress = (state.timer - state.delay_time) / state.flicker_duration;
                    float probability = progress * progress;
                    float dice = (rand() % 100) / 100.0f;
                    if (dice < probability)
                    {
                        element->m_alpha = 0.5f + ((rand() % 50) / 100.0f);
                    }
                    else
                    {
                        element->m_alpha = 0.0f;
                    }
                }
            }
            else
            {
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

    m_ui_root->Update(dt);

    // Update cinematic info panel
    int info_phase = int(*m_context.RealityTime / 2.50f + 1.0) % 3;
    auto top_info = m_ui_root->FindElementAs<UI::CinematicInfoPanel>("gameScreenRoot.topCinematicInfo");
    auto bottom_info = m_ui_root->FindElementAs<UI::CinematicInfoPanel>("gameScreenRoot.bottomCinematicInfo");
    if (top_info != nullptr && bottom_info != nullptr)
    {
        if (info_phase == 0)
        {
            top_info->SetCivilizationData("", "", "", "");
            bottom_info->SetCelestialData("", "", "", "");
        }
        else if (info_phase == 1)
        {
            top_info->SetCivilizationData(
                Npgs::System::TR("i18ntext.cinematic.title.singularity"),
                std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.mass_dominated"), std::make_format_args("2.4e30 kg")),
                std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.star_systems"), std::make_format_args("14201")),
                std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.reward"), std::make_format_args("1.875e+21"))
            );
            bottom_info->SetCelestialData("191981", Npgs::System::TR("i18ntext.cinematic.type.black_hole"), std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.mass"), std::make_format_args("1.91E+36kg")), std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.luminosity"), std::make_format_args("8.10E+30 W")));
        }
        else if (info_phase == 2)
        {
            top_info->SetCivilizationData(
                Npgs::System::TR("i18ntext.cinematic.title.type2_civ"),
                std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.output"), std::make_format_args("3.8e26 W")),
                std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.progress"), std::make_format_args("84.2%")),
                Npgs::System::TR("i18ntext.cinematic.stat.status_warning")
            );
            bottom_info->SetCelestialData("000001", Npgs::System::TR("i18ntext.cinematic.type.red_giant"), std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.mass"), std::make_format_args("1.91E+29kg")), std::vformat(Npgs::System::TR("i18ntext.cinematic.stat.luminosity"), std::make_format_args("8.10E+99 W")));
        }
    }

    // Update beam button executable state
    if (auto beam_button = m_ui_root->FindElementAs<UI::PulsarButton>("gameScreenRoot.beamButton"))
    {
        bool has_target = (int(0.33 * *m_context.RealityTime) % 2 == 1);
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
    if (!m_ui_root) return;

    Npgs::Astro::AStar myStar = Gen.GenerateStar();
    Gen.GenerateStar();
    Npgs::System::UI::CelestialData ui_data = Npgs::System::UI::AstroDataBuilder::BuildDataForObject(&myStar);

    if (auto panel = m_ui_root->FindElementAs<UI::CelestialInfoPanel>("gameScreenRoot.celestialInfoPanel"))
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