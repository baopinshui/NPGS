// --- HackerTextHelper.h ---

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
    // m_display_text 在新逻辑下不再作为唯一的输出，但在非多行同步模式下仍可保留兼容性
    std::string m_display_text;

    bool m_active = false;

    // [修改] 用固定时长来控制进度，而不是字符速率，这样更容易同步
    float m_duration = 0.8f; // 默认 0.8秒完成解码
    float m_timer = 0.0f;

    float m_scramble_interval = 0.05f; // 乱码跳动间隔

    HackerTextHelper();

    void Start(const std::string& text, float delay = 0.0f);
    void Reset();
    void Update(float dt);

    // [新增] 获取当前的总体进度 (0.0 ~ 1.0)
    float GetProgress() const;

    // [新增] 核心函数：传入一段原始文本的起止指针，返回这段文本应用了 progress 后的混合字符串
    // global_offset_bytes: 这一行在原始字符串 m_target_text 中的字节偏移量（用于从 mask 中取对应乱码）
    std::string GetMixedSubString(const char* start, const char* end, size_t global_offset_bytes, float progress);

private:
    float m_delay_timer = 0.0f;
    float m_scramble_timer = 0.0f;

    const char* m_ascii_pool = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$#@%&*<>";
    std::vector<std::string> m_cjk_pool;

    // 乱码掩码 (长度与 m_target_text 一致)
    std::string m_cached_scramble_string;

    size_t GetByteLengthOfChar(unsigned char c);
    size_t GetUtf8CharCount(const char* start, const char* end);
    void InitializePools();
    void RegenerateScrambleMask();
};

_UI_END
_SYSTEM_END
_NPGS_END