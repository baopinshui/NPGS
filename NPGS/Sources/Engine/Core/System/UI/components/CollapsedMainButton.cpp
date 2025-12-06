#include "CollapsedMainButton.h"
_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

CollapsedMainButton::CollapsedMainButton(const std::string& str1, const std::string& str2)
{
    // 1. 自身阻挡输入，作为点击的接收者
    m_block_input = true;

    // --- 创建布局容器 ---
    auto vbox = std::make_shared<VBox>();
    m_layout_vbox = vbox;

    // [关键修正 1] 必须设置为 false，否则点击会被 VBox 拦截，无法传递给按钮
    vbox->m_block_input = false;

    // [关键修正 2] 禁用垂直填充，让 VBox 的高度由内容自动撑开，这样我们才能计算居中
    vbox->m_fill_v = false;
    vbox->m_fill_h = true; // 水平拉伸以支持内部文本的水平居中
    vbox->m_padding = -4.0f; // 紧凑布局
    AddChild(vbox);

    auto& ctx = UIContext::Get();
    const auto& theme = UIContext::Get().m_theme;

    // --- 创建子文本 ---

    // 1. "◈" 符号
    m_symbol = std::make_shared<TechText>("◈");
    m_symbol->m_font = ctx.m_font_large; // 确保 UIContext 初始化了这个字体
    m_symbol->m_align_h = Alignment::Center;
    m_symbol->m_rect.h = 30.0f;
    m_symbol->m_block_input = false; // 点击穿透

    // 2. "MANAGE" 文本
    auto manage_text = std::make_shared<TechText>(str1);
    manage_text->m_align_h = Alignment::Center;
    manage_text->m_font = ctx.m_font_small;
    manage_text->m_rect.h = 16.0f;
    manage_text->SetColor(theme.color_accent); // 修正颜色
    manage_text->m_block_input = false; // 点击穿透

    // 3. "NETWORK" 文本
    auto network_text = std::make_shared<TechText>(str2);
    network_text->m_align_h = Alignment::Center;

    network_text->m_font = ctx.m_font_small;
    network_text->m_rect.h = 16.0f;
    network_text->SetColor(theme.color_accent); // 修正颜色
    network_text->m_block_input = false; // 点击穿透

    vbox->AddChild(m_symbol);
    vbox->AddChild(manage_text);
    vbox->AddChild(network_text);
}
void CollapsedMainButton::Update(float dt, const ImVec2& parent_abs_pos)
{
    // [关键修正 1] 在布局计算前，强制同步 VBox 的尺寸与按钮一致
    // 这样 VBox 内部计算居中时，是以按钮的宽度为基准的
    if (m_layout_vbox)
    {
        m_layout_vbox->m_rect.w = m_rect.w; // 宽度同步
        m_layout_vbox->m_rect.x = 0.0f;     // X 坐标归零（相对于父级）
    }

    // 1. 基类更新
    // 这会触发 VBox::Update，根据上面的宽度重新排列子元素 (TechText) 的 X 坐标
    // 同时 VBox 会根据子元素的高度计算出自己的 m_rect.h
    UIElement::Update(dt, parent_abs_pos);

    if (!m_symbol || !m_layout_vbox) return;

    // 2. 动态垂直居中
    // 利用 VBox 计算出的真实内容高度，将整个 VBox 在按钮中垂直居中
    float content_height = m_layout_vbox->m_rect.h;
    m_layout_vbox->m_rect.y = (this->m_rect.h - content_height) * 0.5f;

    // 3. 符号脉冲动画
    const auto& theme = UIContext::Get().m_theme;
    float pulse_alpha = 0.7f + 0.3f * sinf(ImGui::GetTime() * 4.0f);
    ImVec4 symbol_color = theme.color_accent;
    symbol_color.w = pulse_alpha;
    m_symbol->SetColor(symbol_color);

    // 4. Alpha 传递 (确保整个组一起淡入淡出)
    m_layout_vbox->m_alpha = this->m_alpha;
    for (const auto& child : m_layout_vbox->m_children)
    {
        child->m_alpha = this->m_alpha;
    }
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