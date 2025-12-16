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
    const std::string& lang_code, // 新增参数：当前需要加载的语言代码 ("en", "zh")
    std::unordered_map<std::string, std::string>& dictionary)
{
    // 遍历当前JSON对象的所有键值对
    for (auto& [key, value] : j.items())
    {
        // 构造完整的键名
        std::string current_key = parent_key.empty() ? key : parent_key + "." + key;

        // --- 逻辑判断 ---
        if (value.is_object())
        {
            // 判断这个 object 是 "翻译对象" 还是 "结构对象"
            // 我们通过检查它是否包含当前语言代码的键来实现
            if (value.contains(lang_code))
            {
                // 这是一个 "翻译对象" (如: { "en": "...", "zh": "..." })
                // 检查对应语言的值是否为字符串
                if (value[lang_code].is_string())
                {
                    // 提取该语言的翻译并存入字典，然后停止对该分支的递归
                    dictionary[current_key] = value[lang_code].get<std::string>();
                }
            }
            else
            {
                // 这是一个 "结构对象" (如: "time": { ... }), 继续递归深入
                ParseJsonRecursive(value, current_key, lang_code, dictionary);
            }
        }
        // 我们只处理 object 类型的值，因为所有翻译现在都封装在 object 中
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

    std::string lang_code;
    if (m_current_lang == Language::English) lang_code = "en";
    else if (m_current_lang == Language::Chinese) lang_code = "zh";

    if (lang_code.empty()) return;

    // 所有翻译现在都在同一个文件里
    const std::string filename = "Assets/Lang/translations.json";

    std::ifstream ifs(filename);
    if (!ifs.is_open())
    {
        m_dictionary["error.loading"] = "Failed to load " + filename;
        return;
    }

    try
    {
        // 解析JSON
        json data = json::parse(ifs);

        // 调用递归函数填充字典，传入当前需要加载的语言代码
        ParseJsonRecursive(data, "", lang_code, m_dictionary);
    }
    catch (json::parse_error& e)
    {
        m_dictionary["error.parsing"] = "JSON parse error in " + filename + ": " + e.what();
    }
}

_SYSTEM_END
_NPGS_END

