#include "CinematicInfoPanel.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

CinematicInfoPanel::CinematicInfoPanel(Position pos) : m_position(pos)
{
    m_block_input = false;
    m_visible = false;

    // [NEW] 声明自身尺寸模式
    m_max_width = 800.0f;
    m_width = Length::Fixed(m_max_width);
    m_height = Length::Content(); // 高度由内容决定

    auto& ctx = UIContext::Get();

    // 1. Main VBox (内容容器)
    m_layout_vbox = std::make_shared<VBox>();
    m_layout_vbox->m_width = Length::Stretch();  // 撑满 CinematicInfoPanel
    m_layout_vbox->m_height = Length::Content(); // 高度由内容决定
    m_layout_vbox->m_padding = 4.0f;
    m_layout_vbox->m_block_input = false;
    AddChild(m_layout_vbox);

    // --- 组件初始化 (使用新的声明式布局属性) ---

    // 标题
    m_title_text = std::make_shared<TechText>("", ThemeColorID::TextHighlight, true, true);
    m_title_text->SetName("title"); // [ADD]
    m_title_text->SetGlow(true, ThemeColorID::Accent, 2.5f);
    m_title_text->m_font = ctx.m_font_large;
    m_title_text->SetSizing(TechTextSizingMode::AutoHeight); // 自动换行
    m_title_text->m_align_h = Alignment::Center; // 文本内容居中
    m_title_text->m_width = Length::Stretch();   // 文本框撑满 VBox 宽度
    m_title_text->m_height = Length::Content();

    // 分割线
    m_divider = std::make_shared<TechDivider>(m_position == Position::Top ? ThemeColorID::Accent : ThemeColorID::TextDisabled);
    m_divider->SetName("divider"); // [ADD]
    m_divider->m_use_gradient = (m_position == Position::Top);
    if (m_position == Position::Top) m_divider->m_width = Length::Stretch();
    else m_divider->m_width = Length::Stretch(0.375);
    m_divider->m_height = Length::Fixed(1.0f);
    m_divider->m_align_h = Alignment::Center; // 确保在被动画缩短时保持居中

    // --- 布局构建 ---

    if (m_position == Position::Top)
    {
        // Top 布局: 标题 -> 统计 -> 分割线
        m_top_stats_box = std::make_shared<HBox>();
        m_top_stats_box->m_padding = 20.0f;
        m_top_stats_box->m_width = Length::Content();  // HBox 宽度由其子项决定
        m_top_stats_box->m_height = Length::Content();
        m_top_stats_box->m_align_h = Alignment::Center; // HBox 自身在 VBox 中居中
        m_top_stats_box->m_block_input = false;

        auto setup_stat = [&](std::shared_ptr<TechText>& ptr, const StyleColor& color, const std::string& name)
        {
            ptr = std::make_shared<TechText>("", color, true);
            ptr->SetName(name); // [ADD]
            ptr->SetSizing(TechTextSizingMode::AutoWidthHeight); // 尺寸由文本内容决定
            ptr->m_font = ctx.m_font_bold;
            ptr->m_align_v = Alignment::Center;
            m_top_stats_box->AddChild(ptr);
        };
        setup_stat(m_top_stat_1, ThemeColorID::Text, "stat1");
        setup_stat(m_top_stat_2, ThemeColorID::Text, "stat2");
        setup_stat(m_top_stat_3, ThemeColorID::Accent, "stat3");

        m_layout_vbox->AddChild(m_title_text);
        m_layout_vbox->AddChild(m_top_stats_box);
        m_layout_vbox->AddChild(m_divider);
    }
    else // Position::Bottom
    {
        // Bottom 布局: 标题 -> 分割线 -> 类型 -> 统计
        m_bot_type_text = std::make_shared<TechText>("", ThemeColorID::Text, true);
        m_bot_type_text->SetName("type"); // [ADD]
        m_bot_type_text->m_font = ctx.m_font_regular;
        m_bot_type_text->SetSizing(TechTextSizingMode::AutoHeight);
        m_bot_type_text->m_align_h = Alignment::Center;
        m_bot_type_text->m_width = Length::Stretch();
        m_bot_type_text->m_height = Length::Content();

        m_bot_stats_box = std::make_shared<HBox>();
        m_bot_stats_box->m_padding = 30.0f;
        m_bot_stats_box->m_width = Length::Content();
        m_bot_stats_box->m_height = Length::Content();
        m_bot_stats_box->m_align_h = Alignment::Center;
        m_bot_stats_box->m_block_input = false;

        auto setup_stat = [&](std::shared_ptr<TechText>& ptr, const std::string& name) // [MODIFY] Add name parameter
        {
            ptr = std::make_shared<TechText>("", ThemeColorID::TextDisabled, true);
            ptr->SetName(name); // [ADD]
            ptr->SetSizing(TechTextSizingMode::AutoWidthHeight);
            ptr->m_font = ctx.m_font_bold;
            ptr->m_align_v = Alignment::Center;
            m_bot_stats_box->AddChild(ptr);
        };
        setup_stat(m_bot_stat_1, "stat1");
        setup_stat(m_bot_stat_2, "stat2");

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

void CinematicInfoPanel::Update(float dt)
{
    // Update 函数现在只负责动画逻辑和状态更新，不再关心布局
    if (!m_visible) return;

    // --- 动画逻辑：文字浮现 ---
    if (m_state == State::Expanding && !m_text_anim_triggered && m_anim_progress > 0.9f)
    {
        m_text_anim_triggered = true;
        To(&m_text_alpha, 1.0f, 0.4f, EasingType::EaseOutQuad, [this]()
        {
            m_state = State::Visible;
        });
    }

    // --- Alpha 同步 ---
    m_divider->m_alpha = 1.0f; // 分割线只受长度影响，不受透明度影响
    for (auto& child : m_layout_vbox->m_children)
    {
        if (child == m_divider) continue;
        child->m_alpha = m_text_alpha;
        for (auto& sub : child->m_children) sub->m_alpha = m_text_alpha;
    }

    // 调用基类 Update 来处理 tweens 和递归更新子元素
    UIElement::Update(dt);
}

ImVec2 CinematicInfoPanel::Measure(ImVec2 available_size)
{
    if (!m_visible)
    {
        m_desired_size = { 0, 0 };
        return m_desired_size;
    }

    // 测量内部 VBox 在最大宽度限制下的所需尺寸
    ImVec2 content_size = m_layout_vbox->Measure({ m_max_width, FLT_MAX });

    // 本面板的期望尺寸是：固定的宽度 + 内容的高度
    m_desired_size = { m_max_width, content_size.y };

    return m_desired_size;
}

void CinematicInfoPanel::Arrange(const Rect& final_rect_from_parent)
{
    if (!m_visible) return;

    // [修改] 不再自己计算屏幕位置，直接使用父级（UIRoot）通过锚点计算后传入的 final_rect_from_parent
    // 1. 调用基类 Arrange，让它使用我们计算出的 Rect 来设置自身位置，并自动排列所有子元素
    UIElement::Arrange(final_rect_from_parent);

    // 2. [动画覆盖] 在自动布局之后，手动修改分割线的宽度和位置
    float current_line_w = m_max_width * m_anim_progress;
    if (m_position == Position::Top) current_line_w *= 1.0;
    else current_line_w *= 0.375;
    // a. 修改相对宽度
    m_divider->m_rect.w = current_line_w;
    // b. 因为宽度变了，需要重新计算居中的 x 坐标 (相对于本面板)
    m_divider->m_rect.x = (m_rect.w - current_line_w) * 0.5f;
    // c. 更新分割线的绝对位置
    m_divider->m_absolute_pos.x = m_absolute_pos.x + m_divider->m_rect.x;
}


_UI_END
_SYSTEM_END
_NPGS_END