#include "TimeControlPanel.h"
#include "imgui_internal.h" 

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

TimeControlPanel::TimeControlPanel(double* current_time_ptr, double* time_scale_ptr, const std::string& pause_key, const std::string& resume_key, const std::string& reset_key)
    : m_time_ptr(current_time_ptr), m_scale_ptr(time_scale_ptr), m_pause_key(pause_key), m_resume_key(resume_key)
{
    if (m_scale_ptr) m_visual_target_scale = *m_scale_ptr;

    // 1. 设置自身属性
    m_block_input = false;
    // 尺寸由 Measure 阶段决定，这里设置为 Content 是一种语义上的标记
    m_width = Length::Content();
    m_height = Length::Content();

    // 2. 创建主布局容器 (VBox)
    m_main_layout = std::make_shared<VBox>();
    m_main_layout->m_padding = 4.0f;
    // 让内容在 VBox 内部向右对齐
    m_main_layout->m_align_h = Alignment::End;
    // VBox 自身的宽高由内容决定
    m_main_layout->m_width = Length::Content();
    m_main_layout->m_height = Length::Content();

    // [关键] 将 VBox 添加为 UIElement 的子节点
    // 这样 UIElement::Arrange 就会自动处理它
    AddChild(m_main_layout);

    const auto& theme = UIContext::Get().m_theme;
    auto& ctx = UIContext::Get();

    // --- 第一行：时间显示 ---
    m_text_display = std::make_shared<TechText>("T+00000000.00.00 00:00:00", theme.color_text_highlight, false, true);
    m_text_display->SetSizing(TechTextSizingMode::AutoWidthHeight);
    m_text_display->m_font = ctx.m_font_subtitle;
    m_text_display->m_glow_intensity = 0.5f;
    m_text_display->m_glow_spread = 2.5f;
    m_text_display->SetColor(theme.color_text_highlight);
    m_text_display->m_align_h = Alignment::End;

    m_main_layout->AddChild(m_text_display);

    // --- 第二行：控制区 (HBox) ---
    auto controls_hbox = std::make_shared<HBox>();
    controls_hbox->m_padding = 8.0f;
    controls_hbox->m_align_v = Alignment::Center;
    controls_hbox->m_width = Length::Content();
    controls_hbox->m_height = Length::Content();

    // [A] 暂停/继续按钮
    m_pause_btn = std::make_shared<TechButton>(pause_key, TechButton::Style::Normal);
    m_pause_btn->m_width = Length::Fixed(30.0f);
    m_pause_btn->m_height = Length::Fixed(30.0f);
    m_pause_btn->SetPadding({ 0, 0 });

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
    m_speed_slider->m_width = Length::Fixed(220.0f);
    m_speed_slider->m_height = Length::Fixed(30.0f);
    m_speed_slider->max_label_w = 0.0f;
    m_speed_slider->value_box_w = 70.0f;

    // [C] 重置按钮
    m_1x_btn = std::make_shared<TechButton>(reset_key, TechButton::Style::Normal);
    m_1x_btn->m_width = Length::Fixed(30.0f);
    m_1x_btn->m_height = Length::Fixed(30.0f);
    m_1x_btn->SetPadding({ 0, 0 });

    m_1x_btn->on_click = [this]()
    {
        m_visual_target_scale = 1.0;
    };

    controls_hbox->AddChild(m_pause_btn);
    controls_hbox->AddChild(m_speed_slider);
    controls_hbox->AddChild(m_1x_btn);

    m_main_layout->AddChild(controls_hbox);
}

// -----------------------------------------------------------------------------
// 布局系统实现
// -----------------------------------------------------------------------------

ImVec2 TimeControlPanel::Measure(ImVec2 available_size)
{
    if (!m_visible)
    {
        m_desired_size = { 0, 0 };
        return m_desired_size;
    }

    // 我们的尺寸完全由内部 VBox 决定
    // VBox 会递归 Measure 它的子节点（Text, HBox 等）
    m_desired_size = m_main_layout->Measure(available_size);
    return m_desired_size;
}


// -----------------------------------------------------------------------------
// 业务逻辑
// -----------------------------------------------------------------------------

std::string TimeControlPanel::FormatTime(double total_seconds)
{
    // (代码保持不变)
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

void TimeControlPanel::Update(float dt)
{
    // [修正] Update 不再包含布局代码

    // 1. 更新时间文本
    if (m_time_ptr)
    {
        m_text_display->SetSourceText(FormatTime(*m_time_ptr));
    }

    // 2. 逻辑同步
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

    // 3. 更新按钮图标和字体
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

    // 4. 递归更新子组件
    UIElement::Update(dt);
}

_UI_END
_SYSTEM_END
_NPGS_END