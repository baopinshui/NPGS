#pragma once
#include "../ui_framework.h"
#include "../components/CelestialInfoPanel.h" // For CelestialData struct
#include "Engine/Core/Types/Entries/Astro/Star.h"
#include "Engine/Core/Types/Entries/Astro/Planet.h"
#include <string>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// 静态工具类，负责将 Astro 对象转换为 UI 数据
class AstroDataBuilder
{
public:
    // 主入口函数：传入任何天体对象，返回 UI 数据包
    static CelestialData BuildDataForObject(const Astro::IAstroObject* object);

    // 针对不同类型的具体构建函数
    static CelestialData BuildDataForStar(const Astro::AStar* star);
    static CelestialData BuildDataForPlanet(const Astro::APlanet* planet);
    // ... 未来可以添加 BuildDataForBlackHole 等

    // 辅助函数
    // 将 double/float 格式化为带科学计数法的字符串
    static std::string FormatScientific(double value, const char* unit);
    // 将 enum 转换为可读字符串
    static std::string StarPhaseToString(Astro::AStar::EEvolutionPhase phase);
    static std::string StarFromToString(Astro::AStar::EStarFrom from); // 形成方式
    static std::string PredictOutcomeToString(const Astro::AStar* star); // 推断预期结局
    static std::string PredictRemnantToString(const Astro::AStar* star); // 推断预期残骸
    static std::string PlanetTypeToString(Astro::APlanet::EPlanetType type);
};

_UI_END
_SYSTEM_END
_NPGS_END