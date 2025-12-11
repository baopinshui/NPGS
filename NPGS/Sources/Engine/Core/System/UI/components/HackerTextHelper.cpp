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
void HackerTextHelper::Start(const std::string& text, float delay)
{
    m_target_text = text;
    m_display_text = "";
    m_active = true;
    m_timer = 0.0f; // 重置计时器
    m_delay_timer = delay;
    m_scramble_timer = 0.0f;

    // 立即生成初始掩码
    RegenerateScrambleMask();
}

void HackerTextHelper::Update(float dt)
{
    if (!m_active) return;

    if (m_delay_timer > 0.0f)
    {
        m_delay_timer -= dt;
        return;
    }

    // 1. 更新时间进度
    m_timer += dt;
    if (m_timer >= m_duration)
    {
        m_timer = m_duration;
        m_active = false; // 结束
        // 此时 m_display_text 可以设为 target，虽然在多行模式下我们可能不直接用它
        m_display_text = m_target_text;
    }

    // 2. 掩码刷新 (仅负责让未揭示部分的乱码跳动)
    m_scramble_timer += dt;
    if (m_scramble_timer >= m_scramble_interval)
    {
        RegenerateScrambleMask();
        m_scramble_timer = 0.0f;
    }

    // 注意：旧的 Update 中生成 m_display_text 的逻辑可以移除了，
    // 或者为了兼容单行绘制保留，但现在主要的逻辑转移到了 GetMixedSubString
}

float HackerTextHelper::GetProgress() const
{
    if (m_duration <= 0.001f) return 1.0f;
    return std::clamp(m_timer / m_duration, 0.0f, 1.0f);
}

// [核心实现]
std::string HackerTextHelper::GetMixedSubString(const char* start, const char* end, size_t global_offset_bytes, float progress)
{
    if (start >= end) return "";

    // 如果完全结束，直接返回原文片段
    if (progress >= 1.0f) return std::string(start, end);
    // 如果刚开始，直接返回对应的掩码片段
    if (progress <= 0.0f)
    {
        size_t len = end - start;
        if (global_offset_bytes + len <= m_cached_scramble_string.size())
            return m_cached_scramble_string.substr(global_offset_bytes, len);
        else
            return std::string(start, end); // 越界保护
    }

    // 1. 计算这一行有多少个 UTF-8 字符
    size_t total_chars = GetUtf8CharCount(start, end);

    // 2. 计算应该揭示多少个字符 (基于进度)
    size_t reveal_count = static_cast<size_t>(total_chars * progress);

    // 3. 拼装字符串
    std::string result;
    result.reserve(end - start);

    const char* ptr = start;
    size_t chars_processed = 0;
    size_t bytes_processed_in_line = 0; // 这一行处理了多少字节

    while (ptr < end)
    {
        unsigned char c = (unsigned char)*ptr;
        size_t char_len = GetByteLengthOfChar(c);

        if (chars_processed < reveal_count)
        {
            // [已揭示部分]：从原文取
            result.append(ptr, char_len);
        }
        else
        {
            // [未揭示部分]：从乱码掩码中取
            // 我们需要知道当前字符在 原始整个字符串 中的字节偏移量，以便去 mask 里找
            size_t mask_index = global_offset_bytes + bytes_processed_in_line;

            if (mask_index < m_cached_scramble_string.size())
            {
                // 注意：这里简单假设 mask 的字节结构和 target 一致
                // (RegenerateScrambleMask 保证了这一点)
                // 取出 mask 中对应位置的字符 (字节数应相同)
                result.append(m_cached_scramble_string, mask_index, char_len);
            }
            else
            {
                result.append("?"); // 异常保护
            }
        }

        ptr += char_len;
        chars_processed++;
        bytes_processed_in_line += char_len;
    }

    return result;
}

size_t HackerTextHelper::GetUtf8CharCount(const char* start, const char* end)
{
    size_t count = 0;
    const char* s = start;
    while (s < end)
    {
        count++;
        unsigned char c = (unsigned char)*s;
        s += GetByteLengthOfChar(c);
    }
    return count;
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


_UI_END
_SYSTEM_END
_NPGS_END