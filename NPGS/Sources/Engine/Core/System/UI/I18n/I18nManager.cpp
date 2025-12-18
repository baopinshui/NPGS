#include "I18nManager.h"
#include <fstream>
#include "nlohmann/json.hpp" // 引入 nlohmann::json 库

// 为方便使用，定义一个 using alias
using json = nlohmann::json;

_NPGS_BEGIN
_SYSTEM_BEGIN

// --- [核心修改] 更新递归函数以处理新的JSON结构 ---
void ParseJsonRecursive(
    const json& j,
    const std::string& parent_key,
    const std::string& lang_code, // e.g., "en", "zh", "en_flavor", "zh_flavor"
    std::unordered_map<std::string, std::string>& dictionary)
{
    for (auto& [key, value] : j.items())
    {
        std::string current_key = parent_key.empty() ? key : parent_key + "." + key;

        if (value.is_object())
        {
            // 判断是否是翻译节点（包含 "en" 和 "zh" 键）
            if (value.contains("en") && value.contains("zh"))
            {
                // 是翻译节点，提取对应语言的文本
                if (value.contains(lang_code) && value[lang_code].is_string())
                {
                    dictionary[current_key] = value[lang_code].get<std::string>();
                }
                else
                {
                    // 如果风味语言不存在，则回退到标准语言
                    if (lang_code == "en_flavor" && value.contains("en"))
                        dictionary[current_key] = value["en"].get<std::string>();
                    else if (lang_code == "zh_flavor" && value.contains("zh"))
                        dictionary[current_key] = value["zh"].get<std::string>();
                }
            }
            else
            {
                // 是结构节点，继续递归
                ParseJsonRecursive(value, current_key, lang_code, dictionary);
            }
        }
    }
}

I18nManager::I18nManager()
{
    // 默认加载英文
    SetLanguage(Language::English);
}

void I18nManager::SetLanguage(Language lang)
{
    if (m_current_lang == lang && !m_dictionary.empty()) return;

    m_current_lang = lang;
    LoadDictionary();
    m_version++; // 版本号递增，通知所有静态UI更新

    // 通知所有监听者
    for (auto const& [key, val] : m_callbacks)
    {
        if (val) val();
    }
}

static bool IsI18nKey(const std::string& key)
{
    if (key.empty() || key.find('.') == std::string::npos) return false;
    return key.rfind("i18ntext.", 0) == 0;
}

std::string I18nManager::Get(const std::string& key) const
{
    if (!IsI18nKey(key)) return key;

    auto it = m_dictionary.find(key);
    if (it != m_dictionary.end()) return it->second;

    return "!" + key + "!";
}

std::string I18nManager::Get(const char* key) const
{
    if (!key) return "";
    return Get(std::string(key));
}

void I18nManager::RegisterCallback(void* observer, Callback cb)
{
    m_callbacks[observer] = cb;
}

void I18nManager::UnregisterCallback(void* observer)
{
    m_callbacks.erase(observer);
}

// --- [核心修改] 重写 LoadDictionary 函数以适应新结构 ---
void I18nManager::LoadDictionary()
{
    m_dictionary.clear();

    // 将枚举映射到最终JSON文件中的键名
    std::string lang_code;
    switch (m_current_lang)
    {
    case Language::English:       lang_code = "en"; break;
    case Language::Chinese:       lang_code = "zh"; break;
    case Language::EnglishFlavor: lang_code = "en_flavor"; break;
    case Language::ChineseFlavor: lang_code = "zh_flavor"; break;
    }

    if (lang_code.empty()) return;

    const std::string filename = "Assets/Lang/translations.json";
    std::ifstream ifs(filename);
    if (!ifs.is_open())
    {
        m_dictionary["error.loading"] = "Failed to load " + filename;
        return;
    }

    try
    {
        json data = json::parse(ifs);
        ParseJsonRecursive(data, "", lang_code, m_dictionary);
    }
    catch (json::parse_error& e)
    {
        m_dictionary["error.parsing"] = "JSON parse error: " + std::string(e.what());
    }
}

_SYSTEM_END
_NPGS_END

