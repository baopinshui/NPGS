#pragma once
#include "ui_framework.h"
#include "AppContext.h"
#include <memory>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class ScreenManager;

class IScreen
{
public:
    IScreen(AppContext& context) : m_context(context) {}
    virtual ~IScreen() = default;

    virtual void OnEnter() = 0;
    virtual void OnExit() = 0;

    virtual void Update(float dt) = 0;
    virtual void Draw() = 0;

    void SetManager(ScreenManager* manager) { m_screen_manager = manager; }

    bool BlocksUpdate() const { return m_blocks_update; }

protected:
    ScreenManager* m_screen_manager = nullptr;
    AppContext& m_context;
    std::shared_ptr<UIRoot> m_ui_root;
    bool m_blocks_update = true;
};

_UI_END
_SYSTEM_END
_NPGS_END