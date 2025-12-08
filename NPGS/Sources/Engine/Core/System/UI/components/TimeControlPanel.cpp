#include "TimeControlPanel.h"

    #include "imgui_internal.h" // Include this header to access internal ImGui structures like ImDrawListSharedData
_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

TimeControlPanel::TimeControlPanel(double* current_time_ptr, double* time_scale_ptr)
    : m_time_ptr(current_time_ptr), m_scale_ptr(time_scale_ptr)
{
    // 初始化视觉倍率为当前的真实倍率 (防止一开始UI和实际不一致)
    if (m_scale_ptr) m_visual_target_scale = *m_scale_ptr;

    // 1. 面板自身设置
    m_block_input = false;
    m_rect.w = 100.0f; // 这里的宽度其实由 VBox 撑开，或者根据需求设大
    m_rect.h = 4.0f;

    // 2. 创建垂直布局容器 (VBox)
    auto main_vbox = std::make_shared<VBox>();
    main_vbox->m_padding = 10.0f;
    main_vbox->m_align_h = Alignment::Start;
    main_vbox->m_fill_h = true;
    AddChild(main_vbox);

    const auto& theme = UIContext::Get().m_theme;
    auto& ctx = UIContext::Get();

    // ---------------------------------------------------------
    // 第一行：时间显示
    // ---------------------------------------------------------
    m_text_display = std::make_shared<TechText>("T+00000000.00.00 00:00:00", theme.color_text_highlight, false, true);
    m_text_display->m_font = ctx.m_font_subtitle;
    m_text_display->m_glow_intensity = 0.5;
    m_text_display->m_glow_spread = 2.5;
    m_text_display->SetColor(theme.color_text_highlight);
    m_text_display->m_align_h = Alignment::End;

    main_vbox->AddChild(m_text_display);

    // ---------------------------------------------------------
    // 第二行：控制区
    // ---------------------------------------------------------
    auto controls_hbox = std::make_shared<HBox>();
    controls_hbox->m_padding = 10.0f;
    controls_hbox->m_align_h = Alignment::End;
    controls_hbox->m_fill_h = false;
    controls_hbox->m_rect.h = 30.0f;

    // [A] 暂停/继续按钮
    m_pause_btn = std::make_shared<TechButton>("ui.time.pause", TechButton::Style::Normal);
    m_pause_btn->m_rect.w = 30.0f;
    m_pause_btn->m_rect.h = 30.0f;

    m_pause_btn->on_click = [this]()
    {
        // 切换暂停状态
        m_is_paused = !m_is_paused;

        // 这里的逻辑已简化，核心同步放在 Update 里做，确保状态统一
        // 点击瞬间也可以做一次强制赋值，保证手感
        if (this->m_scale_ptr)
        {
            if (m_is_paused) *this->m_scale_ptr = 0.0;
            else *this->m_scale_ptr = m_visual_target_scale;
        }
    };

    // [B] 油门滑条
    // [关键修改]：滑条绑定到 m_visual_target_scale (成员变量)，而不是 m_scale_ptr (外部指针)
    // 这样无论外部游戏是否暂停，滑条都控制这个“视觉/目标”数值
    m_speed_slider = std::make_shared<ThrottleTechSlider<double>>("", &m_visual_target_scale);
    m_speed_slider->m_rect.w = 220.0f;
    m_speed_slider->m_rect.h = 30.0f;
    m_speed_slider->max_label_w = 0.0f;
    m_speed_slider->value_box_w = 70.0f;

    m_1x_btn = std::make_shared<TechButton>("ui.time.reset_speed", TechButton::Style::Normal);
    m_1x_btn->m_rect.w = 30.0f;
    m_1x_btn->m_rect.h = 30.0f;
      
    m_1x_btn->on_click = [this]()
    {
        m_visual_target_scale = 1.0;

    };



    controls_hbox->AddChild(m_pause_btn);
    controls_hbox->AddChild(m_speed_slider);
    controls_hbox->AddChild(m_1x_btn);
    main_vbox->AddChild(controls_hbox);
}

// FormatTime 函数保持不变...
std::string TimeControlPanel::FormatTime(double total_seconds)
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
    std::snprintf(buf, sizeof(buf), "T+%08lld.%02lld.%02lld %02lld:%02lld:%02lld",
        years, months + 1, days + 1, hours, minutes, seconds);

    return std::string(buf);
}

void TimeControlPanel::Update(float dt, const ImVec2& parent_abs_pos)
{
    // 1. 位置更新
    ImVec2 display_sz = UIContext::Get().m_display_size;
    float margin_right = 20.0f;
    float margin_top = 10.0f;
    m_rect.x = display_sz.x - m_rect.w - margin_right;
    m_rect.y = margin_top;

    // 2. 时间显示更新
    if (m_time_ptr)
    {
        m_text_display->SetText(FormatTime(*m_time_ptr));
    }

    // 3. [关键逻辑] 数据同步与状态管理
    if (m_scale_ptr)
    {


        if (m_is_paused)
        {
            *m_scale_ptr = 0.0;
        }
        else
        {
            if (m_visual_target_scale < 0.0) m_visual_target_scale = 0.0;

            *m_scale_ptr = m_visual_target_scale;
        }
    }

    // 4. 按钮图标状态
    static bool last_paused_state = false;
    if (m_is_paused)
    {
        if (!last_paused_state)
        {
            m_pause_btn->SetI18nKey("ui.time.resume");
			m_pause_btn->SetFont( UIContext::Get().m_font_subtitle);
            last_paused_state = true;
        }
    }
    else
    {
        if (last_paused_state)
        {
            m_pause_btn->SetI18nKey("ui.time.pause");
			m_pause_btn->SetFont(UIContext::Get().m_font_regular);
            last_paused_state = false;
        }
    }

    // 5. 基类更新 (处理滑条交互等)
    UIElement::Update(dt, parent_abs_pos);
}

_UI_END
_SYSTEM_END
_NPGS_END