#include "I18nManager.h"

_NPGS_BEGIN
_SYSTEM_BEGIN

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

    // 通知所有监听者（主要是App主逻辑，用于刷新动态数据）
    for (auto const& [key, val] : m_callbacks)
    {
        if (val) val();
    }
}

std::string I18nManager::Get(const std::string& key) const
{
    auto it = m_dictionary.find(key);
    if (it != m_dictionary.end())
    {
        return it->second;
    }
    // 找不到时返回Key本身，便于调试发现缺失的翻译
    if (key == "") return "";
    return "!" + key + "!";
}

std::string I18nManager::Get(const char* key) const
{
    // unordered_map::find() 有 const char* 的重载，不会创建临时 std::string
    auto it = m_dictionary.find(key);
    if (it != m_dictionary.end())
    {
        return it->second;
    }
    if (key == "") return "";
    return "!" + std::string(key) + "!";
}

void I18nManager::RegisterCallback(void* observer, Callback cb)
{
    m_callbacks[observer] = cb;
}

void I18nManager::UnregisterCallback(void* observer)
{
    m_callbacks.erase(observer);
}

void I18nManager::LoadDictionary()
{
    m_dictionary.clear();
    // In a real project, this should be loaded from a JSON/XML/CSV file.
    if (m_current_lang == Language::English)
    {
        // --- Static UI (General) ---
        m_dictionary["ui.manage"] = "MANAGE";
        m_dictionary["ui.network"] = "NETWORK";
        m_dictionary["ui.settings"] = "> SETTINGS";
        m_dictionary["ui.close_terminal"] = "CLOSE TERMINAL";
        m_dictionary["ui.info"] = "INFO";
        m_dictionary["ui.close_panel"] = "CLOSE PANEL";
        m_dictionary["ui.time.pause"] = " || ";
        m_dictionary["ui.time.resume"] = "▶";
        m_dictionary["ui.time.reset_speed"] = " x1 ";
        m_dictionary["ui.log.system_scan"] = "> System Scan Complete.";
        m_dictionary["ui.log.autosave"] = "> Auto-Save: T+";

        // --- Pulsar Buttons ---
        m_dictionary["ui.status.ready_to_launch"] = "READY TO LAUNCH";
        m_dictionary["ui.status.ready_to_send"] = "READY TO SEND";
        m_dictionary["ui.status.target_locked"] = "TARGET LOCKED123456";
        m_dictionary["ui.status.no_target"] = "NO TARGET";
        m_dictionary["ui.action.fire_beam"] = "FIRE DYSON BEAM";
        m_dictionary["ui.action.launch_rkkv"] = "LAUNCH RKKV";
        m_dictionary["ui.action.launch_vn"] = "SEND VON NEUMANN PROBE";
        m_dictionary["ui.action.send_message"] = "SEND MESSAGE";
        m_dictionary["ui.label.energy"] = "ENERGY";
        m_dictionary["ui.label.mass"] = "MASS";
        m_dictionary["ui.label.weight_time"] = "TIME COST";
        m_dictionary["ui.unit.joules"] = "J";
        m_dictionary["ui.unit.kg"] = "kg";
        m_dictionary["ui.unit.years"] = "yr";

        // --- Log Panel (Dynamic Events) ---
        m_dictionary["log.event.scan_result_title"] = ">> SCAN_RESULT";
        m_dictionary["log.event.scan_result_desc"] = "Test data has been loaded.";
        m_dictionary["log.event.critical_error_title"] = ">> [CRITICAL]";
        m_dictionary["log.event.critical_error_desc"] = "System integrity compromised!";
        m_dictionary["log.event.beam_fired_title"] = ">> DYSON BEAM";
        m_dictionary["log.event.beam_fired_desc"] = "Fired with {} Joules.";

        // --- Cinematic Info Panel ---
        m_dictionary["cinematic.title.singularity"] = "TRANSCENDENT RINGULARITY";
        m_dictionary["cinematic.stat.mass_dominated"] = "Dominated Mass: {}";
        m_dictionary["cinematic.stat.star_systems"] = "Star Systems: {}";
        m_dictionary["cinematic.stat.reward"] = "REWARD: {}";
        m_dictionary["cinematic.type.black_hole"] = "BH";
        m_dictionary["cinematic.stat.mass"] = "Mass: {}";
        m_dictionary["cinematic.stat.luminosity"] = "Luminosity: {}";
        m_dictionary["cinematic.title.type2_civ"] = "TYPE-II CIVILIZATION";
        m_dictionary["cinematic.stat.output"] = "Output: {}";
        m_dictionary["cinematic.stat.progress"] = "Construction: {}";
        m_dictionary["cinematic.stat.status_warning"] = "Status: WARNING";
        m_dictionary["cinematic.type.red_giant"] = "Red Giant";

        // --- Generic / Data Status ---
        m_dictionary["bool.yes"] = "Yes";
        m_dictionary["bool.no"] = "No";
        m_dictionary["data.not_available"] = "N/A";
        m_dictionary["enum.unknown"] = "Unknown";
        m_dictionary["enum.unknown_formation"] = "Unknown Formation";

        // --- AstroDataBuilder (Page Titles) ---
        m_dictionary["astro.page.physical"] = "PHYSICAL";
        m_dictionary["astro.page.composition"] = "COMPOSITION";
        m_dictionary["astro.page.evolution"] = "EVOLUTION";
        m_dictionary["astro.page.orbit"] = "ORBIT";
        m_dictionary["astro.page.civilization"] = "CIVILIZATION";

        // --- AstroDataBuilder (Data Labels) ---
        m_dictionary["astro.type"] = "Type";
        m_dictionary["astro.phase"] = "Evolution Phase";
        m_dictionary["astro.progress"] = "Evolution Progress";
        m_dictionary["astro.age"] = "Age";
        m_dictionary["astro.lifespan"] = "Expected Lifespan";
        m_dictionary["astro.mass"] = "Mass";
        m_dictionary["astro.initial_mass"] = "Initial Mass";
        m_dictionary["astro.luminosity"] = "Luminosity";
        m_dictionary["astro.temp_surface"] = "Surface Temp.";
        m_dictionary["astro.temp_core"] = "Core Temp.";
        m_dictionary["astro.density_core"] = "Core Density";
        m_dictionary["astro.radius"] = "Radius";
        m_dictionary["astro.oblateness"] = "Oblateness";
        m_dictionary["astro.spin"] = "Rotation Period";
        m_dictionary["astro.escape_vel"] = "Escape Velocity";
        m_dictionary["astro.mag_field"] = "Magnetic Field";
        m_dictionary["astro.metallicity"] = "Metallicity [Fe/H]";
        m_dictionary["astro.surf_h1"] = "Surface H1 Mass Fraction";
        m_dictionary["astro.surf_z"] = "Surface Z Mass Fraction";
        m_dictionary["astro.surf_volatiles"] = "Surface Volatiles Mass Fraction";
        m_dictionary["astro.surf_energetic"] = "Surface Energetic Nuclide Mass Fraction";
        m_dictionary["astro.wind_speed"] = "Stellar Wind Speed";
        m_dictionary["astro.wind_loss_rate"] = "Stellar Wind Mass Loss Rate";
        m_dictionary["astro.formation"] = "Formation";
        m_dictionary["astro.predicted_outcome"] = "Predicted Outcome";
        m_dictionary["astro.predicted_remnant"] = "Predicted Remnant";
        m_dictionary["astro.is_single"] = "Is Single Star";
        m_dictionary["astro.has_planets"] = "Has Planets";
        m_dictionary["astro.min_coil_mass"] = "Min. Coil Mass for Lifter";
        m_dictionary["astro.is_migrated"] = "Is Migrated Planet";
        m_dictionary["astro.mass_total"] = "Total Mass";
        m_dictionary["astro.temp_balance"] = "Balance Temperature";
        m_dictionary["astro.mass_atmosphere"] = "Atmosphere Mass";
        m_dictionary["astro.mass_ocean"] = "Ocean Mass";
        m_dictionary["astro.mass_core"] = "Core Mass";
        m_dictionary["astro.mass_crust_mineral"] = "Crust Mineral Mass";
        m_dictionary["astro.mass_atmo_z"] = "Atmosphere Heavy Elements";
        m_dictionary["astro.mass_atmo_volatiles"] = "Atmosphere Volatiles";
        m_dictionary["astro.mass_atmo_energetic"] = "Atmosphere Energetic Nuclides";
        m_dictionary["astro.mass_ocean_z"] = "Ocean Heavy Elements";
        m_dictionary["astro.orbit_sma"] = "Semi-Major Axis";
        m_dictionary["astro.orbit_period"] = "Orbital Period";
        m_dictionary["astro.orbit_eccentricity"] = "Eccentricity";
        m_dictionary["astro.life_phase"] = "Life Evolution Stage";
        m_dictionary["astro.civ_phase"] = "Civilization Stage";
        m_dictionary["astro.kardashev_index"] = "Kardashev Index";

        // --- AstroDataBuilder (Enums) ---
        // Star Phases
        m_dictionary["enum.star.prev_ms"] = "Pre-Main Sequence";
        m_dictionary["enum.star.main_seq"] = "Main Sequence";
        m_dictionary["enum.star.red_giant"] = "Red Giant";
        m_dictionary["enum.star.core_he_burn"] = "Core He Burn";
        m_dictionary["enum.star.early_agb"] = "Early AGB";
        m_dictionary["enum.star.tp_agb"] = "Thermal Pulse AGB";
        m_dictionary["enum.star.post_agb"] = "Post-AGB";
        m_dictionary["enum.star.wolf_rayet"] = "Wolf-Rayet";
        m_dictionary["enum.star.he_wd"] = "Helium White Dwarf";
        m_dictionary["enum.star.co_wd"] = "Carbon-Oxygen White Dwarf";
        m_dictionary["enum.star.onemg_wd"] = "O-Ne-Mg White Dwarf";
        m_dictionary["enum.star.neutron_star"] = "Neutron Star";
        m_dictionary["enum.star.stellar_bh"] = "Stellar Black Hole";
        m_dictionary["enum.star.middle_bh"] = "Intermediate-Mass Black Hole";
        m_dictionary["enum.star.supermassive_bh"] = "Supermassive Black Hole";
        // Star Formation
        m_dictionary["enum.star_from.normal"] = "Normal Collapse";
        m_dictionary["enum.star_from.wd_merge"] = "White Dwarf Merger";
        m_dictionary["enum.star_from.slow_cooling"] = "Slow Cooling";
        m_dictionary["enum.star_from.envelope_disperse"] = "Envelope Dispersal";
        m_dictionary["enum.star_from.ec_supernova"] = "Electron-Capture Supernova";
        m_dictionary["enum.star_from.fe_core_supernova"] = "Iron Core-Collapse Supernova";
        m_dictionary["enum.star_from.jet_hypernova"] = "Relativistic Jet Hypernova";
        m_dictionary["enum.star_from.pair_inst_supernova"] = "Pair-Instability Supernova";
        m_dictionary["enum.star_from.photodisintegration"] = "Photodisintegration";
        // Star Outcome
        m_dictionary["enum.outcome.slow_fade"] = "Slowly Fade";
        m_dictionary["enum.outcome.slow_cool"] = "Slowly Cool";
        m_dictionary["enum.outcome.hawking_radiation"] = "Hawking Radiation";
        m_dictionary["enum.outcome.envelope_disperse"] = "Disperse Envelope";
        m_dictionary["enum.outcome.ec_supernova"] = "Electron-Capture Supernova";
        m_dictionary["enum.outcome.fe_core_supernova"] = "Iron Core-Collapse Supernova";
        m_dictionary["enum.outcome.jet_hypernova"] = "Relativistic Jet Hypernova";
        m_dictionary["enum.outcome.pair_inst_supernova"] = "Pair-Instability Supernova";
        m_dictionary["enum.outcome.photodisintegration"] = "Photodisintegration";
        // Star Remnant
        m_dictionary["enum.remnant.white_dwarf"] = "White Dwarf";
        m_dictionary["enum.remnant.neutron_star"] = "Neutron Star";
        m_dictionary["enum.remnant.black_hole"] = "Black Hole";
        // Planet Types
        m_dictionary["enum.planet.rocky"] = "Rocky Planet";
        m_dictionary["enum.planet.terra"] = "Terrestrial Planet";
        m_dictionary["enum.planet.ice"] = "Ice Planet";
        m_dictionary["enum.planet.chthonian"] = "Chthonian Planet";
        m_dictionary["enum.planet.oceanic"] = "Ocean Planet";
        m_dictionary["enum.planet.sub_ice_giant"] = "Sub-Ice Giant";
        m_dictionary["enum.planet.ice_giant"] = "Ice Giant";
        m_dictionary["enum.planet.gas_giant"] = "Gas Giant";
        m_dictionary["enum.planet.hot_sub_ice_giant"] = "Hot Sub-Ice Giant";
        m_dictionary["enum.planet.hot_ice_giant"] = "Hot Ice Giant";
        m_dictionary["enum.planet.hot_gas_giant"] = "Hot Jupiter";
        m_dictionary["enum.planet.rocky_asteroid"] = "Rocky Asteroid Cluster";
        m_dictionary["enum.planet.rocky_ice_asteroid"] = "Rocky-Ice Asteroid Cluster";
        m_dictionary["enum.planet.artificial_cluster"] = "Artificial Orbital Structure Cluster";
    }
    else if (m_current_lang == Language::Chinese)
    {
        // --- Static UI (General) ---
        m_dictionary["ui.manage"] = "管理";
        m_dictionary["ui.network"] = "网络";
        m_dictionary["ui.settings"] = "> 设置";
        m_dictionary["ui.close_terminal"] = "关闭终端";
        m_dictionary["ui.info"] = "信息";
        m_dictionary["ui.close_panel"] = "关闭面板";
        m_dictionary["ui.time.pause"] = " || ";
        m_dictionary["ui.time.resume"] = "▶";
        m_dictionary["ui.time.reset_speed"] = " x1 ";
        m_dictionary["ui.log.system_scan"] = "> 系统扫描完成。";
        m_dictionary["ui.log.autosave"] = "> 自动存档: T+";

        // --- Pulsar Buttons ---
        m_dictionary["ui.status.ready_to_launch"] = "准备发射";
        m_dictionary["ui.status.ready_to_send"] = "准备发送";
        m_dictionary["ui.status.target_locked"] = "目标已锁定";
        m_dictionary["ui.status.no_target"] = "无目标";
        m_dictionary["ui.action.fire_beam"] = "发射戴森光束";
        m_dictionary["ui.action.launch_rkkv"] = "发射RKKV";
        m_dictionary["ui.action.launch_vn"] = "发送冯诺依曼探测器";
        m_dictionary["ui.action.send_message"] = "发送信息";
        m_dictionary["ui.label.energy"] = "能量";
        m_dictionary["ui.label.mass"] = "质量";
        m_dictionary["ui.label.weight_time"] = "用时";
        m_dictionary["ui.unit.joules"] = "焦耳";
        m_dictionary["ui.unit.kg"] = "千克";
        m_dictionary["ui.unit.years"] = "年";

        // --- Log Panel (Dynamic Events) ---
        m_dictionary["log.event.scan_result_title"] = ">> 扫描结果";
        m_dictionary["log.event.scan_result_desc"] = "测试数据已加载。";
        m_dictionary["log.event.critical_error_title"] = ">> [严重错误]";
        m_dictionary["log.event.critical_error_desc"] = "系统完整性受损！";
        m_dictionary["log.event.beam_fired_title"] = ">> 戴森光束";
        m_dictionary["log.event.beam_fired_desc"] = "发射能量：{} 焦耳。";

        // --- Cinematic Info Panel ---
        m_dictionary["cinematic.title.singularity"] = "你家超绝环状奇点";
        m_dictionary["cinematic.stat.mass_dominated"] = "已支配质量: {}";
        m_dictionary["cinematic.stat.star_systems"] = "恒星系统: {}";
        m_dictionary["cinematic.stat.reward"] = "奖励: {}";
        m_dictionary["cinematic.type.black_hole"] = "黑洞";
        m_dictionary["cinematic.stat.mass"] = "质量: {}";
        m_dictionary["cinematic.stat.luminosity"] = "光度: {}";
        m_dictionary["cinematic.title.type2_civ"] = "II型文明";
        m_dictionary["cinematic.stat.output"] = "输出功率: {}";
        m_dictionary["cinematic.stat.progress"] = "建造进度: {}";
        m_dictionary["cinematic.stat.status_warning"] = "状态: 警告";
        m_dictionary["cinematic.type.red_giant"] = "红巨星";

        // --- Generic / Data Status ---
        m_dictionary["bool.yes"] = "是";
        m_dictionary["bool.no"] = "否";
        m_dictionary["data.not_available"] = "不可用";
        m_dictionary["enum.unknown"] = "未知";
        m_dictionary["enum.unknown_formation"] = "未知形成方式";

        // --- AstroDataBuilder (Page Titles) ---
        m_dictionary["astro.page.physical"] = "物理参数";
        m_dictionary["astro.page.composition"] = "构成成分";
        m_dictionary["astro.page.evolution"] = "演化";
        m_dictionary["astro.page.orbit"] = "轨道";
        m_dictionary["astro.page.civilization"] = "文明";

        // --- AstroDataBuilder (Data Labels) ---
        m_dictionary["astro.type"] = "类型";
        m_dictionary["astro.phase"] = "演化阶段";
        m_dictionary["astro.progress"] = "演化进度";
        m_dictionary["astro.age"] = "年龄";
        m_dictionary["astro.lifespan"] = "预期寿命";
        m_dictionary["astro.mass"] = "质量";
        m_dictionary["astro.initial_mass"] = "初始质量";
        m_dictionary["astro.luminosity"] = "光度";
        m_dictionary["astro.temp_surface"] = "表面温度";
        m_dictionary["astro.temp_core"] = "核心温度";
        m_dictionary["astro.density_core"] = "核心密度";
        m_dictionary["astro.radius"] = "半径";
        m_dictionary["astro.oblateness"] = "扁率";
        m_dictionary["astro.spin"] = "自转周期";
        m_dictionary["astro.escape_vel"] = "逃逸速度";
        m_dictionary["astro.mag_field"] = "磁场强度";
        m_dictionary["astro.metallicity"] = "金属丰度 [Fe/H]";
        m_dictionary["astro.surf_h1"] = "表面氢(H1)质量分数";
        m_dictionary["astro.surf_z"] = "表面金属(Z)质量分数";
        m_dictionary["astro.surf_volatiles"] = "表面挥发物质量分数";
        m_dictionary["astro.surf_energetic"] = "表面含能核素质量分数";
        m_dictionary["astro.wind_speed"] = "星风速度";
        m_dictionary["astro.wind_loss_rate"] = "星风质量流失率";
        m_dictionary["astro.formation"] = "形成方式";
        m_dictionary["astro.predicted_outcome"] = "预期结局";
        m_dictionary["astro.predicted_remnant"] = "预期残骸";
        m_dictionary["astro.is_single"] = "是否为孤星";
        m_dictionary["astro.has_planets"] = "是否存在行星系";
        m_dictionary["astro.min_coil_mass"] = "举星器最低线圈质量";
        m_dictionary["astro.is_migrated"] = "是否迁移行星";
        m_dictionary["astro.mass_total"] = "总质量";
        m_dictionary["astro.temp_balance"] = "平衡温度";
        m_dictionary["astro.mass_atmosphere"] = "大气质量";
        m_dictionary["astro.mass_ocean"] = "海洋质量";
        m_dictionary["astro.mass_core"] = "核心质量";
        m_dictionary["astro.mass_crust_mineral"] = "地壳矿脉质量";
        m_dictionary["astro.mass_atmo_z"] = "大气重元素";
        m_dictionary["astro.mass_atmo_volatiles"] = "大气挥发物";
        m_dictionary["astro.mass_atmo_energetic"] = "大气含能核素";
        m_dictionary["astro.mass_ocean_z"] = "海洋重元素";
        m_dictionary["astro.orbit_sma"] = "轨道半长轴";
        m_dictionary["astro.orbit_period"] = "轨道周期";
        m_dictionary["astro.orbit_eccentricity"] = "轨道偏心率";
        m_dictionary["astro.life_phase"] = "生命演化阶段";
        m_dictionary["astro.civ_phase"] = "文明演化阶段";
        m_dictionary["astro.kardashev_index"] = "卡尔达舍夫指数";

        // --- AstroDataBuilder (Enums) ---
        // Star Phases
        m_dictionary["enum.star.prev_ms"] = "前主序星";
        m_dictionary["enum.star.main_seq"] = "主序星";
        m_dictionary["enum.star.red_giant"] = "红巨星";
        m_dictionary["enum.star.core_he_burn"] = "氦核燃烧";
        m_dictionary["enum.star.early_agb"] = "早期渐近巨星支";
        m_dictionary["enum.star.tp_agb"] = "热脉冲渐近巨星支";
        m_dictionary["enum.star.post_agb"] = "后渐近巨星支";
        m_dictionary["enum.star.wolf_rayet"] = "沃尔夫-拉叶星";
        m_dictionary["enum.star.he_wd"] = "氦白矮星";
        m_dictionary["enum.star.co_wd"] = "碳氧白矮星";
        m_dictionary["enum.star.onemg_wd"] = "氧氖镁白矮星";
        m_dictionary["enum.star.neutron_star"] = "中子星";
        m_dictionary["enum.star.stellar_bh"] = "恒星级黑洞";
        m_dictionary["enum.star.middle_bh"] = "中等质量黑洞";
        m_dictionary["enum.star.supermassive_bh"] = "超大质量黑洞";
        // Star Formation
        m_dictionary["enum.star_from.normal"] = "正常坍缩";
        m_dictionary["enum.star_from.wd_merge"] = "白矮星合并";
        m_dictionary["enum.star_from.slow_cooling"] = "缓慢冷却";
        m_dictionary["enum.star_from.envelope_disperse"] = "包层离散";
        m_dictionary["enum.star_from.ec_supernova"] = "电子俘获超新星";
        m_dictionary["enum.star_from.fe_core_supernova"] = "铁核坍缩超新星";
        m_dictionary["enum.star_from.jet_hypernova"] = "相对论性喷流极超新星";
        m_dictionary["enum.star_from.pair_inst_supernova"] = "不稳定对超新星";
        m_dictionary["enum.star_from.photodisintegration"] = "光致蜕变";
        // Star Outcome
        m_dictionary["enum.outcome.slow_fade"] = "缓慢熄灭";
        m_dictionary["enum.outcome.slow_cool"] = "缓慢冷却";
        m_dictionary["enum.outcome.hawking_radiation"] = "霍金辐射";
        m_dictionary["enum.outcome.envelope_disperse"] = "包层离散";
        m_dictionary["enum.outcome.ec_supernova"] = "电子俘获超新星";
        m_dictionary["enum.outcome.fe_core_supernova"] = "铁核坍缩超新星";
        m_dictionary["enum.outcome.jet_hypernova"] = "相对论性喷流极超新星";
        m_dictionary["enum.outcome.pair_inst_supernova"] = "不稳定对超新星";
        m_dictionary["enum.outcome.photodisintegration"] = "光致蜕变";
        // Star Remnant
        m_dictionary["enum.remnant.white_dwarf"] = "白矮星";
        m_dictionary["enum.remnant.neutron_star"] = "中子星";
        m_dictionary["enum.remnant.black_hole"] = "黑洞";
        // Planet Types
        m_dictionary["enum.planet.rocky"] = "岩石行星";
        m_dictionary["enum.planet.terra"] = "类地行星";
        m_dictionary["enum.planet.ice"] = "冰行星";
        m_dictionary["enum.planet.chthonian"] = "冥府行星";
        m_dictionary["enum.planet.oceanic"] = "海洋行星";
        m_dictionary["enum.planet.sub_ice_giant"] = "亚冰巨星";
        m_dictionary["enum.planet.ice_giant"] = "冰巨星";
        m_dictionary["enum.planet.gas_giant"] = "气态巨行星";
        m_dictionary["enum.planet.hot_sub_ice_giant"] = "热亚冰巨星";
        m_dictionary["enum.planet.hot_ice_giant"] = "热冰巨星";
        m_dictionary["enum.planet.hot_gas_giant"] = "热木星";
        m_dictionary["enum.planet.rocky_asteroid"] = "岩石小行星群";
        m_dictionary["enum.planet.rocky_ice_asteroid"] = "岩冰小行星群";
        m_dictionary["enum.planet.artificial_cluster"] = "人造轨道结构群";
    }
}
_SYSTEM_END
_NPGS_END