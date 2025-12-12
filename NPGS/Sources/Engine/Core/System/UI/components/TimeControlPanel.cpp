#include "TimeControlPanel.h"
#include "imgui_internal.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

ImVec2 TimeControlPanel::Measure(const ImVec2& available_size)
{
    // 1. 如果自身策略是 Fixed 或 Fill，优先使用基类计算
    ImVec2 size = UIElement::Measure(available_size);

    // 2. 如果是 Content 策略，则由子元素决定
    // TimeControlPanel 只有一个主 VBox (main_vbox)，通常是 children[0]
    if (!m_children.empty())
    {
        // 获取子元素的测量结果
        ImVec2 child_size = m_children[0]->Measure(available_size);

        if (m_width_policy.type == LengthType::Content) size.x = child_size.x;
        if (m_height_policy.type == LengthType::Content) size.y = child_size.y;
    }

    return size;
}
TimeControlPanel::TimeControlPanel(double* current_time_ptr, double* time_scale_ptr, const std::string& pause_key, const std::string& resume_key, const std::string& reset_key)
    : m_time_ptr(current_time_ptr), m_scale_ptr(time_scale_ptr), m_pause_key(pause_key), m_resume_key(resume_key)
{
    if (m_scale_ptr) m_visual_target_scale = *m_scale_ptr;

    // 1. 面板自身设置
    m_block_input = false;
    // [MODIFIED] 面板的尺寸由内容决定，不再需要设置固定的 m_rect.w/h
    SetWidth(Length::Content());
    SetHeight(Length::Content());

    // 2. 创建垂直布局容器 (VBox)
    auto main_vbox = std::make_shared<VBox>();
    // [MODIFIED] VBox 宽度填充父级(即TimeControlPanel)，高度自适应内容
    main_vbox->SetWidth(Length::Fill());
    main_vbox->SetHeight(Length::Content());
    main_vbox->m_padding = 10.0f;
    AddChild(main_vbox);

    auto& ctx = UIContext::Get();

    // ---------------------------------------------------------
    // 第一行：时间显示
    // ---------------------------------------------------------
    m_text_display = std::make_shared<TechText>("T+00000000.00.00 00:00:00");
    m_text_display->m_font = ctx.m_font_subtitle;
    m_text_display->SetGlow(true, ThemeColorID::TextHighlight, 0.5f, 2.5f)
        ->SetColor(ThemeColorID::TextHighlight);
    // [MODIFIED] 告诉父容器(VBox)将我右对齐。尺寸由内容决定(默认)。
    m_text_display->SetAlignment(Alignment::End, Alignment::Center);
    main_vbox->AddChild(m_text_display);

    // ---------------------------------------------------------
    // 第二行：控制区
    // ---------------------------------------------------------
    auto controls_hbox = std::make_shared<HBox>();
    // [MODIFIED] HBox 宽度自适应内容，高度填满。告诉父容器(VBox)将我右对齐。
    controls_hbox->SetWidth(Length::Content())
        ->SetHeight(Length::Fill()) // Fill a virtual space, then centers children
        ->SetAlignment(Alignment::End, Alignment::Center);
    controls_hbox->m_padding = 10.0f;

    // [A] 暂停/继续按钮
    m_pause_btn = std::make_shared<TechButton>(pause_key, TechButton::Style::Normal);
    // [MODIFIED] 使用新的API设置固定尺寸
    m_pause_btn->SetFixedSize(30.0f, 30.0f)
        ->SetAlignment(Alignment::Start, Alignment::Center); // 在HBox中垂直居中
    m_pause_btn->on_click = [this]()
    {
        m_is_paused = !m_is_paused;
        if (this->m_scale_ptr)
        {
            if (m_is_paused) *this->m_scale_ptr = 0.0;
            else *this->m_scale_ptr = m_visual_target_scale;
        }
    };

    // [B] 油门滑条
    m_speed_slider = std::make_shared<ThrottleTechSlider<double>>("", &m_visual_target_scale);
    // [MODIFIED] 使用新的API设置固定尺寸
    // 注意：Slider内部是HBox，SetHeight(Length::Fix(...))会覆盖默认的Content行为
    m_speed_slider->SetFixedSize(220.0f, 30.0f)
        ->SetAlignment(Alignment::Start, Alignment::Center);
    // Slider 内部组件宽度比例可以保持不变，由其自身布局管理
    m_speed_slider->m_label_component->SetWidth(Length::Fix(0.0f));

    // [C] 重置按钮
    m_1x_btn = std::make_shared<TechButton>(reset_key, TechButton::Style::Normal);
    // [MODIFIED] 使用新的API设置固定尺寸
    m_1x_btn->SetFixedSize(30.0f, 30.0f)
        ->SetAlignment(Alignment::Start, Alignment::Center);
    m_1x_btn->on_click = [this]()
    {
        m_visual_target_scale = 1.0;
    };

    // --- 组装 HBox ---
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
    // --- 1. [MODIFIED] 布局计算阶段 ---
    // 首先，测量整个面板需要的自然尺寸
    ImVec2 measured_size = Measure(UIContext::Get().m_display_size);
    m_rect.w = measured_size.x;
    m_rect.h = measured_size.y;

    // --- 2. 外部定位 (保持不变) ---
    ImVec2 display_sz = UIContext::Get().m_display_size;
    float margin_right = 20.0f;
    float margin_top = 10.0f;
    m_rect.x = display_sz.x - m_rect.w - margin_right;
    m_rect.y = margin_top;

    // --- 3. 内部数据逻辑 (保持不变) ---
    if (m_time_ptr)
    {
        m_text_display->SetSourceText(FormatTime(*m_time_ptr));
    }
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
    // (按钮状态切换逻辑也保持不变)
    static bool last_paused_state = false;
    if (m_is_paused)
    {
        if (!last_paused_state)
        {
            m_pause_btn->SetSourceText(m_resume_key);
            m_pause_btn->SetFont(UIContext::Get().m_font_subtitle);
            last_paused_state = true;
        }
    }
    else
    {
        if (last_paused_state)
        {
            m_pause_btn->SetSourceText(m_pause_key);
            m_pause_btn->SetFont(UIContext::Get().m_font_regular);
            last_paused_state = false;
        }
    }

    // --- 4. [MODIFIED] 统一更新 ---
    // 调用基类 Update，它会触发 VBox -> HBox 的两阶段布局流程，
    // 自动计算所有子元素的位置和尺寸。
    UIElement::Update(dt, parent_abs_pos);
}

_UI_END
_SYSTEM_END
_NPGS_END