#include "CollapsedMainButton.h"
_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

CollapsedMainButton::CollapsedMainButton(const std::string& key1, const std::string& key2)
{
    // --- 1. 自身属性设置 ---
    m_block_input = true; // 自身作为交互主体
    // 默认撑满父容器，因为通常它被放在一个固定宽度的侧边栏里
    m_width = Length::Stretch();
    m_height = Length::Stretch();


    // --- 2. 创建内部 VBox 布局容器 ---
    auto vbox = std::make_shared<VBox>();
    m_layout_vbox = vbox;

    // a. VBox 尺寸由内容决定
    vbox->m_width = Length::Content();
    vbox->m_height = Length::Content();

    // b. VBox 在 CollapsedMainButton 内居中对齐
    vbox->m_align_h = Alignment::Center;
    vbox->m_align_v = Alignment::Center;

    // c. VBox 不拦截输入
    vbox->m_block_input = false;
    vbox->m_padding = -4.0f; // 紧凑布局
    AddChild(vbox);


    // --- 3. 创建并配置子元素 (全部添加到 VBox 中) ---
    auto& ctx = UIContext::Get();

    // a. "◈" 符号
    m_symbol = std::make_shared<TechText>("◈");
    m_symbol->m_font = ctx.m_font_large;
    m_symbol->m_align_h = Alignment::Center; // 在 VBox 的主轴交叉轴上居中
    m_symbol->m_width = Length::Content();   // 宽度由内容决定
    m_symbol->m_height = Length::Content();  // 高度由内容决定
    m_symbol->m_block_input = false;

    // b. "MANAGE" 文本
    auto manage_text = std::make_shared<TechText>(key1);
    manage_text->m_color = ThemeColorID::Accent;
    manage_text->m_font = ctx.m_font_small;
    manage_text->m_align_h = Alignment::Center;
    manage_text->m_width = Length::Content();
    manage_text->m_height = Length::Content();
    manage_text->m_block_input = false;

    // c. 间距元素
    auto pad = std::make_shared<UIElement>();
    pad->m_height = Length::Fixed(8.0f); // 固定高度
    pad->m_width = Length::Stretch();      // 宽度不重要，撑满 VBox
    pad->m_block_input = false;

    // d. "NETWORK" 文本
    auto network_text = std::make_shared<TechText>(key2);
    network_text->m_color = ThemeColorID::Accent;
    network_text->m_font = ctx.m_font_small;
    network_text->m_align_h = Alignment::Center;
    network_text->m_width = Length::Content();
    network_text->m_height = Length::Content();
    network_text->m_block_input = false;

    // --- 4. 组装层级 ---
    vbox->AddChild(m_symbol);
    vbox->AddChild(manage_text);
    vbox->AddChild(pad);
    vbox->AddChild(network_text);
}

void CollapsedMainButton::Update(float dt)
{
    // --- 纯状态/动画更新，不再有任何布局代码 ---
    if (!m_symbol) return;

    // 1. 符号脉冲动画
    const auto& theme = UIContext::Get().m_theme;
    float pulse_alpha = 0.7f + 0.3f * sinf(ImGui::GetTime() * 4.0f);
    ImVec4 symbol_color = theme.color_accent;
    symbol_color.w = pulse_alpha;
    m_symbol->SetColor(symbol_color);

    // 2. Alpha 传递 (可选，因为基类 Draw 会处理 Alpha，但明确设置无害)
    m_layout_vbox->m_alpha = this->m_alpha;

    // 3. 调用基类 Update，它会递归更新所有子元素的动画状态
    UIElement::Update(dt);
}
ImVec2 CollapsedMainButton::Measure(ImVec2 available_size)
{
    if (!m_visible) return { 0, 0 };

    // CollapsedMainButton 的期望尺寸就是其内容的期望尺寸。
    // 我们只需要测量唯一的子元素 (VBox)，它的尺寸就是我们的尺寸。
    // VBox 会递归地测量它内部的所有 TechText 和间距。
    if (m_layout_vbox)
    {
        m_desired_size = m_layout_vbox->Measure(available_size);
    }
    else
    {
        m_desired_size = { 0, 0 };
    }

    return m_desired_size;
}
void CollapsedMainButton::HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release, bool& handled)
{
    // 调用基类逻辑。
    // 由于子元素 (VBox, TechText) 的 m_block_input 都是 false，
    // 它们在基类逻辑中会返回 false (不消耗事件)。
    // 最终事件会落在 CollapsedMainButton 自身上 (因为它的 m_block_input 是 true)。
     UIElement::HandleMouseEvent(p, down, click, release,  handled);

    // 如果没有任何子元素消耗点击，且自身被点击了
    if (m_clicked && click && on_click_callback)
    {
        on_click_callback();
    }
}

void CollapsedMainButton::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;
    // 纯容器，不手动绘制任何内容，全靠子元素
    UIElement::Draw(dl);
}

_UI_END
_SYSTEM_END
_NPGS_END