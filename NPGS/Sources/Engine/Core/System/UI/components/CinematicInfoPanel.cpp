#include "CinematicInfoPanel.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN


ImVec2 CinematicInfoPanel::Measure(const ImVec2& available_size)
{
    ImVec2 size = UIElement::Measure(available_size);

    if (m_layout_vbox)
    {
        // 传入 available_size，让 VBox 知道边界（例如用于文本换行）
        ImVec2 content_size = m_layout_vbox->Measure(available_size);

        if (m_width_policy.type == LengthType::Content) size.x = content_size.x;
        if (m_height_policy.type == LengthType::Content) size.y = content_size.y;
    }

    return size;
}

CinematicInfoPanel::CinematicInfoPanel(Position pos) : m_position(pos)
{
    m_block_input = false;
    m_visible = false;
    m_alpha = 1.0f;

    auto& ctx = UIContext::Get();

    if (m_position == Position::Top) m_max_width = 800.0f;
    else m_max_width = 300.0f;

    // --- [MODIFIED] 主 VBox 布局策略 ---
    m_layout_vbox = std::make_shared<VBox>();
    // VBox 应该填满整个 CinematicInfoPanel
    m_layout_vbox->SetWidth(Length::Fill());
    m_layout_vbox->SetHeight(Length::Fill());
    m_layout_vbox->m_padding = 4.0f;
    m_layout_vbox->m_block_input = false;
    AddChild(m_layout_vbox);

    // --- 组件初始化 ---

    // [MODIFIED] Title 文本布局策略
    m_title_text = std::make_shared<TechText>("", ThemeColorID::TextHighlight);
    m_title_text->SetAnimMode(TechTextAnimMode::Hacker);
    m_title_text->SetGlow(true, ThemeColorID::Accent, 0.5f, 2.5f);
    m_title_text->m_font = ctx.m_font_large ? ctx.m_font_large : ctx.m_font_bold;
    // 宽度填满父级(VBox)，高度固定，内部文本水平和垂直都居中
    m_title_text->SetWidth(Length::Fill())
        ->SetHeight(Length::Fix(24.0f))
        ->SetAlignment(Alignment::Center, Alignment::Center);

    // [MODIFIED] Divider 布局策略
    if (m_position == Position::Top)
    {
        m_divider = std::make_shared<TechDivider>(ThemeColorID::Accent);
        m_divider->m_use_gradient = true;
    }
    else
    {
        m_divider = std::make_shared<TechDivider>(ThemeColorID::TextDisabled);
    }
    // 宽度由动画控制，所以设为 Fixed(0) 即可。在 VBox 中居中显示。
    m_divider->SetWidth(Length::Fix(0.0f))
        ->SetHeight(Length::Fix(1.0f))
        ->SetAlignment(Alignment::Center, Alignment::Center);


    // --- 布局构造 ---

    if (m_position == Position::Top)
    {
        // Top 布局: Title -> Stats -> Divider

        // [MODIFIED] Stats HBox 布局策略
        m_top_stats_box = std::make_shared<HBox>();
        m_top_stats_box->m_padding = 20.0f;
        m_top_stats_box->m_block_input = false;
        // 关键：HBox 宽度自适应内容，高度也自适应内容。
        // 然后把它自身在父级 VBox 中居中。
        m_top_stats_box->SetWidth(Length::Content())
            ->SetHeight(Length::Content())
            ->SetAlignment(Alignment::Center, Alignment::Start);

        // [MODIFIED] Stats 内部文本的布局策略
        auto setup_stat = [&](std::shared_ptr<TechText>& ptr, const StyleColor& color)
        {
            ptr = std::make_shared<TechText>("", color);
            ptr->SetAnimMode(TechTextAnimMode::Hacker);
            ptr->m_font = ctx.m_font_bold;
            // 尺寸由内容决定，在 HBox 中垂直居中
            ptr->SetWidth(Length::Content())
                ->SetHeight(Length::Content())
                ->SetAlignment(Alignment::Start, Alignment::Center);
            m_top_stats_box->AddChild(ptr);
        };

        setup_stat(m_top_stat_1, ThemeColorID::Text);
        setup_stat(m_top_stat_2, ThemeColorID::Text);
        setup_stat(m_top_stat_3, ThemeColorID::Accent);

        m_layout_vbox->AddChild(m_title_text);
        m_layout_vbox->AddChild(m_top_stats_box);
        m_layout_vbox->AddChild(m_divider);
    }
    else // Position::Bottom
    {
        // Bottom 布局: Title -> Divider -> Type Text -> Stats

        // [MODIFIED] Type Text 布局策略
        m_bot_type_text = std::make_shared<TechText>("", ThemeColorID::Text);
        m_bot_type_text->SetAnimMode(TechTextAnimMode::Hacker);
        m_bot_type_text->m_font = ctx.m_font_regular;
        // 宽度填满，内部文本居中
        m_bot_type_text->SetWidth(Length::Fill())
            ->SetHeight(Length::Content()) // 高度自适应
            ->SetAlignment(Alignment::Center, Alignment::Center);

        // [MODIFIED] Stats HBox 布局策略 (同 Top)
        m_bot_stats_box = std::make_shared<HBox>();
        m_bot_stats_box->m_padding = 30.0f;
        m_bot_stats_box->m_block_input = false;
        m_bot_stats_box->SetWidth(Length::Content())
            ->SetHeight(Length::Content())
            ->SetAlignment(Alignment::Center, Alignment::Start);

        // [MODIFIED] Stats 内部文本的布局策略 (同 Top)
        auto setup_stat = [&](std::shared_ptr<TechText>& ptr)
        {
            ptr = std::make_shared<TechText>("", ThemeColorID::TextDisabled);
            ptr->SetAnimMode(TechTextAnimMode::Hacker);
            ptr->m_font = ctx.m_font_bold;
            ptr->SetWidth(Length::Content())
                ->SetHeight(Length::Content())
                ->SetAlignment(Alignment::Start, Alignment::Center);
            m_bot_stats_box->AddChild(ptr);
        };

        setup_stat(m_bot_stat_1);
        setup_stat(m_bot_stat_2);

        m_layout_vbox->AddChild(m_title_text);
        m_layout_vbox->AddChild(m_divider);
        m_layout_vbox->AddChild(m_bot_type_text);
        m_layout_vbox->AddChild(m_bot_stats_box);
    }
}


void CinematicInfoPanel::SetCivilizationData(const std::string& name, const std::string& stat1, const std::string& stat2, const std::string& stat3)
{
    if (m_position != Position::Top) return;
    bool has_content = !name.empty();
    CheckStateTransition(has_content);
    if (has_content)
    {
        m_title_text->SetSourceText(name);
        m_top_stat_1->SetSourceText(stat1);
        m_top_stat_2->SetSourceText(stat2);
        m_top_stat_3->SetSourceText(stat3);
    }
}

void CinematicInfoPanel::SetCelestialData(const std::string& id, const std::string& type, const std::string& stat1, const std::string& stat2)
{
    if (m_position != Position::Bottom) return;
    bool has_content = !id.empty();
    CheckStateTransition(has_content);
    if (has_content)
    {
        m_title_text->SetSourceText(type + "-" + id);
        m_bot_type_text->SetSourceText(type);
        m_bot_stat_1->SetSourceText(stat1);
        m_bot_stat_2->SetSourceText(stat2);
    }
}

void CinematicInfoPanel::CheckStateTransition(bool has_content)
{
    if (has_content)
    {
        if (m_state == State::Hidden || m_state == State::Contracting)
        {
            m_state = State::Expanding;
            m_visible = true;
            m_anim_progress = 0.0f;
            m_text_alpha = 0.0f;
            m_text_anim_triggered = false;
            To(&m_anim_progress, 1.0f, 0.3f, EasingType::EaseOutQuad);
        }
    }
    else
    {
        if (m_state == State::Visible || m_state == State::Expanding)
        {
            m_state = State::Contracting;
            To(&m_text_alpha, 0.0f, 0.2f, EasingType::EaseInQuad);
            To(&m_anim_progress, 0.0f, 0.3f, EasingType::EaseInQuad, [this]()
            {
                m_state = State::Hidden;
                m_visible = false;
            });
        }
    }
}

void CinematicInfoPanel::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (!m_visible) return;

    auto& ctx = UIContext::Get();
    ImVec2 display_sz = ctx.m_display_size;

    // --- 1. 自身位置和尺寸计算 (逻辑不变) ---
    m_rect.w = m_max_width;
    m_rect.x = (display_sz.x - m_max_width) * 0.5f;

    if (m_position == Position::Top)
    {
        m_rect.y = 10.0f;
        m_rect.h = 120.0f;
    }
    else
    {
        m_rect.h = 90.0f;
        m_rect.y = display_sz.y - m_rect.h - 10.0f;
    }

    // --- [REMOVED] 手动同步 VBox 尺寸的旧代码 ---
    // m_layout_vbox->m_rect.w = m_rect.w; // 不再需要！布局系统会自动处理

    // --- 2. 动画逻辑 (逻辑不变) ---
    if (m_state == State::Expanding && !m_text_anim_triggered && m_anim_progress > 0.9f)
    {
        m_text_anim_triggered = true;
        To(&m_text_alpha, 1.0f, 0.4f, EasingType::EaseOutQuad, [this]()
        {
            m_state = State::Visible;
        });
    }

    // 手动控制 Divider 宽度动画
    float current_line_w = m_max_width * m_anim_progress;
    m_divider->m_rect.w = current_line_w;

    // Alpha 传递
    m_divider->m_alpha = 1.0f;
    for (auto& child : m_layout_vbox->m_children)
    {
        if (child == m_divider) continue;
        child->m_alpha = m_text_alpha;
        for (auto& sub : child->m_children) sub->m_alpha = m_text_alpha;
    }

    // --- 3. [MODIFIED] 调用基类 Update ---
    // 这将触发整个布局链条：
    // UIRoot -> CinematicInfoPanel -> VBox -> (HBox, TechText...)
    // VBox 会根据 Fill 策略自动获得父级的尺寸，并依次布局其子元素
    UIElement::Update(dt, parent_abs_pos);
}

_UI_END
_SYSTEM_END
_NPGS_END