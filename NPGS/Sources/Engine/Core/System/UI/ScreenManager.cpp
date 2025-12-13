#include "ScreenManager.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

ScreenManager::ScreenManager() {}

void ScreenManager::RegisterScreen(const std::string& name, std::shared_ptr<IScreen> screen)
{
    screen->SetManager(this);
    m_registered_screens[name] = screen;
}

// --- 请求方法 ---
void ScreenManager::RequestPushScreen(const std::string& name)
{
    m_pending_changes.push_back({ ActionType::Push, name });
}

void ScreenManager::RequestPopScreen()
{
    m_pending_changes.push_back({ ActionType::Pop, "" });
}

void ScreenManager::RequestChangeScreen(const std::string& name)
{
    m_pending_changes.push_back({ ActionType::Change, name });
}

// --- 核心修改：延迟应用 ---
void ScreenManager::ApplyPendingChanges()
{
    for (const auto& change : m_pending_changes)
    {
        switch (change.Action)
        {
        case ActionType::Push:
        {
            auto it = m_registered_screens.find(change.ScreenName);
            if (it != m_registered_screens.end())
            {
                m_screen_stack.push_back(it->second);
                it->second->OnEnter();
            }
            break;
        }
        case ActionType::Pop:
        {
            if (!m_screen_stack.empty())
            {
                m_screen_stack.back()->OnExit();
                m_screen_stack.pop_back();
            }
            break;
        }
        case ActionType::Change:
        {
            // 先 Pop 所有
            while (!m_screen_stack.empty())
            {
                m_screen_stack.back()->OnExit();
                m_screen_stack.pop_back();
            }
            // 再 Push 新的
            auto it = m_registered_screens.find(change.ScreenName);
            if (it != m_registered_screens.end())
            {
                m_screen_stack.push_back(it->second);
                it->second->OnEnter();
            }
            break;
        }
        case ActionType::None:
            break;
        }
    }
    m_pending_changes.clear(); // 清空队列，为下一帧做准备
}


void ScreenManager::Update(float dt)
{
    if (m_screen_stack.empty()) return;

    for (auto it = m_screen_stack.rbegin(); it != m_screen_stack.rend(); ++it)
    {
        (*it)->Update(dt);
        if ((*it)->BlocksUpdate())
        {
            break;
        }
    }
}

void ScreenManager::Draw()
{
    if (m_screen_stack.empty()) return;

    for (const auto& screen : m_screen_stack)
    {
        screen->Draw();
    }
}

_UI_END
_SYSTEM_END
_NPGS_END