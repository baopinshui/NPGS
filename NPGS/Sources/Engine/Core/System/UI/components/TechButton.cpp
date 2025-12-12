#include "TechButton.h"
#include "../TechUtils.h"
#include "../Utils/I18nManager.h" // [新增]
#include <imgui_internal.h>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

ImVec2 TechButton::Measure(const ImVec2& available_size)
{
    // 1. 获取基准尺寸 (Fixed/Fill)
    ImVec2 size = UIElement::Measure(available_size);

    // 2. 如果是 Content 模式，计算内容尺寸
    if (m_width_policy.type == LengthType::Content || m_height_policy.type == LengthType::Content)
    {
        float content_w = 0.0f;
        float content_h = 0.0f;

        if (m_style == Style::Vertical)
        {
            // 竖排模式通常没有子 TechText，依靠 Draw 手动绘制
            // 这里给一个合理的默认值，或者根据字体计算
            content_w = 30.0f;
            content_h = 100.0f;
        }
        else if (m_label_component) // Normal / Tab
        {
            // 测量内部标签
            // 注意：要减去 padding 传给子元素，否则子元素可能撑破边界
            float horizontal_padding = 20.0f;
            float vertical_padding = 10.0f;

            ImVec2 avail_for_text = available_size;
            if (avail_for_text.x > horizontal_padding) avail_for_text.x -= horizontal_padding;
            if (avail_for_text.y > vertical_padding) avail_for_text.y -= vertical_padding;

            ImVec2 label_size = m_label_component->Measure(avail_for_text);

            content_w = label_size.x + horizontal_padding;
            content_h = label_size.y + vertical_padding;
        }

        // 应用计算结果
        if (m_width_policy.type == LengthType::Content) size.x = content_w;
        if (m_height_policy.type == LengthType::Content) size.y = content_h;
    }

    return size;
}

TechButton::TechButton(const std::string& key_or_text, Style style)
    : m_source_text(key_or_text), m_style(style)
{
    m_block_input = true;
    m_current_display_text = TR(key_or_text); // 用于 Vertical 模式

    // 默认尺寸策略：通常按钮是固定大小或填充
    // 我们可以给一个默认的固定大小，避免作为 Content 时不可见
    m_width_policy = Length::Fix(100.0f);
    m_height_policy = Length::Fix(32.0f);

    switch (m_style)
    {
    case Style::Normal:
        m_color_bg = StyleColor(ThemeColorID::Accent).WithAlpha(0.1f);
        m_color_bg_hover = ThemeColorID::Accent;
        m_color_text = ThemeColorID::Accent;
        m_color_text_hover = ImVec4(0, 0, 0, 1);
        m_color_border = ThemeColorID::Accent;
        m_color_border_hover = ThemeColorID::Accent;
        break;
    case Style::Tab:
        m_color_bg = StyleColor();
        m_color_bg_hover = StyleColor(ThemeColorID::Accent).WithAlpha(0.2f);
        m_color_text = ThemeColorID::TextDisabled;
        m_color_text_hover = ThemeColorID::Accent;
        m_color_selected_bg = ThemeColorID::Accent;
        m_color_selected_text = ImVec4(0, 0, 0, 1);
        break;
    case Style::Vertical:
        m_color_border = ThemeColorID::Border;
        m_color_border_hover = ThemeColorID::Accent;
        m_color_text = ThemeColorID::TextDisabled;
        m_color_text_hover = ThemeColorID::Accent;
        break;
    default: break;
    }

    // 只有非竖排模式才创建 TechText 子组件
    if (m_style != Style::Vertical && m_style != Style::Invisible)
    {
        m_label_component = std::make_shared<TechText>(m_source_text);

        // [核心布局设置]
        // 1. 文本组件大小由内容决定
        m_label_component->SetWidth(Length::Content());
        m_label_component->SetHeight(Length::Content());
        m_label_component->SetAlignment(Alignment::Center, Alignment::Center);
        // 2. 文本不阻挡输入，点击穿透给按钮
        m_label_component->m_block_input = false;

        AddChild(m_label_component);
    }
}

void TechButton::SetSourceText(const std::string& key_or_text, bool with_effect)
{
    if (m_source_text == key_or_text) return;
    m_source_text = key_or_text;

    // 更新显示文本
    m_current_display_text = TR(m_source_text);

    if (m_label_component)
    {
        if (with_effect) m_label_component->SetAnimMode(TechTextAnimMode::Hacker);
        // TechText 现在也使用新的统一接口
        m_label_component->SetSourceText(m_source_text);
    }
}

TechButton* TechButton::SetFont(ImFont* font)
{
    // 仅对 Normal 和 Tab 样式有效，因为只有它们使用 m_label_component
    if (m_label_component)
    {
        m_label_component->m_font = font;
    }
    // 注意：这里我们没有修改 Vertical 样式的字体，因为它被设计为固定使用小字体。
    // 如果需要，也可以在这里加一个成员变量来存储 Vertical 模式的字体。

    return this; // 返回 this 以支持链式调用
}

void TechButton::Update(float dt, const ImVec2& parent_abs_pos)
{
    if (m_style == Style::Vertical)
    {
        auto& i18n = System::I18nManager::Get();
        if (m_local_i18n_version != i18n.GetVersion())
        {
            m_current_display_text = TR(m_source_text);
            m_local_i18n_version = i18n.GetVersion();
        }
    }
    // 1. 动画状态更新
    if (m_hovered) m_hover_progress += dt * m_anim_speed;
    else m_hover_progress -= dt * m_anim_speed;
    m_hover_progress = std::clamp(m_hover_progress, 0.0f, 1.0f);

    const auto& theme = UIContext::Get().m_theme;

    // 2. 根据样式更新子组件颜色 (Normal / Tab)
    if (m_label_component)
    {
        // 1. 测量标签
        ImVec2 label_size = m_label_component->Measure(ImVec2(m_rect.w, m_rect.h));

        // 2. 应用尺寸 (因为标签是 Content 模式，所以它应该等于测量大小)
        m_label_component->m_rect.w = label_size.x;
        m_label_component->m_rect.h = label_size.y;

        // 3. 应用对齐 (由 TechButton 容器负责读取标签的对齐属性并设置其位置)

        // 水平方向
        switch (m_label_component->m_align_h)
        {
        case Alignment::Start:  m_label_component->m_rect.x = 0; break;
        case Alignment::Center: m_label_component->m_rect.x = (m_rect.w - label_size.x) * 0.5f; break;
        case Alignment::End:    m_label_component->m_rect.x = m_rect.w - label_size.x; break;
        }

        // 垂直方向
        switch (m_label_component->m_align_v)
        {
        case Alignment::Start:  m_label_component->m_rect.y = 0; break;
        case Alignment::Center: m_label_component->m_rect.y = (m_rect.h - label_size.y) * 0.5f; break;
        case Alignment::End:    m_label_component->m_rect.y = m_rect.h - label_size.y; break;
        }

        // 更新标签颜色 (样式混合逻辑不变)
        if (m_style == Style::Normal)
        {
            m_label_component->SetColor(TechUtils::LerpColor(m_color_text.Resolve(), m_color_text_hover.Resolve(), m_hover_progress));
        }
        else if (m_style == Style::Tab)
        {
            if (m_selected) m_label_component->SetColor(m_color_selected_text.Resolve());
            else m_label_component->SetColor(TechUtils::LerpColor(m_color_text.Resolve(), m_color_text_hover.Resolve(), m_hover_progress));
        }
    }

    UIElement::Update(dt, parent_abs_pos);
}

void TechButton::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    const auto& theme = UIContext::Get().m_theme;

    ImVec2 p_min = m_absolute_pos;
    ImVec2 p_max = ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h);
    if (m_use_glass)
    {
        DrawGlassBackground(dl, p_min, p_max, {0.0,0.0,0.0,0.3});

    }
    if (m_style == Style::Normal)
    {
        ImU32 col_bg = GetColorWithAlpha(TechUtils::LerpColor(m_color_bg.Resolve(), m_color_bg_hover.Resolve(), m_hover_progress), 1.0f);
        dl->AddRectFilled(m_absolute_pos, ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h), col_bg);

        // 边框: Bracket 风格        
        ImU32 col_border = GetColorWithAlpha(m_color_border.Resolve(), 1.0f);

        TechUtils::DrawBracketedBox(dl, m_absolute_pos, ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h), col_border, 2.0f, 6.0f);
    }
    else if (m_style == Style::Tab)
    {
        ImVec4 bg_col = m_selected ? m_color_selected_bg.Resolve() : TechUtils::LerpColor(m_color_bg.Resolve(), m_color_bg_hover.Resolve(), m_hover_progress);
        dl->AddRectFilled(m_absolute_pos, ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h), GetColorWithAlpha(bg_col, 1.0f));
    }
    else if (m_style == Style::Vertical)
    {
        ImU32 col_border = GetColorWithAlpha(TechUtils::LerpColor(m_color_border.Resolve(), m_color_border_hover.Resolve(), m_hover_progress), 1.0f); 
        TechUtils::DrawBracketedBox(dl, m_absolute_pos, ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h), col_border, 2.0f, 6.0f);
        ImU32 col_text = GetColorWithAlpha(TechUtils::LerpColor(m_color_text.Resolve(), m_color_text_hover.Resolve(), m_hover_progress), 1.0f); 
        DrawVerticalText(dl, col_text);
    }
    UIElement::Draw(dl);
}

void TechButton::DrawVerticalText(ImDrawList* dl, ImU32 col)
{
    ImFont* font = UIContext::Get().m_font_small;
    if (!font) font = ImGui::GetFont();

    float angle = 3.14159f * 0.5f;
    float c = cosf(angle);
    float s = sinf(angle);

    float font_size = font->FontSize;
    ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, m_current_display_text.c_str());
    ImVec2 center = { m_absolute_pos.x + m_rect.w * 0.5f, m_absolute_pos.y + m_rect.h * 0.5f };
    ImVec2 pen_local = { -text_size.x * 0.5f, -text_size.y * 0.5f };
    ImTextureID tex_id = font->ContainerAtlas->TexID;

    if (font) ImGui::PushFont(font);

    const char* text_begin = m_current_display_text.c_str();
    const char* text_end = text_begin + m_current_display_text.length();
    const char* ptr = text_begin;
    while (ptr < text_end)
    {
        unsigned int char_code = 0;

        // [核心修正] ImTextCharFromUtf8 返回消耗的字节数 (int)，而不是指针
        int char_bytes = ImTextCharFromUtf8(&char_code, ptr, text_end);

        // 如果解码失败或到达字符串末尾，则退出循环
        if (char_bytes == 0)
        {
            break;
        }

        const ImFontGlyph* glyph = font->FindGlyph((ImWchar)char_code);
        if (glyph && glyph->Visible)
        {
            float x0 = pen_local.x + glyph->X0; float y0 = pen_local.y + glyph->Y0;
            float x1 = pen_local.x + glyph->X1; float y1 = pen_local.y + glyph->Y1;

            auto rot = [&](float x, float y)
            {
                return ImVec2(center.x + (x * c - y * s), center.y + (x * s + y * c));
            };

            dl->AddImageQuad(tex_id, rot(x0, y0), rot(x1, y0), rot(x1, y1), rot(x0, y1),
                ImVec2(glyph->U0, glyph->V0), ImVec2(glyph->U1, glyph->V0),
                ImVec2(glyph->U1, glyph->V1), ImVec2(glyph->U0, glyph->V1), col);
        }

        if (glyph)
        {
            pen_local.x += glyph->AdvanceX;
        }

        // [核心修正] 手动将指针向前移动消耗的字节数
        ptr += char_bytes;
    }

    if (font) ImGui::PopFont();
}

void TechButton::HandleMouseEvent(const ImVec2& p, bool down, bool click, bool release, bool& handled)
{
    UIElement::HandleMouseEvent(p, down, click, release, handled);

    if (m_clicked && click && on_click)
    {
        on_click();
    }
}

_UI_END
_SYSTEM_END
_NPGS_END