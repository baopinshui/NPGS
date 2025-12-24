#pragma once
#include "../ui_framework.h"
#include <string>
#include <variant>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

// --- 1. 粒子视图 (保持不变) ---
struct Particle
{
    float x, y, vx, vy, size;
    bool active;
};

class NeuralParticleView : public UIElement
{
public:
    std::vector<Particle> m_particles;

    // [新增]用于控制逻辑的状态变量
    ImVec2 m_collapsed_size;
    ImVec2 m_expanded_size;
    bool m_is_expanded = false;   // 目标状态
    bool m_is_animating = false;  // 是否正在动画中
    NeuralParticleView(int particle_count = 80);

    // [新增] 设置状态的方法
    void SetState(bool expanded, bool animating);
    void SetSizes(ImVec2 collapsed, ImVec2 expanded);

    void Update(float dt) override;
    void Draw(ImDrawList* draw_list) override;
    void HandleMouseEvent(const ImVec2& mouse_pos, bool mouse_down, bool mouse_clicked, bool mouse_released, bool& handled) override;
private:
    void SpawnParticle(Particle& p, float w, float h);
    bool m_is_hovered = false;
};

_UI_END
_SYSTEM_END
_NPGS_END