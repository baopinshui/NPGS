#include "CinematicInfoPanel.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

CinematicInfoPanel::CinematicInfoPanel(Position pos) : m_position(pos)
{
    m_block_input = false;
    m_visible = false;
    m_alpha = 1.0f;

    auto& ctx = UIContext::Get();
    // 'theme' is no longer needed here

    if (m_position == Position::Top) m_max_width = 800.0f;
    else m_max_width = 300.0f;

    // 1. Main VBox (unchanged)
    m_layout_vbox = std::make_shared<VBox>();
    m_layout_vbox->m_fill_h = true;
    m_layout_vbox->m_padding = 4.0f;
    m_layout_vbox->m_block_input = false;
    AddChild(m_layout_vbox);

    // --- Component Initialization ---

    // [MODIFIED] Title now uses ThemeColorID
    m_title_text = std::make_shared<TechText>("", ThemeColorID::TextHighlight, true, true);
    m_title_text->m_glow_intensity = 0.5;
    m_title_text->m_glow_spread = 2.5;
    m_title_text->m_font = ctx.m_font_large ? ctx.m_font_large : ctx.m_font_bold;
    m_title_text->m_align_h = Alignment::Center;
    m_title_text->m_fill_h = true;
    m_title_text->m_rect.h = 24.0f;
    m_title_text->m_align_v = Alignment::Center;

    // [MODIFIED] Divider is now initialized with a StyleColor.
    // The conditional logic is much cleaner.
    if (m_position == Position::Top)
    {
        // For the top, we create a divider that defaults to the Accent color
        m_divider = std::make_shared<TechDivider>(ThemeColorID::Accent);
        m_divider->m_use_gradient = true;
    }
    else
    {
        // For the bottom, we create one that defaults to TextDisabled
        m_divider = std::make_shared<TechDivider>(ThemeColorID::TextDisabled);
    }
    m_divider->m_rect.h = 1.0f;
    m_divider->m_align_h = Alignment::Center;

    // --- Layout Construction ---

    if (m_position == Position::Top)
    {
        // Top Layout: Title -> Stats -> Divider

        // Stats Container (unchanged)
        m_top_stats_box = std::make_shared<HBox>();
        m_top_stats_box->m_padding = 20.0f;
        m_top_stats_box->m_fill_h = false;
        m_top_stats_box->m_align_h = Alignment::Center;
        m_top_stats_box->m_block_input = false;
        m_top_stats_box->m_rect.h = 0.0f;

        // [MODIFIED] The complex 'create_top_stat' lambda is completely gone.
        // We can now directly and clearly create the TechText components.
        auto setup_stat = [&](std::shared_ptr<TechText>& ptr, const StyleColor& color)
        {
            ptr = std::make_shared<TechText>("", color, true);
            ptr->m_font = ctx.m_font_bold;
            ptr->m_align_v = Alignment::Center;
            m_top_stats_box->AddChild(ptr);
        };

        setup_stat(m_top_stat_1, ThemeColorID::Text);
        setup_stat(m_top_stat_2, ThemeColorID::Text);
        setup_stat(m_top_stat_3, ThemeColorID::Accent); // The "Reward" stat uses the Accent color.

        m_layout_vbox->AddChild(m_title_text);
        m_layout_vbox->AddChild(m_top_stats_box);
        m_layout_vbox->AddChild(m_divider);
    }
    else // Position::Bottom
    {
        // Bottom Layout: Title -> Divider -> Type Text -> Stats

        // [MODIFIED] Type Text now uses ThemeColorID
        m_bot_type_text = std::make_shared<TechText>("", ThemeColorID::Text, true);
        m_bot_type_text->m_font = ctx.m_font_regular;
        m_bot_type_text->m_align_h = Alignment::Center;
        m_bot_type_text->m_fill_h = true;
        m_bot_type_text->m_align_v = Alignment::Center;

        // Stats Container (unchanged)
        m_bot_stats_box = std::make_shared<HBox>();
        m_bot_stats_box->m_padding = 30.0f;
        m_bot_stats_box->m_fill_h = false;
        m_bot_stats_box->m_align_h = Alignment::Center;
        m_bot_stats_box->m_block_input = false;
        m_bot_stats_box->m_rect.h = 0.0f;

        // [MODIFIED] The 'create_bot_stat' lambda is also gone.
        auto setup_stat = [&](std::shared_ptr<TechText>& ptr)
        {
            ptr = std::make_shared<TechText>("", ThemeColorID::TextDisabled, true);
            ptr->m_font = ctx.m_font_bold;
            ptr->m_align_v = Alignment::Center;
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
void CinematicInfoPanel::UpdateTextWidth(std::shared_ptr<TechText> text_elem, const std::string& content)
{
    if (!text_elem) return;
    ImFont* font = text_elem->GetFont();
    if (font)
    {
        float w = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, content.c_str()).x;
        text_elem->m_rect.w = w + 4.0f; // Add slight padding
        text_elem->m_rect.h = font->FontSize + 2.0f;
    }
    text_elem->SetText(content);
}

void CinematicInfoPanel::SetCivilizationData(const std::string& name, const std::string& stat1, const std::string& stat2, const std::string& stat3)
{
    if (m_position != Position::Top) return;
    bool has_content = !name.empty();
    CheckStateTransition(has_content);
    if (has_content)
    {
        m_title_text->SetText(name);
        UpdateTextWidth(m_top_stat_1, stat1);
        UpdateTextWidth(m_top_stat_2, stat2);
        UpdateTextWidth(m_top_stat_3, stat3);
    }
}

void CinematicInfoPanel::SetCelestialData(const std::string& id, const std::string& type, const std::string& stat1, const std::string& stat2)
{
    if (m_position != Position::Bottom) return;
    bool has_content = !id.empty();
    CheckStateTransition(has_content);
    if (has_content)
    {
        m_title_text->SetText(type + "-" + id);

        m_bot_type_text->SetText(type);

        UpdateTextWidth(m_bot_stat_1, stat1);
        UpdateTextWidth(m_bot_stat_2, stat2);
    }
}

void CinematicInfoPanel::CheckStateTransition(bool has_content)
{
    if (has_content)
    {
        // --- 出现逻辑 ---
        if (m_state == State::Hidden || m_state == State::Contracting)
        {
            m_state = State::Expanding;
            m_visible = true;

            // 初始化状态：横线为0，文字全透明
            m_anim_progress = 0.0f;
            m_text_alpha = 0.0f;
            m_text_anim_triggered = false;

            // 1. 启动横线伸长动画 (0.6s 弹性出现)
            // 2. 文字的出现将在 Update 中监控 m_anim_progress 触发
            To(&m_anim_progress, 1.0f, 0.3f, EasingType::EaseOutQuad);
        }
    }
    else
    {
        // --- 消失逻辑 ---
        if (m_state == State::Visible || m_state == State::Expanding)
        {
            m_state = State::Contracting;

            // 1. 先让文字消失 (0.2s 快速淡出)
            To(&m_text_alpha, 0.0f, 0.2f, EasingType::EaseInQuad);
                // 2. 文字消失回调：启动横线收缩 (0.4s 收回)
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
    if (!m_visible) return; // 移除 alpha 判断，因为 m_alpha 始终为 1

    auto& ctx = UIContext::Get();
    ImVec2 display_sz = ctx.m_display_size;

    // 1. Center Horizontally
    m_rect.w = m_max_width;
    m_rect.x = (display_sz.x - m_max_width) * 0.5f;

    // 2. Position Vertically based on type
    if (m_position == Position::Top)
    {
        m_rect.y = 10.0f; // Higher up
        m_rect.h = 120.0f; // Taller to fit Title + Stats
    }
    else
    {
        m_rect.h = 90.0f;
        m_rect.y = display_sz.y - m_rect.h - 10.0f; // Higher from bottom
    }

    // 3. Sync VBox layout
    m_layout_vbox->m_rect.w = m_rect.w;
    //m_layout_vbox->m_rect.h = m_rect.h; // Keep commented as per original

    // --- 动画逻辑：文字浮现 ---
    // 如果处于伸展阶段，且横线几乎完全展开 (>0.9)，则触发文字浮现
    if (m_state == State::Expanding && !m_text_anim_triggered && m_anim_progress > 0.9f)
    {
        m_text_anim_triggered = true;
        To(&m_text_alpha, 1.0f, 0.4f, EasingType::EaseOutQuad, [this]()
        {
            m_state = State::Visible;
        });
    }

    // 4. Animate Divider Width
    float current_line_w = m_max_width * m_anim_progress;
    m_divider->m_rect.w = current_line_w;

    // 5. Alpha Sync (分离逻辑)

    // 横线：只要面板可见，Alpha 始终为 1 (不随 fade in/out 变暗，只变长度)
    m_divider->m_alpha = 1.0f;

    // 文字/其他组件：跟随 m_text_alpha
    for (auto& child : m_layout_vbox->m_children)
    {
        // 排除 Divider，防止被覆盖
        if (child == m_divider) continue;

        child->m_alpha = m_text_alpha;

        // 递归处理子容器 (HBox) 内的元素
        for (auto& sub : child->m_children) sub->m_alpha = m_text_alpha;
    }

    UIElement::Update(dt, parent_abs_pos);
}

_UI_END
_SYSTEM_END
_NPGS_END