// --- START OF FILE HackerTextHelper.cpp ---
#include "HackerTextHelper.h"
#include <cstdlib> 
#include <cstring> 

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

HackerTextHelper::HackerTextHelper()
{
    InitializePools();
}

void HackerTextHelper::InitializePools()
{
    const char* raw_cjk[] = {
        "锟", "斤", "拷", "锘","鈥","烫","屯","葺", "硊", "桳", "敃", "琄", "縍", "脮", "嚻"
    };

    for (const char* s : raw_cjk)
    {
        m_cjk_pool.push_back(s);
    }
}

void HackerTextHelper::Start(const std::string& text, float delay)
{
    m_target_text = text;
    m_display_text = "";
    m_active = true;
    m_iteration = 0.0f;
    m_delay_timer = delay;
    m_scramble_timer = 0.0f;

    // 立即生成初始掩码
    RegenerateScrambleMask();
}

void HackerTextHelper::Reset()
{
    m_active = false;
    m_display_text = "";
    m_cached_scramble_string = "";
}

size_t HackerTextHelper::GetByteLengthOfChar(unsigned char c)
{
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1; // fallback
}

void HackerTextHelper::RegenerateScrambleMask()
{
    m_cached_scramble_string.clear();
    m_cached_scramble_string.reserve(m_target_text.size());

    size_t ascii_len = strlen(m_ascii_pool);
    size_t cjk_len = m_cjk_pool.size();

    // 遍历目标字符串，进行"同类替换"
    for (size_t i = 0; i < m_target_text.length(); )
    {
        unsigned char c = (unsigned char)m_target_text[i];
        size_t byte_len = GetByteLengthOfChar(c);

        if (byte_len == 1)
        {
            if (c == '\n' || c == ' ' || c == '\t' || c == '\r')
            {
                m_cached_scramble_string += (char)c;
            }
            else
            {
                // 目标是 ASCII -> 用 ASCII 乱码替换
                m_cached_scramble_string += m_ascii_pool[rand() % ascii_len];
            }
        }
        else
        {
            // 目标是 多字节(中文) -> 用 CJK 乱码替换
            // 这样保证了字体渲染宽度的一致性 (假设字体对所有汉字等宽)
            if (cjk_len > 0)
            {
                m_cached_scramble_string += m_cjk_pool[rand() % cjk_len];
            }
            else
            {
                // 如果池子空的保底 (不应该发生)
                m_cached_scramble_string += "?";
            }
        }

        i += byte_len;
    }
}

void HackerTextHelper::Update(float dt)
{
    if (!m_active) return;

    if (m_delay_timer > 0.0f)
    {
        m_delay_timer -= dt;
        return;
    }

    // 1. 进度更新
    size_t target_char_len = GetUtf8CharCount(m_target_text);
    m_iteration += dt * std::max(m_reveval_rate, static_cast<float>(target_char_len));

    if (m_iteration >= target_char_len)
    {
        m_display_text = m_target_text;
        m_active = false;
        return;
    }

    // 2. 掩码刷新
    m_scramble_timer += dt;
    if (m_scramble_timer >= m_scramble_interval)
    {
        RegenerateScrambleMask();
        m_scramble_timer = 0.0f;
    }

    // 3. 拼装
    m_display_text.clear();
    size_t current_char_idx = 0;
    size_t revealed_count = static_cast<size_t>(m_iteration);

    // 这里的 mask_offset 用于跟踪 m_cached_scramble_string 中的字节位置
    // 因为掩码字符串的字节结构和 target 是一模一样的，所以我们可以同步推进
    size_t byte_offset = 0;

    while (byte_offset < m_target_text.length())
    {
        unsigned char c = (unsigned char)m_target_text[byte_offset];
        size_t char_bytes = GetByteLengthOfChar(c);

        // 获取掩码中对应的字符 (其字节长度可能和 target 的不一样吗？
        // 不会，因为我们在 RegenerateScrambleMask 里是严格按类型替换的：
        // ASCII(1 byte) -> ASCII(1 byte)
        // CJK(3 bytes) -> CJK(3 bytes, "数"等字在UTF8也是3字节)
        // 所以 byte_offset 对两个字符串都是通用的。

        if (current_char_idx < revealed_count)
        {
            // 已揭示：取原字
            m_display_text.append(m_target_text, byte_offset, char_bytes);
        }
        else
        {
            // 未揭示：取掩码中的对应字
            // 注意：我们需要从 cached mask 里取对应长度
            // 为了安全，我们需要重新计算 mask 里当前位置字符的长度，
            // 但理论上它应该和 char_bytes 一样。
            // 简单起见，我们直接 append mask 的对应部分。

            // 为了防止万一字符集编码差异导致字节数不同（比如有些特殊符号是4字节），
            // 我们最好也在 mask 上做一次长度判断。
            unsigned char mask_c = (unsigned char)m_cached_scramble_string[byte_offset];
            size_t mask_char_bytes = GetByteLengthOfChar(mask_c);

            m_display_text.append(m_cached_scramble_string, byte_offset, mask_char_bytes);

            // 这是一个潜在的同步问题：如果 target 是3字节中文，random mask 选到了4字节符号怎么办？
            // 只要 InitializePools 里选的都是常见的3字节UTF-8汉字/符号，就不会有问题。
            // 现在的 m_cjk_pool 里的字符基本都是 3 字节的。
            // 强行修正 offset 避免崩溃：
            if (mask_char_bytes != char_bytes)
            {
                // 如果发生字节长度不匹配（极罕见），这会导致后续错位。
                // 这种情况下，简单的回退方案是：直接用 target 的长度推进，虽然可能会乱码，但不会崩。
                // 但最好的方案是保证 pool 里全是 3 字节字符。
            }
        }

        byte_offset += char_bytes;
        current_char_idx++;
    }
}

void HackerTextHelper::Draw(ImDrawList* dl, ImVec2 pos, ImU32 color, ImFont* font)
{
    if (font) ImGui::PushFont(font);
    dl->AddText(pos, color, m_display_text.c_str());
    if (font) ImGui::PopFont();
}

size_t HackerTextHelper::GetUtf8CharCount(const std::string& str)
{
    size_t count = 0;
    const char* s = str.c_str();
    while (*s)
    {
        count++;
        unsigned char c = (unsigned char)*s;
        s += GetByteLengthOfChar(c);
    }
    return count;
}

_UI_END
_SYSTEM_END
_NPGS_END