#include "TechButton.h"
#include "../TechUtils.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

TechButton::TechButton(const std::string& label, Style style)
    : m_text_str(label), m_style(style)
{
    m_block_input = true;

    // 只有非 Vertical 模式才需要内部 TechText 组件来负责布局
    if (m_style != Style::Vertical && m_style != Style::Invisible)
    {
        m_label_component = std::make_shared<TechText>(label);
        m_label_component->m_align_h = Alignment::Center;
        m_label_component->m_align_v = Alignment::Center;
        m_label_component->m_block_input = false; // 点击穿透给按钮
        AddChild(m_label_component);
    }
}
void TechButton::SetLabel(const std::string& text, bool with_effect)
{
    // 1. 更新内部字符串副本 (Vertical 模式直接使用这个变量绘制)
    m_text_str = text;

    // 2. 如果存在子文本组件 (Normal / Tab 模式)
    if (m_label_component)
    {
        // 如果调用者希望有特效，强制开启组件的特效开关
        if (with_effect)
        {
            m_label_component->SetAnimMode(TechTextAnimMode::Hacker);
        }

        // 设置新文本
        // 注意：TechText::SetText 内部逻辑是：如果 m_use_effect 为 true，它会自动调用 Start()
        m_label_component->SetText(text);


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
    // 1. 动画状态更新
    if (m_hovered) m_hover_progress += dt * m_anim_speed;
    else m_hover_progress -= dt * m_anim_speed;
    m_hover_progress = std::clamp(m_hover_progress, 0.0f, 1.0f);

    const auto& theme = UIContext::Get().m_theme;

    // 2. 根据样式更新子组件颜色 (Normal / Tab)
    if (m_label_component)
    {
        // 确保子组件填满按钮
        m_label_component->m_rect.w = m_rect.w;
        m_label_component->m_rect.h = m_rect.h;

        if (m_style == Style::Normal)
        {
            // Normal: 悬停变色
            ImVec4 col_normal = theme.color_accent;
            ImVec4 col_hover = { 0.0f, 0.0f, 0.0f, 1.0f }; // 悬停变黑 (配合背景亮起)
            m_label_component->SetColor(TechUtils::LerpColor(col_normal, col_hover, m_hover_progress));
        }
        else if (m_style == Style::Tab)
        {
            // Tab: 选中时黑色，未选中时灰色(悬停变亮)
            if (m_selected)
            {
                m_label_component->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
            }
            else
            {
                ImVec4 col_idle = theme.color_text_disabled;
                ImVec4 col_hover = theme.color_accent;
                m_label_component->SetColor(TechUtils::LerpColor(col_idle, col_hover, m_hover_progress));
            }
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
        DrawGlassBackground(dl, p_min, p_max);

    }
    if (m_style == Style::Normal)
    {
        ImVec4 bg_idle = theme.color_accent; bg_idle.w = 0.1f;
        ImVec4 bg_hover = theme.color_accent;
        ImU32 col_bg = GetColorWithAlpha(TechUtils::LerpColor(bg_idle, bg_hover, m_hover_progress), 1.0f);

        dl->AddRectFilled(m_absolute_pos, ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h), col_bg);

        // 边框: Bracket 风格
        ImU32 col_border = GetColorWithAlpha(theme.color_accent, 1.0f);
        TechUtils::DrawBracketedBox(dl, m_absolute_pos, ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h), col_border, 2.0f, 6.0f);
    }
    else if (m_style == Style::Tab)
    {
        ImVec4 bg_col;
        if (m_selected)
        {
            bg_col = theme.color_accent;
        }
        else
        {
            bg_col = theme.color_accent;
            bg_col.w = 0.0f + 0.2f * m_hover_progress; 
        }
        dl->AddRectFilled(m_absolute_pos, ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h), GetColorWithAlpha(bg_col, 1.0f));
    }
    else if (m_style == Style::Vertical)
    {
        ImVec4 bg_idle = theme.color_border; bg_idle.w = 1.0f;
        ImVec4 bg_hover = theme.color_accent;
        ImU32 col_border = GetColorWithAlpha(TechUtils::LerpColor(bg_idle, bg_hover, m_hover_progress), 1.0f);
        TechUtils::DrawBracketedBox(dl, m_absolute_pos, ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h), col_border, 2.0f, 6.0f);
        ImU32 col_text = GetColorWithAlpha(m_hovered ? theme.color_accent : theme.color_text_disabled, 1.0f);
        DrawVerticalText(dl, col_text);
    }
    UIElement::Draw(dl);
}

void TechButton::DrawVerticalText(ImDrawList* dl, ImU32 col)
{
    // 复用之前写好的旋转逻辑
    ImFont* font = UIContext::Get().m_font_small;
    if (!font) font = ImGui::GetFont();

    float angle = 3.14159f * 0.5f;
    float c = cosf(angle);
    float s = sinf(angle);

    float font_size = font->FontSize;
    ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, m_text_str.c_str());
    ImVec2 center = { m_absolute_pos.x + m_rect.w * 0.5f, m_absolute_pos.y + m_rect.h * 0.5f };
    ImVec2 pen_local = { -text_size.x * 0.5f, -text_size.y * 0.5f };
    ImTextureID tex_id = font->ContainerAtlas->TexID;

    if (font) ImGui::PushFont(font); // 确保字体状态正确

    for (const char* p = m_text_str.c_str(); *p; p++)
    {
        const ImFontGlyph* glyph = font->FindGlyph(*p);
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
        pen_local.x += glyph->AdvanceX;
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