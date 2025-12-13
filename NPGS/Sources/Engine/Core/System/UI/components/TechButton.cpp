#include "TechButton.h"
#include "../TechUtils.h"
#include "../Utils/I18nManager.h" // [新增]
#include <imgui_internal.h>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

TechButton::TechButton(const std::string& key_or_text, Style style)
    : m_source_text(key_or_text), m_style(style) // [修改] 直接保存源字符串
{
    m_block_input = true;

    // [修改] 初始文本直接通过 TR 获取
    m_current_display_text = TR(key_or_text);

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

    if (m_style != Style::Vertical && m_style != Style::Invisible)
    {
        // [核心修改] 直接将源字符串传给 TechText，它会自己处理
        m_label_component = std::make_shared<TechText>(m_source_text);
        m_label_component->m_align_h = Alignment::Center;
        m_label_component->m_align_v = Alignment::Center;
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
        // 简单地每帧都获取最新文本
        m_current_display_text = TR(m_source_text);
    }
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
            ImVec4 col_normal = m_color_text.Resolve();
            ImVec4 col_hover = m_color_text_hover.Resolve();
            m_label_component->SetColor(TechUtils::LerpColor(col_normal, col_hover, m_hover_progress));
        }
        else if (m_style == Style::Tab)
        {
            // Tab: 选中时黑色，未选中时灰色(悬停变亮)
            if (m_selected)
            {
                m_label_component->SetColor(m_color_selected_text.Resolve());
            }
            else
            {
                ImVec4 col_idle = m_color_text.Resolve();
                ImVec4 col_hover = m_color_text_hover.Resolve();
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
        DrawGlassBackground(dl, p_min, p_max, {0.0,0.0,0.0,0.3});

    }
    if (m_style == Style::Normal)
    {
        ImVec4 bg_idle = m_color_bg.Resolve();
        ImVec4 bg_hover = m_color_bg_hover.Resolve();
        ImU32 col_bg = GetColorWithAlpha(TechUtils::LerpColor(bg_idle, bg_hover, m_hover_progress), 1.0f);

        dl->AddRectFilled(m_absolute_pos, ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h), col_bg);

        // 边框: Bracket 风格        
        ImU32 col_border = GetColorWithAlpha(m_color_border.Resolve(), 1.0f);

        TechUtils::DrawBracketedBox(dl, m_absolute_pos, ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h), col_border, 2.0f, 6.0f);
    }
    else if (m_style == Style::Tab)
    {
        ImVec4 bg_col;
        if (m_selected)
        {
            bg_col = m_color_selected_bg.Resolve();
        }
        else
        {
            ImVec4 bg_idle = m_color_bg.Resolve();
            ImVec4 bg_hover = m_color_bg_hover.Resolve();
            bg_col = TechUtils::LerpColor(bg_idle, bg_hover, m_hover_progress);
        }
        dl->AddRectFilled(m_absolute_pos, ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h), GetColorWithAlpha(bg_col, 1.0f));
    }
    else if (m_style == Style::Vertical)
    {
        ImVec4 border_idle = m_color_border.Resolve();
        ImVec4 border_hover = m_color_border_hover.Resolve();
        ImU32 col_border = GetColorWithAlpha(TechUtils::LerpColor(border_idle, border_hover, m_hover_progress), 1.0f);
        TechUtils::DrawBracketedBox(dl, m_absolute_pos, ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h), col_border, 2.0f, 6.0f);
        ImVec4 text_idle = m_color_text.Resolve();
        ImVec4 text_hover = m_color_text_hover.Resolve();
        ImU32 col_text = GetColorWithAlpha(TechUtils::LerpColor(text_idle, text_hover, m_hover_progress), 1.0f);
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