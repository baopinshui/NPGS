#pragma once
#include "ui_framework.h"
#include <string>
#include <functional>
#include <unordered_map>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class UIFactory
{
public:
    using Creator = std::function<std::shared_ptr<UIElement>()>;

    static UIFactory& Get()
    {
        static UIFactory instance;
        return instance;
    }

    template<typename T>
    void Register(const std::string& type_name)
    {
        m_creators[type_name] = []() { return std::make_shared<T>(); };
    }

    // 特定构造函数的注册
    void Register(const std::string& type_name, Creator creator)
    {
        m_creators[type_name] = creator;
    }

    std::shared_ptr<UIElement> Create(const std::string& type_name)
    {
        auto it = m_creators.find(type_name);
        if (it != m_creators.end())
        {
            return it->second();
        }
        NpgsCoreError("UIFactory: Unknown UI type '{}'", type_name);
        return nullptr;
    }

private:
    UIFactory() = default;
    std::unordered_map<std::string, Creator> m_creators;
};

_UI_END
_SYSTEM_END
_NPGS_END