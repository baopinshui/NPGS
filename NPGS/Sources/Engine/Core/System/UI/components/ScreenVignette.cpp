#include "ScreenVignette.h"
#include "imgui_internal.h" // 访问 ImDrawListSharedData 等内部 ImGui 结构

_NPGS_BEGIN
_SYSTEM_BEGIN
_UI_BEGIN

ScreenVignette::ScreenVignette()
{
    m_block_input = false;
    // [NEW] 设置默认布局属性，使其在父容器中自动拉伸
    m_width = Length::Stretch();
    m_height = Length::Stretch();
}

void ScreenVignette::Update(float dt)
{
    // [MODIFIED] 不再手动计算尺寸和位置，这些由 Arrange 阶段处理
    // Update 只负责状态更新（此组件无状态）
    UIElement::Update(dt);
}

void ScreenVignette::Draw(ImDrawList* dl)
{
    if (!m_visible || m_alpha <= 0.01f || m_feather <= 0.0f) return;

    // [MODIFIED] 使用 Arrange 阶段计算出的 m_rect 和 m_absolute_pos
    // 不再依赖 UIContext 的 display_size
    if (m_rect.w <= 0 || m_rect.h <= 0) return;

    const ImVec2 p_min = m_absolute_pos;
    const ImVec2 p_max = ImVec2(m_absolute_pos.x + m_rect.w, m_absolute_pos.y + m_rect.h);

    // 1. 颜色计算
    ImU32 col_outer = GetColorWithAlpha(ImVec4(m_color.x, m_color.y, m_color.z, 0.5f), m_alpha);
    ImU32 col_inner = GetColorWithAlpha(ImVec4(m_color.x, m_color.y, m_color.z, 0.0f), m_alpha);

    // 2. 几何计算 - 只计算垂直方向的羽化
    float feather_y = m_rect.h * m_feather * 0.5f;

    // 定义8个顶点
    // 0-3: 外圈 (左上, 右上, 右下, 左下)
    // 4-7: 内圈 (左上, 右上, 右下, 左下)
    ImVec2 verts[8];
    verts[0] = ImVec2(p_min.x, p_min.y);                    // Outer TL
    verts[1] = ImVec2(p_max.x, p_min.y);                    // Outer TR
    verts[2] = ImVec2(p_max.x, p_max.y);                    // Outer BR
    verts[3] = ImVec2(p_min.x, p_max.y);                    // Outer BL

    verts[4] = ImVec2(p_min.x, p_min.y + feather_y);        // Inner TL
    verts[5] = ImVec2(p_max.x, p_min.y + feather_y);        // Inner TR
    verts[6] = ImVec2(p_max.x, p_max.y - feather_y);        // Inner BR
    verts[7] = ImVec2(p_min.x, p_max.y - feather_y);        // Inner BL

    // 3. 准备绘制
    const int num_vertices = 8;
    const int num_indices = 12; // 只需要2个矩形（4个三角形），上下各一个
    dl->PrimReserve(num_indices, num_vertices);

    ImDrawVert* vtx_write = dl->_VtxWritePtr;
    ImDrawIdx* idx_write = dl->_IdxWritePtr;
    ImDrawIdx current_idx = (ImDrawIdx)dl->_VtxCurrentIdx;
    ImVec2 uv_white = dl->_Data->TexUvWhitePixel;

    // A. 写入8个顶点数据
    vtx_write[0].pos = verts[0]; vtx_write[0].uv = uv_white; vtx_write[0].col = col_outer;
    vtx_write[1].pos = verts[1]; vtx_write[1].uv = uv_white; vtx_write[1].col = col_outer;
    vtx_write[2].pos = verts[2]; vtx_write[2].uv = uv_white; vtx_write[2].col = col_outer;
    vtx_write[3].pos = verts[3]; vtx_write[3].uv = uv_white; vtx_write[3].col = col_outer;

    vtx_write[4].pos = verts[4]; vtx_write[4].uv = uv_white; vtx_write[4].col = col_inner;
    vtx_write[5].pos = verts[5]; vtx_write[5].uv = uv_white; vtx_write[5].col = col_inner;
    vtx_write[6].pos = verts[6]; vtx_write[6].uv = uv_white; vtx_write[6].col = col_inner;
    vtx_write[7].pos = verts[7]; vtx_write[7].uv = uv_white; vtx_write[7].col = col_inner;

    // B. 写入索引 - 只绘制上下两个矩形
    // 上侧矩形（三角形1: 0-1-5, 三角形2: 0-5-4）
    idx_write[0] = current_idx + 0; idx_write[1] = current_idx + 1; idx_write[2] = current_idx + 5;
    idx_write[3] = current_idx + 0; idx_write[4] = current_idx + 5; idx_write[5] = current_idx + 4;

    // 下侧矩形（三角形1: 3-6-2, 三角形2: 3-7-6） -> 顶点顺序修正为 3-2-6 和 3-6-7
    idx_write[6] = current_idx + 3; idx_write[7] = current_idx + 2; idx_write[8] = current_idx + 6;
    idx_write[9] = current_idx + 3; idx_write[10] = current_idx + 6; idx_write[11] = current_idx + 7;


    // 更新指针
    dl->_VtxWritePtr += num_vertices;
    dl->_IdxWritePtr += num_indices;
    dl->_VtxCurrentIdx += num_vertices;
}
_UI_END
_SYSTEM_END
_NPGS_END