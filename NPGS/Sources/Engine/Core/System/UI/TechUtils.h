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

    static void DrawGradientLineSegment(ImDrawList* dl, ImVec2 p1, ImVec2 p2, float thickness, ImU32 col_start, ImU32 col_end)
    {
        float half_t = thickness * 0.5f;
        ImVec2 rect_min, rect_max;

        // 判断是水平还是垂直
        if (std::abs(p1.y - p2.y) < 0.01f) // 水平
        {
            float x_min = std::min(p1.x, p2.x);
            float x_max = std::max(p1.x, p2.x);

            // 垂直方向扩展厚度
            rect_min = ImVec2(x_min, p1.y - half_t);
            rect_max = ImVec2(x_max, p1.y + half_t);

            // 像素对齐
            rect_min = Snap(rect_min);
            rect_max = Snap(rect_max);

            if (p1.x < p2.x) // 左 -> 右
                dl->AddRectFilledMultiColor(rect_min, rect_max, col_start, col_end, col_end, col_start);
            else // 右 -> 左
                dl->AddRectFilledMultiColor(rect_min, rect_max, col_end, col_start, col_start, col_end);
        }
        else // 垂直
        {
            float y_min = std::min(p1.y, p2.y);
            float y_max = std::max(p1.y, p2.y);

            // 水平方向扩展厚度
            rect_min = ImVec2(p1.x - half_t, y_min);
            rect_max = ImVec2(p1.x + half_t, y_max);

            // 像素对齐
            rect_min = Snap(rect_min);
            rect_max = Snap(rect_max);

            if (p1.y < p2.y) // 上 -> 下
                dl->AddRectFilledMultiColor(rect_min, rect_max, col_start, col_start, col_end, col_end);
            else // 下 -> 上
                dl->AddRectFilledMultiColor(rect_min, rect_max, col_end, col_end, col_start, col_start);
        }
    }

    static void DrawGradientFlow(ImDrawList* dl, ImVec2 base_p_min, ImVec2 base_p_max, float offset, float thickness, ImU32 col, float progress, float total_len_ratio, bool is_gradient, bool is_ccw, int segment_count = 1, float randomness = 0.0f)
    {
        if (segment_count < 1) segment_count = 1;

        // 基础几何计算
        float half_t = thickness * 0.5f;
        ImVec2 p_min = ImVec2(base_p_min.x + offset + half_t, base_p_min.y + offset + half_t);
        ImVec2 p_max = ImVec2(base_p_max.x - offset - half_t, base_p_max.y - offset - half_t);

        if (p_max.x <= p_min.x || p_max.y <= p_min.y) return;

        float w = p_max.x - p_min.x;
        float h = p_max.y - p_min.y;
        float lens[4] = { w, h, w, h };
        float perimeter = 2.0f * (w + h);
        if (perimeter <= 0.001f) return;

        ImVec4 base_col = ImGui::ColorConvertU32ToFloat4(col);

        // --- Step 1: 动态计算每一段的长度权重 ---

        // 使用局部静态buffer避免每帧new，或者直接用std::vector
        std::vector<float> segment_lengths;
        segment_lengths.resize(segment_count);

        float total_weight = 0.0f;
        float PI_2 = 6.2831853f;

        for (int k = 0; k < segment_count; ++k)
        {
            float weight = 1.0f;
            if (randomness > 0.01f)
            {
                // 伪随机连续函数：
                // 使用 progress 作为时间轴，保证随时间平滑变化
                // 使用 k 作为相位偏移，保证不同段的变化不同步
                // 乘数 2.0f 决定了变化的快慢频率
                float phase = progress * PI_2 * 2.0f + (float)k * (PI_2 / 1.618f); // 1.618 黄金分割防止谐波同步

                // sin 的结果是 -1~1，weight 结果范围 [1-randomness, 1+randomness]
                weight = 1.0f + randomness * std::sin(phase);

                // 防止负长度或过短
                if (weight < 0.1f) weight = 0.1f;
            }
            segment_lengths[k] = weight;
            total_weight += weight;
        }

        // --- Step 2: 归一化并计算布局 ---

        // 计算这一帧所有段加起来的总间隙 (0~1 空间)
        float total_gap_ratio = std::max(0.0f, 1.0f - total_len_ratio);
        // 单个间隙大小 (均匀分配间隙)
        float single_gap_ratio = (segment_count > 0) ? (total_gap_ratio / (float)segment_count) : 0.0f;

        // 游标：记录当前这一段的"头部"位置 (0~1空间)
        // 初始位置就是传入的 progress
        float current_head_cursor = progress;

        // --- Step 3: 循环绘制 ---
        for (int k = 0; k < segment_count; ++k)
        {
            // A. 计算当前段的实际长度占比
            // (权重 / 总权重) * 总长度占比
            float current_len_ratio = (segment_lengths[k] / total_weight) * total_len_ratio;
            float trail_px = current_len_ratio * perimeter;

            // B. 计算头尾坐标 (0~1空间)
            // 归一化游标到 0~1 范围用于计算坐标
            float eff_head_ratio = current_head_cursor - std::floor(current_head_cursor);
            if (eff_head_ratio < 0) eff_head_ratio += 1.0f;

            float pos_head, pos_tail;

            if (!is_ccw) // CW (顺时针)
            {
                pos_head = eff_head_ratio * perimeter;
                pos_tail = pos_head - trail_px;
            }
            else // CCW (逆时针)
            {
                // CCW 时，progress 增加代表逆时针转。
                // 我们的游标 current_head_cursor 依然按照 0->1 逻辑处理相对位置
                // 但映射到物理坐标时取反
                pos_head = (1.0f - eff_head_ratio) * perimeter;
                pos_tail = pos_head + trail_px;
            }

            // C. 生成绘制区间 (处理跨越 0/1 边界的情况)
            struct DrawRange { float start; float end; };
            DrawRange ranges[2];
            int range_count = 0;

            if (!is_ccw) // CW
            {
                if (pos_tail < 0.0f)
                {
                    ranges[range_count++] = { perimeter + pos_tail, perimeter };
                    ranges[range_count++] = { 0.0f, pos_head };
                }
                else
                {
                    ranges[range_count++] = { pos_tail, pos_head };
                }
            }
            else // CCW
            {
                if (pos_tail > perimeter)
                {
                    ranges[range_count++] = { pos_head, perimeter };
                    ranges[range_count++] = { 0.0f, pos_tail - perimeter };
                }
                else
                {
                    ranges[range_count++] = { pos_head, pos_tail };
                }
            }

            // D. 绘制逻辑 (与之前相同，只是套在循环里)
            float side_start_accum = 0.0f;
            for (int i = 0; i < 4; ++i)
            {
                float side_len = lens[i];
                float side_end_accum = side_start_accum + side_len;

                for (int r = 0; r < range_count; ++r)
                {
                    float overlap_start = std::max(ranges[r].start, side_start_accum);
                    float overlap_end = std::min(ranges[r].end, side_end_accum);

                    if (overlap_start < overlap_end)
                    {
                        // Alpha 渐变计算
                        float alpha_s = 1.0f, alpha_e = 1.0f;
                        if (is_gradient)
                        {
                            float dist_s, dist_e;
                            if (!is_ccw)
                            {
                                float eff_head = (pos_tail < 0.0f && overlap_start > pos_head) ? (pos_head + perimeter) : pos_head;
                                dist_s = eff_head - overlap_start;
                                dist_e = eff_head - overlap_end;
                            }
                            else
                            {
                                dist_s = overlap_start - pos_head;
                                if (dist_s < 0) dist_s += perimeter;
                                dist_e = overlap_end - pos_head;
                                if (dist_e < 0) dist_e += perimeter;
                            }
                            if (trail_px > 0.001f)
                            {
                                alpha_s = std::clamp(1.0f - (dist_s / trail_px), 0.0f, 1.0f);
                                alpha_e = std::clamp(1.0f - (dist_e / trail_px), 0.0f, 1.0f);
                            }
                        }

                        // 坐标映射
                        float local_s = overlap_start - side_start_accum;
                        float local_e = overlap_end - side_start_accum;
                        ImVec2 p1, p2;
                        bool touches_start = (local_s <= 0.01f);
                        bool touches_end = (local_e >= side_len - 0.01f);

                        // 根据边索引计算物理坐标 (Top, Right, Bottom, Left)
                        if (i == 0)
                        { // Top
                            p1 = ImVec2(p_min.x + local_s, p_min.y); p2 = ImVec2(p_min.x + local_e, p_min.y);
                            if (touches_start) p1.x -= half_t; if (touches_end) p2.x += half_t;
                        }
                        else if (i == 1)
                        { // Right
                            p1 = ImVec2(p_max.x, p_min.y + local_s); p2 = ImVec2(p_max.x, p_min.y + local_e);
                            float y_min = p_min.y + half_t, y_max = p_max.y - half_t;
                            if (p1.y < y_min) p1.y = y_min; if (p2.y > y_max) p2.y = y_max;
                            if (p1.y >= p2.y) continue;
                        }
                        else if (i == 2)
                        { // Bottom
                            p1 = ImVec2(p_max.x - local_s, p_max.y); p2 = ImVec2(p_max.x - local_e, p_max.y);
                            if (touches_start) p1.x += half_t; if (touches_end) p2.x -= half_t;
                        }
                        else
                        { // Left
                            p1 = ImVec2(p_min.x, p_max.y - local_s); p2 = ImVec2(p_min.x, p_max.y - local_e);
                            float y_min = p_min.y + half_t, y_max = p_max.y - half_t;
                            if (p1.y > y_max) p1.y = y_max; if (p2.y < y_min) p2.y = y_min;
                            if (p2.y >= p1.y) continue;
                        }

                        ImU32 col_s = ImGui::ColorConvertFloat4ToU32(ImVec4(base_col.x, base_col.y, base_col.z, base_col.w * alpha_s));
                        ImU32 col_e = ImGui::ColorConvertFloat4ToU32(ImVec4(base_col.x, base_col.y, base_col.z, base_col.w * alpha_e));
                        DrawGradientLineSegment(dl, p1, p2, thickness, col_s, col_e);
                    }
                }
                side_start_accum += side_len;
            }

            // --- Step 4: 移动游标到下一段 ---
            // 游标向"后"移动：移动量 = 当前段长度 + 间隙
            // 在 0~1 的进度条上，"流光"通常是向前流动的，所以下一个段应该在当前段的"后面"
            // 即 progress 值更小的地方。
            current_head_cursor -= (current_len_ratio + single_gap_ratio);
        }
    }
};

_UI_END
_SYSTEM_END
_NPGS_END