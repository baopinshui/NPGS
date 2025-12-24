#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include "../ui_framework.h" 

_NPGS_BEGIN
_SYSTEM_BEGIN

class I18nManager
{
public:
    struct LanguageInfo
    {
        std::string code;
        std::string name;
    };

    static I18nManager& Get() { static I18nManager instance; return instance; }

    // 加载/切换语言
    void SetLanguage(const std::string& lang_code);
    const std::string& GetCurrentLanguageCode() const { return m_current_lang_code; }

    const std::vector<LanguageInfo>& GetAvailableLanguages() const { return m_available_languages; }
    // 核心查找函数
    std::string Get(const std::string& key) const;

    std::string Get(const char* key) const; // 新增重载

    // 版本号，用于UI组件轮询
    uint32_t GetVersion() const { return m_version; }

    // 注册语言变更回调 (用于通知应用层刷新动态数据)
    using Callback = std::function<void()>;
    void RegisterCallback(void* observer, Callback cb);
    void UnregisterCallback(void* observer);

private:
    I18nManager();
    void LoadDictionary();
    void LoadAvailableLanguages(); // [新增] 加载语言列表的私有函数

    std::string m_current_lang_code; // [修改]
    std::vector<LanguageInfo> m_available_languages; // [新增]

    std::unordered_map<std::string, std::string> m_dictionary;
    std::unordered_map<void*, Callback> m_callbacks;
    uint32_t m_version = 1;
};

inline std::string TR(const std::string& key)
{
    return I18nManager::Get().Get(key);
}

// [新增] 针对字符串字面量的优化重载
inline std::string TR(const char* key)
{
    return I18nManager::Get().Get(key);
}

_SYSTEM_END
_NPGS_END