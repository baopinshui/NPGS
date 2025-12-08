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
        page.name = TR("astro.page.physical");

        InfoGroup g_basic;
        g_basic.items.push_back({ TR("astro.type"), star->GetStellarClass().ToString() });
        g_basic.items.push_back({ TR("astro.phase"), StarPhaseToString(star->GetEvolutionPhase()), true });
        g_basic.items.push_back({ TR("astro.progress"), std::to_string(star->GetEvolutionProgress() * 100.0) + "%" });
        g_basic.items.push_back({ TR("astro.age"), FormatScientific(star->GetAge(), "yr") });
        g_basic.items.push_back({ TR("astro.lifespan"), FormatScientific(star->GetLifetime(), "yr") });
        page.groups.push_back(g_basic);

        InfoGroup g_mass;
        g_mass.items.push_back({ TR("astro.mass"), FormatScientific(star->GetMass(), "kg") });
        g_mass.items.push_back({ TR("astro.initial_mass"), FormatScientific(star->GetInitialMass(), "kg") });
        page.groups.push_back(g_mass);

        InfoGroup g_energy;
        g_energy.items.push_back({ TR("astro.luminosity"), FormatScientific(star->GetLuminosity(), "W") });
        g_energy.items.push_back({ TR("astro.temp_surface"), FormatScientific(star->GetTeff(), "K"), true });
        g_energy.items.push_back({ TR("astro.temp_core"), FormatScientific(star->GetCoreTemp(), "K") });
        g_energy.items.push_back({ TR("astro.density_core"), FormatScientific(star->GetCoreDensity(), "kg/m^3") });
        page.groups.push_back(g_energy);

        InfoGroup g_phys;
        g_phys.items.push_back({ TR("astro.radius"), FormatScientific(star->GetRadius(), "m") });
        g_phys.items.push_back({ TR("astro.oblateness"), std::to_string(star->GetOblateness()) });
        g_phys.items.push_back({ TR("astro.spin"), FormatScientific(star->GetSpin(), "s") });
        g_phys.items.push_back({ TR("astro.escape_vel"), FormatScientific(star->GetEscapeVelocity(), "m/s") });
        g_phys.items.push_back({ TR("astro.mag_field"), FormatScientific(star->GetMagneticField(), "T") });
        page.groups.push_back(g_phys);

        data.push_back(page);
    }

    // =======================================================
    // 页 2: COMPOSITION (成分)
    // =======================================================
    {
        InfoPage page;
        page.name = TR("astro.page.composition");

        InfoGroup g_surface;
        g_surface.items.push_back({ TR("astro.metallicity"), std::to_string(star->GetFeH()) });
        g_surface.items.push_back({ TR("astro.surf_h1"), std::to_string(star->GetSurfaceH1()) });
        g_surface.items.push_back({ TR("astro.surf_z"), std::to_string(star->GetSurfaceZ()) });
        g_surface.items.push_back({ TR("astro.surf_volatiles"), std::to_string(star->GetSurfaceVolatiles()) });
        g_surface.items.push_back({ TR("astro.surf_energetic"), std::to_string(star->GetSurfaceEnergeticNuclide()) });
        page.groups.push_back(g_surface);

        InfoGroup g_wind;
        g_wind.items.push_back({ TR("astro.wind_speed"), FormatScientific(star->GetStellarWindSpeed(), "m/s") });
        g_wind.items.push_back({ TR("astro.wind_loss_rate"), FormatScientific(star->GetStellarWindMassLossRate(), "kg/s") });
        page.groups.push_back(g_wind);

        data.push_back(page);
    }

    // =======================================================
    // 页 3: EVOLUTION (演化)
    // =======================================================
    {
        InfoPage page;
        page.name = TR("astro.page.evolution");

        InfoGroup g_evo;
        g_evo.items.push_back({ TR("astro.formation"), StarFromToString(star->GetStarFrom()) });
        g_evo.items.push_back({ TR("astro.predicted_outcome"), PredictOutcomeToString(star) });
        g_evo.items.push_back({ TR("astro.predicted_remnant"), PredictRemnantToString(star) });
        page.groups.push_back(g_evo);

        InfoGroup g_misc;
        g_misc.items.push_back({ TR("astro.is_single"), star->GetIsSingleStar() ? TR("bool.yes") : TR("bool.no") });
        g_misc.items.push_back({ TR("astro.has_planets"), star->GetHasPlanets() ? TR("bool.yes") : TR("bool.no") });
        g_misc.items.push_back({ TR("astro.min_coil_mass"), FormatScientific(star->GetMinCoilMass(), "kg") });
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
        page.name = TR("astro.page.physical");

        InfoGroup g_type;
        g_type.items.push_back({ TR("astro.type"), PlanetTypeToString(planet->GetPlanetType()) });
        g_type.items.push_back({ TR("astro.is_migrated"), planet->GetMigration() ? TR("bool.yes") : TR("bool.no") });
        page.groups.push_back(g_type);

        InfoGroup g_phys;
        g_phys.items.push_back({ TR("astro.mass_total"), planet->GetMass().str() + " kg" });
        g_phys.items.push_back({ TR("astro.radius"), FormatScientific(planet->GetRadius(), "m") });
        g_phys.items.push_back({ TR("astro.temp_balance"), FormatScientific(planet->GetBalanceTemperature(), "K") });
        g_phys.items.push_back({ TR("astro.spin"), FormatScientific(planet->GetSpin(), "s") });
        g_phys.items.push_back({ TR("astro.escape_vel"), FormatScientific(planet->GetEscapeVelocity(), "m/s") });
        g_phys.items.push_back({ TR("astro.age"), FormatScientific(planet->GetAge(), "yr") });
        page.groups.push_back(g_phys);

        data.push_back(page);
    }

    // =======================================================
    // 页 2: COMPOSITION (成分)
    // =======================================================
    {
        InfoPage page;
        page.name = TR("astro.page.composition");

        InfoGroup g_mass;
        g_mass.items.push_back({ TR("astro.mass_atmosphere"), planet->GetAtmosphereMass().str() + " kg" });
        g_mass.items.push_back({ TR("astro.mass_ocean"), planet->GetOceanMass().str() + " kg" });
        g_mass.items.push_back({ TR("astro.mass_core"), planet->GetCoreMass().str() + " kg" });
        g_mass.items.push_back({ TR("astro.mass_crust_mineral"), planet->GetCrustMineralMass().str() + " kg" });
        page.groups.push_back(g_mass);

        InfoGroup g_mass_detail;
        g_mass_detail.items.push_back({ TR("astro.mass_atmo_z"), planet->GetAtmosphereMassZ().str() + " kg" });
        g_mass_detail.items.push_back({ TR("astro.mass_atmo_volatiles"), planet->GetAtmosphereMassVolatiles().str() + " kg" });
        g_mass_detail.items.push_back({ TR("astro.mass_atmo_energetic"), planet->GetAtmosphereMassEnergeticNuclide().str() + " kg" });
        g_mass_detail.items.push_back({ TR("astro.mass_ocean_z"), planet->GetOceanMassZ().str() + " kg" });
        // ...可以继续添加所有详细成分...
        page.groups.push_back(g_mass_detail);

        data.push_back(page);
    }

    // =======================================================
    // 页 3: ORBIT (轨道)
    // =======================================================
    {
        InfoPage page;
        page.name = TR("astro.page.orbit");

        InfoGroup g_orbit;
        // 注意：Builder只接收IAstroObject指针，没有轨道上下文。
        // 如需显示轨道数据，需要修改BuildDataForObject的参数，
        // 比如传入一个包含天体和其轨道的结构体。
        g_orbit.items.push_back({ TR("astro.orbit_sma"), TR("data.not_available") });
        g_orbit.items.push_back({ TR("astro.orbit_period"), TR("data.not_available") });
        g_orbit.items.push_back({ TR("astro.orbit_eccentricity"), TR("data.not_available") });
        page.groups.push_back(g_orbit);

        data.push_back(page);
    }

    // =======================================================
    // 页 4: CIVILIZATION (文明)
    // =======================================================
    {
        InfoPage page;
        page.name = TR("astro.page.civilization");

        InfoGroup g_life;
        g_life.items.push_back({ TR("astro.life_phase"), TR("data.not_available") }); // 需从CivilizationData获取
        page.groups.push_back(g_life);

        InfoGroup g_civ;
        g_civ.items.push_back({ TR("astro.civ_phase"), TR("data.not_available") }); // 需从CivilizationData获取
        g_civ.items.push_back({ TR("astro.kardashev_index"), TR("data.not_available") }); // 需从CivilizationData获取
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
    case EPhase::kPrevMainSequence: return          TR("enum.star.prev_ms");
    case EPhase::kMainSequence: return              TR("enum.star.main_seq");
    case EPhase::kRedGiant: return                  TR("enum.star.red_giant");
    case EPhase::kCoreHeBurn: return                TR("enum.star.core_he_burn");
    case EPhase::kEarlyAgb: return                  TR("enum.star.early_agb");
    case EPhase::kThermalPulseAgb: return           TR("enum.star.tp_agb");
    case EPhase::kPostAgb: return                   TR("enum.star.post_agb");
    case EPhase::kWolfRayet: return                 TR("enum.star.wolf_rayet");
    case EPhase::kHeliumWhiteDwarf: return          TR("enum.star.he_wd");
    case EPhase::kCarbonOxygenWhiteDwarf: return    TR("enum.star.co_wd");
    case EPhase::kOxygenNeonMagnWhiteDwarf: return  TR("enum.star.onemg_wd");
    case EPhase::kNeutronStar: return               TR("enum.star.neutron_star");
    case EPhase::kStellarBlackHole: return          TR("enum.star.stellar_bh");
    case EPhase::kMiddleBlackHole: return           TR("enum.star.middle_bh");
    case EPhase::kSuperMassiveBlackHole: return     TR("enum.star.supermassive_bh");
    case EPhase::kNull: return                      TR("enum.unknown");
    default: return TR("enum.unknown");
    }
}
std::string AstroDataBuilder::StarFromToString(Astro::AStar::EStarFrom from)
{
    using EFrom = Astro::AStar::EStarFrom;
    switch (from)
    {
    case EFrom::kNormalFrom: return                TR("enum.star_from.normal");
    case EFrom::kWhiteDwarfMerge: return           TR("enum.star_from.wd_merge");
    case EFrom::kSlowColdingDown: return           TR("enum.star_from.slow_cooling");
    case EFrom::kEnvelopeDisperse: return          TR("enum.star_from.envelope_disperse");
    case EFrom::kElectronCaptureSupernova: return  TR("enum.star_from.ec_supernova");
    case EFrom::kIronCoreCollapseSupernova: return TR("enum.star_from.fe_core_supernova");
    case EFrom::kRelativisticJetHypernova: return  TR("enum.star_from.jet_hypernova");
    case EFrom::kPairInstabilitySupernova: return  TR("enum.star_from.pair_inst_supernova");
    case EFrom::kPhotondisintegration: return      TR("enum.star_from.photodisintegration");
    default: return TR("enum.unknown_formation");
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
        return TR("enum.outcome.slow_fade");
    }
    if (phase == EPhase::kNeutronStar)
    {
        return TR("enum.outcome.slow_cool");
    }
    if (phase >= EPhase::kStellarBlackHole)
    {
        return TR("enum.outcome.hawking_radiation");
    }

    // 否则，根据初始质量预测
    const double initialMassKg = star->GetInitialMass();
    const double solarMass = 1.98847e30; // 太阳质量
    const double m_sun = initialMassKg / solarMass;

    if (m_sun < 0.5) return TR("enum.outcome.slow_fade");
    if (m_sun < 8.0) return TR("enum.outcome.envelope_disperse");
    if (m_sun < 10.0) return TR("enum.outcome.ec_supernova");
    if (m_sun < 40.0) return TR("enum.outcome.fe_core_supernova");
    if (m_sun < 130.0) return TR("enum.outcome.jet_hypernova");
    if (m_sun < 250.0) return TR("enum.outcome.pair_inst_supernova");

    return TR("enum.outcome.photodisintegration");
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
        return TR("enum.remnant.white_dwarf");
    case EPhase::kNeutronStar:
        return TR("enum.remnant.neutron_star");
    case EPhase::kStellarBlackHole:
    case EPhase::kMiddleBlackHole:
    case EPhase::kSuperMassiveBlackHole:
        return TR("enum.remnant.black_hole");
    }

    // 否则，根据初始质量预测
    const double initialMassKg = star->GetInitialMass();
    const double solarMass = 1.98847e30;
    const double m_sun = initialMassKg / solarMass;

    if (m_sun < 8.0) return TR("enum.remnant.white_dwarf");
    if (m_sun < 25.0) return TR("enum.remnant.neutron_star");

    return TR("enum.remnant.black_hole");
}

std::string AstroDataBuilder::PlanetTypeToString(Astro::APlanet::EPlanetType type)
{
    using EType = Astro::APlanet::EPlanetType;
    switch (type)
    {
    case EType::kRocky: return TR("enum.planet.rocky");
    case EType::kTerra: return TR("enum.planet.terra");
    case EType::kIcePlanet: return TR("enum.planet.ice");
    case EType::kChthonian: return TR("enum.planet.chthonian");
    case EType::kOceanic: return TR("enum.planet.oceanic");
    case EType::kSubIceGiant: return TR("enum.planet.sub_ice_giant");
    case EType::kIceGiant: return TR("enum.planet.ice_giant");
    case EType::kGasGiant: return TR("enum.planet.gas_giant");
    case EType::kHotSubIceGiant: return TR("enum.planet.hot_sub_ice_giant");
    case EType::kHotIceGiant: return TR("enum.planet.hot_ice_giant");
    case EType::kHotGasGiant: return TR("enum.planet.hot_gas_giant");
    case EType::kRockyAsteroidCluster: return TR("enum.planet.rocky_asteroid");
    case EType::kRockyIceAsteroidCluster: return TR("enum.planet.rocky_ice_asteroid");
    case EType::kArtificalOrbitalStructureCluster: return TR("enum.planet.artificial_cluster");
    default: return TR("enum.unknown");
    }
}


_UI_END
_SYSTEM_END
_NPGS_END