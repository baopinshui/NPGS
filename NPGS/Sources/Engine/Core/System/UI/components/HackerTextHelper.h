#pragma once
#include "../ui_framework.h"
#include <string>
#include <variant>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class HackerTextHelper
{
public:
    std::string m_target_text;
    std::string m_display_text;
    bool m_active = false;

    void Start(const std::string& text, float delay = 0.0f);
    void Reset();
    void Update(float dt);
    void Draw(ImDrawList* dl, ImVec2 pos, ImU32 color, ImFont* font = nullptr);

private:
    float m_iteration = 0.0f;
    float m_delay_timer = 0.0f;
    const char* m_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$#@%&*";
};



_UI_END
_SYSTEM_END
_NPGS_END