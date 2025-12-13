#pragma once
#include "imgui.h" // For ImTextureID

#include "../../../../Program/DataStructures.h"
// 前置声明以避免包含完整的头文件，降低编译依赖
namespace Npgs { class FApplication; };
namespace Npgs { namespace Runtime { namespace Graphics { class FVulkanUIRenderer; } } }

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

struct AppContext
{
    // 核心系统和服务
    FApplication* Application;
    Npgs::Runtime::Graphics::FVulkanUIRenderer* UIRenderer;

    // 游戏世界数据
    FGameArgs* GameArgs;
    FBlackHoleArgs* BlackHoleArgs;
    double* GameTime;
    double* TimeRate;
    double* RealityTime; // 有些UI逻辑（如电影感面板）可能需要真实时间

    // 共享资源 (由Application在启动时填充)
    ImTextureID RKKVID;
    ImTextureID stage0ID, stage1ID, stage2ID, stage3ID, stage4ID;
};

_UI_END
_SYSTEM_END
_NPGS_END