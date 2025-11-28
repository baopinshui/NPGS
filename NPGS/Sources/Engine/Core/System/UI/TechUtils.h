#pragma once
#include "ui_framework.h"
#include <cmath>

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

class TechUtils
{
public:
    // 强制像素对齐，解决边缘发虚问题
    static inline ImVec2 Snap(ImVec2 p)
    {
        return ImVec2(std::floor(p.x) + 0.5f, std::floor(p.y) + 0.5f);
    }
    
    static inline float Snap(float p)
    {
        return std::floor(p) + 0.5f;
    }
    
    static ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t)
    {
        return ImVec4(
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t
        );
    }
    
    static void DrawCorner(ImDrawList* dl, ImVec2 p_corner, float arm_len_x, float arm_len_y, float thickness, ImU32 col)
    {
        // 1. 临时关闭抗锯齿，获得绝对硬朗的边缘（科技感关键）
        ImDrawListFlags backup_flags = dl->Flags;
        dl->Flags &= ~ImDrawListFlags_AntiAliasedLines;
        dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;

        float half_t = thickness * 0.5f;

        // 计算中心点的像素对齐坐标
        // 我们以中心点向四周扩散 thickness/2
        float cx = p_corner.x;
        float cy = p_corner.y;

        // 定义两个矩形的范围
        ImVec2 h_min, h_max; // 水平矩形
        ImVec2 v_min, v_max; // 垂直矩形

        // --- 计算水平矩形 (包含拐角部分) ---
        // Y轴范围：中心上下各 half_t
        h_min.y = Snap(cy - half_t);
        h_max.y = Snap(cy + half_t);

        // X轴范围：从中心延伸到 arm_len_x
        // 为了防止像素偏差，我们统一先算两端，再排序
        float x_end = cx + arm_len_x;
        h_min.x = Snap(std::min(cx - half_t, x_end)); // 注意：包含拐角那一侧的宽度
        h_max.x = Snap(std::max(cx + half_t, x_end));
        // 如果 arm_len_x 是负数 (向左)，上面 min/max 会自动处理

        // 修正：上面的逻辑会导致向左延伸时，拐角处的宽度不够或错位。
        // 更严谨的方法：
        // 始终让“水平矩形”占据整个拐角区域。
        // 水平矩形 X 范围： [Min(Start, End), Max(Start, End)]
        // 其中 Start 是拐角的“背侧”边缘，End 是臂的尖端。

        // 让我们简化逻辑：
        // 矩形1 (Horizontal): 负责覆盖拐角中心 + X轴臂
        // 矩形2 (Vertical): 负责 Y轴臂，但要避开与矩形1的重叠区

        float x_start = cx - (arm_len_x > 0 ? half_t : -half_t); // 拐角背侧
        float x_tip = cx + arm_len_x;
        h_min.x = Snap(std::min(x_start, x_tip));
        h_max.x = Snap(std::max(x_start, x_tip));

        // 绘制水平矩形
        dl->AddRectFilled(h_min, h_max, col);

        // --- 计算垂直矩形 (避开重叠) ---
        // X轴范围：中心左右各 half_t
        v_min.x = Snap(cx - half_t);
        v_max.x = Snap(cx + half_t);

        // Y轴范围：从“拐角边缘”延伸到 arm_len_y
        // 关键点：起始点要避开水平矩形的厚度
        float y_start_offset = (arm_len_y > 0 ? half_t : -half_t);
        float y_start = cy + y_start_offset; // 从水平条的边缘开始
        float y_tip = cy + arm_len_y;

        v_min.y = Snap(std::min(y_start, y_tip));
        v_max.y = Snap(std::max(y_start, y_tip));

        // 绘制垂直矩形
        dl->AddRectFilled(v_min, v_max, col);

        // 3. 恢复抗锯齿设置
        dl->Flags = backup_flags;
    }
    
    static void DrawHardLine(ImDrawList* dl, ImVec2 p1, ImVec2 p2, ImU32 col, float thickness)
    {
        // 1. 临时关闭抗锯齿，获得绝对硬朗的边缘
        ImDrawListFlags backup_flags = dl->Flags;
        dl->Flags &= ~ImDrawListFlags_AntiAliasedLines;
        dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;

        // 判断是否为水平或垂直线（通常 UI 边框都是水平垂直的）
        // 使用矩形填充来模拟线段，以保证和 DrawCorner 的厚度算法完全一致
        if (std::abs(p1.y - p2.y) < 0.001f || std::abs(p1.x - p2.x) < 0.001f)
        {
            float half_t = thickness * 0.5f;
            ImVec2 rect_min, rect_max;

            if (std::abs(p1.y - p2.y) < 0.001f) // 水平线
            {
                // Y轴：中心上下各 half_t，使用 Snap 对齐
                rect_min.y = Snap(p1.y - half_t);
                rect_max.y = Snap(p1.y + half_t);

                // X轴：直接取两端点，使用 Snap 对齐
                // 注意：这里如果不希望线段两头“突出”半个厚度，直接取端点即可
                // 如果希望模拟 AddLine 的方头(Projecting Cap)效果，可以像 DrawCorner 一样 +/- half_t
                // 这里采用标准对齐：
                float x1 = std::min(p1.x, p2.x);
                float x2 = std::max(p1.x, p2.x);
                rect_min.x = Snap(x1);
                rect_max.x = Snap(x2);
            }
            else // 垂直线
            {
                // X轴：中心左右各 half_t
                rect_min.x = Snap(p1.x - half_t);
                rect_max.x = Snap(p1.x + half_t);

                // Y轴范围
                float y1 = std::min(p1.y, p2.y);
                float y2 = std::max(p1.y, p2.y);
                rect_min.y = Snap(y1);
                rect_max.y = Snap(y2);
            }

            dl->AddRectFilled(rect_min, rect_max, col);
        }
        else
        {
            // 如果是斜线，回退到关闭 AA 的 AddLine
            // 对端点进行 Snap 处理以保证起止点在像素中心
            dl->AddLine(Snap(p1), Snap(p2), col, thickness);
        }

        // 3. 恢复抗锯齿设置
        dl->Flags = backup_flags;
    }

    static void DrawBracketedBox(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, ImU32 col, float thickness, float corner_len, float offset = 0.0f)
    {
        float half_t = thickness * 0.5f;

        // 自动计算带偏移的坐标
        float x1 = p_min.x + offset;
        float y1 = p_min.y + offset;
        float x2 = p_max.x - offset;
        float y2 = p_max.y - offset;

        // 左上
        DrawCorner(dl, ImVec2(x1 + half_t, y1 + half_t), corner_len, corner_len, thickness, col);
        // 右上
        DrawCorner(dl, ImVec2(x2 - half_t, y1 + half_t), -corner_len, corner_len, thickness, col);
        // 左下
        DrawCorner(dl, ImVec2(x1 + half_t, y2 - half_t), corner_len, -corner_len, thickness, col);
        // 右下
        DrawCorner(dl, ImVec2(x2 - half_t, y2 - half_t), -corner_len, -corner_len, thickness, col);
    }

};

_UI_END
_SYSTEM_END
_NPGS_END