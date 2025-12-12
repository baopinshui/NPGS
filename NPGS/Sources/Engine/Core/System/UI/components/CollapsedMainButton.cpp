// --- START OF FILE CollapsedMainButton.cpp ---

#include "CollapsedMainButton.h"
_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

CollapsedMainButton::CollapsedMainButton(const std::string& key1, const std::string& key2)
{
    // 1. 自身作为点击接收者
    m_block_input = true;

    // [NEW] 设置自身默认尺寸。外部（如 NeuralMenu）仍然可以覆盖它。
    SetFixedSize(80.0f, 80.0f);

    // --- 创建布局容器 ---
    auto vbox = std::make_shared<VBox>();
    m_layout_vbox = vbox;
    vbox->m_block_input = false; // 点击穿透
    vbox->m_padding = -4.0f;     // 紧凑布局

    // [KEY CHANGE] 声明 VBox 的布局行为
    // 1. 尺寸：宽度和高度都由其内容决定（收缩包裹）
    vbox->SetWidth(Length::Content());
    vbox->SetHeight(Length::Content());
    // 2. 对齐：在父容器(CollapsedMainButton)中水平和垂直居中
    vbox->SetAlignment(Alignment::Center, Alignment::Center);

    AddChild(vbox);

    auto& ctx = UIContext::Get();

    // --- 创建子文本 ---
    // 它们的尺寸策略默认就是 Content，这正是我们需要的。
    // 我们只需要设置它们在 VBox 中的对齐方式。

    // 1. "◈" 符号
    m_symbol = std::make_shared<TechText>("◈");
    m_symbol->m_font = ctx.m_font_large;
    m_symbol->SetAlignment(Alignment::Center, Alignment::Center); // 在 VBox 中水平居中
    m_symbol->m_block_input = false;

    // 2. "MANAGE" 文本
    auto manage_text = std::make_shared<TechText>(key1);
    manage_text->SetColor(ThemeColorID::Accent);
    manage_text->m_font = ctx.m_font_small;
    manage_text->SetAlignment(Alignment::Center, Alignment::Start); // 在 VBox 中水平居中
    manage_text->m_block_input = false;

    // 3. "NETWORK" 文本
    auto network_text = std::make_shared<TechText>(key2);
    network_text->SetColor(ThemeColorID::Accent);
    network_text->m_font = ctx.m_font_small;
    network_text->SetAlignment(Alignment::Center, Alignment::Start); // 在 VBox 中水平居中
    network_text->m_block_input = false;

    vbox->AddChild(m_symbol);
    vbox->AddChild(manage_text);
    vbox->AddChild(network_text);
}

// [NEW] 实现 Measure 方法
ImVec2 CollapsedMainButton::Measure(const ImVec2& available_size)
{
    // 如果我是 Content 模式，我的尺寸就等于我唯一的子元素 VBox 的尺寸
    if (m_width_policy.type == LengthType::Content || m_height_policy.type == LengthType::Content)
    {
        if (m_layout_vbox)
        {
            return m_layout_vbox->Measure(available_size);
        }
        return { 0, 0 };
    }
    // 否则，使用基类的默认行为（返回 Fixed 值）
    return UIElement::Measure(available_size);
}

// [MODIFIED] 大幅简化的 Update 方法
void CollapsedMainButton::Update(float dt, const ImVec2& parent_abs_pos)
{
    // 1. 更新自身状态（动画等）
    UpdateSelf(dt, parent_abs_pos);

    if (!m_layout_vbox) return;

    // 2. [NEW] 标准容器布局流程
    //    a. 测量子元素（VBox）需要多大空间
    ImVec2 content_size = m_layout_vbox->Measure({ m_rect.w, m_rect.h });

    //    b. 根据子元素的对齐策略，计算其位置
    //       这里我们复用 VBox 的对齐设置
    m_layout_vbox->m_rect.w = content_size.x;
    m_layout_vbox->m_rect.h = content_size.y;

    if (m_layout_vbox->m_align_h == Alignment::Center)
        m_layout_vbox->m_rect.x = (m_rect.w - content_size.x) * 0.5f;
    else if (m_layout_vbox->m_align_h == Alignment::End)
        m_layout_vbox->m_rect.x = m_rect.w - content_size.x;
    else
        m_layout_vbox->m_rect.x = 0;

    if (m_layout_vbox->m_align_v == Alignment::Center)
        m_layout_vbox->m_rect.y = (m_rect.h - content_size.y) * 0.5f;
    else if (m_layout_vbox->m_align_v == Alignment::End)
        m_layout_vbox->m_rect.y = m_rect.h - content_size.y;
    else
        m_layout_vbox->m_rect.y = 0;

    //    c. 递归调用子元素的 Update，让 VBox 去布局它自己的子元素
    m_layout_vbox->Update(dt, m_absolute_pos);

    // 3. 符号脉冲动画（保持不变）
    if (m_symbol)
    {
        const auto& theme = UIContext::Get().m_theme;
        float pulse_alpha = 0.7f + 0.3f * sinf(ImGui::GetTime() * 4.0f);
        ImVec4 symbol_color = theme.color_accent;
        symbol_color.w = pulse_alpha;
        m_symbol->SetColor(symbol_color);
    }

    // 4. Alpha 传递 (保持不变)
    m_layout_vbox->m_alpha = this->m_alpha;
}

// HandleMouseEvent 和 Draw 保持不变，因为它们不直接参与布局计算
void CollapsedMainButton::HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release, bool& handled)
{
    UIElement::HandleMouseEvent(p, down, click, release, handled);
    if (m_clicked && click && on_click_callback)
    {
        on_click_callback();
    }
}

void CollapsedMainButton::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;
    // 纯容器，不手动绘制，全靠子元素
    UIElement::Draw(dl);
}

_UI_END
_SYSTEM_END
_NPGS_END