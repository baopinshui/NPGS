// --- START OF FILE HackerTextHelper.h ---
#pragma once
#include "../ui_framework.h"
#include <string>
#include <vector>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class HackerTextHelper
{
public:
    std::string m_target_text;
    std::string m_display_text;
    bool m_active = false;

    HackerTextHelper(); 

    void Start(const std::string& text, float delay = 0.0f);
    void Reset();
    void Update(float dt);
    void Draw(ImDrawList* dl, ImVec2 pos, ImU32 color, ImFont* font = nullptr);

private:
    float m_iteration = 0.0f;
    float m_delay_timer = 0.0f;

    // 帧率控制
    float m_scramble_timer = 0.0f;
    const float SCRAMBLE_INTERVAL = 0.016f;

    // 字符池
    const char* m_ascii_pool = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$#@%&*<>";
    std::vector<std::string> m_cjk_pool; 

    // 乱码掩码
    std::string m_cached_scramble_string;

    // 辅助函数
    size_t GetUtf8CharCount(const std::string& str);
    size_t GetByteLengthOfChar(unsigned char c); 
    void InitializePools();
    void RegenerateScrambleMask();
};

_UI_END
_SYSTEM_END
_NPGS_END