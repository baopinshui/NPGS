#include "NeuralParticleView.h"

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

NeuralParticleView::NeuralParticleView(int count)
{
    m_particles.resize(count);
    // 初始化默认尺寸，防止除以零，会在 Controller 中被覆盖
    m_collapsed_size = ImVec2(80, 80);
    m_expanded_size = ImVec2(320, 256);

    for (auto& p : m_particles)
    {
        p.active = false; // 初始全部非激活，由 Update 接管
        SpawnParticle(p, 100, 100);
    }
    m_block_input = false;
}

void NeuralParticleView::SetState(bool expanded, bool animating)
{
    m_is_expanded = expanded;
    m_is_animating = animating;
}

void NeuralParticleView::SetSizes(ImVec2 collapsed, ImVec2 expanded)
{
    m_collapsed_size = collapsed;
    m_expanded_size = expanded;
}

void NeuralParticleView::SpawnParticle(Particle& p, float w, float h)
{
    // 基础随机生成
    p.x = (float(rand()) / RAND_MAX) * w;
    p.y = (float(rand()) / RAND_MAX) * h;
    p.vx = ((float(rand()) / RAND_MAX) - 0.5f) * 0.8f;
    p.vy = ((float(rand()) / RAND_MAX) - 0.5f) * 0.8f;
    p.size = (float(rand()) / RAND_MAX) * 1.5f + 1.0f;
}

void NeuralParticleView::Update(float dt, const ImVec2& parent_abs_pos)
{
    UIElement::Update(dt, parent_abs_pos);

    // [修复]：使用几何检测代替 m_hovered，解决被按钮遮挡无法检测悬停的问题
    bool visual_hover = false;
    if (m_parent)
    {
        ImVec2 p_min = m_parent->m_absolute_pos;
        ImVec2 p_max = ImVec2(p_min.x + m_parent->m_rect.w, p_min.y + m_parent->m_rect.h);
        Rect parent_rect = { p_min.x,p_min.y, m_parent->m_rect.w, m_parent->m_rect.h };
        visual_hover = parent_rect.Contains(ImGui::GetIO().MousePos);
    }
    // [修复]：严格还原 menu.cpp 的速度逻辑
    // menu.cpp: const bool effective_hover = expanded ? false : hovered;
    // menu.cpp: float speed_multiplier = effective_hover ? 3.5f : (expanded ? 1.2f : 1.9f);

    // 注意：这里用 m_rect.w > 100 作为判断是否展开的辅助依据，因为 m_is_expanded 只是目标状态
    // 如果正在动画中或者已经展开，视为 "expanded"
    bool is_visually_expanded = m_is_expanded || m_is_animating || (m_rect.w > 150.0f);

    bool effective_hover = is_visually_expanded ? false : visual_hover;
    float speed_multiplier = effective_hover ? 3.5f : (is_visually_expanded ? 1.2f : 1.9f);

    // [移植核心逻辑 1]：计算当前应该激活多少粒子
    // ... (后续代码保持不变，直接复制原有的逻辑即可) ...
    float current_area = m_rect.w * m_rect.h;
    float max_area = m_expanded_size.x * m_expanded_size.y;
    float min_area = m_collapsed_size.x * m_collapsed_size.y;

    float progress = 0.0f;
    if (max_area > min_area)
    {
        progress = std::clamp((current_area - min_area) / (max_area - min_area), 0.0f, 1.0f);
    }

    int total_particles = static_cast<int>(m_particles.size());
    int min_particles = static_cast<int>(total_particles * (min_area / max_area));
    min_particles = std::clamp(min_particles, 20, total_particles);

    int target_active_count = min_particles + static_cast<int>((total_particles - min_particles) * progress);

    // [移植核心逻辑 2]：确定物理边界
    float physics_w = (m_is_expanded || m_is_animating) ? m_expanded_size.x : m_collapsed_size.x;
    float physics_h = (m_is_expanded || m_is_animating) ? m_expanded_size.y : m_collapsed_size.y;

    for (int i = 0; i < total_particles; i++)
    {
        auto& p = m_particles[i];

        if (i < target_active_count)
        {
            if (!p.active)
            {
                p.active = true;
                // ... (生成逻辑保持不变) ...
                if (progress > 0.1f)
                {
                    float collapsed_w = m_collapsed_size.x;
                    float collapsed_h = m_collapsed_size.y;
                    float right_area = (m_expanded_size.x - collapsed_w) * m_expanded_size.y;
                    float bottom_area = collapsed_w * (m_expanded_size.y - collapsed_h);
                    float total_new_area = right_area + bottom_area;

                    float split_chance = (float(rand()) / RAND_MAX);
                    bool spawn_on_right = (total_new_area > 0) ? (split_chance < (right_area / total_new_area)) : true;

                    if (spawn_on_right)
                    {
                        p.x = collapsed_w + (float(rand()) / RAND_MAX) * (m_expanded_size.x - collapsed_w);
                        p.y = (float(rand()) / RAND_MAX) * m_expanded_size.y;
                    }
                    else
                    {
                        p.x = (float(rand()) / RAND_MAX) * collapsed_w;
                        p.y = collapsed_h + (float(rand()) / RAND_MAX) * (m_expanded_size.y - collapsed_h);
                    }
                    p.vx = ((float(rand()) / RAND_MAX) - 0.5f) * 0.8f;
                    p.vy = ((float(rand()) / RAND_MAX) - 0.5f) * 0.8f;
                }
                else
                {
                    p.x = (float(rand()) / RAND_MAX) * m_collapsed_size.x;
                    p.y = (float(rand()) / RAND_MAX) * m_collapsed_size.y;
                }
            }
        }
        else
        {
            p.active = false;
        }

        if (!p.active) continue;

        p.x += p.vx * speed_multiplier * dt * 60.0f;
        p.y += p.vy * speed_multiplier * dt * 60.0f;

        if (p.x < 0) { p.x = 0; p.vx = std::abs(p.vx); }
        if (p.x > physics_w) { p.x = physics_w; p.vx = -std::abs(p.vx); }
        if (p.y < 0) { p.y = 0; p.vy = std::abs(p.vy); }
        if (p.y > physics_h) { p.y = physics_h; p.vy = -std::abs(p.vy); }

        if (!m_is_expanded && !m_is_animating)
        {
            if (p.x > m_collapsed_size.x) p.x = (float(rand()) / RAND_MAX) * m_collapsed_size.x;
            if (p.y > m_collapsed_size.y) p.y = (float(rand()) / RAND_MAX) * m_collapsed_size.y;
        }
    }
}
void NeuralParticleView::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f) return;

    // [修复 Start] ==========================================
    // 不依赖 m_parent->m_hovered，直接进行几何范围检测
    bool hovered = false;
    if (m_parent)
    {
        ImVec2 p_min = m_parent->m_absolute_pos;
        ImVec2 p_max = ImVec2(p_min.x + m_parent->m_rect.w, p_min.y + m_parent->m_rect.h);
        Rect parent_rect = { p_min.x,p_min.y, m_parent->m_rect.w, m_parent->m_rect.h };
        hovered = parent_rect.Contains(ImGui::GetIO().MousePos);
    }
    // === 逻辑对齐 HTML ===
// JS: const baseConnectDist = isExpanded ? 100 : 50;
    float base_dist = m_is_expanded ? 100.0f : 50.0f;

    // 定义“高亮”状态：只有在悬停 且 未展开 时触发增强效果
    // JS: (hovered && !isExpanded)
    bool is_highlight_state = (hovered && !m_is_expanded);

    // 计算最大连接距离
    // JS: const connectDist = (hovered && !isExpanded) ? baseConnectDist * 1.2 : baseConnectDist;
    float max_dist = is_highlight_state ? (base_dist * 1.2f) : base_dist;
    if (m_is_expanded) { max_dist *= 0.8; }
    
    float max_dist_sq = max_dist * max_dist;

    ImVec2 offset = m_absolute_pos;
    const auto& theme = UIContext::Get().m_theme;

    // 绘制连线
    for (size_t i = 0; i < m_particles.size(); i++)
    {
        auto& p1 = m_particles[i];
        if (!p1.active) continue;

        for (size_t j = i + 1; j < m_particles.size(); j++)
        {
            auto& p2 = m_particles[j];
            if (!p2.active) continue;

            float dx = p1.x - p2.x;
            float dy = p1.y - p2.y;
            float dist_sq = dx * dx + dy * dy;

            if (dist_sq < max_dist_sq)
            {
                float dist = std::sqrt(dist_sq);

                // === 透明度逻辑对齐 HTML ===
                // JS: const alpha = 1 - dist/connectDist;
                float alpha_factor = 1.0f - dist / max_dist;
                if (m_is_expanded) { alpha_factor *= 0.6; }
                // JS: const finalAlpha = (hovered && !isExpanded) ? Math.min(1, alpha + 0.2) : alpha;
                if (is_highlight_state)
                {
                    alpha_factor = std::min(1.0f, alpha_factor + 0.2f);
                }

                // 叠加控件整体的淡入淡出 alpha
                ImU32 col = GetColorWithAlpha(theme.color_accent, alpha_factor * m_alpha);

                // === 线条粗细逻辑对齐 HTML ===
                // JS: const lineWidth = (hovered && !isExpanded) ? 1.0 : 0.5;
                float thickness = is_highlight_state ? 1.0f : 0.5f;

                dl->AddLine(
                    ImVec2(offset.x + p1.x, offset.y + p1.y),
                    ImVec2(offset.x + p2.x, offset.y + p2.y),
                    col,
                    thickness
                );
            }
        }
    }

    // 绘制粒子点
    ImU32 particle_col = GetColorWithAlpha(theme.color_accent, m_alpha);
    for (const auto& p : m_particles)
    {
        if (p.active)
            dl->AddCircleFilled(ImVec2(offset.x + p.x, offset.y + p.y), p.size, particle_col);
    }
}
_UI_END
_SYSTEM_END
_NPGS_END