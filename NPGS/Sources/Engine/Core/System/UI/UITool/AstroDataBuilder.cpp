#include "AstroDataBuilder.h"
#include <cstdio> // for snprintf

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// --- 主分发函数 ---
CelestialData AstroDataBuilder::BuildDataForObject(const Astro::IAstroObject* object)
{
    if (!object) return {};

    // 使用 dynamic_cast 判断具体类型并分发
    if (const auto* star = dynamic_cast<const Astro::AStar*>(object))
    {
        return BuildDataForStar(star);
    }
    if (const auto* planet = dynamic_cast<const Astro::APlanet*>(object))
    {
        return BuildDataForPlanet(planet);
    }
    // ... 可以继续添加对黑洞、小行星等的判断 ...

    // 如果没有匹配的类型，返回空数据
    return {};
}

// [修改] 扩展恒星数据构建，生成多个页面
CelestialData AstroDataBuilder::BuildDataForStar(const Astro::AStar* star)
{
    if (!star) return {};

    CelestialData data;

    // =======================================================
    // 页 1: PHYSICAL (物理参数)
    // =======================================================
    {
        InfoPage page;
        page.name = "PHYSICAL";

        InfoGroup g_basic;
        g_basic.items.push_back({ "类型", star->GetStellarClass().ToString() });
        g_basic.items.push_back({ "演化阶段", StarPhaseToString(star->GetEvolutionPhase()), true });
        g_basic.items.push_back({ "演化进度", std::to_string(star->GetEvolutionProgress() * 100.0) + "%" });
        g_basic.items.push_back({ "年龄", FormatScientific(star->GetAge(), "yr") });
        g_basic.items.push_back({ "预期寿命", FormatScientific(star->GetLifetime(), "yr") });
        page.groups.push_back(g_basic);

        InfoGroup g_mass;
        g_mass.items.push_back({ "质量", FormatScientific(star->GetMass(), "kg") });
        g_mass.items.push_back({ "初始质量", FormatScientific(star->GetInitialMass(), "kg") });
        page.groups.push_back(g_mass);

        InfoGroup g_energy;
        g_energy.items.push_back({ "热功率", FormatScientific(star->GetLuminosity(), "W") });
        g_energy.items.push_back({ "表面温度", FormatScientific(star->GetTeff(), "K"), true });
        g_energy.items.push_back({ "核心温度", FormatScientific(star->GetCoreTemp(), "K") });
        g_energy.items.push_back({ "核心密度", FormatScientific(star->GetCoreDensity(), "kg/m^3") });
        page.groups.push_back(g_energy);

        InfoGroup g_phys;
        g_phys.items.push_back({ "半径", FormatScientific(star->GetRadius(), "m") });
        g_phys.items.push_back({ "扁率", std::to_string(star->GetOblateness()) });
        g_phys.items.push_back({ "自转周期", FormatScientific(star->GetSpin(), "s") });
        g_phys.items.push_back({ "逃逸速度", FormatScientific(star->GetEscapeVelocity(), "m/s") });
        g_phys.items.push_back({ "磁场强度", FormatScientific(star->GetMagneticField(), "T") });
        page.groups.push_back(g_phys);

        data.push_back(page);
    }

    // =======================================================
    // 页 2: COMPOSITION (成分)
    // =======================================================
    {
        InfoPage page;
        page.name = "COMPOSITION";

        InfoGroup g_surface;
        g_surface.items.push_back({ "金属丰度 [Fe/H]", std::to_string(star->GetFeH()) });
        g_surface.items.push_back({ "表面氢(H1)质量分数", std::to_string(star->GetSurfaceH1()) });
        g_surface.items.push_back({ "表面金属(Z)质量分数", std::to_string(star->GetSurfaceZ()) });
        g_surface.items.push_back({ "表面挥发物质量分数", std::to_string(star->GetSurfaceVolatiles()) });
        g_surface.items.push_back({ "表面含能核素质量分数", std::to_string(star->GetSurfaceEnergeticNuclide()) });
        page.groups.push_back(g_surface);

        InfoGroup g_wind;
        g_wind.items.push_back({ "星风速度", FormatScientific(star->GetStellarWindSpeed(), "m/s") });
        g_wind.items.push_back({ "星风质量流失率", FormatScientific(star->GetStellarWindMassLossRate(), "kg/s") });
        page.groups.push_back(g_wind);

        data.push_back(page);
    }

    // =======================================================
    // 页 3: EVOLUTION (演化)
    // =======================================================
    {
        InfoPage page;
        page.name = "EVOLUTION";

        InfoGroup g_evo;
        g_evo.items.push_back({ "形成方式", StarFromToString(star->GetStarFrom()) });
        g_evo.items.push_back({ "预期结局", PredictOutcomeToString(star) });
        g_evo.items.push_back({ "预期残骸", PredictRemnantToString(star) });
        page.groups.push_back(g_evo);

        InfoGroup g_misc;
        g_misc.items.push_back({ "是否为孤星", star->GetIsSingleStar() ? "是" : "否" });
        g_misc.items.push_back({ "是否存在行星系", star->GetHasPlanets() ? "是" : "否" });
        g_misc.items.push_back({ "举星器最低线圈质量", FormatScientific(star->GetMinCoilMass(), "kg") });
        page.groups.push_back(g_misc);

        data.push_back(page);
    }

    return data;
}

// --- 行星数据构建 ---
CelestialData AstroDataBuilder::BuildDataForPlanet(const Astro::APlanet* planet)
{
    if (!planet) return {};

    CelestialData data;

    // =======================================================
    // 页 1: PHYSICAL (物理参数)
    // =======================================================
    {
        InfoPage page;
        page.name = "PHYSICAL";

        InfoGroup g_type;
        g_type.items.push_back({ "类型", PlanetTypeToString(planet->GetPlanetType()) });
        g_type.items.push_back({ "是否迁移行星", planet->GetMigration() ? "是" : "否" });
        page.groups.push_back(g_type);

        InfoGroup g_phys;
        g_phys.items.push_back({ "总质量", planet->GetMass().str() + " kg" });
        g_phys.items.push_back({ "半径", FormatScientific(planet->GetRadius(), "m") });
        g_phys.items.push_back({ "表面温度", FormatScientific(planet->GetBalanceTemperature(), "K") });
        g_phys.items.push_back({ "自转周期", FormatScientific(planet->GetSpin(), "s") });
        g_phys.items.push_back({ "逃逸速度", FormatScientific(planet->GetEscapeVelocity(), "m/s") });
        g_phys.items.push_back({ "年龄", FormatScientific(planet->GetAge(), "yr") });
        page.groups.push_back(g_phys);

        data.push_back(page);
    }

    // =======================================================
    // 页 2: COMPOSITION (成分)
    // =======================================================
    {
        InfoPage page;
        page.name = "COMPOSITION";

        InfoGroup g_mass;
        g_mass.items.push_back({ "大气质量", planet->GetAtmosphereMass().str() + " kg" });
        g_mass.items.push_back({ "海洋质量", planet->GetOceanMass().str() + " kg" });
        g_mass.items.push_back({ "核心质量", planet->GetCoreMass().str() + " kg" });
        g_mass.items.push_back({ "地壳矿脉质量", planet->GetCrustMineralMass().str() + " kg" });
        page.groups.push_back(g_mass);

        InfoGroup g_mass_detail;
        g_mass_detail.items.push_back({ "大气-重元素", planet->GetAtmosphereMassZ().str() + " kg" });
        g_mass_detail.items.push_back({ "大气-挥发物", planet->GetAtmosphereMassVolatiles().str() + " kg" });
        g_mass_detail.items.push_back({ "大气-含能核素", planet->GetAtmosphereMassEnergeticNuclide().str() + " kg" });
        g_mass_detail.items.push_back({ "海洋-重元素", planet->GetOceanMassZ().str() + " kg" });
        // ...可以继续添加所有详细成分...
        page.groups.push_back(g_mass_detail);

        data.push_back(page);
    }

    // =======================================================
    // 页 3: ORBIT (轨道)
    // =======================================================
    {
        InfoPage page;
        page.name = "ORBIT";

        InfoGroup g_orbit;
        // 注意：Builder只接收IAstroObject指针，没有轨道上下文。
        // 如需显示轨道数据，需要修改BuildDataForObject的参数，
        // 比如传入一个包含天体和其轨道的结构体。
        g_orbit.items.push_back({ "轨道半长轴", "N/A" });
        g_orbit.items.push_back({ "轨道周期", "N/A" });
        g_orbit.items.push_back({ "轨道偏心率", "N/A" });
        page.groups.push_back(g_orbit);

        data.push_back(page);
    }

    // =======================================================
    // 页 4: CIVILIZATION (文明)
    // =======================================================
    {
        InfoPage page;
        page.name = "CIVILIZATION";

        InfoGroup g_life;
        g_life.items.push_back({ "生命演化阶段", "N/A" }); // 需从CivilizationData获取
        page.groups.push_back(g_life);

        InfoGroup g_civ;
        g_civ.items.push_back({ "文明演化阶段", "N/A" }); // 需从CivilizationData获取
        g_civ.items.push_back({ "卡尔达舍夫指数", "N/A" }); // 需从CivilizationData获取
        page.groups.push_back(g_civ);

        data.push_back(page);
    }

    return data;
}


// --- 辅助函数实现 ---
std::string AstroDataBuilder::FormatScientific(double value, const char* unit)
{
    char buffer[64];
    // 使用 %.2e 格式化为保留两位小数的科学计数法
    snprintf(buffer, sizeof(buffer), "%.2e %s", value, unit);
    return std::string(buffer);
}

std::string AstroDataBuilder::StarPhaseToString(Astro::AStar::EEvolutionPhase phase)
{
    using EPhase = Astro::AStar::EEvolutionPhase;
    switch (phase)
    {
    case EPhase::kPrevMainSequence: return          "前主序";
    case EPhase::kMainSequence: return              "氢主序";
    case EPhase::kRedGiant: return                  "红巨星";
    case EPhase::kCoreHeBurn: return                "氦主序";
    case EPhase::kEarlyAgb: return                  "早期渐近巨星";
    case EPhase::kThermalPulseAgb: return           "热脉冲渐近巨星";
    case EPhase::kPostAgb: return                   "后渐近巨星支";
    case EPhase::kWolfRayet: return                 "沃尔夫-拉叶星";
    case EPhase::kHeliumWhiteDwarf: return          "氦白矮星";
    case EPhase::kCarbonOxygenWhiteDwarf: return    "碳氧白矮星";
    case EPhase::kOxygenNeonMagnWhiteDwarf: return  "氧氖镁白矮星";
    case EPhase::kNeutronStar: return               "中子星";
    case EPhase::kStellarBlackHole: return          "恒星质量黑洞";
    case EPhase::kMiddleBlackHole: return           "中等质量黑洞";
    case EPhase::kSuperMassiveBlackHole: return     "超大质量黑洞";
    case EPhase::kNull: return                      "未知";
    default: return "未知";
    }
}
std::string AstroDataBuilder::StarFromToString(Astro::AStar::EStarFrom from)
{
    using EFrom = Astro::AStar::EStarFrom;
    switch (from)
    {
    case EFrom::kNormalFrom: return                "普通坍缩";
    case EFrom::kWhiteDwarfMerge: return           "白矮星合并";
    case EFrom::kSlowColdingDown: return           "缓慢冷却"; // 形成黑矮星的过程
    case EFrom::kEnvelopeDisperse: return          "包层吹散"; // 形成白矮星的过程
    case EFrom::kElectronCaptureSupernova: return  "电子俘获型超新星";
    case EFrom::kIronCoreCollapseSupernova: return "铁核坍缩型超新星";
    case EFrom::kRelativisticJetHypernova: return  "相对论性喷流超新星";
    case EFrom::kPairInstabilitySupernova: return  "不稳定对超新星";
    case EFrom::kPhotondisintegration: return      "光致解离";
    default: return "未知形成方式";
    }
}

// [新增] 预测恒星结局
std::string AstroDataBuilder::PredictOutcomeToString(const Astro::AStar* star)
{
    using EPhase = Astro::AStar::EEvolutionPhase;
    const auto phase = star->GetEvolutionPhase();

    // 如果恒星已经是残骸，它的结局就是“缓慢熄灭”或“蒸发”
    if (phase >= EPhase::kHeliumWhiteDwarf && phase <= EPhase::kOxygenNeonMagnWhiteDwarf)
    {
        return "缓慢熄灭";
    }
    if (phase == EPhase::kNeutronStar)
    {
        return "缓慢冷却";
    }
    if (phase >= EPhase::kStellarBlackHole)
    {
        return "霍金辐射蒸发";
    }

    // 否则，根据初始质量预测
    // 注意：这里的质量边界是简化的近似值
    const double initialMassKg = star->GetInitialMass();
    const double solarMass = 1.98847e30; // 太阳质量
    const double m_sun = initialMassKg / solarMass;

    if (m_sun < 0.5) return "缓慢熄灭";
    if (m_sun < 8.0) return "吹散包层";
    if (m_sun < 10.0) return "电子俘获型超新星";
    if (m_sun < 40.0) return "铁核坍缩型超新星";
    if (m_sun < 130.0) return "相对论性喷流超新星"; // 假设
    if (m_sun < 250.0) return "不稳定对超新星";

    return "光致解离";
}

// [新增] 预测恒星残骸
std::string AstroDataBuilder::PredictRemnantToString(const Astro::AStar* star)
{
    using EPhase = Astro::AStar::EEvolutionPhase;
    const auto phase = star->GetEvolutionPhase();

    // 如果已经是残骸，直接返回其类型
    switch (phase)
    {
    case EPhase::kHeliumWhiteDwarf:
    case EPhase::kCarbonOxygenWhiteDwarf:
    case EPhase::kOxygenNeonMagnWhiteDwarf:
        return "白矮星";
    case EPhase::kNeutronStar:
        return "中子星";
    case EPhase::kStellarBlackHole:
    case EPhase::kMiddleBlackHole:
    case EPhase::kSuperMassiveBlackHole:
        return "黑洞";
    }

    // 否则，根据初始质量预测
    const double initialMassKg = star->GetInitialMass();
    const double solarMass = 1.98847e30;
    const double m_sun = initialMassKg / solarMass;

    if (m_sun < 8.0) return "白矮星";
    if (m_sun < 25.0) return "中子星";

    return "黑洞";
}

std::string AstroDataBuilder::PlanetTypeToString(Astro::APlanet::EPlanetType type)
{
    using EType = Astro::APlanet::EPlanetType;
    switch (type)
    {
    case EType::kRocky: return "岩质行星";
    case EType::kTerra: return "类地行星";
    case EType::kIcePlanet: return "冰壳行星";
    case EType::kChthonian: return "冥府行星";
    case EType::kOceanic: return "海洋行星";
    case EType::kSubIceGiant: return "亚冰巨星";
    case EType::kIceGiant: return "冰巨星";
    case EType::kGasGiant: return "气态巨行星";
    case EType::kHotSubIceGiant: return "热亚冰巨星";
    case EType::kHotIceGiant: return "热冰巨星";
    case EType::kHotGasGiant: return "热木星";
    case EType::kRockyAsteroidCluster: return "岩质小天体集群";
    case EType::kRockyIceAsteroidCluster: return "岩冰质小天体集群";
    case EType::kArtificalOrbitalStructureCluster: return "轨道非天然结构集群";
    default: return "未知类型";
    }
}


_UI_END
_SYSTEM_END
_NPGS_END