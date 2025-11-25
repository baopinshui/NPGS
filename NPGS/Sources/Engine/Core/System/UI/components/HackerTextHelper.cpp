#include "HackerTextHelper.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

void HackerTextHelper::Start(const std::string& text, float delay)
{
    m_target_text = text;
    m_display_text = "";
    m_active = true;
    m_iteration = 0.0f;
    m_delay_timer = delay;
}

void HackerTextHelper::Reset()
{
    m_active = false;
    m_display_text = "";
}

void HackerTextHelper::Update(float dt)
{
    if (!m_active) return;

    if (m_delay_timer > 0.0f)
    {
        m_delay_timer -= dt;
        return;
    }

    m_iteration += dt * 10.0f; // 速度系数
    if (m_iteration >= m_target_text.length())
    {
        m_display_text = m_target_text;
        m_active = false;
        return;
    }

    m_display_text = "";
    for (size_t i = 0; i < m_target_text.length(); ++i)
    {
        if (i < m_iteration)
        {
            m_display_text += m_target_text[i];
        }
        else
        {
            m_display_text += m_chars[rand() % (strlen(m_chars))];
        }
    }
}

void HackerTextHelper::Draw(ImDrawList* dl, ImVec2 pos, ImU32 color, ImFont* font)
{
    if (font) ImGui::PushFont(font);
    dl->AddText(pos, color, m_display_text.c_str());
    if (font) ImGui::PopFont();
}


_UI_END
_SYSTEM_END
_NPGS_END