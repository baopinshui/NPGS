#pragma once
#include "IScreen.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional> // [新增]

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class ScreenManager
{
public:
    // [新增] 定义要执行的动作
    enum class ActionType { None, Push, Pop, Change };

    ScreenManager(); // [新增] 构造函数

    void RegisterScreen(const std::string& name, std::shared_ptr<IScreen> screen);

    // [修改] 公共接口现在只“请求”动作，而不是立即执行
    void RequestPushScreen(const std::string& name);
    void RequestPopScreen();
    void RequestChangeScreen(const std::string& name);

    void Update(float dt);
    void Draw();
    void ApplyPendingChanges(); // [新增] 在主循环中调用的新方法

    bool IsEmpty() const { return m_screen_stack.empty(); }

private:
    struct PendingChange
    {
        ActionType Action;
        std::string ScreenName;
    };
    std::vector<PendingChange> m_pending_changes;

    std::vector<std::shared_ptr<IScreen>> m_screen_stack;
    std::unordered_map<std::string, std::shared_ptr<IScreen>> m_registered_screens;
};

_UI_END
_SYSTEM_END
_NPGS_END